//
// Created by Alvis Logins on 2019-06-03.
//

#ifndef SATGREEDY_MYDAG_H
#define SATGREEDY_MYDAG_H

#include <unordered_map>
#include <forward_list>
#include <boost/timer/timer.hpp>
#include <iostream>

#include "head.h"
#include "DGRAIL/Graph.h"

using namespace boost::timer;

class MyDAG : public dagger::DAG {
    std::unordered_map<int, std::forward_list<int>>* out_graph_edges;
    std::unordered_set<int> active_nodes; // nodes of underlying graph
    int new_scc_id = 0; // autoincrement counter for new scc
public:
    std::unordered_map<int, int> node_to_scc;
    std::unordered_map<int, std::unordered_set<int>> scc_to_nodes;

    double time_for_building_scc = 0;
    double time_for_building_edges = 0;

    MyDAG() {}

    void build() {
        calculate_scc(&active_nodes);
        build_scc_edges(&active_nodes);
    }

    inline void set_out_graph_edges_and_active_nodes(std::unordered_map<int, std::forward_list<int>>* _out_graph_edges) {
        out_graph_edges = _out_graph_edges;
        for (auto& v : *out_graph_edges) {
            active_nodes.insert(v.first);
            for (auto u : v.second) active_nodes.insert(u);
        }
    }

    inline int getNodeSize(int index) override {
        return scc_to_nodes[index].size();
    };

    int getSccId(int node_id) override {
        return node_to_scc[node_id];
    }

    std::unordered_set<int> calculate_scc(std::unordered_set<int>* node_subset) { // returns set because it is needed in updates
        /**
         * (Creating condensed subgraph)
         *
         * This method also used to update a list of SCC, so it should assume components might exist.
         *
         * This does not manage edges between SCC and never traverses edges between SCC
         */

        cpu_timer timer;
        std::unordered_map<int, int> oldest_ancestor;
        std::unordered_map<int, int> reaching_time;
        std::unordered_set<int> stack_member;
        std::stack<int> tarjan_stack;
        std::unordered_set<int> new_scc_ids;
        int indextime = 0;

        for (auto v : *node_subset) {
            if (reaching_time.count(v)) continue; // considered in earlier traversals
            auto s = calculate_scc_rec(v, indextime, oldest_ancestor, reaching_time, stack_member, tarjan_stack, node_subset);
            union_sets(s, new_scc_ids);
        }
        ASSERT(tarjan_stack.empty());

        cpu_times times = timer.elapsed();
        time_for_building_scc += times.user;

        return new_scc_ids;
    }

    std::unordered_set<int> calculate_scc_rec(nid_t v, int& indextime,
                           std::unordered_map<int, int>& oldest_ancestor,
                           std::unordered_map<int, int>& reaching_time,
                           std::unordered_set<int>& stack_member,
                           std::stack<int>& tarjan_stack,
                           std::unordered_set<int>* node_subset) {
        /**
         * Function creates SCC-nodes, but does not manage edges.
         * Also, if node_subset is a subset for existing SCC-node, then a new SCC-node will be created without
         * updating or removing the old one.
         */
        std::unordered_set<int> new_scc_ids;
        reaching_time[v] = indextime;
        oldest_ancestor[v] = indextime;
        indextime++;
        tarjan_stack.push(v);
        stack_member.insert(v);

        //add to stack all children

        for (auto u : (*out_graph_edges)[v]) {
            if (node_subset->count(u) == 0) continue;

            if (reaching_time.count(u) == 0) {
                auto s = calculate_scc_rec(u, indextime, oldest_ancestor, reaching_time, stack_member, tarjan_stack, node_subset);
                union_sets(s,new_scc_ids);
                oldest_ancestor[v] = (oldest_ancestor[v] < oldest_ancestor[u]) ? oldest_ancestor[v] : oldest_ancestor[u];
            } else if (stack_member.count(u) > 0) {
                oldest_ancestor[v] = (oldest_ancestor[v] < reaching_time[u]) ? oldest_ancestor[v] : reaching_time[u];
            }
        }

        //create new components
        if (oldest_ancestor[v] == reaching_time[v])
        {
            std::vector<int> empty_vector;
            insertNode(new_scc_id, empty_vector, empty_vector);
            new_scc_ids.insert(new_scc_id);
            std::unordered_set<int> p;
            scc_to_nodes[new_scc_id] = p;
            while (tarjan_stack.top() != v) {
                ASSERT(!tarjan_stack.empty());
                auto w = tarjan_stack.top();
                node_to_scc[w] = new_scc_id;
                stack_member.erase(w);
                tarjan_stack.pop();
                scc_to_nodes[new_scc_id].insert(w);
            };
            auto w = tarjan_stack.top();
            node_to_scc[w] = new_scc_id;
            stack_member.erase(w);
            tarjan_stack.pop();
            scc_to_nodes[new_scc_id].insert(w);
            new_scc_id++;
        }
        return new_scc_ids;
    }


    void build_scc_edges(std::unordered_set<int>* node_subset) {
        // build edge list and root list
        cpu_timer edge_build_timer;
        for (auto i : active_nodes) {
            int source = node_to_scc[i];
            ASSERT(source > -1);
            for (auto v : (*out_graph_edges)[i]) {
//                if (node_subset->count(v) == 0) continue;
                int target = node_to_scc[v];
                if (target == source) continue;
                ASSERT(target > -1);
                if (!edgeExists(source, target)) {
                    insertEdge(source, target);
                }
            }
        }
        time_for_building_edges += edge_build_timer.elapsed().user;
    }

    void update_scc_edges(int old_scc_id,
                          std::unordered_set<int>& new_scc_ids) {
        /**
         * Function creates edges for each new SCC node
         * It assumes that the old SCC node with connections still exists
         * 
         * old scc is not yet deleted
         */
        //update incoming edges
        for (auto in_e: vertices[old_scc_id].inEdges()) { // for all incoming scc nodes
            auto neighbor_scc_id = in_e.first;
            if (!new_scc_ids.empty() > 0) {
                for (auto u : scc_to_nodes[neighbor_scc_id]) { // for all nodes in incoming scc
                    for (auto v : (*out_graph_edges)[u]) { // check outgoing edges from nodes in incoming scc
                        if (node_to_scc.count(v) == 0) continue; // node was deleted, but we don't delete edges
                        int target_scc_id = node_to_scc[v];
                        if (new_scc_ids.count(target_scc_id) > 0) { // add new edges between incoming scc and new scc's

                            // here we don't care about active_nodes leading to blocked nodes, because
                            // in that case node_to_scc will lead to the old scc
                            if (!edgeExists(neighbor_scc_id, target_scc_id)) insertEdge(neighbor_scc_id, target_scc_id); // inhereted from Graph
                        }
                    }
                }
            }
        };
        //update outgoing edges
        for (auto new_scc_id : new_scc_ids) {
            for (auto u : scc_to_nodes[new_scc_id]) {
                for (auto v : (*out_graph_edges)[u]) {
                    // node should not be previously deleted. if it is not deleted, it must be in node_to_scc
                    if (node_to_scc.count(v) == 0) continue;

                    int target = node_to_scc[v];
                    // target can not belong to existing new_scc, because every time a node is blocked,
                    // a new_scc is created with only existing nodes linked
                    if (target == new_scc_id) continue;


                    ASSERT(scc_to_nodes.count(target) > 0); // scc_to_nodes and node_to_scc should be consistent
                    if (!edgeExists(new_scc_id, target)) insertEdge(new_scc_id, target);
                }
            }
        }
    }

    void check_scc_vs_nodes_consistency() {
        for (auto& scc : scc_to_nodes) {
            for (auto v : scc.second) {
                if (node_to_scc[v] != scc.first) {
                    throw std::runtime_error("Node-To-Scc and Scc-To-Node are not consistent.");
                }
            }
        }
        for (auto& v : node_to_scc) {
            if (scc_to_nodes[v.second].count(v.first) == 0) {
                throw std::runtime_error("Node-To-Scc and Scc-To-Node are not consistent.");
            }
        }
    }

    std::unordered_set<int> removeNode(int node_to_remove) {
        // this is supported only my MyDAG and not DAG, because for another DAG it is not DAG that removes nodes,
        // but the index GRAIL
        active_nodes.erase(node_to_remove);

        auto scc_id = node_to_scc[node_to_remove];

        //two cases: if there is only one node - remove it and cluster. if many nodes in cluster - split it and/or move to new cluster
        std::unordered_set<int> new_scc_ids;
        if (scc_to_nodes[scc_id].size() > 1) {
            scc_to_nodes[scc_id].erase(node_to_remove);
            new_scc_ids = calculate_scc(&scc_to_nodes[scc_id]); //creates new scc nodes, even if removal doesn't break it;
        }
        scc_to_nodes.erase(scc_id); //such scc does not exist anymore. Important to place here, as labels already rely on this and edges too
        node_to_scc.erase(node_to_remove);
        update_scc_edges(scc_id, new_scc_ids);
        deleteNode(scc_id);
        return new_scc_ids;
    }

    int get_size_of_scc(int node) {
        if ((active_nodes.count(node) == 0) || (node_to_scc.count(node) == 0)) return 0;
        return scc_to_nodes[node_to_scc[node]].size();
    }

    int get_active_node_size() override {
        return active_nodes.size();
    }



    bool isCyclicUtil(int v, std::unordered_set<int>& visited, std::unordered_set<int>& recStack)
    {
        if(!visited.count(v))
        {
            // Mark the current node as visited and part of recursion stack
            visited.insert(v);
            recStack.insert(v);

            // Recur for all the vertices adjacent to this vertex
            for(auto& e : vertices[v].outEdges())
            {
                auto u = e.first;
                if ( !visited.count(u) && isCyclicUtil(u, visited, recStack) )
                    return true;
                else if (recStack.count(u))
                    return true;
            }

        }
        recStack.erase(v);  // remove the vertex from recursion stack
        return false;
    }

    bool isDagCyclic()
    {
        std::unordered_set<int> visited;
        std::unordered_set<int> recStack;

        // Call the recursive helper function to detect cycle in different
        // DFS trees
        for(auto& v : vertices)
            if (isCyclicUtil(v.first, visited, recStack)) {

                //print all the nodes
                std::cout << std::endl;
                for (auto v : recStack) {
                    std::cout << v << ",";
//                    for (auto u : scc_to_nodes[v]) {
//                        std::cout << u << ",";
//                    }
                }
                std::cout << std::endl;


                return true;
            }

        return false;
    }
};

#endif //SATGREEDY_MYDAG_H
