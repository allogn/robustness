import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import pymongo
import logging
import re
import pandas as pd
from bson.objectid import ObjectId

from Database import *

class MongoDatabase(Database):

    def __init__(self, tag="tmp"):
        #constr = "mongodb+srv://{}:{}@cluster0-gmsw0.mongodb.net/test?retryWrites=true&w=majority&ssl=true&ssl_cert_reqs=CERT_NONE".format(self.MONGO_USER, self.MONGO_PASS)
        constr = "mongodb://127.0.0.1:27017/admin"
        self.client = pymongo.MongoClient(constr, serverSelectionTimeoutMS=60000)
        self.db = self.client.robustExperimentsDB
        logging.debug("Connected to MongoDB")
        Database.__init__(self, tag)

    def __del__(self):
        self.client.close()

    def save(self, row):
        result = self.db.experiments.insert_one(self.enrich_row(row))
        return str(result.inserted_id)

    def load(self, row_id):
        result = self.db.experiments.find_one({'_id': ObjectId(row_id)})
        if result == None:
            raise Exception("Row with id {} does not exist".format(row_id))
        return result

    def load_df_with_tag(self):
        results = self.db.experiments.find({'tag': self.tag})
        r = list(results)
        if len(r) == 0:
            raise Exception("No results for tag {}".format(self.tag))
        return pd.DataFrame(r)

    def remove_tag(self, tag):
        result = self.db.experiments.delete_many({'tag': tag})
        logging.info("Removed {} entries with tag {}.".format(result.deleted_count, tag))

    def get_db(self):
        return self.db
