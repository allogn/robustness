#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/timer/timer.hpp>
#include <unordered_set>

#include "argument.h"
#include "SatGreedy.h"
#include "dim/dim.hpp"

namespace po = boost::program_options;
using namespace boost::timer;

std::vector<nid_t> read_node_sequence(std::string adversary_file) {
    std::vector<nid_t> node_sequence;
    std::ifstream i(adversary_file);
    json j;
    i >> j;
    node_sequence = j["Blocked nodes"].get<std::vector<nid_t>>();
    return node_sequence;
}

int main(int argn, char **argv) {
    std::string adv_file;
    std::string tags;
    bool collect_marginal_gain;
    int lt_model;

    Argument arg;
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "produce help message")
            ("dataset,d", po::value<std::string>(&arg.dataset)->required(), "Graph dataset directory")
            ("output,o", po::value<std::string>(&arg.log)->required(), "Output json file (or directory if not dynamic)") //@todo directory was for airflow, redundant
            ("epsilon,e", po::value<double>(&arg.epsilon)->required(), "Accuracy parameter")
            ("seeds,s", po::value<int>(&arg.s)->default_value(1), "Number of seeds")
            ("lt,t", po::value<int>(&lt_model)->default_value(0), "0 - IC model, 1 - LT model")
            ("sampling,c", po::value<double>(&arg.blocked_nodes_sampling_coef)->default_value(1), "Sample number of blocked nodes")
            ("marginal,m", po::value<bool>(&collect_marginal_gain)->default_value(false), "Collect marginal gain from seeds per iteration")
            ("reward,r", po::value<int>(&arg.reward_type)->default_value(0), "Reward Type (0 - non-weighted hyperedges, 1 - weighted hyperedges)")
            ("adversary,a", po::value<std::string>(&adv_file), "File with sorted list of adversaries");

    // weighted hyperedges goes to the hypothesis that the RR approach is applicable to a wider spectrum of problems
    // including robust seed selection. It is just about how do we weight particular outcomes.

    po::variables_map vm;
    po::store(po::parse_command_line(argn, argv, desc), vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }
    po::notify(vm);

    auto_cpu_timer timer; // prints time on destruction
    arg.mc_iterations = 1000;
    arg.lt_model = lt_model == 1;
    
    if (adv_file != "") {
        Imm imm(arg);
        auto node_sequence = read_node_sequence(adv_file);
        std::unordered_set<int> existing_nodes;

        DIM dim;
        dim.init(); // Call at the beginning
        dim.is_lt = arg.lt_model;
        dim.set_beta(arg.epsilon);

        cpu_timer timer;


        std::vector<int> node_values_to_sample = blocked_nodes_values(arg.blocked_nodes_sampling_coef, imm.g.n);
        unsigned long visited_blocked_node_index = node_values_to_sample.size()-1;
        imm.logging["samples_blocked_values"] = node_values_to_sample;


        std::vector<double> s_values(node_values_to_sample.size());
        std::vector<std::vector<double>> marginal_gain(node_values_to_sample.size());
        for (int i = node_sequence.size()-1; i >= 0; i--) {

            bool sample_this_value = false;
            assert(visited_blocked_node_index >= 0);
            if (i == node_values_to_sample[visited_blocked_node_index]) {
                sample_this_value = true;
                visited_blocked_node_index--;
            }


            auto node_id = node_sequence[i];
            existing_nodes.insert(node_id);
            dim.insert(node_id);
            for (int j = 0; j < imm.g.g[node_id].size(); j++) {
                auto neigh_id = imm.g.g[node_id][j];
                if (existing_nodes.count(neigh_id)) {
//                    std::cout << "inserting " << node_id << " " << neigh_id << " " << imm.g.prob[node_id][j] << std::endl;
                    dim.insert(node_id, neigh_id, imm.g.prob[node_id][j]);
                }
            }
            for (int j = 0; j < imm.g.gT[node_id].size(); j++) {
                auto neigh_id = imm.g.gT[node_id][j];
                if (existing_nodes.count(neigh_id)) {
                    dim.insert(neigh_id, node_id, imm.g.probT[node_id][j]);
                }
            }

            if (sample_this_value) {
                if (arg.s >= existing_nodes.size()) {
                    s_values[visited_blocked_node_index+1] = existing_nodes.size();
                    continue;
                }

                auto best_nodes = dim.infmax(arg.s);
                if (i == node_sequence.size()-1) { //should be i here because i is absolute node index, not index in s_values
                    s_values[visited_blocked_node_index+1] = 1;  // hack because there are indexing problems in the lib
                } else {
                    s_values[visited_blocked_node_index+1] = dim.infest(best_nodes);
                }
                if (collect_marginal_gain) marginal_gain[visited_blocked_node_index+1] = dim.marginal_gain();
            }
        }
        imm.logging["sequence"] = s_values;
        if (collect_marginal_gain) imm.logging["marginal_gain"] = marginal_gain;
        imm.logging["rob"] = std::accumulate(s_values.begin(), s_values.end(), 0.0) / (double)node_sequence.size();
        imm.logging["runtime"] = timer.elapsed().user;
        imm.logging["mode"] = 0; // for general framework, as a part of satgreedy
        imm.logging["beta"] = arg.epsilon;
        imm.logging["seeds"] = arg.s;
        imm.logging["alpha"] = arg.blocked_nodes_sampling_coef;
        imm.logging["rr_sets"] = dim.hs.size();
        imm.logging.merge_patch(imm.g.graph_params);
        // does not work because I misunderstood the algorithm
//        imm.set_single_adversary_sequence(node_sequence);
//        imm.collect_sets_dynamic();
        imm.save_log();
    } else {
        SatGreedy s(arg);
        s.run_imm();
        s.save_log();
    }

    return 0;
}
