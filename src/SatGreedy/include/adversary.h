#ifndef __ADVERSARY_H__
#define __ADVERSARY_H__

#include "graph.h"
#include "argument.h"
#include "head.h"
#include "dim/dim.hpp"

class adversary {
private:
    bool dyn_mode;

    double estimate_opt() {
        //function for collecting sets of TIM
        double epsilon_prime = arg.epsilon * sqrt(2);
        for (int x = 1; ; x++)
        {
            assert(max_S < g.n); // important: not "<="
            int64 ci = (2+2/3 * epsilon_prime)* ( std::log(g.n) + Math::logcnk(g.n, max_S) + std::log(Math::log2(g.n))) * pow(2.0, x) / (epsilon_prime* epsilon_prime);
            g.build_hyper_graph_r(ci);

            double ept = g.build_seedset(max_S);
            double estimate_influence = ept * g.n;
            // INFO(x, estimate_influence, ept, g.n, arg.k);
            if (ept > 1 / pow(2.0, x))
            {
                double OPT_prime = ept * g.n / (1+epsilon_prime);
                // INFO("step1", OPT_prime * (1+epsilon_prime));

                return OPT_prime;
            }
        }
    }
public:
    InfGraph g;
    Argument arg;
    bool cached_estimation = false;
    std::string name;
    nid_t max_S;
    nid_t complete_graph_size;
    double opt;
    DIM dim; // for the case of gradual addition of nodes
    std::vector<nid_t> dynamic_blocked_nodes; // this is a backup of blocked nodes if needed to add them gradually
    std::unordered_set<int> existing_nodes;

    adversary(nid_t complete_graph_size, Argument arg, std::string fname): complete_graph_size(complete_graph_size), arg(arg) {
        /*
         * Creates a graph with blocked nodes, changed edges or new weights.
         * If last two lists are empty, then only nodes are blocked.
         * Otherwise a set of edges and weights will be substituted by those in the lists.
         */
        dyn_mode = arg.number_of_blocked_nodes == -1;

        ifstream f(fname);
        json j;
        f >> j;
        f.close();

        std::vector<nid_t> blocked_nodes;
        try {
            blocked_nodes = j.at("Blocked nodes").get<std::vector<nid_t>>();
            if (!dyn_mode) {
                vector<int> newVec(blocked_nodes.begin(), blocked_nodes.begin() + arg.number_of_blocked_nodes);
                blocked_nodes = newVec;
            } else {
                // load complete graph as there are no blocked nodes at all, and store blocked nodes separately
                dynamic_blocked_nodes = blocked_nodes;
                blocked_nodes.clear();
                dim.init(); // Call at the beginning
                dim.set_beta(arg.dim_beta);
            }
        } catch (json::type_error& e) {
            INFO("Incorrect adversary input. ", j.dump(4));
            throw std::runtime_error("Input error.");
        }


        std::vector<double> edgelist;
        try {
            edgelist = j.at("edgelist").get<std::vector<double>>();
        } catch (json::out_of_range& e) {} // ignore if empty

        std::vector<double> weights;
        try {
            weights = j.at("weights").get<std::vector<double>>();
        } catch (json::out_of_range& e) {} // ignore if empty

        try {
            this->name = j.at("solver");
        } catch (json::type_error& e) {
            INFO("Incorrect adversary input, no solver.", j.dump(4));
            throw std::runtime_error("Input error.");
        }
        g = InfGraph(arg.dataset, "graph.csv", blocked_nodes, edgelist, weights);
    }

    void set_max_S(nid_t max_S) {
        this->max_S = max_S;
    }

    nid_t get_graph_size() {
        return g.n;
    }

    void collect_sets() {
        ASSERT(!dyn_mode);
        double OPT_prime;
        OPT_prime = estimate_opt();

        ASSERT(OPT_prime > 0);
        double e = std::exp(1);
        double alpha = sqrt(std::log(g.n) + std::log(2));
        double beta = sqrt((1-1/e) * (Math::logcnk(g.n, max_S) + std::log(g.n) + std::log(2)));

        int64 R = 2.0 * g.n *  sqrt((1-1/e) * alpha + beta) /  OPT_prime / arg.epsilon / arg.epsilon ;
//        INFO("Estimation of RR sets for " + this->name + ":", R);
        //R/=100;
        g.build_hyper_graph_r(R);
//        INFO("RR collected", OPT_prime, name, g.hyperToNodes.size());
    }

    void find_optimal() {
        ASSERT(!dyn_mode);
        //INFO("Collecting optimal seeds for ", name);
        this->opt = g.build_seedset(max_S)*g.n;
    }

    std::vector<nid_t> get_optimal_seed_set() {
        if (dyn_mode) {
            return dim.infmax(max_S);
        } else {
            std::vector<nid_t> result;
            for (const auto& s : g.seedSet) result.push_back(g.ind_to_id[s]);
            return result;
        }
    }

    void reset() {
        g.reset_covered();
        existing_nodes.clear();
        dim.reset();
    }

    void add_new_seed(nid_t seed) {
        if (dyn_mode) {
            dim.infmax_with_state_add(seed); // no mapping at this level anymore
        } else {
            nid_t mapped_seed = g.id_to_ind[seed];
            if (mapped_seed >= 0) {
                g.estimate_influence_inc(mapped_seed, true);
            }
        }
    }

    double try_new_seed(nid_t seed) {
        if (dyn_mode) {
            if (existing_nodes.count(seed)) {
                return dim.infmax_with_state_get(seed);
            }
            return dim.infmax_with_state_get();
        } else {
            nid_t mapped_seed = g.id_to_ind[seed];
            if (mapped_seed < 0) { // if seed does not exists in the blocked network
                return g.estimate_influence_inc();
            }
            return g.estimate_influence_inc(mapped_seed, false);
        }
    }

    double try_new_seeds(std::vector<nid_t> new_seeds) {
        if (dyn_mode) {
            return dim.infest(new_seeds);
        } else {
            // map forward
            std::vector<nid_t> mapped_S;
            for (const auto& s: new_seeds) {
                if (g.id_to_ind[s] < 0) {
                } else {
                    mapped_S.push_back(g.id_to_ind[s]);
                }
            }
            return g.estimate_influence(mapped_S);
        }
    }

    double get_optimal_influece() {
        assert(!dyn_mode);
        return opt;
    }

    void add_one_node() {
        ASSERT(dyn_mode);
        if (dynamic_blocked_nodes.size() == existing_nodes.size()) {
            throw std::runtime_error("No more nodes exist to add");
        }
        int node_id = dynamic_blocked_nodes[dynamic_blocked_nodes.size() - existing_nodes.size() - 1];
        existing_nodes.insert(node_id);
        dim.insert(node_id);
        for (int j = 0; j < g.g[node_id].size(); j++) {
            auto neigh_id = g.g[node_id][j];
            if (existing_nodes.count(neigh_id)) {
                dim.insert(node_id, neigh_id, g.prob[node_id][j]);
            }
        }
        for (int j = 0; j < g.gT[node_id].size(); j++) {
            auto neigh_id = g.gT[node_id][j];
            if (existing_nodes.count(neigh_id)) {
                dim.insert(neigh_id, node_id, g.probT[node_id][j]);
            }
        }
    }
};

#endif //__ADVERSARY_H__
