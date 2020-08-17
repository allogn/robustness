import pandas as pd
import numpy as np
import sys
import logging
import os
import json
import warnings

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
import helpers
from FileManager import *
from Artist import *
from ParameterManager import *
from MongoDatabase import *

class Analyzer:
    def __init__(self, tag):
        self.tag = tag
        self.df = pd.DataFrame()
        self.db_wrapper = MongoDatabase(tag)
        self.db = self.db_wrapper.db
        self.load_dataframe()

    def load_dataframe(self):
        q = {"tag": self.tag}
        satgreedy_exists = False
        for solver_result in self.db.seeding.find(q):
            row = {}
            row.update(solver_result)
            if row['cmd'].find("/Rob ") > 0 or row['cmd'].find("/Imm ") > 0:
                immunizer = self.db.immunization.find_one(row['related_obj_id'])
                assert(immunizer != None)
                row.update(immunizer)
                graph_id = row['graph']
            if row['cmd'].find("/Simulator ") > 0:
                imm_simulations = self.db.imm_simulations.find_one(row['related_obj_id'])
                assert(imm_simulations != None)
                row.update(imm_simulations)
                graph_id = row['graph']
            if row['cmd'].find("/SatGreedy ") > 0:
                satgreedy_exists = True
                graph_id = row['related_obj_id']
            graph = self.db.graphs.find_one(graph_id)
            assert(graph != None)
            row.update(graph)
            self.df = self.df.append(row,ignore_index=True)
        assert(len(self.df) > 0)

        if satgreedy_exists: # there is at least one entry with satgreedy
            self.unfold_satgreedy_solvers()

    def unfold_satgreedy_solvers(self):
        new_df = pd.DataFrame()
        possible_solvers = ['satgreedy', 'all_greedy', 'single_greedy', 'imm', 'single_greedy_celf']
        for row_id in self.df.index:
            if self.df.loc[row_id,'cmd'].find("/SatGreedy ") == -1:
                new_df = new_df.append(self.df.loc[row_id])
            else:
                for s in possible_solvers:
                    if s in self.df.columns:
                        new_series = pd.Series(self.df.loc[row_id])
                        new_series = new_series.append(pd.Series(self.df.loc[row_id,s]))
                        if 'objective' in pd.Series(self.df.loc[row_id,s]):
                            new_series = new_series.drop('objective')
                            new_series['objective'] = pd.Series(self.df.loc[row_id,s])['objective']
                        new_series['solver'] = s
                        new_df = new_df.append(new_series, ignore_index=True)
        new_df.drop(columns=possible_solvers,errors='ignore',inplace=True)

        # move data to sequence column
        if 'sequence' not in new_df.columns:
            new_df = new_df.rename(columns={'objectives': "sequence"})
        else:
            for i in new_df[new_df['objectives'].notna()].index:
                new_df.at[i,'sequence'] = list(np.array(new_df.loc[i,'objectives'])*new_df.loc[i,'n'])
            new_df.drop(columns=['objectives'],inplace=True)

        self.df = new_df


    ########### deprecated all below ---------

    # def __init__(self, parameter_manager):
    #     self.pm = parameter_manager
    #     self.solver_column_name = "seed_solver"

    def set_results(self, results):
        self.results = pd.DataFrame([self.pm.flatten_dict_rec(d) for d in results])
        logging.info("Result columns:\n {}".format(self.results.columns))
        assert(self.solver_column_name in self.results.columns)
        self.defaults = self.pm.get_flatten_defaults()
        assert(len(self.results) > 0)

    def generate_df_for_errorbar_plotting(self):
        for fig in self.pm.get('figures')['errorbar']:
            x_label = fig[0]
            assert(x_label != None)
            y_label = fig[1]

            param_columns = self.pm.get_param_cols()
            if (x_label not in param_columns):
                logging.info("x_label {} is not among parameters".format(x_label))
                continue

            df = self.fix_constant_params(self.results, param_columns, x_label)
            assert(self.solver_column_name in df.columns)
            assert(x_label in df.columns)
            try:
                df = self.group_by_hyperparameter(df, x_label, y_label)
            except KeyError:
                logging.info("y_label {} is not among results".format(y_label))
                continue
            assert(self.solver_column_name in df.columns)
            if y_label.find("_mean") > -1:
                y_label = y_label[:-5]
            if len(df) > 0:
                r = df[[self.solver_column_name, x_label, y_label + " mean", y_label + " std"]]
                r = r[r[y_label + " mean"] != -1]
                yield r

    def generate_df_for_bar_plotting(self):
        for y_label in self.pm.get('figures')['bar']:
            param_columns = self.pm.get_param_cols()
            df = self.fix_constant_params(self.results, param_columns)
            if len(df) > 0:
                df = self.add_solvercase_column(df)
                try:
                    y_col = self.get_result_column_name_before_grouping(y_label)
                except KeyError:
                    logging.info("y_label {} is not among results".format(y_label))
                    continue
                df = df.rename(columns={y_col: y_label})
                logging.info("Collected {} rows for {} algorithms for a bar plot {}".format(len(df), len(df['solvercase'].unique()), y_label))
                assert(len(df) > 0)
                yield df[['solvercase', y_label]]

    def fix_constant_params(self, df, param_columns, x_label=None):
        for param in param_columns:
            if param == x_label:
                continue
            if param not in df or param not in self.defaults:
                raise Exception("Parameter {} is not among results or defaults".format(param))
            df = df[df[param] == self.defaults[param]]
        return df

    @staticmethod
    def get_readable_solver_name(param_list, entry):
        name = entry['solver']
        for col in param_list:
            if col not in entry:
                continue  # if an algorithm happens not to be there at all
            value = entry[col]
            if value != -1:
                if isinstance(value, float):
                    if np.isnan(value):
                        continue
                    value = str(value)
                    value = value.replace(".", "")
                col_parts = col.split("_")
                param_name = "".join([s[0] for s in col_parts[1:]])
                name += "_{}{}".format(value, param_name)
        return name

    def group_by_hyperparameter(self, df, x_label, y_label):
        '''
        group by hyperparameters and solver
        '''
        assert(len(self.pm.get_hyperparam_cols()) > 0) # at least solver name
        constant_fields = [col for col in self.pm.get_hyperparam_cols() if col in df.columns]
        assert(self.solver_column_name in constant_fields)
        constant_fields.append(x_label)
        if y_label.find("_mean") > 0:
            y_column_without_mean = y_label[:-5]
            var_column = y_column_without_mean + "_var"
            std_column = y_column_without_mean + " std" # space obligatory, because in second case it is automatic
            grouped = df.fillna(-1).groupby(constant_fields, as_index=False).agg({y_label: 'mean', var_column: 'mean'})
            grouped = grouped.rename(columns={var_column: std_column, y_label: y_column_without_mean + " mean"})
            grouped[std_column] = np.sqrt(grouped[std_column])
        else:
            grouped = df.fillna(-1).groupby(constant_fields, as_index=False).agg({y_label: ['mean', 'std']})
            grouped = grouped.fillna(0)  # fill std in case of only one row
            grouped.columns = [' '.join(col).strip() for col in grouped.columns.values]
        return grouped
