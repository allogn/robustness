import pandas as pd

import sys, os
sys.path.append(os.path.dirname(os.path.abspath('')))
sys.path.append(os.path.join(os.path.abspath(''),'..','scripts'))
sys.path.append(os.path.join(os.path.abspath(''),'..','src'))

from Experiment import *

tag = "ba_mm_twist"
params = {
    "tag": tag,
    "data": {
        "graph_type": "powerlaw_cluster",
        "both_directions": 1,
        "weight_scale": 0.3,
        "n": [1000, list(range(1000,2000,20))],
        "p": 0.4,
        "mm": 10
    },
    "full_rerun": 1,
    "strategy": {
        "Degree": {}
    },
    "solver": {
      "Rob": {
        "mode": [0,4],
        "number_of_blocked_nodes": 0
      }
    }
}
experiment = Experiment(params)
experiment.generate_datasets()
experiment.run_immunization()
n_default = 1000
#reduce_graphs_size_to_n()
db = experiment.db
for graph in db.graphs.find({"tag": tag}):
    G = nx.read_gpickle(graph['pickle_path'])
    immunization = db.immunization.find_one({"graph": graph['_id']})
    seq = json.load(open(immunization['solution_path'], 'r'))['Blocked nodes']
    if len(G) > n_default:
        delta = len(G) - n_default
        G.remove_nodes_from(seq[:delta])
        G = nx.convert_node_labels_to_integers(G)
        nx.write_gpickle(G, graph['pickle_path'])
        gen = Generator({})
        gen.save_to_adjlist(tag, G)
        db.graphs.update_one({'_id': graph['_id']}, {'$set': {'n': n_default, 'delta': delta}})

experiment.run_seeding()
