#ifndef __IMM_H__
#define __IMM_H__

#include "infgraph.h"
#include "argument.h"
#include "graph.h"
#include "head.h"
#include "json.h"
#include <set>
#include <map>
#include <boost/filesystem.hpp>

using json = nlohmann::json;
namespace fs = boost::filesystem;

class Math{
    public:
        static double log2(int n){
            return log(n) / log(2);
        }
        static double logcnk(int n, int k) {
            double ans = 0;
            for (int i = n - k + 1; i <= n; i++)
            {
                ans += log(i);
            }
            for (int i = 1; i <= k; i++)
            {
                ans -= log(i);
            }
            return ans;
        }
};
class Imm
{

    double epsilon;
    long W, unblocked_edges, unblocked_nodes;// for dynamic imm

    double get_min_W() {
        return (1./epsilon) * (double)(unblocked_nodes + unblocked_edges) * log2(unblocked_nodes) + 1; // 1 if there is only one node
    }


    // private:
        double step1()
        {
            double epsilon_prime = arg.epsilon * sqrt(2);
            Timer t(1, "step1");
            for (int x = 1; ; x++)
            {
                int64 ci = (2+2/3 * epsilon_prime)* ( std::log(g.n) + Math::logcnk(g.n, arg.s) + std::log(Math::log2(g.n))) * pow(2.0, x) / (epsilon_prime* epsilon_prime);
                g.build_hyper_graph_r(ci);

                double ept = g.build_seedset(arg.s);
                double estimate_influence = ept * g.n;
                INFO(x, estimate_influence, ept, g.n);
                if (ept > 1 / pow(2.0, x))
                {
                    double OPT_prime = ept * g.n / (1+epsilon_prime);
                    // INFO("step1", OPT_prime * (1+epsilon_prime));

                    return OPT_prime;
                }
            }
        }
        void step2(double OPT_prime)
        {
            Timer t(2, "step2");
            ASSERT(OPT_prime > 0);
            double e = std::exp(1);
            double alpha = sqrt(std::log(g.n) + std::log(2));
            double beta = sqrt((1-1/e) * (Math::logcnk(g.n, arg.s) + std::log(g.n) + std::log(2)));

            int64 R = 2.0 * g.n *  sqrt((1-1/e) * alpha + beta) /  OPT_prime / arg.epsilon / arg.epsilon ;
            INFO(R);
            //R/=100;
            g.build_hyper_graph_r(R);
        }

        double step3() {
          double opt = g.build_seedset(arg.s)*g.n;
          return opt;
        }


public:
    InfGraph g;
    Argument arg;
    json logging;

    Imm(Argument arg,
        std::vector<nid_t> blocked_nodes = std::vector<nid_t>(),
        std::vector<double> edgelist = std::vector<double>(),
        std::vector<double> weights = std::vector<double>()) {
      g = InfGraph(arg.dataset, "graph.csv", blocked_nodes, edgelist, weights);

      this->arg = arg;
      this->epsilon = arg.epsilon;
      g.reward_type = (RewardType)arg.reward_type;
      logging["s"] = arg.s;
      logging["epsilon"] = arg.epsilon;
    }
    ~Imm() {}

    void set_single_adversary_sequence(vector<nid_t>& v) {
        g.single_adversary_sequence = v;
        if (v.size() != g.n) {
            throw std::runtime_error("Single adversary sequence must have the same size as the network.");
        }
    }

    double get_optimal()
    {
        double opt = step3();

        std::vector<nid_t> result;
        for (const auto& s : g.seedSet) result.push_back(g.ind_to_id[s]);

        logging["selected_seeds"] = result;
        // logging["estimate_influence"] = opt;
        return opt;
    }

    std::vector<nid_t> get_optimal_seed_set(int k) {

      // return an optimal set selected by IM algorithm of size k
      std::vector<nid_t> result;

      k = std::min({(nid_t)g.n,(nid_t)k});
      double opt = g.build_seedset(k)*g.n;;

      assert(g.seedSet.size() >= k);
      for (nid_t i = 0; i < k; i++) result.push_back(g.ind_to_id[g.seedSet[i]]);
      // this->log_array["max_selected_seeds"] = result;
      // this->logging["estimate_influence"] = opt;

      // if more seeds required than there is non-blocked available, then return arbitrary blocked one.
      // it is possible to query some blocked nodes, because robust version may decide that blocked by one adversary node is still worth selecting


      return result;
    }

    double get_best_seed_objective() {
        return g.build_seedset(1);
    }

    double estimate_influence(std::vector<nid_t>& S)
    {
      // map forward
      std::vector<nid_t> mapped_S;
      for (const auto& s: S) {
        if (g.id_to_ind[s] < 0) {
          // INFO("Seed node s is blocked: ", s);
        } else {
          mapped_S.push_back(g.id_to_ind[s]);
        }
      }
      // INFO(mapped_S.size());
      return g.estimate_influence(mapped_S);
    }

    void collect_sets() {
        if (this->arg.reward_type == 0) {

            Timer t(100, "InfluenceMaximize(Total Time)");

            INFO("########## Step1 ##########");

            // debugging mode lalala
            double OPT_prime;
            OPT_prime = step1(); //estimate OPT

            INFO("########## Step2 ##########");


            double opt_lower_bound = OPT_prime;
            logging["opt_lower_bound"] = opt_lower_bound;
            INFO(opt_lower_bound);
            step2(OPT_prime);
            INFO("step2 finish");
        } else {
            g.build_hyper_graph_r(arg.rr_iterations);
        }
    }

    void collect_sets_dynamic() {
        std::vector<double> s_values(g.single_adversary_sequence.size(), 0);
        unblocked_nodes = 0;
        unblocked_edges = 0;
        W = 0;

        for (int i = 0; i < g.n; i++) {
            g.single_adversary_blocked_nodes.insert(i);
        }

        for (int i = g.single_adversary_sequence.size()-1; i >= 0; i--) {
            nid_t newly_unblocked_node_id = g.single_adversary_sequence[i];
            g.single_adversary_blocked_nodes.erase(newly_unblocked_node_id);
            unblocked_nodes++;
            unblocked_edges += g.g[newly_unblocked_node_id].size();
            g.rebuild_hyper_graph_after_node_insertion(newly_unblocked_node_id, &W);
            double min_W = get_min_W();
            while (W < min_W) {
                g.build_hyper_graph_r(g.get_hyper_size() + 1, std::vector<std::vector<nid_t>>(), &W);
            }
            s_values[i] = get_best_seed_objective() * unblocked_nodes; // we must use non-normalized
        }
        logging["s values"] = s_values;
    }

    void save_log() {
        ofstream logf;
        std::string fn = arg.log;
        logf.open(fn);
        logf << logging.dump(4);
        logf.close();
    }
};


#endif // __IMM_H__
