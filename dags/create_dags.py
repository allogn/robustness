from airflow import DAG, utils
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
from DagFactory import *
import json
import helpers
from ParameterManager import *

dags_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','dags')
dag_defaults = helpers.load_config("dag_defaults")

for f in os.listdir(dags_path):
    filename, file_extension = os.path.splitext(f)
    if file_extension == '.json':
        f = open(os.path.join(dags_path, f),'r')
        params = json.load(f)
        f.close()

        ParameterManager.populate_dic_rec(params, dag_defaults)

        params['title'] = params.get("title", os.path.basename(filename))
        dag_factory = DagFactory(params)
        globals()[filename] = dag_factory.create_dag()
