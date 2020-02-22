import unittest
#from src.DagFactory import *
import logging

from airflow.models import DagBag
from airflow.models import TaskInstance

# AirFlow support deprecated

class TestDagIntegrity(unittest.TestCase):

    LOAD_SECOND_THRESHOLD = 2

    @unittest.skip
    def setUp(self):
        self.dagbag = DagBag()

    @unittest.skip
    def test_import_dags(self):
        self.assertFalse(
            len(self.dagbag.import_errors),
            'DAG import failures. Errors: {}'.format(
                self.dagbag.import_errors
            )
        )

    @unittest.skip
    def test_alert_email_present(self):

        for dag_id, dag in self.dagbag.dags.iteritems():
            emails = dag.default_args.get('email', [])
            msg = 'Alert email not set for DAG {id}'.format(id=dag_id)
            self.assertIn('alert.email@gmail.com', emails, msg)

    @unittest.skip
    def test_task_count(self):
        """Check task count of hello_world dag"""
        dag_id='test'
        dag = self.dagbag.get_dag(dag_id)
        self.assertEqual(len(dag.tasks), 3)


    @unittest.skip
    def test_results(self):

        defaults = helpers.load_config("dag_defaults")
        defaults['title'] = 'test'

        number_of_graphs = defaults['number_of_graphs']
        number_of_data_cases = 2
        number_of_problem_cases = 2
        number_of_solvers = 1 # 4 -- not supported currently, because each solver rerun all other algorithms
        number_of_seed_sets = 8 # satgreedy*4 + imm + SatGreedy produce all_greedy and simple_greedy and simple_greedy_celf
        number_of_imm_solvers = 6
        number_of_plots = 14 # len(defaults["figures"]['errorbar']) -- this does not work because all figures added to defaults now

        df = DagFactory(defaults)
        dag = df.create_dag()

        tasks = dag.tasks
        tasks_dict = {}
        for task in tasks:
            tasks_dict[task.task_id] = task
        task_ids = list(map(lambda task: task.task_id, tasks))

        init_task = tasks_dict['init_dag']
        ti = TaskInstance(task=init_task, execution_date=datetime.now())
        result = init_task.execute(ti.get_template_context())
        self.assertEqual(len(init_task.upstream_list), 0)
        self.assertEqual(len(init_task.downstream_list), number_of_data_cases)

        i = 0
        for k in tasks_dict:
            if k.find("gen_") > -1:
                i += 1
                task = tasks_dict[k]
                self.assertEqual(len(task.upstream_list), 1)
                downstream_num = number_of_problem_cases + 1
                self.assertEqual(len(task.downstream_list), downstream_num)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
        self.assertEqual(i, number_of_data_cases)
        res_files = len(FileManager.list_files_with_ext(df.pm.fm.get_graphs_path(), 'pkl', recursive=True))
        self.assertEqual(res_files, number_of_graphs * number_of_data_cases)

        i = 0
        handler = logging.StreamHandler(sys.stdout)
        for k in tasks_dict:
            if k.find("imm_seed_") > -1:
                i += 1
                task = tasks_dict[k]
                task.log.addHandler(handler)
                self.assertEqual(len(task.upstream_list), 1)
                downstream_num = number_of_imm_solvers
                self.assertEqual(len(task.downstream_list), downstream_num)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
        self.assertEqual(i, number_of_data_cases * number_of_problem_cases)
        res_files_imm_seed = len(FileManager.list_files_with_ext(df.pm.fm.get_seed_path(), 'json', recursive=True))
        self.assertEqual(res_files_imm_seed, number_of_graphs * number_of_data_cases * number_of_problem_cases)

        i = 0
        for k in tasks_dict:
            if k.find("_imm_") > -1:
                i += 1
                task = tasks_dict[k]
                self.assertEqual(len(task.upstream_list), 1)
                self.assertEqual(len(task.downstream_list), number_of_solvers)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
        random_solutions = defaults['problem']['imm_solver']['Random']['n'] + defaults['problem']['imm_solver']['RandPert']['n']
        number_of_executions = number_of_problem_cases * number_of_data_cases * number_of_imm_solvers
        number_of_files = number_of_problem_cases * number_of_data_cases * (number_of_imm_solvers - 2 + random_solutions) * number_of_graphs
        self.assertEqual(i, number_of_executions)
        res_files = len(FileManager.list_files_with_ext(df.pm.fm.get_imm_solutions_path(), 'json', recursive=True))
        self.assertEqual(res_files, number_of_files) # each solver produce a solution per graph per problem case
        # except of RandomSolver that provides N solutions

        i = 0
        for k in tasks_dict:
            if k.find("analysis_") > -1:
                i += 1
                task = tasks_dict[k]
                self.assertEqual(len(task.upstream_list), 1)
                self.assertEqual(len(task.downstream_list), 0)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
        self.assertEqual(i, number_of_data_cases)

        i = 0
        for k in tasks_dict:
            if k.find("satgreedy_") > -1:
                i += 1
                task = tasks_dict[k]
                task.log.addHandler(handler)
                self.assertEqual(len(task.upstream_list), number_of_imm_solvers)
                self.assertEqual(len(task.downstream_list), 1)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
                if task.sp.returncode:
                    handler = logging.StreamHandler(sys.stdout)
                    task.log.addHandler(handler)
                    result = task.execute(ti.get_template_context())

                self.assertFalse(task.sp.returncode)
                logging.info("SatGreedy completed.")
        number_of_executions = number_of_problem_cases * number_of_data_cases * number_of_solvers
        self.assertEqual(i, number_of_executions)
        number_of_seed_files = number_of_executions * number_of_graphs + res_files_imm_seed
        res_files = len(FileManager.list_files_with_ext(df.pm.fm.get_seed_path(), 'json', recursive=True))
        self.assertEqual(res_files, number_of_seed_files) # one script execution per problem/data case

        # i = 0
        # for k in tasks_dict:
        #     if k.find("sim_") > -1:
        #         i += 1
        #         task = tasks_dict[k]
        #         self.assertEqual(len(task.upstream_list), number_of_solvers)
        #         self.assertEqual(len(task.downstream_list), 1)
        #         ti = TaskInstance(task=task, execution_date=datetime.now())
        #         result = task.execute(ti.get_template_context())
        #         logging.info("Simulation completed.")
        # self.assertEqual(i, number_of_data_cases * number_of_problem_cases) # one script per problem/data case
        # res_files = len(FileManager.list_files_with_ext(df.pm.fm.get_simulations_path(), 'json', recursive=True))
        #
        # sim_files = number_of_data_cases * number_of_problem_cases * number_of_seed_sets * number_of_graphs
        # self.assertEqual(res_files, sim_files)


        for k in tasks_dict:
            if k.find("upd_") > -1:
                task = tasks_dict[k]
                self.assertEqual(len(task.upstream_list), 1) # should be the same as number of simulations, as now every solver is inside satgreedy
                self.assertEqual(len(task.downstream_list), 1)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
                logging.info("Drawing completed.")
        res_files = FileManager.list_files_with_ext(df.pm.fm.get_seed_path(), 'json', recursive=True)
        check_entry = json.load(open(res_files[0],'r'))
        self.assertTrue("data" in check_entry)
        self.assertEqual(len(res_files), number_of_seed_files) # one script execution per problem/data case

        for k in tasks_dict:
            if k.find("draw") > -1:
                task = tasks_dict[k]
                self.assertEqual(len(task.upstream_list), number_of_solvers*number_of_data_cases*number_of_problem_cases) # should be the same as number of simulations, as now every solver is inside satgreedy
                self.assertEqual(len(task.downstream_list), 0)
                ti = TaskInstance(task=task, execution_date=datetime.now())
                result = task.execute(ti.get_template_context())
                logging.info("Drawing completed.")
        res_files = len(FileManager.list_files_with_ext(df.pm.fm.get_pic_path(), 'png', recursive=True))
        self.assertEqual(res_files, number_of_plots)
