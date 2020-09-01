#define HEAD_INFO

#include "argument.h"
#include "graph.h"
#include "iheap.h"
#include "head.h"
#include "json.h"
#include <queue>
#include <utility>
#include <math.h>
#include <unordered_set>
#include <random>
#include <unordered_map>
#include <numeric>
#include <boost/functional/hash.hpp>

#include <Eigen/Sparse>
//#include <Spectra/GenEigsSolver.h>
//#include <Spectra/MatOp/SparseGenMatProd.h>


using json = nlohmann::json;

typedef std::pair<double,int> dipair;

struct CompareBySecond {
	bool operator()(pair<int, int> a, pair<int, int> b)
	{
		return a.second < b.second;
	}
};

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
		std::size_t seed = 0;
        boost::hash_combine(seed, p.first);
        boost::hash_combine(seed, p.second);
        return seed;  
    }
};


typedef enum {UNIT, LAPLACIAN} RewardType;

class InfGraph: public Graph
{
private:
    vector<bool> visit;
    vector<int> visit_mark;
    bool collect_convergence = false;
    vector<nid_t> random_seeds;
	std::default_random_engine generator;
public:
	json logging;
    vector<vector<int>> nodesToHyper;
    vector<vector<int>> hyperToNodes;
    vector<double> hyperToReward;
	// nodesToHyper is an array <id of node> -> <list of hyperedges>
	// hyperToNodes is an array <id of hyperedge> -> <list of nodes>
	vector<double> nodeReward; // store size of nodesToHyper[i], or arbitrary function on graph samples
	vector<double> reward_convergence;
	unordered_set<nid_t> single_adversary_blocked_nodes; // for dynamic version
	vector<nid_t> single_adversary_sequence; // for dynamic version, a sequence of nodes to block

	RewardType reward_type = UNIT;
	bool is_lt;

    InfGraph() {}

    InfGraph(string folder, string graph_file, bool _is_lt, std::vector<nid_t> blocked_nodes = std::vector<nid_t>(),
							std::vector<double> edgelist = std::vector<double>(),
							std::vector<double> weights = std::vector<double>()): Graph(folder, graph_file, blocked_nodes, edgelist, weights)
    {
        sfmt_init_gen_rand(&sfmtSeed , 95082);
        init_hyper_graph();
        visit = vector<bool> (n);
        visit_mark = vector<int> (n);
		is_lt = _is_lt;

        if (collect_convergence) {
            for (int i = 0; i < 0.1*n; i++)
            {
                long new_seed = sfmt_genrand_uint32(&sfmtSeed) % n;
                while (std::find(random_seeds.begin(), random_seeds.end(), new_seed) != random_seeds.end()) {
                    new_seed = sfmt_genrand_uint32(&sfmtSeed) % n;
                }
                random_seeds.push_back(new_seed);
            }
        }
    }


    void init_hyper_graph(){
        nodesToHyper.clear();
        for (int i = 0; i < n; i++)
            nodesToHyper.push_back(vector<int>());
        hyperToNodes.clear();
        nodeReward.clear();
        nodeReward.resize(n, 0);
        hyperToReward.clear();
    }

    void rebuild_hyper_graph_after_node_insertion(nid_t new_node, long* W) {
		vector<int> random_number;
		double p = 1./(n-single_adversary_blocked_nodes.size()); // single_adv was already incremented, so corresponds to n+1
		for (long i = 0; i < hyperToNodes.size(); i++)
		{
			double r = sfmt_genrand_real1(&sfmtSeed);
			if (r < p) {
				clear_hyperedge(i, W);
				BuildHypergraphNode(new_node, i, W);

                for (int t : hyperToNodes[i]) {
                    // previously this hyper was removed from all nodes, now new set of reachable nodes found and we need to put it back
                    nodesToHyper[t].push_back(i);
                    nodeReward[t] += hyperToReward[i];
                }
			}

		}

		if (collect_convergence) {
			throw std::runtime_error("Convergence are not implemented for dynamic IMM");
		}
    }

    void clear_hyperedge(long hyperedge_ind, long* W) {
    	for(auto node_id : hyperToNodes[hyperedge_ind]) {
    		auto& v = nodesToHyper[node_id];
			v.erase(std::find(v.begin(), v.end(), hyperedge_ind));
			nodeReward[node_id]--;
    	}
        // we also need to reduce W by number of edges, but larger W is not bad, and we don't store number of edges
        // so we just estimate it by number of nodes again
        (*W) -= 2*hyperToNodes[hyperedge_ind].size() + 1;
		hyperToNodes[hyperedge_ind].clear();
    }

    void build_hyper_graph_r(int64 R, std::vector<std::vector<nid_t>> blocked_nodes = std::vector<std::vector<nid_t>>(), long* W = nullptr)
    {
        //R is a new size, not additional size
        if( R > INT_MAX ){
            cout<<"Error:R too large"<<endl;
            exit(1);
        }
//         INFO("build_hyper_graph_r", R);


        int prevSize = hyperToNodes.size();
        while ((int)hyperToNodes.size() <= R) {
			hyperToNodes.push_back( vector<int>() );
        }
		hyperToReward.resize(R+1);

        vector<int> random_number;
        for (int i = 0; i <= R-prevSize; i++)
        {
        	if (!single_adversary_sequence.empty()) {
        		// dynamic im case
        		// we assume that nodes that are not blocked are the last nodes in single_adversary_sequence
				int number_of_active_nodes = n - single_adversary_blocked_nodes.size();
				int random_node_ind = sfmt_genrand_uint32(&sfmtSeed) % number_of_active_nodes;
				random_number.push_back(single_adversary_sequence[n-random_node_ind-1]);
        	} else {
				random_number.push_back(  sfmt_genrand_uint32(&sfmtSeed) % n);
        	}
        }


        int totAddedElement = 0;

        for (int i = prevSize; i <= R; i++)
        {
			if (blocked_nodes.size() > 0) {
            	BuildHypergraphNodeBlocked(random_number[i-prevSize], i, blocked_nodes, this->is_lt);
			} else {
            	BuildHypergraphNode(random_number[i-prevSize], i, this->is_lt, W); //W is the measure for dynamic IM, it should min required samples
			}

            for (int t : hyperToNodes[i]) {
                nodesToHyper[t].push_back(i);
//                INFO(hyperToReward.size(), prevSize, i, hyperToNodes.size());
                assert(hyperToReward.size() > i); // i is index, so should be at most 1 smaller
                    nodeReward[t] += hyperToReward[i];
                    totAddedElement++;
            }
			if (collect_convergence && ((i % (long) (R * 0.1) == 0 ) || i == R-1)) {
			    reward_convergence.push_back(get_expected_reward(random_seeds));
			    INFO(reward_convergence[reward_convergence.size()-1]);
			}
        }

        if (collect_convergence) {

			double ei = estimate_influence(random_seeds);
			INFO(ei);
			collect_bruteforced_stats(random_seeds, 30);
        }
    }

    //return the number of edges visited
    deque<int> q;
    sfmt_t sfmtSeed;
	vector<int> seedSet;

	//This is build on Mapped Priority Queue
	double build_seedset(int k)
	{
		priority_queue<pair<int, double>, vector<pair<int, double>>, CompareBySecond>heap;
		vector<double>oldReward(n, 0);
		for (int i = 0; i < n; i++)
		{
			pair<int, double>tep(make_pair(i, nodeReward[i]));
			heap.push(tep);
			oldReward[i] = nodeReward[i];
		}

		int maxInd;

		double influence = 0;
		long long numEdge = hyperToNodes.size();

		// check if an edge is removed
		vector<bool> edgeMark(numEdge, false);
		// check if an node is remained in the heap
		vector<bool> nodeMark(n + 1, true);

		seedSet.clear();
		while ((int)seedSet.size()<k)
		{
			ASSERT(!heap.empty());
			pair<int, double>ele = heap.top();
			heap.pop();
			if (ele.second > oldReward[ele.first])
			{
				ele.second = oldReward[ele.first];
				heap.push(ele);
				continue;
			}

			maxInd = ele.first;
			vector<int>e = nodesToHyper[maxInd];
			influence += oldReward[maxInd];
			seedSet.push_back(maxInd);
			nodeMark[maxInd] = false;

			for (unsigned int j = 0; j < e.size(); ++j){
				if (edgeMark[e[j]]) continue;

				vector<int>nList = hyperToNodes[e[j]];
				for (unsigned int l = 0; l < nList.size(); ++l){
					if (nodeMark[nList[l]]) {
						oldReward[nList[l]]-= hyperToReward[e[j]];
					}
				}
				edgeMark[e[j]] = true;
			}
		}
//		INFO("Collected ",seedSet.size()," optimal seeds");
		return 1.0*influence / hyperToNodes.size();
	}

	double estimate_influence(std::vector<nid_t> S) {
		ASSERT(nodesToHyper.size() > 0);
		std::unordered_set<nid_t> covered;
		for (auto it = S.begin(); it != S.end(); it++)
		{
			ASSERT(*it < nodesToHyper.size());
			for (auto it2 = nodesToHyper[*it].begin(); it2 != nodesToHyper[*it].end(); it2++) {
				covered.insert(*it2);
			}
		}
		// INFO(covered.size(),hyperToNodes.size(),this->n);
		return (1.0*covered.size() / hyperToNodes.size()) * this->n;
	}


	std::unordered_set<int> covered_inc;
	double cached_influence = 0;
	double estimate_influence_inc(nid_t new_node, bool accept_change) {
		ASSERT(new_node < nodesToHyper.size());
		ASSERT(hyperToNodes.size() > 0);
		int uncovered = 0;
		int covered_size = covered_inc.size();
		for (auto it2 = nodesToHyper[new_node].begin(); it2 != nodesToHyper[new_node].end(); it2++) {
			if (covered_inc.count(*it2) == 0) {
				uncovered++;
			}
			if (accept_change) covered_inc.insert(*it2);
		}
		double tmp_cached_influence = (1.0*(covered_size + uncovered) / hyperToNodes.size()) * this->n;
		if (accept_change) {
			cached_influence = tmp_cached_influence;
		}
		return tmp_cached_influence;
	}
	double estimate_influence_inc() {
		return cached_influence;
	}
	void reset_covered() {
		covered_inc.clear();
		cached_influence = 0;
	}

	double get_reward(vector<vector<nid_t>>& active_edges, long source) {
		if (reward_type == UNIT) {
			return 1;
		}
		if (reward_type == LAPLACIAN) {
			return get_xi(active_edges, source);
		}
		assert(false);
	}

	double get_xi(vector<vector<nid_t>>& active_edges, long source) { // mean laplacian eigenvalue
		assert(active_edges.size() == n);

		//build index
//		std::unordered_map<long, long> node_index;
//		long ii = 0;
//		for (long j = 0; j < active_edges.size(); j++) {
//			if (j == source) continue;
//			if (active_edges[j].size() > 0) {
//				node_index[j] = ii;
//				ii += 1;
//			}
//		}
//
//		if (node_index.size() == 0) {
//			// matrix of one element
////			INFO("Matrix of only source");
//			return 0;
//		}

		//todo where inverse matrix where not inverse

		std::vector<Eigen::Triplet<double>> tripletList;
//		long new_n = node_index.size();
		tripletList.reserve(5*n);
		for(long i = 0; i < n; i++)
		{
		    if (i == source) continue;
			for (const auto& t : active_edges[i]) {
			    if (t == source) continue;

				tripletList.push_back(Eigen::Triplet<double>(i,t,-1));
			}
			tripletList.push_back(Eigen::Triplet<double>(i,i,active_edges[i].size()));
		}

		Eigen::SparseMatrix<double> sm1(n,n);
		sm1.setFromTriplets(tripletList.begin(), tripletList.end());

//		Eigen::VectorXd x(n), b(n);
		Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
		// fill A and b;
		// Compute the ordering permutation vector from the structural pattern of A
		solver.analyzePattern(sm1);
		// Compute the numerical factorization
		solver.factorize(sm1);
		if (solver.info()!=Eigen::Success) {
			return 0; //singular matrix
//			cout << solver.lastErrorMessage() << endl;
//			assert(false);
		}
		//Use the factors to solve the linear system
		double det = log(solver.determinant());
//		cout << "det " << det << endl;
// the rest in case we need only largest eigenvalues
//		// Construct matrix operation object using the wrapper class SparseGenMatProd
//		Spectra::SparseGenMatProd<double> op(sm1);
//
//		// Construct eigen solver object, requesting the largest three eigenvalues
//        Spectra::GenEigsSolver< double, Spectra::LARGEST_MAGN, Spectra::SparseGenMatProd<double> > eigs(&op, n-2, n);

        // assume n-1 -th eigenvalue is too small. the library supports only up to n-2 eigenvalues. n is convergence rate

		// Initialize and compute
//		eigs.init();
//		int nconv = eigs.compute();

		// Retrieve results
//		Eigen::VectorXcd evalues;
//		if(eigs.info() == Spectra::SUCCESSFUL) {
//            evalues = eigs.eigenvalues();
//		} else {
//		    assert(false);
//		}
//
//		std::cout << "Eigenvalues found:\n" << evalues << std::endl;

		return det;
	}

	long BuildHypergraphNodeLT(nid_t uStart, nid_t hyperiiid, long* W = nullptr) {
		ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
		hyperToNodes[hyperiiid].push_back(uStart);

		nid_t n_visit_mark = 0;
		long n_visit_edge = 1;
		
		q.clear();
		q.push_back(uStart);
		visit_mark[n_visit_mark++] = uStart;
		long visited = 1;
		visit[uStart] = true;
		vector<vector<nid_t>> active_used_edges(n);
		vector<bool> nodes_with_edges_checked(n, false);
		std::unordered_set<std::pair<nid_t, nid_t>, pair_hash> active_edges;

		long active_edge_count = 0;
		while (!q.empty())
		{
			// pick the node
			nid_t expand = q.front();
			q.pop_front();
			nid_t i = expand;

			// for all out-neighbors
			for (nid_t j = 0; j < (nid_t)gT[i].size(); j++)
			{
				// check all incoming edges of each out-neighbor that hasn't been checked
				// add non-traversed neighbors that have successful edge to the queue
				nid_t v = gT[i][j];
				if (single_adversary_blocked_nodes.count(v)) continue; // for dynamic imm

				if (!nodes_with_edges_checked[v]) {
					double sum_of_income_edges = std::accumulate(prob[v].begin(), prob[v].end(), 0);
					double randDouble = sfmt_genrand_real1(&sfmtSeed);
					if (randDouble <= sum_of_income_edges) {
						// choose one incoming edge, otherwise don't choose any
						std::discrete_distribution<nid_t> distribution(prob[v].begin(), prob[v].end());
						int inverse_incoming_edge_neigh = distribution(generator);
						active_edges.emplace(inverse_incoming_edge_neigh,v);
					} 
					nodes_with_edges_checked[v] = true;
				}

				n_visit_edge++;
				if (active_edges.count(std::make_pair(i,v)) == 0) {
					continue;
				}
				active_used_edges[i].push_back(j);
				active_edge_count++;

				if (visit[v])
					continue;
				if (!visit[v])
				{
					ASSERT(n_visit_mark < n);
					visit_mark[n_visit_mark++] = v;
					visit[v] = true;
					visited++;
				}
				q.push_back(v);
				ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
				hyperToNodes[hyperiiid].push_back(v);
			}
		}
		for (nid_t i = 0; i < n_visit_mark; i++)
			visit[visit_mark[i]] = false;


		hyperToReward[hyperiiid] = get_reward(active_used_edges, uStart);

		if (W != nullptr) (*W) += visited + active_edge_count;

		return n_visit_edge;
	}


	long BuildHypergraphNode(nid_t uStart, nid_t hyperiiid, bool is_lt, long* W = nullptr)
	{
		if (is_lt) {
			throw "Not implemented";
		}
		long n_visit_edge = 1;
		ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
		hyperToNodes[hyperiiid].push_back(uStart);

		nid_t n_visit_mark = 0;

		q.clear();
		q.push_back(uStart);
		ASSERT(n_visit_mark < n);
		visit_mark[n_visit_mark++] = uStart;
		long visited = 1;
		visit[uStart] = true;
		vector<vector<nid_t>> active_edges(n);
		long active_edge_count = 0;
		while (!q.empty())
		{
			nid_t expand = q.front();
			q.pop_front();
			nid_t i = expand;
			for (nid_t j = 0; j < (nid_t)gT[i].size(); j++)
			{
				//int u=expand;
				nid_t v = gT[i][j];
				if (single_adversary_blocked_nodes.count(v)) continue; // for dynamic imm
				n_visit_edge++;
				double randDouble = sfmt_genrand_real1(&sfmtSeed);
				if (randDouble > probT[i][j])
					continue;
				if (reward_type != UNIT) {
					active_edges[i].push_back(j);
				}
                active_edge_count++;
				if (visit[v])
					continue;
				if (!visit[v])
				{
					ASSERT(n_visit_mark < n);
					visit_mark[n_visit_mark++] = v;
					visit[v] = true;
					visited++;
				}
				q.push_back(v);
				ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
				hyperToNodes[hyperiiid].push_back(v);
			}
		}
		for (nid_t i = 0; i < n_visit_mark; i++)
			visit[visit_mark[i]] = false;


		hyperToReward[hyperiiid] = get_reward(active_edges, uStart);

		if (W != nullptr) (*W) += visited + active_edge_count;

		return n_visit_edge;
	}

	double get_expected_reward(std::vector<nid_t>& seeds) {
        double influence = 0;
        std::set<long> set_of_hyperedges;
	    for (const auto& seed : seeds) {
            for (const auto& hyperedge : nodesToHyper[seed]) {
                if (set_of_hyperedges.find(hyperedge) == set_of_hyperedges.end()) {
                    influence += hyperToReward[hyperedge];
                }
                set_of_hyperedges.insert(hyperedge);
            }
	    }
        return 1.0*influence / hyperToNodes.size() * n;
	}

	long BuildHypergraphNodeBlocked(nid_t uStart, nid_t hyperiiid, std::vector<std::vector<nid_t>>& blocked_nodes, bool is_lt)
	{

		long n_visit_edge = 1;
		ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
		hyperToNodes[hyperiiid].push_back(uStart);

		nid_t n_visit_mark = 0;

		q.clear();
		q.push_back(uStart);
		ASSERT(n_visit_mark < n);
		visit_mark[n_visit_mark++] = uStart;
		visit[uStart] = true;
		std::vector<nid_t> worst_candidate_nodes;
		nid_t worst_nodes = INT_MAX;
		for (auto& a : blocked_nodes)
		{
			std::vector<nid_t> candidate_nodes;

			if (a[uStart] < 0) {
				q.clear(); // initial node is blocked
			}
			while (!q.empty())
			{
				nid_t expand = q.front();
				q.pop_front();
				nid_t i = expand;

				if (is_lt) {
					double sum_of_income_edges = std::accumulate(probT[i].begin(), prob[i].end(), 0);
					double randDouble = sfmt_genrand_real1(&sfmtSeed);
					if (randDouble <= sum_of_income_edges) {
						// choose one incoming edge, otherwise don't choose any
						std::discrete_distribution<nid_t> distribution(probT[i].begin(), probT[i].end());
						int j = distribution(generator);
						int v = gT[i][j];

						if (a[v] < 0) {
							continue;
						}
						n_visit_edge++;
						double randDouble = sfmt_genrand_real1(&sfmtSeed);
						if (randDouble > probT[i][j])
							continue;
						if (visit[v])
							continue;
						if (!visit[v])
						{
							ASSERT(n_visit_mark < n);
							visit_mark[n_visit_mark++] = v;
							visit[v] = true;
						}
						q.push_back(v);
						candidate_nodes.push_back(v);
					} 
				} else {
					for (nid_t j = 0; j < (nid_t)gT[i].size(); j++)
					{
						//int u=expand;
						nid_t v = gT[i][j];
						if (a[v] < 0) {
							continue;
						}
						n_visit_edge++;
						double randDouble = sfmt_genrand_real1(&sfmtSeed);
						if (randDouble > probT[i][j])
							continue;
						if (visit[v])
							continue;
						if (!visit[v])
						{
							ASSERT(n_visit_mark < n);
							visit_mark[n_visit_mark++] = v;
							visit[v] = true;
						}
						q.push_back(v);
						candidate_nodes.push_back(v);
					}
				}
				
			}
			for (nid_t i = 0; i < n_visit_mark; i++)
				visit[visit_mark[i]] = false;
			if (worst_nodes > candidate_nodes.size()) {
				worst_candidate_nodes = candidate_nodes;
				worst_nodes = candidate_nodes.size();
				INFO("Updated worst: ", worst_nodes);
			}
		}
		INFO("Added edge with nodes: ", worst_candidate_nodes.size());
		ASSERT(worst_nodes == worst_candidate_nodes.size());
		ASSERT((nid_t)hyperToNodes.size() > hyperiiid);
		hyperToNodes[hyperiiid] = worst_candidate_nodes;


		hyperToReward[hyperiiid] = 1;

		return n_visit_edge;
	}

	inline long get_hyper_size() { return hyperToNodes.size(); }

	void collect_bruteforced_stats(std::vector<nid_t>& seeds, long iterations) {
		/*
		 * Sampling G and collecting statistics on the infected graph
		 */
        Timer t(141234, "Collecting bruteforced stats", true);
        INFO("Collecting bruteforced stats");
		std::vector<double> stats;
		for (long it = 0; it < iterations; it++) {
            std::vector<std::vector<nid_t>> active_edges(n);

            for (nid_t i = 0; i < n; i++) {
                for (nid_t j = 0; j < gT[i].size(); j++) {
                    double randDouble = sfmt_genrand_real1(&sfmtSeed);
                    if (randDouble > probT[i][j])
                        continue;
                    active_edges[i].push_back(gT[i][j]);
                }
            }

            double xi = get_xi(active_edges, sfmt_genrand_uint32(&sfmtSeed) % n);
            stats.push_back(xi);
            INFO(it, xi);
		}
        logging["bf_xi_size"] = stats.size();
        logging["bf_xi_min"] = *std::min_element(stats.begin(), stats.end());
        logging["bf_xi_max"] = *std::max_element(stats.begin(), stats.end());
        logging["bf_xi_mean"] = vector_mean(stats);
        logging["bf_xi_median"] = vector_median(stats);
	}

};
