import os
import sys
import time
import hashlib
import argparse
import numpy as np
import signal
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
from FileManager import *
from ParameterManager import *
import helpers

from DegreeSolver import *
from NetShieldSolver import *
from DomSolver import *
from RandomSolver import *
from RandPertSolver import *

class SolverTimeoutException(Exception):
    pass


def handler(signum, frame):
    raise SolverTimeoutException()

def load_best_seeds(seed_path, graph_id):
    '''
    Given a path with seeds - find the optimal seeds from thereself.
    In the directory with seeds there are several directories, each named
    by id of a graph. Each graph directory has a single file with IMM seeds.
    '''
    seed_solutions = list(FileManager.list_files_with_ext(seed_path, "json"))
    seed_solutions = [s for s in seed_solutions if s.find(graph_id) > 0]
    assert(len(seed_solutions) == 1) # assume we run only one seed finder before running immunization adversaries
    # and the seed finder gives one seed set per graph (the finder is imm solver)
    ff = open(seed_solutions[0],'r')
    seed_info = json.load(ff)
    ff.close()
    logging.info("Found seeds {} for graph {}".format(seed_solutions[0], seed_path, graph_id))
    return seed_info['imm']['seeds']

def run_imm_solver(input_path, output_path, solver_name, solver_params, problem_params, solver_timeout, seed_path, **other_params):
    for graph_file in FileManager.list_files_with_ext(input_path, "pkl"):
        G = helpers.load_graph(graph_file)
        logging.info("Running IMM for Graph {}".format(G.graph["graph_id"]))

        if solver_name == "DomDegree":
            Solver = eval("DomSolver")
        else:
            Solver = eval(solver_name + "Solver")
        z = dict(solver_params)

        # do not allow different k for different solvers
        # if 'l' in solver_params:
        #     z['k'] = int(solver_params["l"] * len(G))
        # else:
        z['k'] = problem_params["l"]
        z.update(other_params)
        # watch out the great confution: k for immunization algorithms is l for maximization algorithms (both are blocked nodes)
        # and k for maximization algorithm is k for immunization

        if solver_name == "DomDegree":
            degree_best_seeds = []
            degrees = [(node, G.degree([node])[node]) for node in G.nodes()]
            degrees.sort(key=lambda t: t[1])
            for i in range(len(load_best_seeds(seed_path, G.graph['graph_id']))):
                degree_best_seeds.append(degrees.pop()[0])

            z['seeds'] = degree_best_seeds
            for i in z['seeds']:
                assert(G.has_node(i))

        if solver_name == "Dom":
            z['seeds'] = load_best_seeds(seed_path, G.graph['graph_id'])

        assert(problem_params["l"] + problem_params['k'] < len(G))

        if solver_name == "Random" or solver_name == "RandPert":
            repeat = z['n']
            for i in range(repeat):
                solver = Solver(G, **z)
                signal.signal(signal.SIGALRM, handler)
                signal.alarm(solver_timeout.seconds)
                solver.run()  # may raise a timeout exception that should be caught by airflow
                signal.alarm(0)
                problem_id = G.graph['graph_id']

                fname = "{}{}_{}.json".format(solver_name, i, problem_id)
                output_path1 = os.path.join(output_path, G.graph['graph_id'])
                FileManager.create_path(output_path1)
                outf = open(os.path.join(output_path1, fname), 'w')
                results = {'results': solver.log}
                results['problem'] = problem_params
                # results['problem']['k'] = int(len(G) * results['problem']['l'])
                # assert(results['problem']['l'] > 0) -- maybe rand pert
                results['problem']['data'].update(G.graph)
                results['solver'] = solver_name + str(i)
                results['solver_params'] = solver_params
                json.dump(results, outf)
                outf.close()
            continue

        solver = Solver(G, **z)
        signal.signal(signal.SIGALRM, handler)
        signal.alarm(solver_timeout.seconds)
        solver.run()  # may raise a timeout exception that should be caught by airflow
        signal.alarm(0)
        problem_id = G.graph['graph_id']

        fname = "{}_{}.json".format(solver_name, problem_id)
        output_path1 = os.path.join(output_path, G.graph['graph_id'])
        FileManager.create_path(output_path1)
        outf = open(os.path.join(output_path1, fname), 'w')
        results = {'results': solver.log}
        results['problem'] = problem_params
        # results['problem']['k'] = int(len(G) * results['problem']['l'])
        assert(results['problem']['l'] > 0)
        results['problem']['data'].update(G.graph)
        results['solver'] = solver_name
        results['solver_params'] = solver_params
        json.dump(results, outf)
        outf.close()

        # raise Exception("AAAA")
        # outf2 = open(os.path.join(input_path, graph_file[:-4], solver_name + ".csv"), 'w')
        # for i in solver.log["Blocked nodes"]:
        #     outf2.write("{}\n".format(i))
        # outf2.close()

        logging.info("Experiment for graph %s completed by %s" % (G.graph['graph_id'], solver.get_name()))

if __name__ == "__main__":
    t1 = time.time()
    parser = argparse.ArgumentParser(description="Run solver on a single graph with seeds")
    parser.add_argument("graph", type=str)
    parser.add_argument("seeds", type=str)
    parser.add_argument("nodes_to_block", type=int)
    parser.add_argument("algorithm", type=str)
    parser.add_argument("-i", "--iterations", type=int, default="100")
    parser.add_argument("-j", "--simulation_iterations", type=int, default="100")
    parser.add_argument("-o", "--outfile", type=str, default="a.out")
    parser.add_argument("-p", "--problem_params", type=str, nargs="*")
    args = parser.parse_args()

    G, seeds = helpers.load_graph_and_seed(args.graph, args.seeds)
    k = args.nodes_to_block
    problem_params = {}
    if args.problem_params:
        for i in range(int(len(args.problem_params)/2)):
            if args.problem_params[2*i+1].isdigit():
                problem_params[args.problem_params[2*i]] = int(args.problem_params[2*i+1])
            else:
                problem_params[args.problem_params[2*i]] = args.problem_params[2*i+1]
    if args.algorithm == 'imin':
        solver = islv.iMinSolver(G, seeds, k, iters=args.iterations, **problem_params)
    if args.algorithm == 'imin_rand':
        solver = islv.iMinSolver(G, seeds, k, iters=args.iterations, random_wave=True, **problem_params)
    if args.algorithm == 'imin_enum':
        solver = islv.iMinSolver(G, seeds, k, iters=args.iterations, enum_wave=True, **problem_params)
    if args.algorithm == 'dom':
        solver = dslv.DomSolver(G, seeds, k, **problem_params)
    solver.run()
    print("%s blocked %d nodes in a graph of size %d" % (solver.get_name(), k, len(G)))

    results = []
    for i in range(args.simulation_iterations):
        results.append(simulation.simulate(G, seeds, solver.log['Blocked nodes'])['saved nodes'])
    solver.log.update({"saved mean": np.mean(results), "saved std": np.std(results)})
    solver.save_log(args.outfile)
    print("Time: %1.5fs; Objective (saved): %1.1f pm %1.1f; Total time: %1.5s" % (solver.log["Total time"], np.mean(results), np.std(results), (time.time() - t1)))
