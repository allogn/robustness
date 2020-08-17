import networkx as nx
import time
import numpy as np
from Solver import *

class PagerankSolver(Solver):

    def run(self):
        t1 = time.time()
        rank_dic = nx.pagerank(self.G, tol=0.01)
        t2 = time.time()
        data = [(k, rank_dic[k]) for k in rank_dic]
        sorted_by_second = sorted(data, key=lambda tup: tup[1], reverse=True)
        blocked_list = [v[0] for v in sorted_by_second]

        self.log['Total time'] = (t2-t1)
        self.set_blocked(blocked_list)
