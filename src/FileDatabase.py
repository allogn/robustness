import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import pymongo
import json
import logging
import uuid
import hashlib
import pandas as pd
import re

import helpers
from FileManager import *
from Database import *

class FileDatabase(Database):

    def __init__(self, tag = "tmp"):
        self.fm = FileManager("robustDatabaseFiles")
        self.fm.create_path(self.fm.get_data_path())
        Database.__init__(self, tag)

    def get_full_path(self, row_id):
        r_filename = row_id + ".json"
        complete_path = os.path.join(self.fm.get_data_path(), r_filename)
        return complete_path

    def count(self):
        return len([f for f in FileManager.list_files(self.fm.get_data_path()) if f.find(str(helpers.get_static_hash(self.tag))) > -1])

    def save(self, row):
        random_id = "{}_{}".format(helpers.get_static_hash(self.tag), self.get_random_filename())
        complete_path = self.get_full_path(random_id)
        with open(complete_path,"w") as f:
            json.dump(self.enrich_row(row), f)
        logging.info("New file created at {}".format(complete_path))
        return random_id

    def load(self, row_id):
        filename = row_id + ".json"
        full_path = os.path.join(self.fm.get_data_path(), filename)
        r = {}
        if not self.fm.is_file(full_path):
            raise Exception("File with id {} does not exist".format(random_id))
        with open(full_path, "r") as f:
            r = json.load(f)
            if len(r) == 0:
                logging.warning("Loading empty result for id {}".format(random_id))
        return r

    def load_df_with_tag(self):
        ht = helpers.get_static_hash(self.tag)
        files = [f for f in os.listdir(self.fm.get_data_path()) if re.match("{}_.*\.json".format(ht), f)]
        if (len(files) == 0):
            raise Exception("Results for tag {} do not exist".format(self.tag))

        results = pd.DataFrame()
        for f in files:
            full_path = os.path.join(self.fm.get_data_path(), f)
            with open(full_path,'r') as fp:
                r = json.load(fp)
                if len(r) == 0:
                    logging.warning("File {} has empty result".format(f))
                else:
                    results = results.append(r, ignore_index=True)
        return results

    @staticmethod
    def get_random_filename():
        return str(uuid.uuid4())

    def remove_tag(self, tag):
        ht = helpers.get_static_hash(tag)
        files = [f for f in os.listdir(self.fm.get_data_path()) if re.match("{}_.*\.json".format(ht), f)]
        for f in files:
            os.remove(os.path.join(self.fm.get_data_path(),f))
        logging.info("{} files with tag {} removed.".format(len(files), tag))
