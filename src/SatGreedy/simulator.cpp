#include <iostream>
#include <vector>
#include "argument.h"
#include "SatGreedy.h"

int main(int argn, char **argv) {
  Argument arg;
  std::string seedsf;
  for (int i = 0; i < argn; i++)
  {
      if (argv[i] == string("-help") || argv[i] == string("--help") || argn == 1)
      {
          cout << "./imm -dataset *** -seeds *** -iter ***" << endl;
          return 1;
      }
      if (argv[i] == string("-dataset"))
          arg.dataset = argv[i + 1];
      if (argv[i] == string("-seeds"))
          seedsf = argv[i + 1];
      if (argv[i] == string("-iter"))
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
  return 0;
}
