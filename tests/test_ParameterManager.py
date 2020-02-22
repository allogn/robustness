import unittest
from collections.abc import Iterable

from src.ParameterManager import *

class TestParameterManager(unittest.TestCase):
    def setUp(self):
        pass

    def test_ParamFeasibility(self):
        fail = {"problem": {"a": 1}}
        fail2 = {"a" : 20}
        with self.assertRaises(AssertionError):
            pm = ParameterManager(fail, load_defaults = False)
        with self.assertRaises(AssertionError):
            pm = ParameterManager(fail2, load_defaults = False)

    def test_get_data_param_sets(self):
        pm = ParameterManager({}, load_defaults=True)
        required = ["graph_type", "weight_scale", "random_weight"]
        for p in pm.get_data_param_sets():
            for r in required:
                self.assertTrue(r in p)
            for k in p:
                self.assertFalse(type(p[k]) == dict)

        pm2 = ParameterManager({"data": {
              "graph_type": "path",
              "weight_scale": [0.5, [1]],
              "n": [10, [15, 20]]
            }})
        self.assertEqual(len(list(pm2.get_data_param_sets())), 4)

    def test_get_immunization_params(self):
        pm = ParameterManager({}, load_defaults=True)
        self.assertEqual(len(pm.get_immunization_params()), len(pm.get_defaults()['strategy']))

        pm = ParameterManager({'strategy': {'some': {'param': 'value'}}})
        self.assertEqual(len(pm.get_immunization_params()), 1)
        v = pm.get_immunization_params()[0]
        self.assertEqual(v['solver_name'], 'some')
        self.assertEqual(v['param'], 'value')
        self.assertTrue('solver_timeout' in v)

    def test_get_hyperparam_sets(self):
        pm = ParameterManager({}, load_defaults=True)
        s = pm.params['solver']
        i = len(s)
        for v in s:
            for v2 in s[v]:
                if isinstance(s[v][v2], Iterable):
                    i += len(s[v][v2]) - 1

        problem_param_size = 1
        for v in pm.params['problem']:
            if isinstance(pm.params['problem'][v], Iterable):
                problem_param_size += len(pm.params['problem'][v][1])
        i = i * problem_param_size
        self.assertEqual(len(pm.get_hyperparam_sets()), i)

        pm = ParameterManager({"solver": {"SatGreedy": {}}})
        s = pm.get_hyperparam_sets()[0]
        self.assertGreater(len(s), 0)

    def test_populate_dic_rec(self):
        d1 = {
            'a': {
                'b': 1
            }
        }
        d2 = {'a': {
            'c': 2
        }}
        ParameterManager.populate_dic_rec(d2, d1)
        self.assertEqual(d2['a']['b'], 1)

        d1 = {
            "problem": {
              "seed_imm_epsilon": 0.1,
              "beta": 2,
              "l": [2, [1]],
              "k": 2
            },
            "strategy": {
              "Degree": {}
            }
        }
        d2 = {
            "data": {
              "graph_type": "path",
              "weight_scale": 0.5,
              "random_weight": 0,
              "n": [10, [15]]
            },
            "number_of_graphs": 1,
            "solver": {
              "SatGreedy": {
                "epsilon": 0.5,
                "original_objective": [0, 1],
                "new_h": 0,
                "gamma": 0.001,
                "strict": [0, 1]
              }
            },
            "problem": {
              "seed_imm_epsilon": 0.1,
              "beta": 2,
              "l": [2, [1]],
              "k": 2
            },
            "strategy": {
              "Degree": {},
              "NetShield": {},
              "Dom": {},
              "DomDegree": {},
              "Random": {
                "n": 2
              },
              "RandPert": {
                "n": 2
              }
            }
        }
        ParameterManager.populate_dic_rec(d1, d2)
        self.assertEqual(len(d1['strategy']), 1)
        self.assertEqual(d1['data']['n'], [10, [15]])

    def test_flatten_dict_rec(self):
        d = {
            'a': 1,
            'b': {
                'c': 2
            }
        }
        ans = {
            'pre_a': 1,
            'pre_b_c': 2
        }
        d2 = ParameterManager.flatten_dict_rec(d, 'pre')
        self.assertEqual(d2, ans)

if __name__ == '__main__':
    unittest.main()
