#include "infgraph.h"

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE InfGraphSolver
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "json.h"

namespace fs = boost::filesystem;
namespace utf = boost::unit_test;


BOOST_AUTO_TEST_CASE(testIncrementalEstimation, *utf::tolerance(0.1)) {
  char* data_path_str = std::getenv("ALLDATA_PATH");
  fs::path data_dir = data_path_str;
  fs::path tmp_subdir = "tmp";

  fs::path graph_dir = data_dir;
  graph_dir /= tmp_subdir;
  fs::remove_all(graph_dir);
  fs::create_directories(graph_dir);

  ofstream attf;
  attf.open((graph_dir / "attributes.txt").c_str());
  attf << "n=5\nm=7\n";
  attf.close();

  ofstream f;
  f.open((graph_dir / "graph.csv").c_str());
  f << "0 1 0.1\n1 2 0.2\n0 2 0.3\n2 3 0.4\n3 4 0.5\n4 3 0.6\n4 0 0.7";
  f.close();

  InfGraph g(graph_dir.native(), "graph.csv", false);
  g.init_hyper_graph();
  g.build_hyper_graph_r(1000);
  double cached = g.estimate_influence_inc(0, false);
  BOOST_TEST(g.estimate_influence_inc(0, false) == g.estimate_influence(std::vector<nid_t>({0})));
  BOOST_TEST(cached == 1 + 0.1 + (1-(1-0.1*0.2)*0.7) + (1-(1-0.1*0.2)*0.7)*0.4 + (1-(1-0.1*0.2)*0.7)*0.4*0.5);
  BOOST_TEST(g.estimate_influence_inc(0, false) == cached);
  g.estimate_influence_inc(0, true);
  BOOST_TEST(cached == g.estimate_influence_inc());
  BOOST_TEST(g.estimate_influence_inc(2, false) == g.estimate_influence(std::vector<nid_t>({0,2})));
  cached = g.estimate_influence_inc(2, false);
  BOOST_TEST(cached == 2 + 0.4 + 0.4*0.5 + 0.1);
}

BOOST_AUTO_TEST_CASE(testIncrementalEstimation2) {
  char* data_path_str = std::getenv("ALLDATA_PATH");
  fs::path data_dir = data_path_str;
  fs::path tmp_subdir = "tmp";

  fs::path graph_dir = data_dir;
  graph_dir /= tmp_subdir;
  fs::remove_all(graph_dir);
  fs::create_directories(graph_dir);

  ofstream attf;
  attf.open((graph_dir / "attributes.txt").c_str());
  attf << "n=5\nm=7\n";
  attf.close();

  ofstream f;
  f.open((graph_dir / "graph.csv").c_str());
  f << "0 1 0.1\n1 2 0.2\n0 2 0.3\n2 3 0.4\n3 4 0.5\n4 3 0.6\n4 0 0.7";
  f.close();

  InfGraph g(graph_dir.native(), "graph.csv", false);
  g.init_hyper_graph();
  g.build_hyper_graph_r(1000);
  g.estimate_influence_inc(0, true);
  g.estimate_influence_inc(2, true);
  double cached = g.estimate_influence_inc(4, true);
  BOOST_TEST(cached == g.estimate_influence(std::vector<nid_t>({0,2,4})));
}

BOOST_AUTO_TEST_CASE(testLaplasian) {
  char* data_path_str = std::getenv("ALLDATA_PATH");
  fs::path data_dir = data_path_str;
  fs::path tmp_subdir = "tmp";

  fs::path graph_dir = data_dir;
  graph_dir /= tmp_subdir;
  fs::remove_all(graph_dir);
  fs::create_directories(graph_dir);

  ofstream attf;
  attf.open((graph_dir / "attributes.txt").c_str());
  attf << "n=5\nm=7\n";
  attf.close();

  ofstream f;
  f.open((graph_dir / "graph.csv").c_str());
  f << "0 1 0.1\n1 2 0.2\n0 2 0.3\n2 3 0.4\n3 4 0.5\n4 3 0.6\n4 0 0.7";
  f.close();

  InfGraph g(graph_dir.native(), "graph.csv", false);
  g.reward_type = LAPLACIAN;
  std::vector<std::vector<nid_t>> active_edges(5);
  active_edges[0].push_back(2);
  active_edges[0].push_back(1);
  active_edges[1].push_back(2);
  active_edges[2].push_back(3);
  active_edges[3].push_back(4);
  active_edges[4].push_back(3);
  active_edges[4].push_back(0);
  auto det = g.get_xi(active_edges, 0);
  BOOST_TEST(det == 1);
  auto det2 = g.get_xi(active_edges, 2);
  BOOST_TEST(det2 == 2);
}