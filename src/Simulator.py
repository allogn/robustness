import time
import networkx as nx
import random
import numpy as np
import logging
from collections import defaultdict
from subprocess import Popen, PIPE
import warnings
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from FileManager import *

class Simulator():
    '''
    Modes:
        - Possible World
        - C
    '''

    BIN_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'SatGreedy', 'build')

    def __init__(self, G, seeds, mode="Possible World", blockable_seeds=True, number_of_seeds=-1):
        self.G = G
        self.seeds = seeds
        self.blocked = {} # dictionary adv.strategy -> list of blocked nodes
        self.mode = mode
        self.blockable_seeds = blockable_seeds
        self.log = {}

        # if seeds are blockable, and this number is not -1, then
        # blocked seeds are substituted by extra seeds in seeds array, until
        # number_of_seeds is reached. in this case seeds array should contain
        # more seeds than required
        self.number_of_seeds = number_of_seeds

    def run_c_sim(self, iterations, verbose=False):

        # reset node index first
        nonblocked_seeds = self.seeds
        for b in self.blocked:
            if self.blockable_seeds:
                nonblocked_seeds = [s for s in nonblocked_seeds if s not in self.blocked[b]]
        if self.number_of_seeds > -1:
            if len(nonblocked_seeds) < self.number_of_seeds:
                raise Exception("Provided seeds are not enough to build non-blocked seed set of size {}".format(self.number_of_seeds))
            nonblocked_seeds = nonblocked_seeds[:self.number_of_seeds]

        if len(self.blocked) == 0:
            self.blocked['null'] = []
        for blocked_name in self.blocked:
            blocked = self.blocked[blocked_name]

            nodes = [n for n in self.G.nodes() if n not in self.blocked[blocked_name] or n in nonblocked_seeds]
            G_blocked = self.G.subgraph(nodes)

            mapping = {old_label:new_label for new_label, old_label in enumerate(G_blocked.nodes())}
            H = nx.relabel_nodes(G_blocked, mapping)

            fm = FileManager('tmpForSimulator')
            fm.clean_data_path()
            nx.write_edgelist(H, os.path.join(fm.get_data_path(), "graph.csv"), delimiter=" ", data=['weight'])

            with open(os.path.join(fm.get_data_path(), "attributes.txt"),'w') as f:
                f.write("n={}\nm={}\nid={}".format(H.number_of_nodes(), H.number_of_edges(), self.G.graph['graph_id']))


            with open(os.path.join(fm.get_data_path(), "seeds.txt"),'w') as f:
                for s in self.seeds:
                    f.write("{}\n".format(s))

            cmd = [ os.path.join(Simulator.BIN_PATH,'Simulator'),
                    '-dataset', fm.get_data_path(),
                    '-seeds', os.path.join(fm.get_data_path(), "seeds.txt"),
                    '-iter', str(iterations)]
            process = Popen(cmd, stdout=PIPE, stderr=PIPE)
            result = ''

            for line in process.stdout:
                if verbose:
                    print(line.decode('utf8'), end="")
                result += line.decode('utf8')

            for line in process.stderr:
                print("Error: ", line.decode('utf8'), end="")
            rc = process.poll()
            if rc != None and rc != 0:
                fm.remove_all()
                raise Exception("Terminated with error {}".format(rc))

            fm.remove_all()

            obj = float(result[result.find("\n")+1:result.find(", ")])
            obj_std = float(result[result.find(", ")+2:result.find("\nDone.")+1])
            if blocked_name == 'null':
                self.log['mc_objective'] = obj
                self.log['mc_objective_std'] = obj_std
            else:
                self.log[blocked_name] = {}
                self.log[blocked_name]['mc_objective'] = obj
                self.log[blocked_name]['mc_objective_std'] = obj_std

    def add_blocked(self, name, node_set):
        self.blocked[name] = node_set

    def run(self, iterations):
        assert(sum([not self.G.has_node(n) for n in self.seeds]) == 0)
        self.log['iterations'] = iterations

        if not self.blockable_seeds and self.mode != 'C':
            raise Exception("Not implemented")

        if self.mode == 'C':
            self.run_c_sim(iterations)
            return self.log

        for key in self.blocked:
            blocked_list = self.blocked[key]
            assert(sum([not self.G.has_node(n) for n in blocked_list]) == 0) # any blocked or seed node should exist in the graph
        iteration_results = []
        for i in range(iterations):
            iteration_results.append(self.run_iteration())
        self.log.update(self.merge_results_across_iterations(iteration_results))
        return self.log

    def run_iteration(self):
        if self.mode == "Possible World":
            return self.simuation_as_possible_world()
        # else:
        #     return self.simulation_without_building_all_active_graph()

    def simuation_as_possible_world(self):
        '''
        Allows to calculate the number of saved nodes
        '''
        t1 = time.time()
        front_nodes = self.seeds
        active_series = []
        active_series.append(len(front_nodes))
        active = set()
        active.update(self.seeds)

        iterations = 0
        active_edges = set()
        active_subgraph = nx.DiGraph()
        active_subgraph.add_nodes_from([key for key in active])
        while (len(front_nodes) > 0):
            front_edges = self.get_front_edges(front_nodes)
            active_edges.update(front_edges)
            front_nodes = [e[1] for e in front_edges if e[1] not in active]
            active.update(front_nodes)
            active_series.append(active_series[-1]+len(front_nodes))
            iterations += 1
        active_subgraph.add_edges_from(active_edges)
        results = {}
        results['iterations until termination in unblocked graph'] = iterations
        results['active nodes in unblocked graph'] = len(active_subgraph)
        results['solvers'] = {}
        for blocked_set_name in self.blocked:
            blocked_list = self.blocked[blocked_set_name]
            results['solvers'][blocked_set_name] = {}
            active_subgraph_with_blocked = active_subgraph.subgraph([node for node in active_subgraph.nodes() if node not in blocked_list])
            active_subgraph_with_blocked = self.get_reachable_subgraph_from_seeds(active_subgraph_with_blocked)
            activated_node_amount = len(active_subgraph_with_blocked)
            saved_node_amount = results['active nodes in unblocked graph'] - activated_node_amount
            results['solvers'][blocked_set_name]['activated nodes'] = activated_node_amount
            results['solvers'][blocked_set_name]['saved nodes'] = saved_node_amount
            results['solvers'][blocked_set_name]['fraction of saved nodes to active nodes'] = saved_node_amount/results['active nodes in unblocked graph']
        t2 = time.time()
        results['simulation time'] = t2 - t1
        return results

    def get_reachable_subgraph_from_seeds(self, G):
        G = G.copy()
        G.add_node("superseed")
        G.add_edges_from([("superseed", n) for n in self.seeds if G.has_node(n)])
        node_subset = nx.descendants(G, "superseed")
        return G.subgraph(node_subset - set(["superseed"]))

    def get_front_edges(self, front_nodes):
        new_front_edges = []
        for v in front_nodes:
            for u in self.G.successors(v):
                if (np.random.rand() <= self.G[v][u]['weight']):
                    new_front_edges.append((v,u))
        return new_front_edges

    def merge_results_across_iterations(self, results):
        assert(len(results) > 0)
        r = results[0]
        N = len(results)
        merged = {}
        for key in r:
            if key == "solvers":
                continue
            merged[key] = self.get_list_stats([results[i][key] for i in range(N)])
        merged['solvers'] = {}
        for alg in r['solvers']:
            merged['solvers'][alg] = {}
            for key in r['solvers'][alg]:
                l = [results[i]['solvers'][alg][key] for i in range(N)]
                merged['solvers'][alg][key] = self.get_list_stats([results[i]['solvers'][alg][key] for i in range(N)])
        return merged

    def get_list_stats(self, l):
        s = {}
        warnings.simplefilter("ignore", category=RuntimeWarning)
        s['mean'] = np.mean(l)
        s['var'] = np.var(l, ddof=1)
        return s

    # def run_some_iterations(G, seeds, total_iterations):
    #     raise Exception("Not implemented, should not work for possible worlds")
    #     '''
    #     Run several iterations for graph in order to propogate influence without blocking nodes.
    #     Use return values for simulation after blocking nodes.
    #     @return new_seeds (current front), inactive nodes (activated before)
    #     '''
    #     assert(sum([not G.has_node(n) for n in seeds]) == 0)
    #     G = G.copy()
    #     active_series = []
    #     frontend = seeds
    #     active = defaultdict(lambda: False)
    #     for n in seeds:
    #         active[n] = True
    #     iterations = 0
    #
    #     while (len(frontend) > 0) and (iterations < total_iterations):
    #         frontend = sim_iteration(G, frontend, active, defaultdict(lambda: False))
    #         iterations += 1
    #
    #     ntd = []
    #     for n in G.nodes_iter():
    #         if active[n] and (n not in frontend):
    #             ntd.append(n)
    #     G.remove_nodes_from(ntd)
    #
    #     return G, frontend
    #
    # def simulation_without_building_all_active_graph(self):
    #     raise Exception("Not implemented for multiple blocked sets")
    #     blocked = defaultdict(lambda: False)
    #     set_name = next(key in self.blocked)
    #     for b in self.blocked[set_name]:
    #         blocked[b] = True
    #
    #     active_series = []
    #     frontend = seeds
    #     active_series.append(len(frontend))
    #     active = defaultdict(lambda: False)
    #     for n in seeds:
    #         active[n] = True
    #         if n in blocked_list:
    #             raise Exception("Seed can not be blocked")
    #     iterations = 0
    #     while (len(frontend) > 0):
    #         frontend = sim_iteration(G, frontend, active, blocked)
    #         active_series.append(active_series[-1]+len(frontend))
    #         iterations += 1
    #
    #     results = {}
    #     results['activated nodes'] = np.sum(np.array([active[key] for key in active]))
    #     results['iterations until termination'] = iterations
    #     results['active nodes per iteration'] = active_series
    #     return results
    #
    #
    # def sim_iteration(G, frontend, active, blocked):
    #     raise Exception("Not implemented")
    #     new_frontend = []
    #     for v in frontend:
    #         for u in G.successors(v):
    #             if not u in blocked and not active[u]:
    #                 if (np.random.rand() <= G[v][u]['weight']):
    #                     new_frontend.append(u)
    #                     active[u] = True
    #     return new_frontend
