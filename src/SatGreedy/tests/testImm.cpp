#include <iostream>
#include <boost/filesystem.hpp>
#include <fstream>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE HKSolver
#include <boost/test/unit_test.hpp>

#include "imm.h"

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_CASE(testSestimationLinear) {
  //create some simple graphs
  char* tmp_path_str = std::getenv("ALLDATA_PATH");
  BOOST_CHECK (tmp_path_str!=nullptr);
  fs::path tmp_dir = tmp_path_str;
  fs::path subdir = "tmp";
  tmp_dir /= subdir;
  fs::create_directories(tmp_dir);

  ofstream attf;
  attf.open((tmp_dir / "attributes.txt").c_str());
  attf << "n=6\nm=5\n";
  attf.close();

  ofstream f;
  f.open((tmp_dir / "graph.csv").c_str());
  f << "0 1 0.1\n";
  f << "1 2 0.9\n";
  f << "2 3 0.5\n";
  f << "3 4 0.4\n";
  f << "4 5 0.3\n";
  f.close();

  Argument arg;
  arg.dataset = tmp_dir.native();
  arg.model = "IC";
  arg.epsilon = 0.01;
  arg.log = (tmp_dir / "log.json").c_str();
  arg.s = 1;
  arg.reward_type = 0;
  Imm imm(arg);
  imm.collect_sets();
  imm.get_optimal();
  imm.save_log();

  ifstream logf(arg.log);
  json j;
  logf >> j;
  std::vector<nid_t> seeds = j.at("selected_seeds");
  BOOST_TEST(seeds == std::vector<nid_t>({1}));
  logf.close();

  std::vector<nid_t> s1({1});
  double infl1 = imm.estimate_influence(s1);
  BOOST_CHECK_CLOSE( infl1, 2.592, 1);

  std::vector<nid_t> s2({2,3});
  double infl2 = imm.estimate_influence(s2);
  BOOST_CHECK_CLOSE( infl2, 2.52, 1);

  std::vector<nid_t> s3({1,2,3,4});
  double infl3 = imm.estimate_influence(s3);
  BOOST_CHECK_CLOSE( infl3, 4.3, 1);

}


BOOST_AUTO_TEST_CASE(testBlockedSetAndIncrementalOptimum) {
  char* tmp_path_str = std::getenv("ALLDATA_PATH");
  BOOST_CHECK (tmp_path_str!=nullptr);
  fs::path tmp_dir = tmp_path_str;
  fs::path subdir = "tmp";
  tmp_dir /= subdir;
  fs::create_directories(tmp_dir);

  ofstream attf;
  attf.open((tmp_dir / "attributes.txt").c_str());
  attf << "n=6\nm=6\n";
  attf.close();

  ofstream f;
  f.open((tmp_dir / "graph.csv").c_str());
  f << "0 1 0.1\n";
  f << "1 2 0.2\n";
  f << "2 3 0.3\n";
  f << "3 4 0.4\n";
  f << "4 0 0.5\n";
  f << "2 5 0.6\n";
  f.close();

  Argument arg;
  arg.dataset = tmp_dir.native();
  arg.model = "IC";
  arg.epsilon = 0.01;
  arg.log = (tmp_dir / "log.json").c_str();
  arg.s = 2;
  arg.reward_type = 0;
  Imm imm(arg, std::vector<nid_t>({1,3}));

  BOOST_CHECK_EQUAL(imm.g.n, 4);
  BOOST_CHECK_EQUAL(imm.g.m, 2);
}

