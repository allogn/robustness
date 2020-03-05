#include <iostream>
#include <vector>
#include "argument.h"
#include "SatGreedy.h"
#include "json.h"

using json = nlohmann::json;

int main(int argn, char **argv) {
  Argument arg;
  std::string seedsf, outf;
  for (int i = 0; i < argn; i++)
  {
      if (argv[i] == string("-help") || argv[i] == string("--help") || argn == 1)
      {
          cout << "./imm -d *** -s *** -i *** -o ***" << endl;
          return 1;
      }
      if (argv[i] == string("-d")) // dataset
          arg.dataset = argv[i + 1];
      if (argv[i] == string("-s")) // seeds
          seedsf = argv[i + 1];
      if (argv[i] == string("-o")) // output file
          outf = argv[i + 1];
      if (argv[i] == string("-i")) // number of iterations
          arg.mc_iterations = atoi(argv[i + 1]);
  }
  SatGreedy solver(arg);
  std::vector<nid_t> seeds;

  ifstream f;
  f.open(seedsf);
  if (!f.is_open()) {
      throw runtime_error("Seed file does not exist: " + seedsf);
  }
  long s;
  while (f >> s) {
      seeds.push_back(s);
  }
  f.close();
  INFO("Collected ",seeds.size()," seeds");

  auto mean_std = solver.run_simulations(solver.g, seeds);
  std::cout << mean_std.first << ", " << mean_std.second << std::endl;
  INFO("Done.");

  json logging;
  logging["objective"] = mean_std.first;
  logging["objective_std"] = mean_std.second;
  logging["number_of_seeds"] = seeds.size();
  std::ofstream logf;
  logf.open(outf);
  logf << logging.dump(4);
  logf.close();
  return 0;
}
