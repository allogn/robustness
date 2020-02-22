import helpers
import time
import json
import uuid
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from FileManager import *

class Solver:
    def __init__(self, G, **params):
        if len(G) == 0:
            raise Exception("Graph can not be empty")
        self.G = G.copy()
        self.log = {}
        self.log['created'] = time.time()
        self.log['solver'] = self.get_name()[:self.get_name().find("Solver")]
        self.params = params
        self.k = params.get('k', len(G))
        self.clear()

    def clear(self):
        pass

    def get_name(self):
        return self.__class__.__name__

    def add_immunized_graph_to_json(self):
        edgelist = []
        weights = []
        for e in self.G.edges(data=True):
            if e[0] in self.blocked or e[1] in self.blocked:
                continue
            edgelist += [e[0], e[1]]
            if self.params.get('static',0) == 1:
                weights.append(1)
            else:
                if "weight" in e[2]:
                    weights.append(e[2]['weight'])
        self.log['edgelist'] = edgelist
        self.log['weights'] = weights
        self.log['N'] = len(self.G) - len(self.blocked)

    def set_blocked(self, blocked):
        b = [int(node) for node in blocked]
        self.blocked = set(b)
        self.log['Blocked nodes'] = b
        assert(len(self.blocked) == len(blocked)) # no repeating nodes in the blocked set
        self.add_immunized_graph_to_json()

    def export_blocked(self, tag):
        fm = FileManager(tag)
        outfile = os.path.join(fm.get_data_path(), str(uuid.uuid4()) + ".csv")
        with open(outfile,'w') as f:
            for n in self.log["Blocked nodes"]:
                f.write("{}\n".format(n))
        return outfile
