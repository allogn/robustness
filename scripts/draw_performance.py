import sys
import logging
import os

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
import helpers
from Analyst import *
from Artist import *
from FileManager import *

def draw_performance(parameter_manager):

    analyst = Analyst(parameter_manager)
    results = []
    # files = FileManager.list_files_with_ext(parameter_manager.fm.get_simulations_path(),'json',recursive=True)
    files = FileManager.list_files_with_ext(parameter_manager.fm.get_seed_path(),'json',recursive=True)
    for result_file in files:
        f = open(result_file, 'r')
        result = json.load(f)
        f.close()
        if 'satgreedy' not in result:
            continue # IMM result was loaded, skipping
        solvers = ['satgreedy', 'all_greedy', 'single_greedy', 'single_greedy_celf', 'imm']
        for solver in solvers:
            one_result = {}
            one_result['results'] = result[solver];
            one_result['seed_solver'] = solver
            for key in result:
                if key not in solvers:
                    one_result[key] = result[key]
            results.append(one_result)
    assert(len(results) > 0)
    analyst.set_results(results)
    artist = Artist(parameter_manager)
    for df in analyst.generate_df_for_errorbar_plotting():
        artist.plot_errorbar(parameter_manager.fm.get_pic_path(), "", "{}_{}".format(df.columns[1], df.columns[2]), df)
    for df in analyst.generate_df_for_bar_plotting():
        artist.plot_bar(parameter_manager.fm.get_pic_path(), "", "{}".format(df.columns[1]), df)
