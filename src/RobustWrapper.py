import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import logging
import helpers
from subprocess import Popen, PIPE
import json
import networkx as nx
import uuid
from FileManager import *

class RobustWrapper:
    '''
    This class is a wrapper over C++ implementation
    It takes Networkx graphs and runs algorithms by writing graphs into files

    All parameters should go into resulting json files
    '''
    BIN_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'SatGreedy', 'build')

    @staticmethod
    def run_cmd(cmd, verbose, timeout):
        process = Popen(cmd, stdout=PIPE, stderr=PIPE)

        if verbose:
            print("Running:$", " ".join(cmd))
            for line in process.stdout:
                print(line.decode('utf8'), end="")

            for line in process.stderr:
                print("Error: ", line.decode('utf8'), end="")

            rc = process.poll()
            if rc != None and rc != 0:
                print("Terminated with error ", rc)
                return -1
            return 1
        else:
            output, errors = process.communicate(timeout = timeout)
            return output, errors

    @staticmethod
    def get_cmd_satgreedy(graph_dir, epsilon, out_file, adv_set, beta, mc_iterations, gamma, number_of_seeds, number_of_blocked_nodes, solvers, dimbeta, alpha):
        cmd = [ os.path.join(RobustWrapper.BIN_PATH,'SatGreedy'),
                '-dataset', graph_dir,
                '-epsilon', str(epsilon),
                '-s', str(number_of_seeds),
                '-log', out_file,
                '-beta', str(beta),
                '-mc_iterations', str(mc_iterations),
                '-strict', str(0),
                '-orobj', '0',
                '-dimbeta', str(dimbeta),
                '-sampling', str(alpha),
                '-l', str(number_of_blocked_nodes),
                '-gamma', str(gamma)]
        for s in solvers:
            cmd += ['-with_{}'.format(s)]

        i = 0
        for a in adv_set:
            cmd += ['-adversary{}'.format(i), a]
            i += 1
        # print(" ".join(cmd))
        return cmd

    @staticmethod
    def get_cmd_rob(graph_dir, adv_file, out_file, mode, iterations = 1, number_of_blocked_nodes = None, seeds = 1, alpha=1):
        cmd = [ os.path.join(RobustWrapper.BIN_PATH,'Rob'),
                '-d', graph_dir,
                '-i', str(int(iterations)),
                '-m', str(int(mode)),
                '-s', str(seeds),
                '-c', str(alpha),
                '-a', adv_file,
                '-o', out_file]
        if number_of_blocked_nodes != None:
            cmd += ['-l', str(number_of_blocked_nodes)]
        return cmd

    @staticmethod
    def get_cmd_dyn_imm(graph_dir, adversary_file, out_file, beta = 32, seeds=1, alpha=1):
        assert(beta > 1);
        return [ os.path.join(RobustWrapper.BIN_PATH,'Imm'),
                '-d', graph_dir,
                '-e', str(beta), # epsilon for imm, but beta for dyn imm
                '-m', '0',
                '-c', str(alpha),
                '-s', str(seeds),
                '-a', adversary_file,
                '-o', out_file]


    @staticmethod
    def get_cmd_imm(graph_dir, out_file, beta, seeds):
        assert(beta < 1);
        return [ os.path.join(RobustWrapper.BIN_PATH,'Imm'),
                '-d', graph_dir,
                '-s', str(seeds),
                '-e', str(beta), # epsilon for imm, but beta for dyn imm
                '-o', out_file]
