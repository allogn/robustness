import unittest
import pandas as pd
import src.helpers as helpers
import sys, os
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from MongoDatabase import *
from FileDatabase import *

class TestDatabase(unittest.TestCase):
    def setUp(self):
        self.databases = [("mongo", MongoDatabase()), ("file", FileDatabase())]

    def test_single(self):
        for dbp in self.databases:
            with self.subTest(name="Mode: {}".format(dbp[0])):
                db = dbp[1]
                row = {"a": "b"}
                row_id = db.save(row)
                row2 = db.load(row_id)
                row2.pop('created')
                row2.pop('tag')
                if "_id" in row2:
                    row2.pop('_id')
                self.assertDictEqual(row, row2)
                db.clean()

                # bug below  --- delete testing tags first #fixme
    # def test_many(self):
    #     for dbp in self.databases:
    #         with self.subTest(name="Mode: {}".format(dbp[0])):
    #             row1 = {"1":2}
    #             row2 = {"3":4}
    #             row3 = {"dasda": 42134234}
    #             db = dbp[1]
    #             db.set_tag("blabla")
    #             db.save(row1)
    #
    #             db.set_tag("ttt")
    #             db.save(row2)
    #             db.save(row3)
    #
    #             db.set_tag("blabla")
    #             df = db.load_df_with_tag()
    #             self.assertEqual(len(df), 1)
    #
    #             db.set_tag("ttt")
    #             df = db.load_df_with_tag()
    #             self.assertEqual(len(df), 2)
    #             self.assertTrue("3" in dict(df.iloc[0]))
    #             self.assertTrue("dasda" in dict(df.iloc[1]))
    #
    #             db.set_tag("ttt")
    #             db.clean()
    #             db.set_tag("blabla")
    #             db.clean()
    #
    #             db.set_tag("ttt")
    #             with self.assertRaises(Exception):
    #                 db.load_df_with_tag()
    #
    #             db.set_tag("blabla")
    #             with self.assertRaises(Exception):
    #                 db.load_df_with_tag()
