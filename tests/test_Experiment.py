import unittest
from src.Experiment import *

class TestExperiment(unittest.TestCase):
    def setUp(self):
        pass

    def test_init(self):
        # test default values
        experiment = Experiment()
        db = experiment.db
        q = {'tag': experiment.pm.get('tag')}
        self.assertEqual(db.immunization.find_one(q), None)
        self.assertEqual(db.seeding.find_one(q), None)
        self.assertEqual(db.analysis.find_one(q), None)
        params = db.parameters.find_one(q)
        del params['_id']
        self.assertEqual(params, experiment.pm.load_defaults())

    def test_generation(self):
        experiment = Experiment()
        self.assertEqual(len(experiment.fm.list_files(experiment.fm.get_data_path())), 0)
        self.assertEqual(len(experiment.fm.list_dirs(experiment.fm.get_data_path())), 0)
        db = experiment.db
        experiment.generate_datasets()
        q = {'tag': experiment.pm.get('tag')}
        self.assertEqual(db.graphs.count_documents(q), 2)
        self.assertEqual(len(experiment.fm.list_dirs(experiment.fm.get_data_path())), 2)
        for graph in db.graphs.find(q):
            path = graph['adjlist_path']
            self.assertEqual(len(experiment.fm.list_files_with_ext(path, 'pkl')), 1)
            self.assertEqual(len(experiment.fm.list_files_with_ext(path, 'csv')), 1)
            self.assertEqual(len(experiment.fm.list_files_with_ext(path, 'txt')), 1)
            G = nx.read_gpickle(graph['pickle_path'])
            self.assertGreater(G.number_of_nodes(),1)

        experiment.generate_datasets()
        self.assertEqual(len(experiment.fm.list_dirs(experiment.fm.get_data_path())), 2)
        self.assertEqual(db.graphs.count_documents(q), 2)

        experiment.generate_datasets(True)
        self.assertEqual(db.graphs.count_documents(q), 2)
        self.assertEqual(len(experiment.fm.list_dirs(experiment.fm.get_data_path())), 4)

    def test_immunization(self):
        experiment = Experiment()
        experiment.generate_datasets()
        experiment.run_immunization()
        number_of_immunizers = len(experiment.pm.get("strategy"))
        number_of_graphs = 2
        expected_files = number_of_immunizers * number_of_graphs
        q = {'tag': experiment.pm.get('tag')}
        self.assertEqual(experiment.db.immunization.count_documents(q), expected_files)
        self.assertEqual(experiment.file_db.count(), expected_files)

    def test_seeding(self):
        experiment = Experiment()
        experiment.run()
        q = {'tag': experiment.pm.get('tag')}
        self.assertGreater(experiment.db.seeding.count_documents(q), 0)

if __name__ == '__main__':
    unittest.main()
