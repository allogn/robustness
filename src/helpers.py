import configparser
import logging
import pickle as pkl
import sys
import os
import numpy as np
import networkx as nx
import json
from collections import Iterable
import hashlib

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))


class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return json.JSONEncoder.default(self, obj)


def load_config(config_name):
    filename = os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','config', config_name + ".json")
    f = open(filename)
    config = json.load(f)
    f.close()
    return config

def mov_avg(arr):
    if len(arr) < 2:
        return arr
    return np.divide(np.cumsum(arr), np.array(range(1,len(arr)+1)))


def el_in_set(set1, set2):
    '''
    Check if any element from set1 is in set2
    '''
    for el in set1:
        if el in set2:
            return True
    return False


def set_in_set(set1, set2):
    '''
    Check if all elements of set1 are in set2
    '''
    for el in set1:
        if el not in set2:
            return False
    return True


def flatten(l):
    return [item for sublist in l for item in sublist]


def flip_result_dict(data_dict):
    result = {}
    for solname in data_dict:
        for paramset in data_dict[solname]:
            if paramset not in result:
                result[paramset] = dict()
            result[paramset][solname] = data_dict[solname][paramset]
    return result


def get_static_hash(string):
    h = int(hashlib.md5(string.encode('utf-8')).hexdigest(), 16)
    return h


def load_graph(f):
    ff = open(f,'rb')
    a = pkl.load(ff)
    ff.close()
    return a


def load_seeds(f):
    return np.atleast_1d(np.loadtxt(f))
