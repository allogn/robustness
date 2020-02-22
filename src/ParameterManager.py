from collections import Iterable
from datetime import datetime, timedelta
from itertools import product
import sys
import os
import copy

sys.path.append(os.path.dirname(__file__))
import helpers
from FileManager import *


class ParameterManager:
    '''
    Parameter Manager takes json that describes an experiment and contains some results,
    and outputs necessary parameters per each script.

    Put all solvers from default dag to active_solvers if latter is empty
    '''

    def __init__(self, params = {}, load_defaults = True):
        default_params = self.load_defaults()
        self.params = copy.deepcopy(params)
        if load_defaults:
            self.populate_dic_rec(self.params, default_params)
        self.check_params()
        self.fm = FileManager(self.params.get("tag", "untitled"))

    def load_defaults(self):
        dag_defaults = helpers.load_config("dag_defaults")
        return dag_defaults

    def check_params(self):
        assert("problem" in self.params)
        assert("data" in self.params)
        assert(len(self.params["solver"]) > 0)
        assert(len(self.params["strategy"]) > 0)

    @staticmethod
    def flatten_dict_rec(d, prefix=""):
        d_flat = {}
        if prefix != "":
            prefix += "_"
        for key in d:
            new_key = prefix + str(key)
            if isinstance(d[key], dict):
                d_flat.update(ParameterManager.flatten_dict_rec(d[key], new_key))
            else:
                d_flat[new_key] = d[key]
        return d_flat

    @staticmethod
    def populate_dic_rec(dic1, dic2):
        for key in dic2:

            if (key == "solver" or key == "strategy") and (key in dic1): # do not add new solvers if they don't exist
                for key2 in dic1[key]:
                    if key2 in dic2[key]:
                        ParameterManager.populate_dic_rec(dic1[key][key2], dic2[key][key2])
                continue

            if key not in dic1:
                dic1[key] = dic2[key]
            else:
                if isinstance(dic2[key], dict):
                    ParameterManager.populate_dic_rec(dic1[key], dic2[key])

    def get_data_param_sets(self):
        sets = []
        defaults = self.get_defaults()['data']
        for s in self.generate_sets_rec(self.get("data"), defaults):
            sets.append(s)
        sets.append(defaults)
        return sets

    def get_problem_param_sets(self):
        sets = []
        defaults = self.get_defaults()['problem']
        for s in self.generate_sets_rec(self.get("problem"), defaults):
            sets.append(s)
        sets.append(defaults)
        return sets

    def get_hyperparam_sets(self):
        # fills each solver set with problem parameters, so in total there is #problems X #solvers sets
        # for each solver fills missing values with defaults

        sets = []
        if len(self.params["solver"]) == 0:
            raise Exception("Hyperparameter sets in ParameterManager can not generate entries. "
                            "No solver data to plot, or missing Solver column in df.")
        for problem_param_set in self.get_problem_param_sets():
            for solver_name in self.params["solver"]:
                param_names = []
                param_values = []
                for solver_param in self.params["solver"][solver_name]:
                    param_names.append(solver_param)
                    a = self.params["solver"][solver_name][solver_param]
                    param_values.append(a if isinstance(a, list) else [a])
                all_solver_param_sets = product(*param_values)
                for param_set in all_solver_param_sets:
                    solver_case_dict = {'solver': solver_name}
                    for i in range(len(param_names)):
                        solver_case_dict[param_names[i]] = param_set[i]
                    solver_case_dict.update(problem_param_set)
                    sets.append(solver_case_dict)
        return sets

    def get_immunization_params(self):
        # does not support "filling up" default parameters of mentioned solvers, and does not support generation of parameters!
        sets = []
        for solver_name in self.params["strategy"]:
            params = dict(self.params["strategy"][solver_name])
            params['solver_name'] = solver_name
            params['solver_timeout'] = self.params['solver_execution_timeout']
            sets.append(params)
        return sets

    def get_defaults(self):
        return ParameterManager.get_defaults_rec(self.params)

    def get_flatten_defaults(self):
        return self.flatten_dict_rec(self.get_defaults())

    @staticmethod
    def get_defaults_rec(d):
        s = {}
        for key in d:
            if isinstance(d[key], dict):
                s[key] = ParameterManager.get_defaults_rec(d[key])
            else:
                if isinstance(d[key], list):
                    if len(d[key]) == 0:
                        s[key] = None
                    else:
                        s[key] = d[key][0]
                else:
                    s[key] = d[key]
        return s

    def generate_sets_rec(self, params, defaults):
        for v in sorted([key for key in params]):
            if isinstance(params[v], dict):
                for i in self.generate_sets_rec(params[v], defaults[v]):
                    new_param_set = dict(defaults)
                    new_param_set[v] = i
                    yield new_param_set
            else:
                if isinstance(params[v], Iterable):
                    if isinstance(params[v], str) or len(params[v]) != 2 or not isinstance(params[v][1], list):
                        continue
                    for val in params[v][1]:
                        new_param_set = dict(defaults)
                        new_param_set[v] = val
                        yield new_param_set

    @staticmethod
    def get_param_footprint(params):
        return "_".join(sorted(ParameterManager.get_param_list_rec(params))).replace(".", "")

    @staticmethod
    def get_abbr(s):
        return "".join([word[0] for word in s.split("_")])

    @staticmethod
    def get_param_list_rec(params):
        s = []
        for key in params:
            if not isinstance(params[key], dict):
                val = "%.4f" % params[key] if isinstance(params[key], float) else str(params[key])
                s.append(ParameterManager.get_abbr(key) + val)
            else:
                s += ParameterManager.get_param_list_rec(params[key])
        return s

    def get(self, name):
        return self.params[name]

    def get_all_params(self):
        return self.params
