from airflow import DAG
from airflow.operators.python_operator import PythonOperator
from airflow.operators.bash_operator import BashOperator
from airflow.utils.trigger_rule import TriggerRule
import sys
import os
import copy
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
from ParameterManager import *

from run_imm_solver import *
from generate_graphs import *
from init_dag import *
from draw_performance import *
from analyze_graphs import *
from simulate import *

class DagFactory():
    '''
    This class is deprecetad. Previously used for Airflow to create DAGs for experiments with Robust Influence Maximization.

    Now this function is moved to the Experiment class. 
    '''

    def __init__(self, params):
        self.pm = ParameterManager(params)

    def init_dag(self):
        return DAG(self.pm.get("title"), schedule_interval=None, default_args=self.pm.get_dag_params())

    def create_dag(self):

        full_rerun = int(self.pm.get("full_rerun")) == 1
        simulations_rerun = int(self.pm.get("simulations_rerun")) == 1

        dag = self.init_dag()
        if simulations_rerun:
            drawing_task = self.create_drawing_task(dag)
        if not full_rerun and len(self.pm.get("repeat_experiment_for")) == 0:
            return dag  # in order to prevent simulation task creation

        init_task = self.create_init_task(dag)
        for dataset_params in self.pm.get_data_param_sets():
            if full_rerun:
                generation_task = self.create_graphs_task(dag, dataset_params)
                analysis_task = self.create_analysis_task(dag, dataset_params)
                init_task >> generation_task
                generation_task >> analysis_task
                # if simulations_rerun:
                #     drawing_task.set_upstream(analysis_task)

            for problem_params in self.pm.generate_problem_param_sets(dataset_params):
                # if simulations_rerun:
                #     simulation_task = self.create_simulation_task(dag, problem_params)
                #     simulation_task >> drawing_task

                imm_seed_task = self.create_imm_seed_task(dag=dag, **self.pm.get_imm_params(problem_params))
                if full_rerun:
                    generation_task >> imm_seed_task

                # solve seed-blind IM for each graph by each IM adversary
                # put that inside SatGreedy algorithm and solve IRIM
                immunization_task_list = list(self.create_blind_immunization_tasks(dag, problem_params))
                imm_seed_task >> immunization_task_list

                for solver_params in self.pm.get_satgreedy_sets():
                    # solve SatGreedy per each graph in a folder that corresponds to the parameter value
                    solve_irim_hk = self.create_satgreedy_task(dag, problem_params, solver_params)
                    immunization_task_list >> solve_irim_hk

                    combined_params = copy.deepcopy(problem_params)
                    combined_params['satgreedy_params'] = solver_params
                    footprint = self.pm.get_param_footprint(combined_params)
                    upd_res = PythonOperator(task_id='upd_' + footprint,
                                              python_callable=self.update_satgreedy_result_json,
                                              op_kwargs={"problem_params": problem_params},
                                              dag=dag)


                    solve_irim_hk >> upd_res;
                    upd_res >> drawing_task
                    # if simulations_rerun:
                    #     solve_irim_hk >> simulation_task

        return dag

    def create_satgreedy_task(self, dag, problem_params, solver_params):
        combined_params = copy.deepcopy(problem_params)
        combined_params['satgreedy_params'] = solver_params
        footprint = self.pm.get_param_footprint(combined_params)
        epsilon = solver_params['epsilon']
        gamma = solver_params['gamma']
        beta = problem_params['beta']
        if 'n' not in problem_params['data']:
            raise Exception("todo: write this so that the graph file is opened and n get from there")
        number_of_seeds = combined_params['k']
        satgreedy_bin = os.path.join(os.path.dirname(os.path.realpath(__file__)),'SatGreedy','build','SatGreedy')

        all_graph_dir = self.pm.fm.get_graphs_path(self.pm.get_param_footprint(combined_params['data']))
        all_tasks = []
        i = 0

        seed_path = self.pm.fm.get_seed_path(self.pm.get_param_footprint(problem_params))
        log_dir = seed_path

        fp1 = self.pm.get_param_footprint(problem_params)
        adversary_dir = self.pm.fm.get_imm_solutions_path(fp1)

        orobj = solver_params['original_objective']
        strict_s = solver_params['strict']
        mc_iterations = solver_params['mc_iterations']

        # graph id needed becase in dagFactory one bash script execute satgreedy for all graphs for same param footprint in one directory. dagParam does not have access to graph_id to pass as parameter. so, adding here right from file name of a graph

        subcommand = "{} -epsilon {} -s {} -log {} -gamma {} -beta {} -adversaries {} -strict {} -orobj {} -mc_iterations {} -dataset %%"
        subcommand = subcommand.format(satgreedy_bin, epsilon, number_of_seeds, log_dir, gamma, beta, adversary_dir, strict_s, orobj, mc_iterations)
        command = 'echo "\n -------- \n"; find {} -maxdepth 1 -mindepth 1 -type d \( ! -name . \) -print0 | xargs -I%% -0 {}'
        command = command.format(all_graph_dir, subcommand)
        # logging.info("Command {}".format(command))

        t = BashOperator(
            task_id='satgreedy_{}_{}'.format(footprint, i),
            bash_command=command,
            execution_timeout=timedelta(seconds=self.pm.params['solver_execution_timeout']),
            dag=dag
        )
        return t


    def update_satgreedy_result_json(self, problem_params):
        seed_path = self.pm.fm.get_seed_path(self.pm.get_param_footprint(problem_params))
        for sf in self.pm.fm.list_files_with_ext(seed_path, "json"):
            with open(sf,"r") as f:
                res = json.load(f)
                res['problem'] = problem_params
                res['data'] = problem_params['data']

            with open(sf,'w') as f:
                json.dump(res, f)

    def create_init_task(self, dag):
        return PythonOperator(task_id='init_dag',
                              python_callable=init_dag_func,
                              op_kwargs=self.pm.get_init_task_params(),
                              dag=dag)

    def create_graphs_task(self, dag, dataset_params):
        t1 = PythonOperator(
            task_id="gen_" + self.pm.get_param_footprint(dataset_params),
            python_callable=generate_graphs,
            op_kwargs=self.pm.get_generator_params(dataset_params),
            dag=dag)
        return t1

    def create_analysis_task(self, dag, dataset_params):
        t3 = PythonOperator(
            task_id="analysis_" + self.pm.get_param_footprint(dataset_params),
            python_callable=analyze_graphs,
            op_kwargs=self.pm.get_graph_analysis_params(dataset_params),
            dag=dag)
        return t3

    def create_blind_immunization_tasks(self, dag, problem_params):
        for solver_case in problem_params['imm_solver']:
            task = PythonOperator(
                        task_id=solver_case + "_imm_" + self.pm.get_param_footprint(problem_params),
                        python_callable=run_imm_solver,
                        op_kwargs=self.pm.get_run_imm_solver_params(problem_params, solver_case),
                        dag=dag)
            yield task

    def create_drawing_task(self, dag):
        return PythonOperator(
                    task_id='draw',
                    python_callable=draw_performance,
                    op_kwargs=self.pm.get_plotting_params(),
                    trigger_rule=TriggerRule.ALL_DONE,
                    dag=dag)

    def create_simulation_task(self, dag, problem_params):
        simulation_task = PythonOperator(
                            task_id='sim_' + self.pm.get_param_footprint(problem_params),
                            python_callable=simulate,
                            op_kwargs=self.pm.get_simulation_params(problem_params),
                            trigger_rule=TriggerRule.ALL_DONE,
                            dag=dag)
        return simulation_task


    def create_imm_seed_task(self, dag, seed_path, problem_params, solver_params):
        combined_params = copy.deepcopy(problem_params)
        combined_params['imm_params'] = solver_params
        footprint = self.pm.get_param_footprint(combined_params)
        epsilon = solver_params['epsilon']
        if 'n' not in problem_params['data']:
            raise Exception("todo: write this so that the graph file is opened and n get from there")
        number_of_seeds = combined_params['k']*solver_params['beta']
        imm_bin = os.path.join(os.path.dirname(os.path.realpath(__file__)),'SatGreedy','build','Imm')

        fp1 = self.pm.get_param_footprint(problem_params)
        all_graph_dir = self.pm.fm.get_graphs_path(self.pm.get_param_footprint(combined_params['data']))
        seed_path = self.pm.fm.get_seed_path(fp1)
        log_dir = seed_path

        subcommand = "{} -epsilon {} -s {} -log {} -rw 1 -dataset %%"
        subcommand = subcommand.format(imm_bin, epsilon, number_of_seeds, log_dir)
        command = 'echo "\n -------- \n"; find {} -maxdepth 1 -mindepth 1 -type d \( ! -name . \) -print0 | xargs -I%% -0 {}'
        command = command.format(all_graph_dir, subcommand)

        t = BashOperator(
            task_id='imm_seed_{}'.format(footprint),
            bash_command=command,
            dag=dag,
        )
        return t
