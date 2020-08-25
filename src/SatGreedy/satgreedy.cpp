#include <iostream>
#include "argument.h"
#include "SatGreedy.h"

// Last folder name in dataset is considered ID of a graph
// adversaries path is a path to blocked nodes from different graphs, so graph_id should be appended

int main(int argn, char **argv) {
  Argument arg;
  std::vector<std::string> solvers;
  for (int i = 0; i < argn; i++)
  {
      if (argv[i] == string("-help") || argv[i] == string("--help") || argn == 1)
      {
          cout << "./satgreedy -lt *** -sampling *** -dimbeta *** -dataset *** -l *** -epsilon *** -s *** -log *** -gamma *** -adversaries *** -orobj *** -strict *** -beta *** -mc_iterations *** (-with_<solvername>)" << endl;
          return 1;
      }

    
      if (argv[i] == string("-lt")) // if to use LT model
          arg.lt_model = argv[i + 1][0] == '1';

      if (argv[i] == string("-dataset"))
          arg.dataset = argv[i + 1];
      /*
       * Convergence of TIM
       */
      if (argv[i] == string("-epsilon"))
          arg.epsilon = atof(argv[i + 1]);
      /*
       * Seed set size
       */
      if (argv[i] == string("-s"))
          arg.s = atoi(argv[i + 1]);
      if (argv[i] == string("-log"))
          arg.log = argv[i + 1];

      /*
       * Params for SatGreedy
       */
      if (argv[i] == string("-gamma"))
          arg.gamma = atof(argv[i + 1]);
      if (argv[i] == string("-beta"))
          arg.beta = atof(argv[i + 1]);

      // param of Dynamic IM : accuracy
      if (argv[i] == string("-dimbeta"))
          arg.dim_beta = atoi(argv[i + 1]);

      /*
       * Loads all json files in folder <adversaries>/graph_id
       *
       * Each json has "results": "Blocked nodes", "edgelist", "weights"
       * See Adversary.h for details.
       */
      auto ss = string(argv[i]);
      if (ss.find("-adversary") != std::string::npos)
          arg.adversaries.push_back(argv[i + 1]);

      /*
       * Number of blocked nodes. Set to -1 for evaluation of all values of l.
       */
      if (argv[i] == string("-l"))
          arg.number_of_blocked_nodes = atoi(argv[i + 1]);

      /*
       * Original objective (perturbation model : fraction of seeds over optimal seeds).
       * Default 0.
       */
      if (argv[i] == string("-orobj"))
          arg.original_objective = atoi(argv[i + 1]) == 1;

      /*
       * Allows or restricts violation of seed set size
       * Default 1.
       */
      if (argv[i] == string("-strict"))
          arg.strict_s = atoi(argv[i + 1]) == 1;

      /*
       * Calculates robust values for subset of sampled nodes with geometrical progression with this coefficient
       */
      if (argv[i] == string("-sampling"))
          arg.blocked_nodes_sampling_coef = atof(argv[i + 1]);

      if (argv[i] == string("-mc_iterations"))
          arg.mc_iterations = atoi(argv[i + 1]);
      if (argv[i] == string("-with_singlegreedy")) solvers.push_back("single_greedy");
      if (argv[i] == string("-with_satgreedy")) solvers.push_back("satgreedy");
      if (argv[i] == string("-with_singlegreedycelf")) solvers.push_back("single_greedy_celf");
      if (argv[i] == string("-with_allgreedy")) solvers.push_back("all_greedy");

  }

  SatGreedy s(arg);
  s.run_all(solvers);
  return 0;
}
