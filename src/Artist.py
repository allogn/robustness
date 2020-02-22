import matplotlib
matplotlib.use('Agg')
import re
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
import seaborn as sns
import pandas as pd
import sys
import logging
import os
import json
import numpy as np
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','src'))
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),'..','scripts'))
import helpers

# sns.set(rc={'figure.figsize':(4,3)}, style="whitegrid", font_scale=1.4)
# SMALL_SIZE = 18
# MEDIUM_SIZE = 20
# BIGGER_SIZE = 22
#
# plt.rc('font', size=SMALL_SIZE)          # controls default text sizes
# plt.rc('axes', titlesize=SMALL_SIZE)     # fontsize of the axes title
# plt.rc('axes', labelsize=MEDIUM_SIZE)    # fontsize of the x and y labels
# plt.rc('xtick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
# plt.rc('ytick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
# plt.rc('legend', fontsize=SMALL_SIZE)    # legend fontsize
# plt.rc('figure', titlesize=BIGGER_SIZE)  # fontsize of the figure title

class Artist():
    def __init__(self):
        self.figsize = (4, 3)
        sns.set(style="whitegrid", font_scale=1.4, rc={'axes.grid': True})
        self.ext = "png"
        self.dpi = 120
        self.separate_legend = True
        self.small_font = FontProperties()
        self.small_font.set_size('small')
        self.LINE_STYLES = ['solid', 'dashed', 'dashdot', 'dotted']
        self.markers = ['D', 's', 'v', '^', 'd', '.', '<', '>', '1', '2', '3', '4', '8', '+', 'p', '.', 'x', 'h']
        self.markers_and_lines = [('--','o'),('-','v'),('-.','^'),(':','D'),('-','s'),('--','h')]

    def create_figure(self):
        fig = plt.figure(figsize=self.figsize, dpi=self.dpi)
        if self.separate_legend:
            self.figlegend = plt.figure(figsize=(4, 1), dpi=self.dpi)
        ax = fig.add_subplot(1, 1, 1)
        ## below - for label on the right
        # box = ax.get_position()
        # ax.set_position([box.x0, box.y0, box.width * 0.8, box.height])
        return fig, ax

    def save_figure(self, fig, prefix, title, output_path):
        filename = "{}{}.{}".format(prefix, title, self.ext)
        filename = filename.replace(" ", "_")
        full_filename = os.path.join(output_path, filename)
        fig.savefig(full_filename, bbox_inches='tight')
        logging.info("New pic %s is created." % full_filename)
        plt.close(fig)

    def transform_to_title(self, s):
        s = s.replace("_", " ").title()
        d = self.pm.get_plotting_replacements()
        for k in d:
            s = re.sub(k, d[k], s)
        return s

    def plot_errorbar(self, output_path, prefix, title, df):
        fig, ax = self.create_figure()
        solver_col = df.columns[0]
        x_label = df.columns[1]
        y_label = df.columns[2]
        NUM_STYLES = len(self.LINE_STYLES)

        i = 0
        all_solvers, ids = self.get_ordered_unique_solvers_with_ids(df, solver_col)
        colors = self.get_color_palette(ids)
        all_lines = []
        all_names = []
        for solver in all_solvers:
            df_case = df[df[solver_col] == solver]
            lines = ax.errorbar(df_case[x_label], df_case[y_label], yerr=df_case.iloc[:, 3],
                                label=self.transform_to_title(solver),
                                capsize=4, elinewidth=1, capthick=1, marker=self.markers[i])
            all_lines.append(lines[0])
            all_names.append(self.transform_to_title(solver))
            # markers don't stick to algorithms because they need to prevent overlapping
            lines[0].set_color(colors[i])
            lines[0].set_linestyle(self.LINE_STYLES[i % NUM_STYLES])
            lines[1][0].set_color(colors[i])
            lines[1][1].set_color(colors[i])
            lines[2][0].set_color(colors[i])
            i += 1

        if self.separate_legend:
            self.figlegend.legend(all_lines, all_names, 'center', ncol=6)

        ax.set_xlabel(self.transform_to_title(x_label))
        ax.set_ylabel(self.transform_to_title(y_label))
        # plt.legend(loc='center left', bbox_to_anchor=(1, 0.5), prop=self.small_font) -- right box

        if self.separate_legend:
            self.save_figure(self.figlegend, "legend_" + prefix, title, output_path)
        else:
            plt.legend(loc=9, bbox_to_anchor=(0.5, -0.25), prop=self.small_font, ncol=2)
        self.save_figure(fig, prefix, title, output_path)

    def get_ordered_unique_solvers_with_ids(self, df, solver_col):
        all_solvers = []
        ids = []  # we use this in order to preserve ids of colors independently of the presence of some
        existing_solvers = df[solver_col].unique()
        i = 0
        for solver in self.pm.get_plotting_order():
            p = re.compile(solver)
            for s in existing_solvers:
                if p.search(s):
                    all_solvers.append(s)
                    ids.append(i)
                    break
            i += 1
        existing_solvers = [s for s in existing_solvers if s not in all_solvers]
        existing_solvers = sorted(existing_solvers)
        ids += list(range(i, i+len(existing_solvers)))
        return all_solvers + existing_solvers, ids

    def get_color_palette(self, ids):
        assert(max(ids) < 25)  # if this fails -- increase total number of colors
        #clrs = ["#e6194B", "#3cb44b", "#ffe119", "#4363d8", "#f58231", "#46f0f0", "#f032e6", "#fabebe", "#008080",
        #        "#e6beff", "#9A6324", "#fffac8", "#800000", "#aaffc3", "#000075", "#a9a9a9", "#ffffff", "#000000"]
        clrs = ["#9b59b6", "#e74c3c", "#3498db", "#95a5a6", "#34495e", "#2ecc71", "#fac205"]  # https://xkcd.com/color/rgb/

        clrs += ["#e6194B", "#3cb44b", "#ffe119", "#4363d8", "#f58231", "#46f0f0", "#f032e6", "#fabebe", "#008080",
                 "#e6beff", "#9A6324", "#fffac8", "#800000", "#aaffc3", "#000075", "#a9a9a9", "#ffffff", "#000000"]

        # sns.set_palette('hls', n_colors=10)
        # clrs = sns.color_palette(n_colors=10)
        # clrs = sns.hls_palette(10, l=.55, h=.01, s=.60)
        return [clrs[i] for i in ids]

    def plot_bar(self, output_path, prefix, title, df):
        fig, ax = self.create_figure()
        param = df.columns[1]
        solvers_col = df.columns[0]

        renamed_param = self.transform_to_title(param)
        renaming = {param: renamed_param}
        renamed_solvers_order = []
        solvers, ids = self.get_ordered_unique_solvers_with_ids(df, solvers_col)
        for solver in solvers:
            renaming[solver] = self.transform_to_title(solver)
            renamed_solvers_order.append(self.transform_to_title(solver))
        colors = self.get_color_palette(ids)

        df = df.copy()
        # df['solvercase'] = df['solvercase'].map(renaming)  # rename solvers
        df = df.rename(columns=renaming)  # rename parameter
        sns.boxplot(x=renamed_param, y=solvers_col, data=df, ax=ax, palette=colors, order=renamed_solvers_order)

        # horizontal barplot:
        # x_axis = ax.axes.get_xaxis()
        # x_label = x_axis.get_label()
        # x_label.set_visible(False)
        # plt.setp(ax.get_xticklabels(), rotation=90, fontsize=8)

        y_axis = ax.axes.get_yaxis()
        y_label = y_axis.get_label()
        y_label.set_visible(False)

        self.save_figure(fig, prefix, title, output_path)

    def plot_line(self, output_path, prefix, title, df):
        # plot without error bars
        fig, ax = self.create_figure()
        solver_col = df.columns[0]
        x_label = df.columns[1]
        y_label = df.columns[2]
        for solver in df[solver_col].unique():
            df_case = df[df[solver_col] == solver]
            if len(df_case) > 0:
                df_case.plot(x=x_label, y=y_label, label=solver, ax=ax, legend=True)
        ax.set_xlabel(x_label)
        ax.set_ylabel(y_label)
        ax.set_title(title)
        self.save_figure(fig, prefix, title, output_path)
