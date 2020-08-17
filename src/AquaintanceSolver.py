import networkx as nx
import time
import numpy as np
from Solver import *

class AquaintanceSolver(Solver):

    def run(self):
        t1 = time.time()

        blocked_list = []
        available_nodes = set([node for node in self.G.nodes()])
        while len(blocked_list) < len(self.G):
            id = np.random.choice(list(available_nodes))
            options = [v for v in self.G.neighbors(id) if v in available_nodes]
            if len(options) > 0:
                to_remove = np.random.choice(options)
            else:
                to_remove = id
            blocked_list.append(to_remove)
            available_nodes.remove(to_remove)
        t2 = time.time()

        self.log['Total time'] = (t2-t1)
        self.set_blocked(blocked_list)
