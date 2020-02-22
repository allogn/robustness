import sys, os
sys.path.append(os.path.join(os.path.abspath(''), '..', 'src'))
from Experiment import *
from FileManager import *
from Analyzer import *

import json
import datetime
import math
import copy
import logging
import networkx as nx
import pickle as pkl
from scipy.interpolate import CubicSpline

params = {
    "tag": "manual",
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
      "seeds": 5,
      "alpha": 1.15
    },
    "full_rerun": 1,
    "graph_analysis": 1,
    "solver_execution_timeout": 86400
}

# set up logging to file - see previous section for more details
logging.basicConfig(level=logging.WARNING,
                    format='%(asctime)-3s :: %(message)s',
                    datefmt='%m-%d %H:%M')
# define a Handler which writes INFO messages or higher to the sys.stderr
# console = logging.StreamHandler()
# console.setLevel(logging.warning)
# # add the handler to the root logger
# logging.getLogger('').addHandler(console)

def get_all_fake_values(y, x, n):
    cs = CubicSpline(x, y, extrapolate=True)
    return np.array(cs(range(n)))

def get_robustness(G_, verbose = False):
    G = G_.__class__()
    G.add_nodes_from(G_)
    G.add_edges_from(G_.edges(data=True))

    G.graph['graph_id'] = '0'
    G.graph['n'] = len(G)
    experiment = Experiment(params, G, verbose)
    experiment.run()

    dd = Analyzer("manual").df

    emf = dd[dd['mode'] == 5]
    emf_seq = np.amin([emf.loc[i,'sequence'] for i in emf.index],axis=0)

    fake_x = emf.iloc[0]['samples_blocked_values']
    fake_values = emf_seq
    fake_all = get_all_fake_values(fake_values, fake_x[:len(fake_values)], len(G))

    res1 = sum(fake_all)/int(dd.iloc[0]['n'])

    #####
    emf = dd[dd['mode'] == 0]
    emf_seq = np.amin([emf.loc[i,'sequence'] for i in emf.index],axis=0)

    fake_x = emf.iloc[0]['samples_blocked_values']
    fake_values = emf_seq
    fake_all = get_all_fake_values(fake_values, fake_x[:len(fake_values)], len(G))

    res2 = sum(fake_all)/int(dd.iloc[0]['n'])

    #####
    emf = dd[dd['mode'].isna()]
    fake_x = emf.iloc[0]['samples_blocked_values']
    fake_values = emf.iloc[0]['sequence']
    fake_all = get_all_fake_values(fake_values, fake_x[:len(fake_values)], len(G))

    res3 = sum(fake_all)/int(dd.iloc[0]['n'])

    return res1, res2, res3

if __name__ == "__main__":

    # let us improve srim
    fm = FileManager("manual_orig")
    path = fm.get_data_path()
    G = nx.read_gpickle(os.path.join(path,'G.pkl'))
    logging.warning("Graph with {} nodes and {} edges. Results saving to {}.".format(len(G),nx.number_of_edges(G),path))

    max_imp = 10*nx.number_of_edges(G)
    improvement = max_imp # consecutive trials
    res1, res2, res3 =  get_robustness(G, False)
    trials = [(res1, res2, res3)]
    iteration = 0
    succ_iter = 0

    while improvement > 0:
        # swap two edges
        iteration += 1
        l = [e for e in G.edges(data=True)]

        G2 = G.__class__()
        G2.add_nodes_from(G)
        G2.add_edges_from(G.edges(data=True))

        e1, e2 = 0, 0
        while True:
            ei = np.random.choice(len(l),2,replace=False)
            e1 = l[ei[0]]
            e2 = l[ei[1]]
            if G2.has_edge(e1[0],e2[1]) or G2.has_edge(e2[0],e1[1]):
                continue
            G2.remove_edge(e1[0],e1[1])
            G2.remove_edge(e2[0],e2[1])
            G2.add_edge(e1[0],e2[1],weight=e1[2]['weight'])
            G2.add_edge(e2[0],e1[1],weight=e2[2]['weight'])
            break
        res1, res2, res3 =  get_robustness(G2, False)
        logging.warning("{} (rate {}): {} -> {}".format(iteration, succ_iter/iteration, trials[-1], (res1, res2, res3)))
        if res3 > trials[-1][2]:
            G = G2
            trial_time = max_imp-improvement
            trials.append((res1, res2, res3, trial_time))
            logging.warning("Success with Delta {}, trial {}".format(res3 - trials[-2][2], trial_time))
            nx.write_gpickle(G, os.path.join(path,'G{}.pkl'.format(succ_iter)))
            pkl.dump(trials, open(os.path.join(path,'trials.pkl'),"wb"))
            succ_iter += 1
            improvement = max_imp
        else:
            improvement -= 1
