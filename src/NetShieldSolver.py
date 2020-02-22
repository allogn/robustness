import networkx as nx
import numpy as np
import time
import sys
import itertools
from scipy.linalg import eigh
from scipy.sparse.linalg import eigs
import os
from heapq import *
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from Solver import *

class PriorityQueue:
    def __init__(self, initlist):
        self.counter = itertools.count()  # unique sequence count
        self.entry_finder = {}  # mapping of tasks to entries
        self.pq = []
        for el in initlist:
            entry = [-el[0], next(self.counter), el[1]]
            self.pq.append(entry)
            self.entry_finder[el[1]] = entry
        heapify(self.pq)  # list of entries arranged in a heap
        self.REMOVED = '<removed-task>'  # placeholder for a removed task

    def update_task_add(self, task, add_value):
        priority = 0
        if task in self.entry_finder:
            entry = self.entry_finder.pop(task)
            entry[-1] = self.REMOVED
            priority = entry[0]
        count = next(self.counter)
        entry = [priority-add_value, count, task]
        self.entry_finder[task] = entry
        heappush(self.pq, entry)

    def add_task(self, task, priority=0):
        'Add a new task or update the priority of an existing task'
        if task in self.entry_finder:
            self.remove_task(task)
        count = next(self.counter)
        entry = [-priority, count, task]
        self.entry_finder[task] = entry
        heappush(self.pq, entry)

    def remove_task(self, task):
        'Mark an existing task as REMOVED.  Raise KeyError if not found.'
        entry = self.entry_finder.pop(task)
        entry[-1] = self.REMOVED

    def pop_task(self):
        'Remove and return the lowest priority task. Raise KeyError if empty.'
        while self.pq:
            priority, count, task = heappop(self.pq)
            if task is not self.REMOVED:
                del self.entry_finder[task]
                return task
        raise KeyError('pop from an empty priority queue')

class NetShieldSolver(Solver):

    def net_shield(self):
        t1 = time.time()
        A = nx.to_scipy_sparse_matrix(self.G.to_undirected())
        vals, vecs = eigs(A, k=1, which='LR')
        max_eig = np.real(vals[0])
        max_eigvec = np.real(vecs[:,0].reshape((vecs.shape[0],)))

        # G = self.G.to_undirected()
        nodelist = [n for n in self.G.nodes()]
        inverse_index = {}
        for i in range(len(nodelist)):
            inverse_index[nodelist[i]] = i
        #
        # A = nx.to_numpy_matrix(G, nodelist=nodelist, weight=None)
        # M = len(G)
        # W, V = eigh(A, eigvals=(M-1, M-1), type=1, overwrite_a=True)
        # max_eig = W[0]
        # max_eigvec = V[:,0].reshape((V.shape[0],))

        self.log["Eigenvalue"] = max_eig

        scores = 2*max_eig*(max_eigvec**2)
        pk = PriorityQueue(zip(scores.tolist(), list(range(len(self.G)))))

        S = set()
        Slist = []
        for it in range(self.k):
            next_best = pk.pop_task()
            S.add(next_best)
            Slist.append(next_best) # return in order
            for n in self.G.neighbors(nodelist[next_best]):
                j = inverse_index[n]
                if j not in S:
                    pk.update_task_add(j, -2 * max_eigvec[next_best] * max_eigvec[j])

        t2 = time.time()
        self.log['Total time'] = t2-t1

        return [nodelist[i] for i in Slist]

    def run(self):
        self.set_blocked(self.net_shield())
