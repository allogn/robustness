import networkx as nx
import time
import numpy as np
from Solver import *

class KatzSolver(Solver):

    def run(self):
        # change solver for Stanford network
        t1 = time.time()
        #rank_dic = nx.katz_centrality(self.G, tol=0.1, max_iter=100000, weight="weight")
        rank_dic = nx.katz_centrality_numpy(self.G, weight="weight")
        t2 = time.time()
        data = [(k, rank_dic[k]) for k in rank_dic]
        sorted_by_second = sorted(data, key=lambda tup: tup[1], reverse=True)
        blocked_list = [v[0] for v in sorted_by_second]

        self.log['Total time'] = (t2-t1)
        self.set_blocked(blocked_list)
