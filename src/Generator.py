import networkx as nx
import random
import sys
import time
import os
import argparse
import numpy as np
import scipy.io
import logging
import uuid
from scipy.sparse import csr_matrix
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
import helpers
from FileManager import *

class Generator:
    '''
    params:
        {
            'graph_type': <string>,
            'random_weight': <0/1>,
            'weight_scale':
                <if random_weight then graph weights = weight_scale*<random number between 0 and 1>,
                    otherwise graph weights = weight_scale>,
            'both_directions': % supported only for graph types that are not directed natively
                <0 if direction is chosen randomly with equal probability
                 1 if graph if each undirected edge is represented by two edges with opposite directions>,
            ... other graph_type-specific parameters
        }

    Usage:
        .generate(n) returns generator of n graphs
        .run() returns a single graph
    '''
    def __init__(self, params):
        self.params = params
        if "random_weights" in params:
            raise Exception("Typo in params: should be random_weight instead random_weightS")
        self.generators = {
            'powerlaw_cluster': lambda: nx.powerlaw_cluster_graph(params["n"], params["mm"], params["p"]),
            'stanford': lambda: self.get_stanford_graph(),
            'gnutella': lambda: self.get_gnutella_graph(),
            'grid': lambda: nx.convert_node_labels_to_integers(nx.grid_2d_graph(params['n'], params['n'])),
            'path': lambda: nx.path_graph(params["n"], create_using=nx.DiGraph),
            'binomial': lambda: nx.fast_gnp_random_graph(params['n'], params['p'], directed=True),
            'watts_strogatz': lambda: nx.watts_strogatz_graph(params['n'], params['k'], params['p']),
            'karate': lambda: nx.karate_club_graph(),
            'vk': lambda: self.get_vk_graph(),
            'dblp': lambda: self.get_dblp_graph(),
            'advogato': lambda: self.get_advogato_graph(),
            'epinions': lambda: self.get_epinions_graph(),
            'cph': lambda: self.get_cph_graph(),
            'brightkite': lambda: self.get_brightkite_graph(),
            'mit': lambda: self.get_mit_graph(),
            'blogs': lambda: self.get_blogs_graph(),
            'minnesota': lambda: self.get_minnesota_graph(),
            'gaussian_random_partition': lambda: nx.gaussian_random_partition_graph(params['n'], params['s'], params['v'], params['p_in'], params['p_out'], directed=True)
        }
        self.cached = None

    def gen_graph_id(self):
        return str(uuid.uuid4())

    def generate(self, number_of_graphs=1, weights=[]):
        '''
        Weights list can be used to generate a set of graphs that differ only in assigned weights
        '''
        allG = None # if to generate one graph only and assign different weights to it
        if len(weights) > 0:
            allG = self.generators[self.params["graph_type"]]()
            if self.params.get("both_directions", 0) == 1:
                raise Exception("Graph type " + self.params["graph_type"] + " does not support both_directions")

        number_of_graphs = max([number_of_graphs, len(weights)])
        for i in range(number_of_graphs):
            if allG == None:
                G = self.generators[self.params["graph_type"]]()
            else:
                G = allG.copy()

            if self.params["graph_type"] in ['watts_strogatz', 'powerlaw_cluster', 'grid', "karate", 'minnesota']:
                G = self.add_random_directions(G, self.params["both_directions"])
            else:
                if self.params.get("both_directions", 0) == 1:
                    raise Exception("Graph type " + self.params["graph_type"] + " does not support both_directions")

            weight = -1
            if self.params["graph_type"] != 'vk':
                if len(weights) == 0:
                    weight = self.params["weight_scale"]
                else:
                    weight = weights[i]
                G = self.assign_weights(G, self.params["weight_scale"], self.params["random_weight"])

            G.graph = {}
            G.graph['graph_id'] = self.gen_graph_id()
            G.graph.update(self.params)
            G.graph['weight_scale'] = weight
            G.graph['m'] = G.number_of_edges()
            if "n" in self.params:
                assert(len(G) == self.params["n"])
            assert(nx.is_directed(G))
            yield G

    def get(self):
        # caching makes it possible to get graph and save to adjlist
        if self.cached == None:
            self.cached = next(self.generate())

        return self.cached

    @staticmethod # used in tests
    def assign_weights(G, weight_scale, random_weight):
        if random_weight:
            if weight_scale == -2:
                for e in G.edges():
                    G[e[0]][e[1]]['weight'] = np.random.random()
            else:
                for e in G.edges():
                    G[e[0]][e[1]]['weight'] = np.random.random()*weight_scale
        else:
            for e in G.edges():
                G[e[0]][e[1]]['weight'] = weight_scale if weight_scale > 0 else 1

        # normalize for LT
        if weight_scale == -2:
            for n in G.nodes():
                p = 0
                for in_n in G.predecessors(n):
                    p += G[in_n][n]['weight']
                for in_n in G.predecessors(n):
                    G[in_n][n]['weight'] = G[in_n][n]['weight'] / p

        # assert sum to 1 for LT, comment out when using IC
        for n in G.nodes():
            assert np.sum([G[in_n][n]['weight'] for in_n in G.predecessors(n)]) <= 1

        return G

    @staticmethod
    def add_random_directions(G, both=False):
        # note that if both=True and there are two mutually connected nodes already
        # then edge weight will be selected as one value depending on edge traversal of networkx
        assert(not nx.is_directed(G))
        dG = nx.DiGraph()
        dG.add_nodes_from(range(len(G)))

        for e in G.edges():
            if both:
                dG.add_edge(e[0],e[1])
                dG.add_edge(e[1],e[0])
                for key in G[e[0]][e[1]]:
                    dG[e[0]][e[1]][key] = G[e[0]][e[1]][key]
                    dG[e[1]][e[0]][key] = G[e[0]][e[1]][key]
            else:
                if np.random.random() < 0.5:
                    dG.add_edge(e[0],e[1])
                    for key in G[e[0]][e[1]]:
                        dG[e[0]][e[1]][key] = G[e[0]][e[1]][key]
                else:
                    dG.add_edge(e[1],e[0])
                    for key in G[e[1]][e[0]]:
                        dG[e[1]][e[0]][key] = G[e[0]][e[1]][key]
        assert(len(dG) == len(G))
        return dG

    def get_stanford_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'web-Stanford.txt')) as f:
            for line in f:
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.DiGraph()
        G.add_edges_from(edges)
        G = nx.convert_node_labels_to_integers(G)
        self.params["n"] = len(G)
        return G

        mat = scipy.io.loadmat(os.path.join(os.environ['ALLDATA_PATH'], 'web-Stanford.txt'))


        sparse = mat['Problem'][0][0][2]
        m = csr_matrix(sparse)
        g = nx.DiGraph()
        G = nx.from_numpy_matrix(m.toarray(), create_using=g)
        return G

    def get_gnutella_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'p2p-Gnutella31.txt')) as f:
            nodes, edge_count = f.readline().split()
            nodes = int(nodes)
            edge_count = int(edge_count)
            for line in f:
                edges.append((int(line.split()[0]), int(line.split()[1])))
        assert(len(edges) == edge_count)
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_epinions_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'soc-epinions.mtx')) as f:
            nodes, edge_count = f.readline().split()
            nodes = int(nodes)
            edge_count = int(edge_count)
            for line in f:
                edges.append((int(line.split()[0]), int(line.split()[1])))
                edges.append((int(line.split()[1]), int(line.split()[0])))
        assert(len(edges) == edge_count*2)
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_mit_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'socfb-MIT.mtx')) as f:
            nodes, edge_count = f.readline().split()
            nodes = int(nodes)
            edge_count = int(edge_count)
            for line in f:
                edges.append((int(line.split()[0]), int(line.split()[1])))
                edges.append((int(line.split()[1]), int(line.split()[0])))
        assert(len(edges) == edge_count*2)
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_brightkite_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'soc-brightkite.mtx')) as f:
            comment = f.readline()
            nodes_dub, nodes, edge_count = f.readline().split()
            nodes = int(nodes)
            edge_count = int(edge_count)
            for line in f:
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_blogs_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'moreno_blogs', 'out.moreno_blogs_blogs')) as f:
            nodes, edge_count = f.readline().split()
            nodes = int(nodes)
            edge_count = int(edge_count)

            for line in f:
                if (int(line.split()[0]) == int(line.split()[1])):
                    continue
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G


    def get_advogato_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'soc-advogato', 'soc-advogato.edges')) as f:
            f.readline()
            info = f.readline().split()
            edge_count = info[1]
            nodes = info[2]
            f.readline()
            nodes = int(nodes)
            edge_count = int(edge_count)

            for line in f:
                if (int(line.split()[0]) == int(line.split()[1])):
                    continue
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_minnesota_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'road-minnesota.mtx')) as f:
            info = f.readline().split()
            edge_count = info[2]
            nodes = info[1]
            f.readline()
            nodes = int(nodes)
            edge_count = int(edge_count)

            for line in f:
                if (int(line.split()[0]) == int(line.split()[1])):
                    continue
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.Graph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_dblp_graph(self):
        edges = []
        with open(os.path.join(os.environ['ALLDATA_PATH'], 'dblp-cite', 'out.dblp-cite')) as f:
            f.readline()
            info = f.readline().split()
            edge_count = info[1]
            nodes = info[2]
            f.readline()
            nodes = int(nodes)
            edge_count = int(edge_count)

            for line in f:
                if (int(line.split()[0]) == int(line.split()[1])):
                    continue
                edges.append((int(line.split()[0]), int(line.split()[1])))
        G = nx.DiGraph()
        G.add_nodes_from(range(nodes))
        G.add_edges_from(edges)
        self.params["n"] = len(G)
        return G

    def get_vk_graph(self):
        G = nx.read_gpickle(os.path.join(os.environ['ALLDATA_PATH'], 'vk_graph_cleaned.pkl'))
        self.params["n"] = len(G)
        G = nx.convert_node_labels_to_integers(G)
        return G

    def get_cph_graph(self):
        G = nx.read_adjlist(os.path.join(os.environ['ALLDATA_PATH'], 'copenhagen.adj'), create_using=nx.DiGraph())
        self.params["n"] = len(G)
        G = nx.convert_node_labels_to_integers(G)
        return G

    @staticmethod
    def save_to_adjlist_dir(graph_dir, G):
        nx.write_edgelist(G, os.path.join(graph_dir, "graph.csv"), delimiter=" ", data=['weight'])
        with open(os.path.join(graph_dir, "attributes.txt"),'w') as f:
            f.write("n={}\nm={}\n".format(G.number_of_nodes(), G.number_of_edges()))
            for param in G.graph:
                if param == "n":
                    continue
                if param == "m":
                    f.write("{}={}\n".format("gg_m", G.graph[param]))
                else:
                    f.write("{}={}\n".format(param, G.graph[param]))

    @staticmethod
    def save_to_adjlist(tag, G):
        fm = FileManager(tag)
        if G == None:
            G = self.get()
        graph_dir = os.path.join(fm.get_data_path(), G.graph['graph_id'])
        fm.create_path(graph_dir)
        Generator.save_to_adjlist_dir(graph_dir, G)
        return graph_dir #os.path.join(fm.get_relative_data_path(), G.graph['graph_id'])

    @staticmethod
    def save_to_pickle(tag, G):
        fm = FileManager(tag)
        if G == None:
            G = self.get()
        relative_gpath = os.path.join(fm.get_relative_data_path(), G.graph['graph_id'], G.graph['graph_id'] + ".pkl")
        graph_path = os.path.join(fm.get_root_path(), relative_gpath)
        fm.create_path(os.path.join(fm.get_data_path(), G.graph['graph_id']))
        nx.write_gpickle(G, graph_path)
        return graph_path #relative_gpath
