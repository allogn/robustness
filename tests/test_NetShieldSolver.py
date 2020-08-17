import unittest
from src.NetShieldSolver import *
import networkx as nx
import numpy as np

class TestSolver(unittest.TestCase):
    def setUp(self):
        np.random.seed(seed=123)

    def test_pk(self):
        pk = PriorityQueue([(3,1),(0,4),(1,10)])
        self.assertEqual(pk.pop_task(), 1)
        pk.add_task(2, 0)
        pk.update_task_add(2, 20)
        self.assertEqual(pk.pop_task(), 2)
        self.assertEqual(pk.pop_task(), 10)
        self.assertEqual(pk.pop_task(), 4)

    def test_net_shield(self):
        with self.subTest(graph="trivialStar"):
            G = nx.DiGraph()
            G.add_edges_from([(0,1),(2,1),(3,1),(1,4)])
            solver = NetShieldSolver(G, seeds=[0], k=1)
            solver.run()
            self.assertAlmostEqual(np.max(solver.log['Eigenvalue']), 2)
            self.assertSetEqual(solver.blocked, set([1]))

            G_blocked = nx.DiGraph()
            edges = list(zip(*[iter(solver.log['edgelist'])] * 2))
            G_blocked.add_edges_from(edges)
            self.assertEqual(len(G_blocked), 0) # no edges
            self.assertEqual(solver.log['N'], 4) # 4 nodes should remain

        with self.subTest(graph="fig2 paper graph"):
            e = [(1,2),(1,3),(1,4),(1,15),(15,13),(13,14),(13,16),(14,5),(5,8),(5,7),(5,6),(16,9),(9,10),(9,11),(9,12)]
            G = nx.DiGraph()
            G.add_edges_from(e)

            solver = NetShieldSolver(G, seeds=[0], k=1)
            solver.run()
            self.assertSetEqual(solver.blocked, set([13]))

            solver = NetShieldSolver(G, seeds=[0], k=4)
            solver.run()
            self.assertSetEqual(solver.blocked, set([1, 13, 5, 9]))
