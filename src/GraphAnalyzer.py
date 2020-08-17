import networkx as nx
import numpy as np
import logging
from networkx.algorithms import approximation

from MongoDatabase import *

class GraphAnalyzer:
    def __init__(self, tag):
        self.tag = tag
        self.db_wrapper = MongoDatabase(tag)
        self.db = self.db_wrapper.db

    def run(self):
        # load all networks with a tag, update files (attributes), and mongo entry
        for g_info in self.db.graphs.find({"tag": self.tag}):
            G = nx.read_gpickle(g_info['pickle_path'])
            a = self.analyze_graph(G)
            G.graph.update(a)
            nx.write_gpickle(G, g_info['pickle_path'])

            with open(os.path.join(g_info['adjlist_path'],'attributes.txt'), 'a') as f:
                for p in a:
                    f.write("{}={}\n".format(p, a[p]))

            self.db.graphs.update_one({'_id': g_info['_id']}, {'$set': a})

    def analyze_graph(self, G):
        new_attributes = {}

        new_attributes['directed'] = nx.is_directed(G)
        G_und = G.to_undirected()
        #new_attributes['connected_components'] = nx.number_connected_components(G_und)
        #new_attributes['largest_component'] = len(max(nx.connected_components(G_und), key=len))
        new_attributes['scc_number'] = len(list(nx.strongly_connected_components(G)))
        new_attributes['average_clustering'] = approximation.average_clustering(G_und)
        #new_attributes['algebraic_connectivity'] = nx.linalg.algebraicconnectivity.algebraic_connectivity(G_und, normalized=True)

        degrees = [d for n, d in G.degree()]
        new_attributes['min_degree'] = min(degrees)
        new_attributes['max_degree'] = max(degrees)
        new_attributes['avg_degree'] = np.mean(degrees)
        new_attributes['std_degree'] = np.std(degrees)
        new_attributes['median_degree'] = np.median(degrees)
        logging.info("Graph ID {}: degrees analyzed.".format(G.graph['graph_id']))

        return new_attributes
