import unittest
from src.Simulator import *
import networkx as nx
import numpy as np


class TestSimulator(unittest.TestCase):
    def test_merge_results_across_iterations(self):
        G = nx.DiGraph()
        s = Simulator(G, [], 0)
        r = []
        for i in [10, 20, 30]:
            d = {}
            d['blabla'] = i
            d['123'] = i
            d['solvers'] = {}
            d['solvers']['alg1'] = {}
            d['solvers']['alg1']['11'] = i
            r.append(d)
        m = s.merge_results_across_iterations(r)
        self.assertEqual(m['blabla']['mean'], 20)
        self.assertAlmostEqual(m['solvers']['alg1']['11']['var'], 100, 2)

    def test_simulate(self):
        G = nx.DiGraph()
        G.add_weighted_edges_from([(1,2,1),(2,3,1),(3,4,1)])
        s = Simulator(G, [1])
        s.add_blocked("alg", [3])
        result = s.run(2)
        self.assertEqual(result['iterations until termination in unblocked graph']['mean'], 4)
        self.assertEqual(result['active nodes in unblocked graph']['mean'], 4)
        self.assertEqual(result['solvers']['alg']['activated nodes']['mean'], 2)
        self.assertEqual(result['solvers']['alg']['saved nodes']['mean'], 2)
        self.assertAlmostEqual(result['solvers']['alg']['fraction of saved nodes to active nodes']['mean'], 1/2, 5)
