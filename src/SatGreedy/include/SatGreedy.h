#include <set>
#include <math.h>
#include <float.h>
#include <stdexcept>
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <numeric>
#include <exception>
#include <cfloat>
#include <ctime>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>
#include <boost/timer/timer.hpp>

#include "imm.h"
#include "head.h"
#include "adversary.h"
#include "sfmt/SFMT.h"
#include "Solver.h"

//#define VERBOSE

#ifdef VERBOSE
#define VERBOSE_INFO(...) INFO(__VA_ARGS__)
#else
#define VERBOSE_INFO(...) ;
#endif

namespace fs = boost::filesystem;
using json = nlohmann::json;
using namespace boost::timer;

struct CompareBySecondDouble {
	inline bool operator()(pair<nid_t, double> a, pair<nid_t, double> b) { return a.second < b.second; }
};

struct ExceedSeedsException : public std::exception {
	const char* what() const throw() {
		return "Seed set size reached maximum.";
	}
};

class SatGreedy : public Solver {
public:
	std::vector<adversary> adversaries;
	nid_t max_S;
	std::vector<nid_t> S;
	double objective;
	double cmax;
	sfmt_t sfmtSeed;
	bool dyn_mode;

	SatGreedy(Argument arg): Solver(arg) {
		sfmt_init_gen_rand(&sfmtSeed , 95082);
		dyn_mode = arg.number_of_blocked_nodes == -1;

		logging["dimbeta"] = arg.dim_beta;
		logging["alpha"] = arg.blocked_nodes_sampling_coef;
	}

	void add_adversaries() {
		for(auto& fname : arg.adversaries)
		{
			if (fname.substr( fname.length() - 4 ) == "json") {
				adversaries.emplace_back(g.n, arg, fname);
                VERBOSE_INFO("Added adversary: ", fname);
			}
			if((adversaries.size() > 1) &&
			   (adversaries[0].get_graph_size() != adversaries[adversaries.size()-1].get_graph_size())) {
				throw std::runtime_error("Different adversaries can not have different blocked nodes.");
			}
			ASSERTT(adversaries[adversaries.size()-1].g.n <= g.n, adversaries[adversaries.size()-1].g.n, g.n);
		}
		ASSERT(adversaries.size() > 0);
        ASSERT(adversaries.size() == arg.adversaries.size());
		logging["adversaries_size"] = adversaries.size();
	}

	void set_beta() {
		double beta_max = 1 + std::log(adversaries.size()) + std::log(3/arg.gamma);
		if (arg.beta > 0 && beta_max < arg.beta) {
			throw std::runtime_error("Config Beta is larger than Approximation Beta");
		}
		double beta = (arg.beta == 0) ? beta_max : arg.beta;
		max_S = beta * arg.s;
		max_S = (max_S > g.n) ? g.n-arg.number_of_blocked_nodes-1 : max_S;
		if (g.n-arg.number_of_blocked_nodes <= arg.s) {
			throw std::runtime_error("Number of blocked nodes, seeds and graph size are infeasible.");
			// if there are exactly only seeds (trivial problem), then its already infesible
		}
		logging["beta"] = beta;
        VERBOSE_INFO(beta, adversaries.size(), arg.gamma, arg.s, arg.original_objective);

		for (auto& a : adversaries) {
			a.set_max_S(max_S);
		}
	}

	void collect_rr() {
		double rr_time = 0;
		double rr_opt_time = 0;
		for (auto& a : adversaries) {
			clock_t begin = clock();
			a.collect_sets();
			clock_t end = clock();
			a.find_optimal();
			clock_t end2 = clock();
			rr_time += double(end - begin) / CLOCKS_PER_SEC;
			rr_opt_time += double(end2 - begin) / CLOCKS_PER_SEC;
		}
		logging["rr_collection_time"] = rr_time;
		logging["rr_opt_time"] = rr_opt_time;
	}

	double get_obj_with_new_seed(nid_t new_seed, double c, bool submodular) {
		std::vector<double> gval;
		for (auto& a : adversaries) {
			double norm = (arg.original_objective) ? a.get_optimal_influece() : g.n;
			double obj_est_for_a = a.try_new_seed(new_seed);

			// auto S2 = S;
			// S2.push_back(new_seed);
			// ASSERTT(a.try_new_seeds(S2) == a.try_new_seed(new_seed), a.try_new_seeds(S2), a.try_new_seed(new_seed));

			ASSERTT(((obj_est_for_a >= 0) && (obj_est_for_a <= g.n)), obj_est_for_a);
			double obj = min(c, obj_est_for_a/norm);
			gval.push_back(obj);
		}
		if (submodular) {
			return std::accumulate(gval.begin(), gval.end(), 0.0);
		} else {
			return *std::min_element(std::begin(gval), std::end(gval));
		}
	}

	double get_obj_for_set(std::vector<nid_t> seeds) {
		//only non-submodular
		double worst_obj = DBL_MAX;
		for (auto& a : adversaries) {
			double norm = (arg.original_objective) ? a.get_optimal_influece() : g.n;
			double obj_est_for_a = a.try_new_seeds(seeds);
			ASSERTT(((obj_est_for_a >= 0) && (obj_est_for_a <= g.n)), obj_est_for_a);
			double obj = obj_est_for_a/norm;
			if (obj < worst_obj) {
				worst_obj = obj;
			}
			// INFO(obj_est_for_a, a.name);
		}
		return worst_obj;
	}

    std::unordered_map<std::string, double> get_obj_for_set_per_adversary(std::vector<nid_t> seeds) {
        std::unordered_map<std::string, double> res;
        for (auto& a : adversaries) {
        	if (seeds.size() == 0) {
				res[a.name] = 0;
				continue;
			}
            double norm = (arg.original_objective) ? a.get_optimal_influece() : g.n;
            double obj_est_for_a = a.try_new_seeds(seeds);
            ASSERTT(((obj_est_for_a >= 0) && (obj_est_for_a <= g.n)), obj_est_for_a);
            double obj = obj_est_for_a/norm;
            res[a.name] = obj;
        }
        return res;
    }

	void add_new_seed(nid_t new_seed) {
		if (S.size() == max_S) {
#ifdef VERBOSE
			std::cout << std::endl; // for celf objective cout
#endif
			throw ExceedSeedsException();
		}
		S.push_back(new_seed);
		for (auto& a : adversaries) {
			a.add_new_seed(new_seed);
		}
	}

	double Celf(double max_obj, double c, bool submodular) {
		std::priority_queue<pair<long, double>, std::vector<pair<long, double>>, CompareBySecondDouble>heap;

		for (nid_t i = 0; i < g.n; i++)
		{
			double cost = get_obj_with_new_seed(i, c, submodular);
			pair<long, double>tep(make_pair(i, cost));
			heap.push(tep);
		}

		std::unordered_set<nid_t> visited;
		double obj = 0;
        VERBOSE_INFO("CELF:", max_obj, c);
		double last_delta = DBL_MAX;
		while (obj < max_obj) {
//#ifdef VERBOSE
//		    print_heap(heap);
//#endif
			if (heap.size() == 0) {
				throw std::runtime_error("Problem infeasible: not enough nodes for seeding to archieve robust result");
			}
			pair<nid_t, double>ele = heap.top();
			heap.pop();

			double delta_obj_candidate = ele.second;
//			assert(delta_obj_candidate > 0); // new candidate should bring something
			if (visited.find(ele.first) == visited.end()) {
				double new_obj_candidate = get_obj_with_new_seed(ele.first, c, submodular); // throws exception if too large S
				delta_obj_candidate = new_obj_candidate - obj;
				if (ele.second != delta_obj_candidate)
				{
					ele.second = delta_obj_candidate;
					heap.push(ele);
					visited.insert(ele.first);

					// INFO(ele.first, "Value updated", new_obj_candidate, delta_obj_candidate);
					// print_heap(heap);
					continue;
				}
			}

			//check CELF
//			if (submodular) {
//				double check_best_cost = 0;
//				nid_t best;
//				for (nid_t i = 0; i < g.n; i++)
//				{
//					if (std::find(S.begin(), S.end(), i) != S.end()) {
//						continue;
//					}
//					double cost = get_obj_with_new_seed(i, c, submodular);
//					std::cout << "Check " << i << "/" << cost << std::endl;
//					if (cost > check_best_cost) {
//						check_best_cost = cost;
//					}
//				}
//				ASSERTT(check_best_cost == obj + delta_obj_candidate, check_best_cost, obj + delta_obj_candidate);
//				ASSERTT(last_delta > delta_obj_candidate, last_delta, delta_obj_candidate);
//
//				last_delta = delta_obj_candidate;
//			}

			add_new_seed(ele.first);
			obj += delta_obj_candidate;

			// if (c > 10) {
			// 	auto S2 = S;
			// 	INFO(S2.size(), c, submodular);
			// 	S2.push_back(ele.first);
			// 	ASSERTT(get_obj_for_set(S2) == get_obj_with_new_seed(ele.first, c, submodular), get_obj_for_set(S2), get_obj_with_new_seed(ele.first, c, submodular));
			// 	ASSERTT(get_obj_for_set(S2) == obj, obj);
			// }

#ifdef VERBOSE
			std::cout << obj << " ";
#endif
			visited.clear();
		}

#ifdef VERBOSE
		std::cout << std::endl;
#endif

		return obj;
	}

	void run_imm() {
		INFO("Collecting RR for non-blocked graph");
		clock_t begin = clock();
		Imm imm(arg, std::vector<nid_t>());
		imm.collect_sets();
		auto s_nonbl = imm.get_optimal_seed_set(arg.s);
		ASSERT(s_nonbl.size() == arg.s);
		clock_t end = clock();

		logging["imm"]["time"] = double(end - begin) / CLOCKS_PER_SEC;
		logging["imm"]["seeds"] = s_nonbl;
		logging["imm"]["objective"] = imm.estimate_influence(s_nonbl);
		logging["imm"]["convergence"] = imm.g.reward_convergence;
		logging["imm"]["stats"] = imm.logging;
		logging["infgraph"] = imm.g.logging;

		for (auto it = s_nonbl.begin(); it != s_nonbl.end(); it++) {
			ASSERT((*it) >= 0);
			ASSERT((*it) < g.n);
		}
		ASSERT(imm.g.n = g.n);

		// double dev = 1;
		// std::cout << "[";
		// for (int i = 1; i <=8; i++) {
		// 	vector<nid_t> degree2(degree.begin(), degree.begin()+arg.s/dev);
		// 	vector<nid_t> imm2(s_nonbl.begin(), s_nonbl.begin()+arg.s/dev);
		//
		// 	std::pair<double, double> aa = run_simulations(imm.g, degree2);
		// 	// INFO(aa.first, aa.second);
		//
		// 	std::pair<double, double> a = run_simulations(imm.g, imm2);
		// 	// INFO(a.first, a.second);
		// 	dev *= 2;
		// 	std::cout << "[" << aa.first << ", " << a.first << "],";
		// }
		// std::cout << std::endl;

        VERBOSE_INFO("Imm Seeds calculated");
		return;
	}

	void run_adversarial_imm() {
        VERBOSE_INFO("Collecting RR for non-blocked graph");
		clock_t begin = clock();
		std::vector<std::vector<nid_t>> blocked_nodes_sets;
		for (auto& a : adversaries) {
			blocked_nodes_sets.push_back(a.g.id_to_ind);
		}
		Imm imm(arg);
		int R = 1000;
		imm.g.build_hyper_graph_r(R, blocked_nodes_sets);
        VERBOSE_INFO("Sets collected", R);
		auto s_nonbl = imm.get_optimal_seed_set(max_S);
		ASSERT(s_nonbl.size() == max_S);
		clock_t end = clock();
		logging["adversary_imm"]["time"] = double(end - begin) / CLOCKS_PER_SEC;
		S = s_nonbl;
		objective = imm.estimate_influence(s_nonbl);
		INFO(objective);
		for (auto it = s_nonbl.begin(); it != s_nonbl.end(); it++) {
			ASSERT((*it) >= 0);
			ASSERT((*it) < g.n);
		}
		ASSERT(imm.g.n = g.n);

        VERBOSE_INFO("AdvImm Seeds calculated");
		return;
	}

	void run_sat_greedy() {
		ASSERT(adversaries.size() > 0);

		double cmin = 0;
		double cmax = 1;
		int iter = 0;
		double c;
		double coef = adversaries.size() - arg.gamma / 3;
		while (cmax - cmin >= arg.gamma) {
			iter++;
			c = (cmax + cmin) / 2;
			try {
				Celf(c * coef, c, true);
				cmin = c * (1 - arg.gamma/3);
			} catch (ExceedSeedsException& e) {
				cmax = c;
			}
			VERBOSE_INFO(iter, cmax, cmin, cmax-cmin, S.size());
		}
		this->cmax = cmax;//for approximation guarantees
		if (arg.strict_s && S.size() > max_S) {
			S.resize(max_S); //troubles here
		}

		if (S.size() == 0) {
			throw std::runtime_error("No optimal seed candidates found.");
		}

		if (S.size() < max_S) {
			INFO("Small Gamma! Filling up greedily...");
			try {
				Celf(DBL_MAX, c, true); //c remains the same.
			} catch (ExceedSeedsException& e) {}
		}
		ASSERT(S.size() == max_S);
		this->objective = get_obj_for_set(S);
        VERBOSE_INFO("SatGreedy objective found", this->objective);
	}

	void run_all_greedy() {
		double best_obj = 0;
		for (auto& a : adversaries) {
			std::vector<nid_t> opt_set = a.get_optimal_seed_set();
			double min_obj = get_obj_for_set(opt_set);
			if (min_obj > best_obj) {
				S = opt_set;
				best_obj = min_obj;
				VERBOSE_INFO("New best objective:", a.name, best_obj);
			}
		}
		this->objective = best_obj;
	}

	void run_single_greedy(bool celf) {
		if (celf) {
			try {
				Celf(DBL_MAX, DBL_MAX, false);
			}  catch (ExceedSeedsException& e) {}
			this->objective = get_obj_for_set(S);
            VERBOSE_INFO("Single Greedy objective", this->objective);
			return;
		}

		std::unordered_set<nid_t> greedySset;
		double best_obj;
		while (greedySset.size() < max_S) {
			nid_t best_n = -1;
			best_obj = -1;
			for (nid_t i = 0; i < g.n; i++) {
				if (greedySset.find(i) != greedySset.end()) {
					continue;
				}
				double new_obj = get_obj_with_new_seed(i, DBL_MAX, false);
				if (new_obj > best_obj) {
					best_obj = new_obj;
					best_n = i;
				}
			}
			ASSERT(best_n > -1); // best_obj can be zero if any seed does not bring benefit (tough problem of non-overlapping adversaries)
			add_new_seed(best_n);
			greedySset.insert(best_n);
            VERBOSE_INFO("Greedy iteration", S.size(), best_obj);

			if (!arg.strict_s && arg.beta == 0 && S.size() <= arg.s) {
				ASSERT(best_obj <= cmax || (cmax == 1 && best_obj > 1)); // a guarantee of H&K
			}
		}
		this->objective = best_obj;
        VERBOSE_INFO("Single Greedy objective", best_obj);
	}

	void reset() {
		// total reset before running another solver
		S.clear();
		objective = -1;
		for (auto& a : adversaries) {
			a.reset();
		}
	}

	void run_mc_for_all_adversaries(std::string solver_name) {
	    if (arg.mc_iterations == 0) {
	        return;
	    }
		INFO("Running MC simulations", solver_name);
		clock_t begin = clock();
		std::pair<double, double> worst_mc_obj = std::make_pair(DBL_MAX,DBL_MAX);
		std::string worst_name;
		for (auto& a : adversaries) {
			std::pair<double, double> mc_results = run_simulations(a.g, S);
			// INFO(mc_results.first, mc_results.second, g.n);
			double norm = (arg.original_objective) ? a.get_optimal_influece() : g.n;
			mc_results.first = mc_results.first/norm;
			mc_results.second = mc_results.second/norm;
			if (worst_mc_obj.first > mc_results.first) {
				worst_mc_obj = mc_results;
				worst_name = a.name;
			}
		}
		clock_t end = clock();
		logging[solver_name]["mc_simulation_time"] = double(end - begin) / CLOCKS_PER_SEC;
		logging[solver_name]["worst_adv_name"] = worst_name;
		logging[solver_name]["mc_obj"] = worst_mc_obj.first;
		logging[solver_name]["mc_obj_std"] = worst_mc_obj.second;
		INFO(worst_name, worst_mc_obj.first, worst_mc_obj.second);

		// assert MC and RR shows the same
		// objective variable is set globally in the class, so be sure to run seed allocation first
		double error = abs(worst_mc_obj.first - objective)/objective;
		INFO("MC vs RR", error);
		if (error > 0.065) {
			INFO("!!!!!!!!!WARNING!!!!!!!!", error);
			// throw std::runtime_error("MC != RR (>6.5%). Increase accuracy.");
		}
	}

	std::pair<double, double> run_simulations(Graph& g, std::vector<nid_t> seeds) {
		std::vector<nid_t> spread;
		nid_t n = g.n;
		vector<bool> visit = vector<bool> (n);
		vector<nid_t> visit_mark = vector<nid_t> (n);
		std::deque<nid_t> init_q;
		for (auto& s : seeds) {
			nid_t mapped_seed = g.id_to_ind[s];
			if (mapped_seed < 0) {
				continue; // no such seed in this graph
			}
			init_q.push_back(mapped_seed); //activate all seeds
			ASSERT(!visit[mapped_seed]); //otherwise there are non-unique seeds
			visit[mapped_seed] = true;
			// don't mark them as visit_mark, because they should not be released
		}
		ASSERTT(init_q.size() <= g.n, init_q.size(), g.n);
		for (int iteration = 0; iteration < arg.mc_iterations; iteration++) {
			std::deque<nid_t> q(init_q.begin(), init_q.end());
			nid_t n_visit_mark = 0;

			while (!q.empty())
			{
				nid_t expand = q.front();
				q.pop_front();
				nid_t i = expand;
				for (nid_t j = 0; j < (nid_t)g.g[i].size(); j++)
				{
					//int u=expand;
					nid_t v = g.g[i][j];
					if (visit[v])
						continue;
					double randDouble = sfmt_genrand_real1(&sfmtSeed);
					if (randDouble > g.prob[i][j])
						continue;
					if (!visit[v])
					{
						ASSERT(n_visit_mark < n);
						visit_mark[n_visit_mark++] = v;
						visit[v] = true;
					}
					q.push_back(v);
				}
			}
			for (nid_t i = 0; i < n_visit_mark; i++)
				visit[visit_mark[i]] = false;
			spread.push_back(n_visit_mark + init_q.size());
			ASSERTT(n_visit_mark <= g.n, n_visit_mark, g.n);
		}

		double sum = std::accumulate(spread.begin(), spread.end(), 0.0);
		double mean = sum / spread.size();

		std::vector<double> diff(spread.size());
		std::transform(spread.begin(), spread.end(), diff.begin(), [mean](double x) { return x - mean; });

		double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
		double stdev = std::sqrt(sq_sum / spread.size());
		return std::make_pair(mean, stdev);
	}

	void init() {
		if (adversaries.size() > 0) {
			throw std::runtime_error("Solver initialized twice");
		}
		add_adversaries();
		set_beta();
	}

	void run_solver_mapping(std::string solvername) {
		if (solvername == "satgreedy") run_sat_greedy();
		if (solvername == "all_greedy") run_all_greedy();
		if (solvername == "single_greedy_celf") run_single_greedy(true);
		if (solvername == "single_greedy") run_single_greedy(false);
	}

	void run_solver(std::string solvername, int added_nodes) {
	    S.clear();
	    for (auto& a : adversaries) {
	        if (dyn_mode) {
                a.dim.infmax_with_state_init();
	        } else {
                a.g.reset_covered();
	        }
	    }

		objective = -1;
		VERBOSE_INFO("\n> Running ",solvername);
        run_solver_mapping(solvername);
	}

	void run_all(std::vector<std::string> solvers = std::vector<std::string>({"satgreedy", "all_greedy", "single_greedy_celf", "single_greedy"})) {
		cpu_timer total_timer;
		init();
		if (dyn_mode) {
            std::unordered_map<std::string,std::vector<double>> objectives;
            double adding_nodes_time = 0;
            std::unordered_map<std::string, double> solver_times;


			std::vector<int> node_values_to_sample = blocked_nodes_values(arg.blocked_nodes_sampling_coef, g.n);
			unsigned long visited_blocked_node_index = node_values_to_sample.size()-1;
			logging["samples_blocked_values"] = node_values_to_sample;

            for (auto& s : solvers) {
                solver_times[s] = 0;
                objectives[s] = std::vector<double>(node_values_to_sample.size());
            }


            for (int added_nodes = 0; added_nodes < g.n; added_nodes++) {
            	VERBOSE_INFO(">>> STARTING ", added_nodes);

            	bool sample_this_value = false;
            	assert(visited_blocked_node_index >= 0);
            	if (g.n-added_nodes-1 == node_values_to_sample[visited_blocked_node_index]) {
            		sample_this_value = true;
					visited_blocked_node_index--;
            	}

                cpu_timer timer;
                for (auto& a : adversaries) {
                    a.add_one_node();
                }
                adding_nodes_time += timer.elapsed().user;

                if (sample_this_value) {
					for (auto& s : solvers) {
						cpu_timer timer;

						if (added_nodes <= max_S) {
							objective = 0;
							S.clear();
						} else {
							run_solver(s, added_nodes);
						}
						if ((added_nodes == max_S+1) && (objective > 0)) {
							logging[s]["error"] = "Solver does not converge to 0 for S=max_S. Such case not implemented";
							std::cout << "Solver does not converge to 0 for S=max_S. Such case not implemented" << std::endl;
//                        throw std::runtime_error("Solver does not converge to 0 for S=max_S. Such case not implemented");
						}

						logging[s]["objective_per_adversary"][visited_blocked_node_index+1] = get_obj_for_set_per_adversary(S);
						solver_times[s] = solver_times[s] + timer.elapsed().user;
						objectives[s][visited_blocked_node_index+1] = objective;
					}
                }
            }
            for (auto& s : solvers) {
                logging[s]["objectives"] = objectives[s];
                logging[s]["time"] = solver_times[s];
            }
            logging["dim_time"] = adding_nodes_time;

		} else {
		    cpu_timer timer;
            collect_rr();
            logging["rr_collection"] = timer.elapsed().user;

            for (auto& s : solvers) {
                run_solver(s, 0);
                logging[s]["time"] = timer.elapsed().user;
                logging[s]["seeds"] = S;
                logging[s]["objective"] = objective;
            }

		}
		logging["runtime"] = total_timer.elapsed().user;
        save_log();
	}


	void print_heap(std::priority_queue<pair<long, double>, std::vector<pair<long, double>>, CompareBySecondDouble> heap) {
		std::cout << "Heap: " << std::endl;
		while (heap.size() > 0) {
			auto t = heap.top();
			heap.pop();
			std::cout << t.first << "/" << t.second << " ";
		}
		std::cout << std::endl;
	}
};
