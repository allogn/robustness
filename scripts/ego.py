# load vk and sample ego networks
import sys, os
sys.path.append(os.path.dirname(os.path.abspath('')))
sys.path.append(os.path.join(os.path.abspath(''),'..','scripts'))
sys.path.append(os.path.join(os.path.abspath(''),'..','src'))
from Generator import *
import pandas as pd
import pickle as pkl
import re

from Experiment import *
from Analyzer import *
from Artist import *
from FileManager import *

from tqdm import tqdm
from scipy.interpolate import CubicSpline

params = {
    "tag": "manual_vk",
    "data": {
      "manual": 1
    },
    "solver": {
      "Rob": {
        "mode": [0, 5],
        "iterations": 100,
        "number_of_blocked_nodes": -1,
        "beta": 32
      },
      "SatGreedy": {
        "epsilon": 0.2,
        "gamma": 0.1,
        "satgreedy": 0,
        "singlegreedy": 0,
        "singlegreedycelf": 1,
        "allgreedy": 0,
        "dimbeta": 32
      }
    },
    "strategy": {
      "Degree": {},
      "NetShield": {},
      "Random": {
        "random_id": 1
      },
      "Pagerank": {},
      "Katz": {},
      "Aquaintance": {},
      "Betweenness": {}
    },
    "problem": {
      "seeds": 1,
      "alpha": 1
    },
    "full_rerun": 1,
    "graph_analysis": 0,
    "solver_execution_timeout": 86400
}



def get_all_fake_values(y, x, n):
    cs = CubicSpline(x, y, extrapolate=True)
    return np.array(cs(range(n)))

def get_robustness(G_, verbose = False):
    G = copy.deepcopy(G_)
    G = nx.convert_node_labels_to_integers(G)

    G.graph['graph_id'] = '0'
    G.graph['n'] = len(G)
    experiment = Experiment(params, G, verbose)
    experiment.run()

    dd = Analyzer("manual_vk").df

    emf = dd[dd['mode'] == 5]
    emf_seq = np.amin([emf.loc[i,'sequence'] for i in emf.index],axis=0)
    res1 = sum(emf_seq)/int(dd.iloc[0]['n'])


    emf = dd[dd['mode'] == 0]
    emf_seq = np.amin([emf.loc[i,'sequence'] for i in emf.index],axis=0)
    res2 = sum(emf_seq)/int(dd.iloc[0]['n'])

    emf = dd[dd['mode'].isna()]
    emf_seq = emf.iloc[0]['sequence']
    res3 = sum(emf_seq)/int(dd.iloc[0]['n'])

    return res1, res2, res3


if __name__ == "__main__":
    gen = Generator({"graph_type": "vk"})
    g = gen.get()

    fm = FileManager("manual_vk_graphs")
    dpath = fm.get_data_path()
    fm.clean_data_path()

    for it in range(100):
        print("===== IT {} =====".format(it))
        a, b = 0, 0
        while a < 10 or a > 50 or b < 10 or b > 300:
            sample_ego = np.random.choice([n for n in g.nodes()])
            ego_network = g.subgraph([n for n in g.neighbors(sample_ego)] + [sample_ego])
            a, b = nx.number_of_nodes(ego_network), nx.number_of_edges(ego_network)
            print(a, b)

        ego_network = nx.DiGraph(ego_network)
        nx.write_gpickle(ego_network, os.path.join(dpath,"ego{}.pkl".format(it)))

        seq = []
        graphs = []
        mid = 1
        G = nx.DiGraph(ego_network)
        res = get_robustness(G)
        seq.append((0, res[0], res[1], res[2]))

        for nn in tqdm(range(len(ego_network)-3)):
            rank = []
            for n in tqdm(G.nodes()):
                G2 = nx.DiGraph(G)
                G2.remove_node(n)
                res = get_robustness(G2)
                rank.append((n, res[0], res[1], res[2]))

            best_node = sorted(rank,key=lambda x: x[mid],reverse=True)[0]
            G.remove_node(best_node[0])
            seq.append(best_node)
            graphs.append(nx.DiGraph(G))

        pkl.dump(graphs, open(os.path.join(dpath, "graphs{}.pkl".format(it)),"wb"))
        pkl.dump(seq, open(os.path.join(dpath, "seq{}.pkl".format(it)), "wb"))
