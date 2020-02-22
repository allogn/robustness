import networkx as nx
import time
from collections import defaultdict
import sys
import os
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import Solver as slv
from functools import reduce
import math
import numpy as np

class DomSolver(slv.Solver):

    def clear(self):
        for e in self.G.edges(data=True):
            if e[2]['weight'] == 1:
                e[2]['weight'] = 0.99999  # No p=1 allowed due to probability calculation along shortest path
            assert(e[2]['weight'] < 1)
        self.create_superseed_and_update_weights()

    def create_superseed_and_update_weights(self):
        self.superseed_index = len(self.G)
        while self.superseed_index in self.G:
            self.superseed_index += 1
        neighbors = defaultdict(lambda: [])
        for seed in self.params["seeds"]:
            for n in self.G.neighbors(seed):
                neighbors[n].append(self.G[seed][n]['weight'])
        new_edges = [(self.superseed_index, n, DomSolver.get_total_weight(neighbors[n])) for n in neighbors]
        self.G.add_weighted_edges_from(new_edges)
        self.G = self.G.subgraph((set(self.G.nodes()) - set(self.params["seeds"])) | set([self.superseed_index])).copy()
        for edge in self.G.edges():
            self.G[edge[0]][edge[1]]['weight'] = -math.log(self.G[edge[0]][edge[1]]['weight'])

    @staticmethod
    def get_total_weight(list_of_probabilities):
        return 1. - reduce(lambda x, y: x*y, [(1.-p) for p in list_of_probabilities])

    def run(self):
        t1 = time.time()
        blocked = []
        extra_time = 0

        if not self.params.get("fast", False):
            for iteration in range(self.params["k"]):
                self.build_domtree()
                if iteration == 0:
                    extra_time += self.save_tree_stats_return_time("first it")
                if iteration == self.params["k"] - 1:
                    extra_time += self.save_tree_stats_return_time("last it")
                blocked += self.get_best_nodes(1)
                self.G.remove_node(blocked[-1])
        else:
            self.build_domtree()
            extra_time += self.save_tree_stats_return_time("first it")
            blocked = self.get_best_nodes(self.params["k"])
        t2 = time.time()
        self.log['Total time'] = t2 - t1 - extra_time
        self.set_blocked(blocked)

    def save_tree_stats_return_time(self, prefix):
        t1 = time.time()
        g = self.domtree
        sp = nx.single_source_shortest_path_length(g,self.superseed_index)
        self.log['tree depth ' + prefix] = max([sp[n] for n in sp])
        self.log['first level node fraction ' + prefix] = g.degree(self.superseed_index)/len(self.G)
        first_level_degrees = [g.out_degree(n) for n in g.neighbors(self.superseed_index)]
        self.log['second level node fraction ' + prefix] = sum(first_level_degrees)/len(self.G)
        self.log['second level avg degree ' + prefix] = 0 if len(first_level_degrees) == 0 else np.mean(first_level_degrees)
        t2 = time.time()
        return t2 - t1

    def build_domtree(self):
        tree_dict = nx.algorithms.dominance.immediate_dominators(self.G, self.superseed_index)
        self.domtree = nx.DiGraph()
        self.domtree.add_node(self.superseed_index)
        self.domtree.add_edges_from([(edge[1],edge[0]) for edge in tree_dict.items() if edge[0] != edge[1]])
        probabilities_from_root = nx.single_source_dijkstra_path_length(self.G, self.superseed_index)

        #probability (v,u) = p(u)/p(v) from root
        for edge in self.domtree.edges():
            if edge[0] == self.superseed_index:
                probability = math.exp(-probabilities_from_root[edge[1]])
            else:
                probability = math.exp(-probabilities_from_root[edge[1]]+probabilities_from_root[edge[0]])
            self.domtree[edge[0]][edge[1]]['weight'] = probability

    def traverseTreeRec(self, node):
        benefit = 1
        for n in self.domtree.neighbors(node):
            benefit += self.traverseTreeRec(n)*self.domtree[node][n]['weight']
        return benefit

    def get_rank(self):
        rank = []
        if self.params["k"] > self.domtree.degree(self.superseed_index):
            self.log['error'] = "Problem is trivial"
            if self.domtree.degree(self.superseed_index) == 0:
                return [(0,np.random.choice([n for n in self.G.nodes() if n != self.superseed_index and n not in self.params["seeds"]], replace=False))]
            return [(0, next(self.domtree.neighbors(self.superseed_index)))]
        for n in self.domtree.neighbors(self.superseed_index):
            benefit = self.traverseTreeRec(n)*self.domtree[self.superseed_index][n]['weight']
            rank.append((benefit, n))
        return rank

    def get_best_nodes(self, number_of_nodes):
        rank = self.get_rank()
        return [int(a[1]) for a in sorted(rank)[-number_of_nodes:]]


    def add_immunized_graph_to_json(self):
        if self.params.get('static',0) < 1:
            self.log['edgelist'] = []
            self.log['weights'] = []
        else:
            edgelist = []
            weights = []
            for e in self.G.edges(data=True):
                if e[0] in self.blocked or e[1] in self.blocked:
                    continue
                edgelist += [e[0], e[1]]
                if "weight" in e[2]:
                    weights.append(1)
            self.log['edgelist'] = edgelist
            self.log['weights'] = weights
        self.log['N'] = len(self.G) - len(self.blocked)
