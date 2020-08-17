import logging
import os
import sys
import time
import numpy as np
import time
import json
from collections import defaultdict

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
import helpers
from Simulator import *
from FileManager import *
from ParameterManager import *

def simulate(input_graph_path, input_seeds_path, input_blocked_path, output_path, iterations, **other_params):
    '''
    For each graph and seeds set, add all solutions, conduct simulations for all blocked at once
    Simulate per each graph+seed+blocked, grouped by blocked, write results
    '''
    graphs = {}
    seeds_per_graph = {}
    blocked_per_graph = {}

    for g in FileManager.list_files_with_ext(input_graph_path, 'pkl'):
        G = helpers.load_graph(g)
        # we should run for all solvers again since we want to use the same possible worlds for all
        graphs[G.graph['graph_id']] = G

    set_of_imm_solvers = set()
    for b in FileManager.list_files_with_ext(input_blocked_path, 'json', recursive=True):
        f = open(b,'r')
        blocked = json.load(f)
        f.close()
        gid = blocked['problem']['data']['graph_id']
        assert(gid in graphs)
        if gid not in blocked_per_graph:
            blocked_per_graph[gid] = []
        blocked_per_graph[gid].append(blocked)
        set_of_imm_solvers.add(blocked["solver"])

    extra_sg_added_per_graph = set()
    for s in FileManager.list_files_with_ext(input_seeds_path, 'json'):
        f = open(s,'r')
        seeds = json.load(f)
        f.close()
        gid = seeds['graph_id']
        if gid not in seeds_per_graph:
            seeds_per_graph[gid] = []

        rr_objective = 0
        runtime = seeds["satgreedy_time"] # tobefixed.
        if 'satgreedy_objective' in seeds:
            rr_objective = seeds['satgreedy_objective']
        seeds_per_graph[gid].append((seeds['solver'], seeds['optimal_seeds'], rr_objective, runtime))

        # for different SatGreedy solutions (two graphs, for example) optimal seeds of other algorithms
        # may depend only on epsilon, but seed set may vary, so solutions may be different
        # however in the plotting we assume there is only one solutions of the "true" value

        # the issue should not appear if there is only one graph #multigraphissue

        if seeds['solver'].find('SatGreedy') > -1: # we want to add other solvers calculated in SatGreedy, but only once because those should not depend on any parameter (maybe epsilon, but should not depend)
            assert(seeds['adversaries_size'] == len(set_of_imm_solvers))

            if gid in extra_sg_added_per_graph:
                pass

                # different SatGreedy can result in different seed sets -- we don't want to plot each single one.
                # so we plot first random one, of one SatGreedy
                # one run of SatGreedy gives one set of optimal seeds per imm_solver, however two
                # different parameters of SG can result in different seed set sizes, and different imm_solver solutions

                # for s in set_of_imm_solvers:
                #     check = False
                #     for a,b,c in seeds_per_graph[gid]:
                #         if a == s and b == seeds['optimal_seeds_' + s]:
                #             check = True
                #             break
                #     if not check:
                #         print(seeds_per_graph[gid],'\n', (s, seeds['optimal_seeds_' + s]))
                #     assert(check)
            else:
                seeds_per_graph[gid].append(("all_greedy", seeds['all_greedy_seeds'], seeds["all_greedy"],seeds["all_greedy_time"]))
                seeds_per_graph[gid].append(("single_greedy", seeds['single_greedy_seeds'], seeds["single_greedy"],seeds["single_greedy_time"]))
                seeds_per_graph[gid].append(("single_greedy_celf", seeds['single_greedy_celf_seeds'], seeds["single_greedy_celf"],seeds["single_greedy_celf_time"]))

                # this is outdated: no more plotting individual results
                # for s in set_of_imm_solvers:
                #     seeds_per_graph[gid].append((s, seeds['optimal_seeds_' + s], seeds))
                extra_sg_added_per_graph.add(gid)

    FileManager.create_path(output_path)
    all_seed_set_sizes = set()
    for gid in seeds_per_graph:
         # not all graphs in graph_path are needed, one script execution for one problem_set (may be several graphs for the same set of params)
        G = helpers.load_graph(os.path.join(input_graph_path, gid + ".pkl"))
        for seed_solver, seeds, seed_rr_obj, seed_time in seeds_per_graph[gid]:
            simulator = Simulator(G, seeds)
            problem = {}
            for blocked in blocked_per_graph[gid]:
                if problem == {}:
                    problem = blocked['problem']
                else:
                    assert(problem == blocked['problem']) # two blocked graphs should claim same problem
                simulator.add_blocked(blocked['solver'], blocked['results']['Blocked nodes'])

            # save regular result of simulations
            results = {}
            results['results'] = simulator.run(iterations)
            results['results']['seed_set_size'] = len(seeds)
            all_seed_set_sizes.add(len(seeds))
            assert(len(seeds) >= problem['k'])
            results['data'] = G.graph
            results['problem'] = problem

            results['seed_solver'] = seed_solver

            all_sim_results = [results['results']['solvers'][s]['activated nodes']['mean'] for s in results['results']['solvers']]
            results['results']['seed_mc_obj'] = min(all_sim_results)
            results['results']['seed_rr_obj'] = seed_rr_obj
            results['results']['seed_time'] = seed_time

            # save
            filename = "{}_{}_sim.json".format(gid, seed_solver)
            ff = open(os.path.join(output_path, filename), 'w')
            json.dump(results, ff, cls=helpers.NumpyEncoder)
            ff.close()

            # additionally for checking print results produced previously by SatGreedy

            # todo: compare simulation with RR
            # relative_max_error_between_mc_and_RR = 0.1

            # if seed_solver.find("SatGreedy") > -1:
            #     min_val = 100000000
            #     for s in set_of_imm_solvers:
            #         # expected spread for blocked nodes s for current seed set, simulated
            #         mc_result = results['results']['solvers'][s]['activated nodes']['mean']
            #         # same expected spread for these seeds calculated as optimal value for this blocked nodes (note the "s" in the key)
            #         rr_result = seed_info[s + "_expected_spread"]
            #         dif = abs(rr_result - mc_result)/rr_result
            #         # check that simulation gives the same estimation as RR
            #         logging.info("SG result for {}: {}; MC result: {}; difference: {}".format(s, rr_result, mc_result, dif))
            #         assert(dif < relative_max_error_between_mc_and_RR or dif < 2)
            #         min_val_obj = min(mc_result, min_val)/len(G)
            #     dif = abs(min_val_obj - seed_info["new_objective"])/min_val_obj
            #     s = "SG result for objective: {}; MC result: {}; difference: {}"
            #     logging.info(s.format(seed_info["new_objective"], min_val_obj, dif))
            #     assert(dif < relative_max_error_between_mc_and_RR or dif < 2)

        logging.info("Experiments for graph %s completed" % gid)
        assert(len(all_seed_set_sizes)==1)
