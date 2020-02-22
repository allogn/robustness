import networkx as nx
import time
import numpy as np
from Solver import *

class RandomSolver(Solver):
    # adding different ids allows to run multiple random instances per network

    def run(self):
        t1 = time.time()
        random_blocked_set = np.random.choice([node for node in self.G.nodes()], len(self.G), replace=False)
        t2 = time.time()

        self.log['Total time'] = (t2-t1)
        self.log['random_id'] = self.params["random_id"]
        self.set_blocked(random_blocked_set)
