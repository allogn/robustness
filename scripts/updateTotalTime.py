import sys
import os
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
from MongoDatabase import *

db_wrapper = MongoDatabase(tag)
db = db_wrapper.db

for solver_result in db.seeding.find(q):
    if row['cmd'].find("/Rob ") > 0 or row['cmd'].find("/Imm ") > 0:
        immunizer = self.db.immunization.find_one(row['related_obj_id'])

        #load total time
        t = json.load(open(immunizer['solution_path']))
        immunizer[t['solver_name'] + " total time"] = t['Total time']
        print(t['Total time'])
