# Introduction

This repository provides with an experimental framework for testing robustness of diffusion under Independent Cascade model. The general outline of the framework:
- Generate a network
- Apply Node Immunization (NI) algorithm to find best nodes to immunize
- Apply Robust Influence Maximization or Influence Maximization (IM) to find best robust seeds
- Perform Monte Carlo simulations to evaluate expected spread

# Installation

File `requirements.txt` provides with a list of dependencies for the __Conda__ packet manager.

Install Miniconda from here:
https://docs.conda.io/projects/conda/en/latest/user-guide/install/index.html

Create environment and install dependencies by
```
$ conda create --name ic_robustness --file requirements.txt
```

Next, build the SatGreedy solution by
```bash
$ mkdir src/SatGreedy/bin
$ cd src/SatGreedy/bin
$ cmake ..
$ make
```

Scripts read and write data from ALLDATA_PATH directory, which should be set up as an environmental variable. If using Conda, follow this instructions:
https://docs.conda.io/projects/conda/en/latest/user-guide/tasks/manage-environments.html

Otherwise, run `export ALLDATA_PATH=/path/to/datasets`

## Database

Results of experiments are stored in __MongoDB__ database, that comes with the dependencies. Before running experiments, start MongoDB instance by
```
mongod --dbpath <path to db files>
```

Dumping and restoring results from the database:
```
$ mongodump --db robustExperimentsDB
$ mongorestore <path to folder with backup files>
```

Use `--port <port>` if running instance on a non-default port, and update the connection settings in `src/MongoDatabase.py`.

# Solvers
The repository has implementations of the following solvers:
- IMM (Reverse Reachable set based Influence Maximization)
- Saturate Greedy (Robust Influence Maximization solver)
- Robust Influence Maximization based on the DAGGER reachability index
Immunization:
- PageRank
- NetShield
- Katz
- Aquaintance
- Betweenness
- DAVA
For the details on the solvers, refer to our publication.

# Structure

The repository consists of several "modules". All classes, except of Robust Influence Maximization and Influence Maximization algorithms, are implemented in Python and located in the `src` directory.

`Experiment` class loads JSON file, parses it and runs all the solvers in parallel. Parallelisation is done by another wrapper class `ParallelRobustWrapper`, where number of threads can be adjusted.

Immunization algorithms inherit from `Solver.py`, and follow name convention `<name of solver>Solver.py`.

Influence Maximization and Robust Influence Maximization are implemented in C++, but wrapped by the `RobustWrapper` class, which is used by `ParallelRobustWrapper`. All different robust solutions are embedded in `Rob.cpp`, which compiles into `Rob` binary. The `Rob` binary reads the sequence of nodes immunized in advance, i.e. the immunization solvers define the sequence for immunization, writes into a file, and then this file is used by Rob.cpp in order to derive a robust seeding strategy. So, the overall stack of calls is:
```
Experiment > ParallelRobustWrapper > RobustWrapper > Rob
```

The `Rob` binary takes a parameter `mode`, which corresponds to different techniques of seeding, described in our paper (see the "Publication" section below).

All results except files with generated networks are stored in the database.

# Parsing experimental parameters

Parsing is performed with a help of `ParameterManager`, and it additionally loads the `config/dag_default.json` file. The default configuration is complete and correct. If any JSON in the `dags` directory missing a parameters, then this parameter is substituted by the default one.

JSON is parsed in a way that allows to define a range of parameters per one experiment. For example, if we want to run an experiment for a graph with sizes 100,200,300; and for the seed set of sizes 20, 30; then we write
```
{
  "graph_size": [100, [200, 300]],
  "seed_set_size": [20, [30]]
}
```
Note here, that the first value is the default one (size 100, set size 20), and the default value should not be in an array of possible values. At the end, the framework will run 4 different of parameter sets: (100, 20), (200, 20), (300, 20), (100, 30).

Parameters are grouped by "data", "solver", "strategy" and "problem". The procedure above is performed for parameters within the same group. For each set of parameters in one group, the parser generates a cartesian product with all parameter sets in another group. For instance, each (graph_size, seed_set_size) will be run for each possible parameter set of immunization strategies.

For 'solver' group, possible parameter values are defined as an array
```
{
  "solver":
  {
    "solver_name":
      {
        "solver_parameter1": [1,2],
        "solver_parameter2": ["r", "s"]
      }
  }
}
```
The resulting set is a cartesian product of solver parameters 1 and 2.

### Running Simple IM

Setting `mode=-1` for the Rob solver corresponds to the simple IM. Simple IM is implemented in `src/SatGreedy/imm.cpp`, and is compiled in a separate binary from Rob. The `Experiment.py` class considers a special case of `mode=-1`, and schedules Imm instead of Rob.

### Running Robust algorithms for a sequence of blocked nodes

Setting `number_of_blocked_nodes=-1` will run all algorithms for a sequence of blocked nodes up to n-1, where n is the size of a graph.

# Running Experiments

All experiments are described as JSON files in the `dags/emt` directory. The following script loads a JSON file and runs the corresponding experiment:

```
python3 scripts/load_and_run_experiment.py <name of experiment>
```
The name of experiment is the same as the name of the JSON file that describes it.

Some experiments are run by separate scripts in the `scripts` directory.

# Plotting Results

Results analysis and plotting is done using Python. The `notebooks` directory contains Jupyter notebooks with plotting scripts. Names of notebooks correspond to types of network. All notebooks use Experiment, Database, Analyzer and Artist classes as common scripts for fetching the results from database, summarizing and setting styles for plotting.

# Testing

Run testing by
```bash
$ ./run_tests.sh
```

# Publication

This repository is supplementary to the publication

A. Logins, P. Karras, "On the Robustness of Cascade Diffusion under Node Attacks", In *Proceedings of the Web Conference*, 2020.

Please, consider citing if using the code.

# FAQ

Please, feel free to contact me if you have any questions
allogn@gmail.com
