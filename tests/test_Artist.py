import unittest
import src.helpers as helpers
from Artist import *
from FileManager import *
from ParameterManager import *

class TestArtist(unittest.TestCase):

    def setUp(self):
        self.fm = FileManager("testing_artist")
        self.temp_path = self.fm.get_data_path()
        self.pm = ParameterManager({
            "problem": {},
            "data": {},
            "solver": {"some_solver": {}},
            "figures": {
                "ordering": [],
                "renaming": []
            },
            "dpi": 72
        })

        df = pd.DataFrame()
        for i in range(2):
            df_case = pd.DataFrame({"alg": ["alg"+str(i)]*3, "x_label": [1,2,3], "y_label": [4+i,5+i,6+i], "error": [0.1,0.2,0.3]})
            df = pd.concat([df, df_case])
        self.df = df

    ## outdated, todo

    # def test_plot_errorbar(self):
    #     artist = Artist(self.pm)
    #     artist.plot_errorbar(self.temp_path, "prefix_", "hello_errorbar", self.df[["alg", "x_label", "y_label", "error"]])
    #     self.assertTrue(FileManager.is_file(os.path.join(self.temp_path, "prefix_hello_errorbar.png")))
    #
    # def test_plot_bar(self):
    #     artist = Artist(self.pm)
    #     artist.plot_bar(self.temp_path, "prefix_", "hello_bar", self.df[["alg", "x_label"]])
    #     self.assertTrue(FileManager.is_file(os.path.join(self.temp_path, "prefix_hello_bar.png")))
    #
    # def test_plot_line(self):
    #     artist = Artist(self.pm)
    #     artist.plot_line(self.temp_path, "prefix_", "hello_line", self.df[["alg", "x_label", "y_label"]])
    #     self.assertTrue(FileManager.is_file(os.path.join(self.temp_path, "prefix_hello_line.png")))
    #
    # def tearDown(self):
    #     self.fm.remove_all()
