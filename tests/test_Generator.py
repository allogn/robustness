import unittest
import src.helpers as helpers
import networkx as nx
import sys
import os
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from Generator import *


class TestGenerator(unittest.TestCase):
    def test_generate(self):
        param_dict = {
            "random_weight": 0,
            "weight_scale": 1,
            "graph_type": "path",
            "n": 3
        }
        gen = Generator(param_dict)
        G = next(gen.generate())
        self.assertEqual(len(G), 3)
        self.assertListEqual([(0, 1, 1), (1, 2, 1)], [e for e in G.edges(data="weight")])
