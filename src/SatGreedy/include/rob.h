#ifndef SATGREEDY_ROB_H
#define SATGREEDY_ROB_H

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <exception>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <boost/timer/timer.hpp>
#include <math.h>
#include <queue>

#include "graph.h"
#include "json.h"
#include "head.h"
#include "nheap.h"
#include "DGRAIL/Grail.h"
#include "MyDAG.h"
#include "ReachabilityLabels.h"

//#define DEBUG
//#define PROFILE

using json = nlohmann::json;
using namespace boost::timer;

class Rob {
    json logging;
    double sampling_of_blocked_nodes;
    std::vector<nid_t> node_sequence; // sequence of all nodes in which to block them
    sfmt_t sfmtSeed;
    std::unordered_map<int, std::forward_list<nid_t>> active_edges;
    std::unordered_map<int, std::forward_list<nid_t>> reverse_active_edges; // used only for Grail
    int maxtree_dagger_addition; // we don't need heap for dagger, just substitute here if new tree appears. it will always be larger

    std::unordered_map<int, ReachabilityLabels> reachability_labels;
    BitIndex node_id_to_bit_id_index;//for bitset operations we need only positive dag node ids

    std::unordered_set<nid_t> blocked_nodes;
    fHeap<int, int> maxtree_roots;

    // time performance
    double time_for_traversing_dag = 0;

    void read_node_sequence(std::string adversary_file) {
        logging["adversary file"] = adversary_file;
        std::ifstream i(adversary_file);
        json j;
        i >> j;
        node_sequence = j["Blocked nodes"].get<std::vector<nid_t>>();

        std::unordered_set<int> check_unique(node_sequence.begin(), node_sequence.end());
        if (check_unique.size() != node_sequence.size()) {
            throw std::runtime_error("Adversary contains duplicates");
        }
    }

    long get_largest_component(nid_t nodes_to_block) {
        // should be implemented only for undirected network
        throw std::runtime_error("Not implemented");
    }

    nid_t get_largest_tree_by_dfs(nid_t nodes_to_block) {
        /**
         * This function calculates the number of descendants for each node, by BFS
         *
         * Complexity is O(V*E)
         * @input The number of nodes from node_sequence to ignore in the network
         * @return the largest number of descendants
         */

        std::vector<bool> visited_tmpl(g.n, false);
        for (nid_t i = 0; i < nodes_to_block; i++) {
            visited_tmpl[node_sequence[i]] = true;
        }

        nid_t largest_tree = 0;
        for (nid_t i = 0; i < g.n; i++) {
            if (visited_tmpl[i]) continue;

            auto visited = visited_tmpl;
            std::stack<nid_t> st;
            st.push(i);
            visited[i] = true;
            nid_t desc = 0;
            while (st.size() > 0) {
                auto node = st.top();
                desc++;
                st.pop();
                for (const auto& v : active_edges[node]) {
                    if (!visited[v]) {
                        st.push(v);
                        visited[v] = true;
                    }
                }
            }
            largest_tree = std::max(largest_tree, desc);
        }

        return largest_tree;
    }

    void build_active_edges() {
        active_edges.clear();
        reverse_active_edges.clear();
        for (nid_t i = 0; i < g.n; i++) {
            if (blocked_nodes.find(i) != blocked_nodes.end()) continue;
            std::forward_list<nid_t> empty_list;
            active_edges[i] = empty_list; // touch. important because mydag builds active_nodes based on existance in active_edges
            for (nid_t j = 0; j < g.g[i].size(); j++) {
                if (blocked_nodes.find(g.g[i][j]) != blocked_nodes.end()) continue;
                double randDouble = sfmt_genrand_real1(&sfmtSeed);
                if (randDouble <= g.prob[i][j]) {
                    active_edges[i].push_front(g.g[i][j]);
                    if (mode == DYN_GRAIL_MAX_TREE || mode == DYN_GRAIL_MAX_TREE_BIT) {
                        reverse_active_edges[g.g[i][j]].push_front(i);
                    }
                }
            }
        }
    }

    unsigned int find_updown_maxtree_for_node(int node) {
        unsigned int max_size = 0;
        std::stack<int> dfs;
        std::set<int> visited;
        dfs.push(node);
        visited.insert(node);
        unsigned int size = 0;
        while (dfs.size() > 0) {
            auto scc_id = dfs.top();
            dfs.pop();
            size += dag->getNodeSize(scc_id);
            for (auto& out_edge : dag->vertices[scc_id].outEdges()) {
                auto out_scc_id = out_edge.first;
                if (visited.count(out_scc_id) == 0) {
                    dfs.push(out_scc_id);
                    visited.insert(out_scc_id);
                }
            }
        }
        max_size = (max_size > size) ? max_size : size;
        return max_size;
    }


    unsigned int find_updown_maxtree() {
        unsigned int max_size = 0;
        for (auto& v : dag->vertices) {
            if (v.second.inDegree() == 0) {
                auto size = find_updown_maxtree_for_node(v.first);
                max_size = (max_size > size) ? max_size : size;
            }
        }
        return max_size;
    }

    unsigned int find_updown_maxtree_dagger() {
        unsigned int max_size = 0;
        for (auto& v : dagger_graph->vertices) {
            auto root_scc_id = dag->getSccId(v.first);
            dagger::EdgeList tmp;
            if (dagger_index->dag.getInEdges(root_scc_id, tmp).empty()) {
                std::stack<int> dfs;
                std::set<int> visited;
                dfs.push(root_scc_id);
                visited.insert(root_scc_id);
                unsigned int size = 0;
                while (dfs.size() > 0) {
                    auto scc_id = dfs.top();
                    dfs.pop();
                    size += dag->getNodeSize(scc_id);
                    dagger::EdgeList tmp2;
                    for (auto& out_edge : dagger_index->dag.getOutEdges(scc_id, tmp2)) {
                        auto out_scc_id = out_edge.first;
                        if (visited.count(out_scc_id) == 0) {
                            dfs.push(out_scc_id);
                            visited.insert(out_scc_id);
                        }
                    }
                }
                max_size = (max_size > size) ? max_size : size;
            }
        }
        return max_size;
    }

    unsigned int find_downup_maxtree() {
        /*
         * not for dagger. for dagger there is a problem ("optimization") that dag does not contain single-vertex nodes
         */
//        ASSERT(!mydag_index->isDagCyclic());

        unsigned int max_size = 0;
        std::queue<int> bfs;
        std::unordered_map<int,int> scc_degrees;
        int non_zero = 0;
        std::unordered_set<int> visited;
        reachability_labels.clear();
        node_id_to_bit_id_index.clear();
        for (auto& v : dag->vertices) {
            scc_degrees[v.first] = v.second.outDegree();
            if (scc_degrees[v.first] > 0) non_zero++;

            reachability_labels.emplace(std::piecewise_construct, std::make_tuple(v.first),
                                        std::make_tuple(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index));
            reachability_labels.at(v.first).set(v.first);

            if (v.second.outDegree() == 0) {
                bfs.push(v.first);
                visited.insert(v.first);
            }
        }
        int total_nodes = dag->vertices.size();


        // do bfs traversal (like in iterations)
        while (!bfs.empty()) {
            // propagate from each node in front upwards
            // if the parent node has information from all descendants - put it in the next front
            auto scc_v_id = bfs.front();
            auto& scc_v = dag->vertices[scc_v_id];
            bfs.pop();

            if (scc_v.inDegree() == 0) { // we have reached a root
                int size = get_maxtree_size(reachability_labels.at(scc_v_id));
                max_size = (max_size > size) ? max_size : size;

                if (mode == DYN_BOTUP_MAX_TREE) {
                    if (!maxtree_roots.isExisted(scc_v_id)) {
                        maxtree_roots.enqueue(scc_v_id, size);
                    }
                }
            }

            for (auto& inEdge : scc_v.inEdges()) {
                auto scc_u = inEdge.first;
                ASSERT(scc_degrees[scc_u] > 0);
                scc_degrees[scc_u]--;
                if (scc_degrees[scc_u] == 0) {
                    ASSERT(!visited.count(scc_u));
                    bfs.push(scc_u);
                    visited.insert(scc_u);
                }

                reachability_labels.at(scc_u).union_with(reachability_labels.at(scc_v_id));
            }
        }

        return max_size;
    }

    int get_maxtree() {
        int top_scc_id, top_maxtree_size;
        ASSERT(maxtree_roots.size() > 0);
        maxtree_roots.getTop(top_scc_id, top_maxtree_size);
        return top_maxtree_size;
    }

    void remove_mydag_node(int node_id) {
        int old_scc_id;
        std::forward_list<int> old_in_neighbors;
        std::forward_list<int> old_out_neighbors;
        old_scc_id = mydag_index->getSccId(node_id);
        for (auto e : mydag_index->vertices[old_scc_id].outEdges()) {
            old_out_neighbors.push_front(e.first);
        }
        for (auto e : mydag_index->vertices[old_scc_id].inEdges()) {
            old_in_neighbors.push_front(e.first);
        }


        std::unordered_set<int> new_scc_ids = mydag_index->removeNode(node_id);
        update_labels(old_scc_id, new_scc_ids, old_in_neighbors, old_out_neighbors);

        int maxtree_dyn = get_maxtree();
    }

    void update_labels(int old_scc_id,
            std::unordered_set<int>& new_scc_ids,
            std::forward_list<int>& old_in_neighbors,
            std::forward_list<int>& old_out_neighbors) {
        /**
         * This function should traverse given that some SCC nodes were deleted.
         * Node is removed iff the size of vertex array is zero
         *
         * traverse up while there are updates. if a node after looking to children have not been updated - do not traverse further
         * traverse up in dfs manner again
         *
         */
        std::stack<int> reverse_bfs; // reverse because we are collecting reachability bottom-up
        std::unordered_set<int> in_queue;

        // start traversing from all OLD in neighbors and new ssc's
        for (auto new_scc_id : new_scc_ids) {
            in_queue.insert(new_scc_id);
            reverse_bfs.push(new_scc_id);
        }
        for (auto in_scc_id : old_in_neighbors) {
            if (in_queue.count(in_scc_id) == 0) {
                in_queue.insert(in_scc_id);
                reverse_bfs.push(in_scc_id);
            }
        }
        while (!reverse_bfs.empty()) {
            auto scc_v = reverse_bfs.top();
            reverse_bfs.pop();
            in_queue.erase(scc_v);
            //check all children and check if label has changed

            //scc_v can be a newly appeared scc, so we may want to add new label
            int old_label_size;
            if (reachability_labels.count(scc_v)) {
                old_label_size = reachability_labels.at(scc_v).size();
            } else {
                reachability_labels.emplace(std::piecewise_construct, std::make_tuple(scc_v),
                        std::make_tuple(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index));
                old_label_size = 0;
            }

            ReachabilityLabels new_label(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index);
            new_label.set(scc_v);

            for (auto& e : dag->vertices[scc_v].outEdges()) {
                auto scc_u = e.first;
                if (reachability_labels.count(scc_u)) { // otherwise scc_u is also new but haven't been reached yet
                    new_label.union_with(reachability_labels.at(scc_u));
                }
            }
            if (old_label_size != new_label.size()) {
                reachability_labels.at(scc_v) = new_label;
                for (auto in_edge : dag->vertices[scc_v].inEdges()) {
                    auto scc_u = in_edge.first;
                    if (in_queue.count(scc_u) == 0) {
                        reverse_bfs.push(scc_u);
                        in_queue.insert(scc_u);
                    }
                }
                if (dag->vertices[scc_v].inDegree() == 0) {
                    maxtree_roots.updateorenqueue(scc_v, get_maxtree_size(reachability_labels.at(scc_v)));
                }
            }
        }

        // some nodes may additionally become roots, although their labels can not be updated
        // for example A->B->C; B deleted => C label updated, but A becomes root.
        // so we need to traverse out neighbors of the old scc
        for (auto out_scc : old_out_neighbors) {
            if (dag->vertices[out_scc].inDegree() == 0) {
                maxtree_roots.updateorenqueue(out_scc, get_maxtree_size(reachability_labels.at(out_scc)));
            }
        }

        if (maxtree_roots.isExisted(old_scc_id)) {
            maxtree_roots.remove(old_scc_id);
        }
    }


    void update_labels_dagger(int new_node_id, bool single_seed_mode = true) {
        std::stack<int> reverse_bfs; // reverse because we are collecting reachability bottom-up
        std::unordered_set<int> has_been_updated;
        int new_scc_id = dag->getSccId(new_node_id); // might be appended to existing scc with existing id
        // start traversing from all in neighbors and new ssc's
        has_been_updated.insert(new_scc_id);
        reverse_bfs.push(new_scc_id);
        rebuild_label(new_scc_id);
        ReachabilityLabels& label_to_propagate = reachability_labels.at(new_scc_id);

        // scc ids can be OLD, and might already exist!
        while (!reverse_bfs.empty()) {
            auto scc_v = reverse_bfs.top();
            reverse_bfs.pop();
            // in_queue.erase(scc_v); -- no need to update if already updated, we only propagate label_to_propagate once

            if (!reachability_labels.count(scc_v)) {
                rebuild_label(scc_v);
            }
            reachability_labels.at(scc_v).union_with(label_to_propagate);
            reachability_labels.at(scc_v).mask(*labels_to_maintain);
            // we must propagate even if reachability has not been changed, because root nodes must be reached if cluster sizes have been changed
            dagger::EdgeList tmp;
            for (auto in_edge : dagger_index->dag.getInEdges(scc_v, tmp)) {
                auto scc_u = in_edge.first;
                if (has_been_updated.count(scc_u) == 0) {
                    reverse_bfs.push(scc_u);
                    has_been_updated.insert(scc_u);
                }
            }
            if (single_seed_mode) {
                if (dagger_index->dag.getInEdges(scc_v, tmp).empty()) {
                    int new_size = get_maxtree_size(reachability_labels.at(scc_v));
                    maxtree_dagger_addition = (new_size > maxtree_dagger_addition) ? new_size : maxtree_dagger_addition;
                }
            }

        }

        //no additional roots or deletions in the heap
    }

    int collect_polytree_size(int number_of_seeds) {
        //collect a set of roots
        std::forward_list<int> roots;
        dagger::EdgeList tmp;
        std::priority_queue<tuple<int, int, int> > Q;

//        for (auto& node : dagger_index->dag.csize) { // existing scc
//            auto v = node.first;
//            if (dagger_index->dag.getInEdges(v, tmp).empty()) {
//                roots.push_front(v);
//            }
//        }
//
//        for (auto& v : dagger_index->dag.single_nodes) {
//            if (dagger_index->dag.getInEdges(v, tmp).empty()) {
//                roots.push_front(v);
//            }
//        }

        for (auto& v : dagger_index->dag.roots) {
            if (!reachability_labels.count(v)) {
                rebuild_label(v);
            }
            int new_size = get_maxtree_size(reachability_labels.at(v));
            Q.push(make_tuple(new_size, v, 0));
        }

        int polytree_size = 0;
        ReachabilityLabels total_covered(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index);
        for (int iter = 0; iter < number_of_seeds;) {
            if (Q.size() == 0) {
                return -1; // no roots means all nodes have been influenced (too many seeds)
            }
            auto e = Q.top();
            Q.pop();
            int gain = get < 0 > (e), v = get < 1 > (e), tick = get < 2 > (e);
            if (tick == iter) {
                polytree_size += gain;
                total_covered.union_with(reachability_labels.at(v));
                iter++;
            } else {
                reachability_labels.at(v).substract(total_covered);
                int new_size = get_maxtree_size(reachability_labels.at(v));
                Q.push(make_tuple(new_size, v, iter));
            }
        }
        return polytree_size;
    }

    void rebuild_label(int scc_v) {
        if (!reachability_labels.count(scc_v)) {
            reachability_labels.emplace(std::piecewise_construct, std::make_tuple(scc_v),
                                        std::make_tuple(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index));
        }
        reachability_labels.at(scc_v).set(scc_v);
        dagger::EdgeList tmp2;
        for (auto& e : dagger_index->dag.getOutEdges(scc_v, tmp2)) {
            auto scc_u = e.first;
            if (reachability_labels.count(scc_u)) { // otherwise scc_u is also new but haven't been reached yet
                reachability_labels.at(scc_v).union_with(reachability_labels.at(scc_u));
            }
        }
    }

    void check_maxtree_roots_consistency_mydag() {
        int total = 0;
        for (auto& scc : mydag_index->scc_to_nodes) {
            if (dag->vertices[scc.first].inDegree() == 0) {
                total++;
                if (!maxtree_roots.isExisted(scc.first)) {
                    throw std::runtime_error("Maxtree not consistent.");
                }
            }
        }
        if (total != maxtree_roots.size()) {
            throw std::runtime_error("Maxtree size not consistent.");
        }
    }

    inline int get_maxtree_size(ReachabilityLabels& reachability_set) {
        if (mode == DYN_GRAIL_MAX_TREE || mode == DYN_GRAIL_MAX_TREE_BIT)
        {
            // using this only with dagger
            // idea is that in reachability set we need to traverse through existing scc's (negative index), and scc's with a single node (positive index)
            // that should not be present in dag's sccmap
            return reachability_set.get_nodesize_sum(dag, &dagger_index->dag.csize, &dagger_index->dag.single_nodes);
        } else {
            return reachability_set.get_nodesize_sum(dag);
        }
    }

    void print_stack(std::stack<nid_t>& stack) {
        auto copy = stack;
        while (copy.size() > 0) {
            std::cout << copy.top() << " ";
            copy.pop();
        }
        std::cout << std::endl;
    }

    void reset_iteration() {
        maxtree_roots.sign = 1;
        maxtree_roots.reset();
        maxtree_dagger_addition = 0;
    }

    void reset() {
        reset_iteration();
        blocked_nodes.clear();
        s_values.clear();
        s_values.resize(g.n, 0);
        s_values_std.clear();
        s_values_std.resize(g.n, 0);
        number_of_scc.clear();
        number_of_scc.resize(g.n, 0);
    }

    void check_equality_with_orig() { // for dagger
        ASSERT(dagger_graph->vertices.size() == g.n);
        for (int i = 0; i < g.n; i++) {
            int scc_id = dag->getSccId(i);
            dagger::EdgeList tmp;
            dagger_index->dag.getOutEdges(i, tmp);
            auto v = dagger_graph->vertices[i];
            dagger::EdgeList tmp2 = v.outEdges();
            int total = 0;
            for (auto nid : active_edges[i]) {
                ASSERT(tmp.count(nid));
                ASSERT(tmp2.count(nid));
                total++;
            }
            ASSERT(total == tmp2.size());
            ASSERT(total == tmp.size());
        }
    }

public:
    Graph g;
    std::vector<double> s_values;
    std::vector<double> s_values_std;
    dagger::DAG* dag;
    MyDAG* mydag_index;
    dagger::Grail* dagger_index;
    dagger::Graph* dagger_graph; // index embedded as observable
    ReachabilityLabels* labels_to_maintain;

    //logging
    std::vector<int> number_of_scc;

    typedef enum {LARGEST_COMPONENT,
                  DFS_MAX_TREE,
                  TOPDOWN_MAX_TREE,
                  BOTUP_MAX_TREE,
                  DYN_BOTUP_MAX_TREE,
                  DYN_GRAIL_MAX_TREE,
                  DYN_GRAIL_MAX_TREE_BIT} Mode;
    Mode mode;

    Rob(Mode m, std::string graph_dir, std::string adversary_file) {
        mode = m;
        if (mode > 6 || mode < 0) {
            throw std::runtime_error("Mode is not implemented");
        }
        sfmt_init_gen_rand(&sfmtSeed , 95082);
        mydag_index = nullptr;
        dagger_graph = nullptr;
        dagger_index = nullptr;
        dag = nullptr;

        // adversary file should contain a sorted list of nodes to remove from the network
        g = Graph(graph_dir, "graph.csv");
        read_node_sequence(adversary_file);
        if ((g.n > ReachabilityLabels::MAX_GRAPH_SIZE) && (mode == 6)) {
            throw std::runtime_error("Increase MAX_GRAPH_SIZE, too large graph");
        }

        labels_to_maintain = new ReachabilityLabels(mode == DYN_GRAIL_MAX_TREE_BIT, &node_id_to_bit_id_index);
    }

    ~Rob() {
        delete mydag_index;
        delete dagger_graph;
        delete dagger_index;
        delete labels_to_maintain;
    }

    inline void update_blocked_nodes_set(long q) {
        blocked_nodes.clear();
        for (long i = 0; i < q; i++) {
            blocked_nodes.insert(node_sequence[i]);
        }
    }

    void insert_new_node(nid_t node_id) {

        std::vector<int> out_neighbors;
        std::vector<int> in_neighbors;

        for (auto v : active_edges[node_id]) {
            if (dagger_graph->nodeExists(v)) out_neighbors.push_back(v);
        }
        for (auto v : reverse_active_edges[node_id]) {
            if (dagger_graph->nodeExists(v)) in_neighbors.push_back(v);
        }

        dagger_graph->insertNode(node_id, in_neighbors, out_neighbors);
    }

    void save_log(std::string filename) {
        std::ofstream logf;
        logf.open(filename);
        logf << logging.dump(4);
        logf.close();
    }

    void rebuild_mydag() {
        delete mydag_index;
        mydag_index = new MyDAG();
        mydag_index->set_out_graph_edges_and_active_nodes(&active_edges);
        mydag_index->build();
        dag = (dagger::DAG*)mydag_index;
//        ASSERT(!mydag_index->isDagCyclic());
    }

    void rebuild_dagger() {
        delete dagger_graph;
        delete dagger_index;
        dagger_graph = new dagger::Graph();

        labels_to_maintain->reset();
        labels_to_maintain->flip();
        dagger_index = new dagger::Grail(*dagger_graph,1,0,10, *labels_to_maintain);
        dag = &(dagger_index->dag);
        dagger_graph->AddObserver((dagger::Grail &)*dagger_index);
    }


    double run(int iterations, int number_of_blocked_nodes = -1, int number_of_seeds = 1, double sampling_of_blocked_nodes = 1) {
        this->sampling_of_blocked_nodes = sampling_of_blocked_nodes;
        logging["alpha"] = sampling_of_blocked_nodes;

        if ((node_sequence.size() != g.n) && (number_of_blocked_nodes == -1)) {
                throw std::runtime_error("Advarsarial node list should contain all nodes of the network.");
        }
        if ((sampling_of_blocked_nodes > 1) && (number_of_blocked_nodes != -1)) {
            throw std::runtime_error("number_of_blocked_nodes and sampling_of_blocked_nodes can not be both true");
        }
        
        cpu_timer timer;
        reset();
        double time_for_node_insertion = 0;
        double time_for_label_updates = 0;
        std::vector<std::vector<double>> zero_blocked_treesize_sequence(5);
        for (int it = 0; it < iterations;) {
            reset_iteration();
            if (mode == LARGEST_COMPONENT || mode == DFS_MAX_TREE || mode == TOPDOWN_MAX_TREE || mode == BOTUP_MAX_TREE) {
                if (number_of_seeds > 1) {
                    throw std::runtime_error("Not implemented");
                }
                int low = 0;
                int high = g.n;
                if (number_of_blocked_nodes > -1) {
                    low = number_of_blocked_nodes;
                    high = number_of_blocked_nodes+1;
                }
                for (int q = low; q < high; q++) {
                    update_blocked_nodes_set(q);
                    build_active_edges();

                    switch (mode) {
                        case LARGEST_COMPONENT:
                            s_values[q] +=  get_largest_component(q) / (double) iterations;
                            break;
                        case DFS_MAX_TREE:
                            s_values[q] +=  get_largest_tree_by_dfs(q) / (double) iterations;
                            break;
                        case TOPDOWN_MAX_TREE:
                            rebuild_mydag();
                            s_values[q] +=  find_updown_maxtree() / (double) iterations;
                            break;
                        case BOTUP_MAX_TREE:
                            rebuild_mydag();
                            s_values[q] += find_downup_maxtree() / (double) iterations;
                            break;
                        default:
                            throw std::runtime_error("Impossible");
                    }

                }
                it++;
            } else {
                build_active_edges();
                if (mode == DYN_BOTUP_MAX_TREE) {
                    if (number_of_seeds > 1) {
                        throw std::runtime_error("Not implemented");
                    }
                    int max_number_of_blocked_nodes = (number_of_blocked_nodes > -1) ? number_of_blocked_nodes : g.n-1;
                    rebuild_mydag();
                    find_downup_maxtree();
                    s_values[0] += get_maxtree() / (double) iterations;
                    number_of_scc[0] = dag->vertices.size();
                    for (int i = 0; i < max_number_of_blocked_nodes; i++) {
                        remove_mydag_node(node_sequence[i]);
                        s_values[i+1] += get_maxtree() / (double) iterations;
                        number_of_scc[i+1] = dag->vertices.size();
                    }
                    it++;
                }
                if (mode == DYN_GRAIL_MAX_TREE || mode == DYN_GRAIL_MAX_TREE_BIT) {
                    try {
                        int t = (number_of_blocked_nodes > -1) ? number_of_blocked_nodes : 0;
                        rebuild_dagger();
                        reachability_labels.clear();
                        node_id_to_bit_id_index.clear();

                        std::vector<int> node_values_to_sample = blocked_nodes_values(sampling_of_blocked_nodes, g.n);
                        unsigned long visited_blocked_node_index = node_values_to_sample.size()-1;
                        s_values.resize(node_values_to_sample.size());
                        s_values_std.resize(node_values_to_sample.size());
                        logging["samples_blocked_values"] = node_values_to_sample;

                        for (int i = g.n-1; i >= t; i--) {
                            bool sample_this_value = false;
                            assert(visited_blocked_node_index >= 0);
                            if (i == node_values_to_sample[visited_blocked_node_index]) {
                                sample_this_value = true;
                                visited_blocked_node_index--;
                            }
                            cpu_timer timer1;
                            insert_new_node(node_sequence[i]);
                            time_for_node_insertion += timer1.elapsed().user;
                            //auto treesize = find_downup_maxtree_dagger(); //find_updown_maxtree_dagger() works
                            cpu_timer timer2;
                            int treesize;
                            if (number_of_seeds > 1) {
                                update_labels_dagger(node_sequence[i], false);

                                if (sample_this_value) {
                                    treesize = collect_polytree_size(number_of_seeds);
                                    if (treesize == -1) {
                                        treesize = dagger_graph->vsize;
                                    }
                                }
                            } else {
                                update_labels_dagger(node_sequence[i]);
                                treesize = maxtree_dagger_addition;
                            }
                            time_for_label_updates += timer2.elapsed().user;
                            if (sample_this_value) {
                                s_values[visited_blocked_node_index+1] += (double)treesize / (double) iterations;
                                s_values_std[visited_blocked_node_index+1] += (double)treesize*treesize;
                            }
                            number_of_scc[i] = dagger_index->dag.dagSize();
                        }
                        it++;
                    } catch (GrailFail& e) {
                        std::cout << "Grail failed (it " << it << ")" << std::endl;
                        // try another iteration
                    }
                }
            }

            for (int iii = 0; iii < 5; iii++) {
                zero_blocked_treesize_sequence[iii].push_back(s_values[iii*s_values.size()/4-1] * iterations);
            }
        }

        for (int k = zero_blocked_treesize_sequence[0].size()-1; k > 0; k--) {
            for (int iii = 0; iii < 5; iii++) {
                zero_blocked_treesize_sequence[iii][k] -= zero_blocked_treesize_sequence[iii][k-1];
            }
        }

        //calculate deviations
        if (mode == DYN_GRAIL_MAX_TREE || mode == DYN_GRAIL_MAX_TREE_BIT) {
            int t = (number_of_blocked_nodes > -1) ? number_of_blocked_nodes : 0;
            for (int i = g.n - 1; i >= t; i--) {
                double tt = s_values[i] * s_values[i];
                s_values_std[i] = sqrt(s_values_std[i] / (double) iterations - tt);
            }
        }

        double r, r_std;
        if (number_of_blocked_nodes > -1) {
            r = s_values[number_of_blocked_nodes];
            r_std = s_values_std[number_of_blocked_nodes] / (double) (iterations);
        } else {
            r = std::accumulate(s_values.begin(), s_values.end(), 0.0) / ((double)g.n);
            r_std = std::accumulate(s_values_std.begin(), s_values_std.end(), 0.0) / ((double)g.n);
        }
        logging["rob"] = r;
        logging["rob_std"] = r_std;
        logging["runtime"] = timer.elapsed().user;
        logging["sequence"] = s_values;
        logging["seeds"] = number_of_seeds;
        logging["dag traversal"] = time_for_traversing_dag;
        logging["mode"] = mode;
        logging["scc size"] = number_of_scc;
        logging["zero_blocked_treesize_sequence"] = zero_blocked_treesize_sequence;
        logging["time_for_label_updates"] = time_for_label_updates;
        logging["time_for_node_insertion"] = time_for_node_insertion;
        logging["number_of_blocked_nodes"] = number_of_blocked_nodes;
        logging.merge_patch(g.graph_params);
        return r;
    }

    int get_dagger_scc_num() {
        std::unordered_set<int> scc;
        for (auto& v : dagger_graph->vertices) {
            scc.insert(dag->getSccId(v.first));
        }
        return scc.size();
    }
};

#endif //SATGREEDY_ROB_H
