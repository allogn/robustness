import unittest
from src.Analyst import *
from src.Experiment import *

class TestAnalyst(unittest.TestCase):

    def setUp(self):
        pass
        # dag_defaults = helpers.load_config("dag_defaults")
        # params = {"title": "test_analyst"}
        # params.update(dag_defaults)
        # self.pm = ParameterManager(params)

    def test_default_experiment_loading(self):
        experiment = Experiment()
        experiment.run()
        analyst = Analyst(experiment.pm.get("tag"))

    @unittest.skip
    def test_set_results(self):
        # deprecated from airflow
        pm = ParameterManager({
            "problem": {},
            "data": {},
            "solver": {"some_solver": {}}
        })
        test_results = [
            {
                'problem': {'a': 11},
                'solver': {'solvername': {'b': 22}},
                'results': {'c': 33},
                'seed_solver': "aaa"
            }
        ]
        analyst = Analyst(pm)
        ans = [
            {
                'problem_a': 11,
                'solver_solvername_b': 22,
                'results_c': 33,
                'seed_solver': "aaa"
            }
        ]
        ans = pd.DataFrame(ans)
        analyst.set_results(test_results)
        self.assertTrue(analyst.results.equals(ans))
