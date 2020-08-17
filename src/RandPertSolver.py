import networkx as nx
import time
import numpy as np
from Solver import *

class RandPertSolver(Solver):

    def run(self):
        t1 = time.time()
        k = self.params['k']
        random_blocked_set = np.random.choice([node for node in self.G.nodes()], k, replace=False)

        for edge in self.G.edges():
            self.G[edge[0]][edge[1]]['weight'] = self.G[edge[0]][edge[1]]['weight'] * 2 #(0.5+4 * np.random.random())
            if self.G[edge[0]][edge[1]]['weight'] > 1:
                self.G[edge[0]][edge[1]]['weight'] = 0.99

        t2 = time.time()

        self.log['Total time'] = (t2-t1)
        self.set_blocked(random_blocked_set)
