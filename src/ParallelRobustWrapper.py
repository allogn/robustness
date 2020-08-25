import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import logging
import helpers
from subprocess import Popen, PIPE
from multiprocessing.pool import ThreadPool
import pymongo
import datetime
import tqdm
import uuid

from MongoDatabase import *
from RobustWrapper import *

class ParallelRobustWrapper(RobustWrapper):
    WORKERS_NUM = None  # set to the number of workers you want (it defaults to the cpu count of your machine)
    TIMEOUT = 36000 # in seconds

    def __init__(self, tag, verbose = True):
        self.all_cmds = []
        self.tag = tag
        self.verbose = verbose
        self.fm = FileManager(tag)

    def run_parallel(self):
        logging.info("\n===================\nStarting parallel execution of {} tasks\n++++++++++++++++++".format(len(self.all_cmds)))
        results = []
        verbose = self.verbose
        if verbose:
            t = tqdm.tqdm(total=len(self.all_cmds), desc='Parallel Seeding')
        def callbackfunct(smth):
            if verbose:
                t.update()
                t.refresh()
        def handle_error(error):
            # if there is a solver error, then it falls WITHIN a thread, not here.
            # Here it falls only if there is a bug with ParallelWrapper itself
            logging.error(error)

        tp = ThreadPool(ParallelRobustWrapper.WORKERS_NUM)
        for cmd, output, related_obj_id in self.all_cmds:
            results.append(tp.apply_async(ParallelRobustWrapper.add_cmd_and_data_loading, (cmd, self.tag, output, related_obj_id),
                                            callback=callbackfunct, error_callback=handle_error))

        tp.close()
        tp.join()
        if verbose:
            t.close()

    @staticmethod
    def add_cmd_and_data_loading(cmd, tag, result_file, related_obj_id):
        starting_time = str(datetime.datetime.now())
        logging.info("Starting {}".format(" ".join(cmd)))
        cmd_output, errors = ParallelRobustWrapper.run_cmd(cmd,False,ParallelRobustWrapper.TIMEOUT)
        if len(errors.decode('utf-8')) > 0:
            logging.error("Thread terminated with error {}. Cmd: {}".format(errors.decode('utf-8'), " ".join(cmd)))
            return 1
        else:
            try:
                f = open(result_file, 'r')
            except Exception:
                logging.error("Thread terminated with no result and message:\n {}".format(cmd_output.decode('utf-8')))
                return 1
            result = json.load(f)
            f.close()
            result['filename'] = result_file
            result['cmd'] = " ".join(cmd)
            result['tag'] = tag
            result['related_obj_id'] = related_obj_id

            db_wrapper = MongoDatabase(tag)
            db = db_wrapper.db
            db.seeding.insert_one(result)

            logging.info("Thread finished with output: \n{}".format(cmd_output.decode('utf8')))
        return 0

    def add_dyn_imm(self, graph_dir, immunization_info, beta, seeds, alpha, lt):
        out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
        adv_file = immunization_info['solution_path']
        imm_id = immunization_info['_id']
        self.all_cmds.append((self.get_cmd_dyn_imm(graph_dir, adv_file, out_file, lt, beta, seeds, alpha), out_file, imm_id))

    def add_rob(self, graph_dir, immunization_info, iterations, mode, number_of_blocked_nodes, seeds, alpha, lt):
        out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
        adv_file = immunization_info['solution_path']
        imm_id = immunization_info['_id']
        self.all_cmds.append((self.get_cmd_rob(graph_dir, adv_file, out_file, mode, lt, iterations, number_of_blocked_nodes, seeds, alpha), out_file, imm_id))

    def add_imm(self, graph_dir, immunization_info, seeds, lt):
        out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
        imm_id = immunization_info['_id'] # for sticking to network
        self.all_cmds.append((self.get_cmd_imm(graph_dir, out_file, 0.2, seeds, lt), out_file, imm_id))

    def add_satgreedy(self, graph_dir, graph_id, epsilon, adv_set, gamma, number_of_seeds, number_of_blocked_nodes, solvers, dimbeta, alpha, lt):
        out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
        self.all_cmds.append((self.get_cmd_satgreedy(graph_dir, epsilon, out_file, adv_set, 1, 0, gamma, number_of_seeds, 
                                number_of_blocked_nodes, solvers, dimbeta, alpha, lt), out_file, graph_id))

    def add_simulation(self, graph_dir, seed_path, simulation_id):
        out_file = os.path.join(self.fm.get_data_path(), str(uuid.uuid4()) + ".json")
        self.all_cmds.append((self.get_cmd_simulation(graph_dir, seed_path, 10000, out_file), out_file, simulation_id))

    @staticmethod
    def read_log():
        s = ""
        with open(os.path.join(FileManager.get_root_path(),'parallelWrapper.log'), 'r', encoding='utf-8') as f:
            for line in f:
                s += line
        return s
