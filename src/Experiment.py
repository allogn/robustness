import pandas as pd
import numpy as np
from collections import defaultdict
from tqdm import tqdm
import logging
import signal
import uuid

import sys, os
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

from Generator import *
from ParallelRobustWrapper import *

from NetShieldSolver import *
from DegreeSolver import *
from RandomSolver import *
from PagerankSolver import *
from BetweennessSolver import *
from KatzSolver import *
from AquaintanceSolver import *

from MongoDatabase import *
from ParameterManager import *
from FileDatabase import *

from GraphAnalyzer import *

class SolverTimeoutException(Exception):
    pass
def handler(signum, frame):
    raise SolverTimeoutException()

class Experiment:

    def __init__(self, parameters = {}, G = None, verbose=True):
        self.pm = ParameterManager(parameters)
        self.tag = self.pm.get('tag')

        self.db_wrapper = MongoDatabase(self.tag)
        self.db = self.db_wrapper.get_db()
        self.pw = ParallelRobustWrapper(self.tag, verbose)
        self.fm = FileManager(self.tag)
        self.file_db = FileDatabase(self.tag)
        self.manual_G = G
        self.verbose = verbose

        if self.pm.get('full_rerun') == 1:
            self.clear()
            parameters = self.pm.get_all_params()
            self.db.parameters.insert_one(parameters)

    def clear(self):
        logging.info("Cleaning data path")
        self.fm.clean_data_path()
        self.file_db.clean()
        q = {"tag": self.tag}
        self.db.graphs.delete_many(q)
        self.db.immunization.delete_many(q)
        self.db.simple_seeding.delete_many(q)
        self.db.imm_simulations.delete_many(q)
        self.db.seeding.delete_many(q)
        self.db.analysis.delete_many(q)
        self.db.parameters.delete_many(q)

    def generate_datasets(self, force=False):
        if force:
            self.db.graphs.delete_many({'tag': self.tag})
        if self.db.graphs.find_one({'tag': self.tag}) != None:
            logging.info("Dataset for {} has been found".format(self.tag))
            return
        total_datasets = list(self.pm.get_data_param_sets())
        if len(total_datasets) == 0:
            raise Exception("No datasets generated, bad parameter values")
        generated = 0
        for p in total_datasets:
            if p.get('manual',0) == 1:
                if self.manual_G is None:
                    raise Exception("You must pass graph into Experiment second parameter")
                G = self.manual_G
            else:
                gen = Generator(p)
                G = gen.get()
            graph_info = G.graph
            graph_info['adjlist_path'] = Generator.save_to_adjlist(self.tag, G)
            graph_info['pickle_path'] = Generator.save_to_pickle(self.tag, G)
            graph_info['tag'] = self.tag
            self.db.graphs.insert_one(graph_info)
            generated += 1
        return generated


    def run_immunization(self, force=False):
        if force:
            self.db.immunization.delete_many({'tag': self.tag})
        if self.db.immunization.find_one({'tag': self.tag}) != None:
            return

        # find all datasets for specific tag
        q = {"tag": self.tag}
        total_datasets = self.db.graphs.count_documents(q)
        if (total_datasets == 0):
            raise Exception("No graphs found for the tag")
        all_imm_params = list(self.pm.get_immunization_params())
        if self.verbose:
            t = tqdm.tqdm(total=total_datasets * len(all_imm_params), desc='Immunization')
        for d in self.db.graphs.find(q):
            G = helpers.load_graph(d['pickle_path'])
            logging.info("Running immunization for Graph {}".format(G.graph["graph_id"]))

            for solver_params in all_imm_params:
                solver_name = solver_params['solver_name']
                logging.info("Running {}".format(solver_name))
                #if self.db.immunization.count({"tag": self.tag, "solver": solver_name}) > 0:
                 #   logging.info("{} Immunization for {} has been found".format(solver_name, self.tag))
                  #  t.update()
                   # t.refresh()
                    #continue

                Solver = eval(solver_name + "Solver")
                z = dict(solver_params)

                solver = Solver(G, **z)
                signal.signal(signal.SIGALRM, handler)
                signal.alarm(solver_params['solver_timeout'])
                solver.run()  # may raise a timeout exception that should be caught by airflow
                solution_path = self.file_db.get_full_path(self.file_db.save(solver.log))
                signal.alarm(0)
                # todo check graph loading from relative path

                results = {"tag": self.tag}
                results['graph'] = d['_id']
                results['solver'] = solver_name
                results['solver_params'] = z
                results['solution_path'] = solution_path
                self.db.immunization.insert_one(results)

                if self.verbose:
                    t.update()
                    t.refresh()
        if self.verbose:
            t.close()

    def run_satgreedy(self, seeding_parameters):
        q = {"tag": self.tag}
        graph_to_imm = {}
        for immunization in self.db.immunization.find(q):
            graph_info = self.db.graphs.find_one(immunization['graph'])
            id = graph_info['_id']
            if id not in graph_to_imm:
                graph_to_imm[id] = (graph_info, [])
            graph_to_imm[id][1].append(immunization['solution_path'])

        for graph_id in graph_to_imm:
            graph = graph_to_imm[graph_id][0]

            solvers = ["singlegreedy", "singlegreedycelf", "allgreedy", "satgreedy"]
            active_solvers = []
            for s in solvers:
                if seeding_parameters[s] == 1:
                    active_solvers.append(s)

            self.pw.add_satgreedy(graph['adjlist_path'], graph['_id'], seeding_parameters['epsilon'],
                                  graph_to_imm[graph_id][1], seeding_parameters['gamma'], seeding_parameters['seeds'],
                                  seeding_parameters['number_of_blocked_nodes'], active_solvers, seeding_parameters['dimbeta'], 
                                  seeding_parameters['alpha'], seeding_parameters['lt_model'])
                                  # todo just single seed for now

    def run_rob(self, seeding_parameters):
        q = {"tag": self.tag}
        for immunization in self.db.immunization.find(q):
            graph_info = self.db.graphs.find_one(immunization['graph'])
            g_dir = graph_info['adjlist_path']
            self.pw.add_rob(g_dir, immunization, seeding_parameters['iterations'],
                            seeding_parameters['mode'], seeding_parameters['number_of_blocked_nodes'],
                            seeding_parameters['seeds'], seeding_parameters['alpha'], seeding_parameters['lt_model'])

    def run_dyn_imm(self, seeding_parameters):
        q = {"tag": self.tag}
        for immunization in self.db.immunization.find(q):
            graph_info = self.db.graphs.find_one(immunization['graph'])
            g_dir = graph_info['adjlist_path']
            logging.info("Adding simple DIM to schedule")
            self.pw.add_dyn_imm(g_dir, immunization, seeding_parameters['beta'],
                                seeding_parameters['seeds'], seeding_parameters['alpha'], seeding_parameters['lt_model'])

    def run_imm_simulation(self, seeding_parameters):
        for immunization in self.db.immunization.find({"tag": self.tag}):
            graph_info = self.db.graphs.find_one(immunization['graph'])
            G = helpers.load_graph(graph_info['pickle_path'])
            blocked = seeding_parameters['number_of_blocked_nodes']
            assert blocked > -1
            with open(immunization['solution_path'], 'r') as f:
                solution = json.load(f)

            ind = {}
            j = 0
            for i in G.nodes():
                if i not in solution['Blocked nodes'][:blocked]:
                    assert(i not in ind)
                    ind[i] = j
                    j += 1
            assert len(ind) > 0
            G = nx.DiGraph(G.subgraph([n for n in ind])) if G.is_directed() else nx.Graph(G.subgraph([n for n in ind]))
            G = nx.relabel_nodes(G, ind, copy=True)

            blocked_graph_path = os.path.join(self.file_db.fm.get_data_path(), str(uuid.uuid4()))
            self.file_db.fm.create_path(blocked_graph_path)
            Generator.save_to_adjlist_dir(blocked_graph_path, G)

            for seeds in self.db.simple_seeding.find({"graph_id": graph_info['_id']}):

                with open(seeds['out_file'],'r') as f:
                    seeds_info = json.load(f)
                out_file_seeds_only = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".csv")
                with open(out_file_seeds_only, 'w') as f:
                    for s in seeds_info['imm']['seeds']:
                        if s in ind:
                            f.write(str(ind[s]) + "\n")

                r = {}
                r.update(immunization)
                r.update(seeds)
                r["blocked_adjlist_path"] = blocked_graph_path
                r["out_file_seeds_only"] = out_file_seeds_only
                r['number_of_blocked_nodes'] = blocked
                r["tag"] = self.tag
                r.pop("_id")
                rr = self.db.imm_simulations.insert_one(r)
                self.pw.add_simulation(blocked_graph_path, out_file_seeds_only, rr.inserted_id)

    def run_simple_seeding(self):
        all_params = list(self.pm.get_hyperparam_sets())
        for seeding_parameters in all_params:
            if seeding_parameters['solver'] == "Imm" and seeding_parameters['number_of_blocked_nodes'] > -1:
                for graph_info in self.db.graphs.find({"tag": self.tag}):
                    graph_dir = graph_info['adjlist_path']
                    out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
                    out_file_seeds_only = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".csv")
                    imm_cmd = self.pw.get_cmd_imm(graph_dir, out_file, seeding_parameters['epsilon'], 
                                                    seeding_parameters['seeds'], seeding_parameters['lt_model'])
                    t = time.time()
                    self.pw.run_cmd(imm_cmd,False,ParallelRobustWrapper.TIMEOUT)
                    t = time.time() - t
                    result = json.load(open(out_file, "r"))
                    result['out_file'] = out_file
                    result['imm_cmd'] = imm_cmd
                    result['tag'] = self.tag
                    result['runtime'] = t
                    result['epsilon'] = seeding_parameters['epsilon']
                    result['number_of_seeds'] = seeding_parameters['seeds']
                    result['graph_id'] = graph_info['_id']
                    self.db.simple_seeding.insert_one(result)

    def run_seeding(self, force=False):
        q = {"tag": self.tag}
        if force:
            self.db.seeding.delete_many(q)
        all_params = list(self.pm.get_hyperparam_sets())
        for seeding_parameters in all_params:
            total_immunizators = self.db.immunization.count_documents(q)
            if total_immunizators == 0:
                raise Exception("No immunizers found")

            if seeding_parameters['solver'] == "SatGreedy":
                self.run_satgreedy(seeding_parameters)
            if seeding_parameters['solver'] == "Rob" and seeding_parameters['mode'] != 0:
                self.run_rob(seeding_parameters)
            if (seeding_parameters['solver'] == "Imm" and seeding_parameters['number_of_blocked_nodes'] == -1) \
                or (seeding_parameters['solver'] == "Rob" and seeding_parameters['mode'] == 0): #back compatibility
                self.run_dyn_imm(seeding_parameters)
            if seeding_parameters['solver'] == "Imm" and seeding_parameters['number_of_blocked_nodes'] > -1:
                self.run_imm_simulation(seeding_parameters)
        self.pw.run_parallel()
        # solvers save in json file, wrapper loads and moves to mongo database

    def run(self):
        logging.info("============================ NEW EXPERIMENT ===============================")
        generated = self.generate_datasets()
        logging.info("Dataset of {} graph generated".format(generated))
        self.run_simple_seeding()
        logging.info("Simple Seeding completed")
        self.run_immunization()
        logging.info("Immunization completed")
        self.run_seeding()
        logging.info("Seeding completed")

        if self.pm.get("graph_analysis") == 1:
            ga = GraphAnalyzer(self.tag)
            ga.run()
            logging.info("Graph analysis completed")

        logging.info("Experiment {} completed".format(self.tag))
