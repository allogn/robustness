import unittest
import src.helpers as helpers

class TestHelpers(unittest.TestCase):
    def test_load_config(self):
        dag = helpers.load_config("dag_defaults")
        self.assertTrue("dag" in dag)
