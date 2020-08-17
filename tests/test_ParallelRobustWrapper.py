import unittest
import numpy as np
from src.ParallelRobustWrapper import *

class TestSolver(unittest.TestCase):
    def setUp(self):
        np.random.seed(seed=123)

    def test_workflow(self):
        rw = ParallelRobustWrapper("test")
