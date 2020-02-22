//
// Created by Alvis Logins on 2019-06-13.
//

#ifndef SATGREEDY_REACHABILITYLABELS_H
#define SATGREEDY_REACHABILITYLABELS_H

#include <bitset>
#include <unordered_set>
#include <unordered_map>
#include <forward_list>
#include <exception>
#include <iostream>
#include <cassert>

class ReachabilityLabels;

class BitIndex {
public:
    int used_ids = 0;
//    std::unordered_map<int, int>* node_id_to_bit_id_index; -- deprecated

    std::vector<int> positive_node_id_to_bit_id;
    std::vector<int> negative_node_id_to_bit_id;

    inline int get_positive_node_id(int node_id) {
        if (node_id >= positive_node_id_to_bit_id.size()) {
            positive_node_id_to_bit_id.resize(node_id*2+1, -1);
        }
        if (positive_node_id_to_bit_id[node_id] < 0) {
            positive_node_id_to_bit_id[node_id] = used_ids++;
        }
        return positive_node_id_to_bit_id[node_id];
    }

    inline int get_negative_node_id(int node_id) {
        int neg_id = - node_id;
        if (neg_id >= negative_node_id_to_bit_id.size()) {
            negative_node_id_to_bit_id.resize((neg_id)*2, -1);
        }
        if (negative_node_id_to_bit_id[neg_id] < 0) {
            negative_node_id_to_bit_id[neg_id] = used_ids++;
        }
        return negative_node_id_to_bit_id[neg_id];
    }

    inline int get_bit_id(int node_id) {
        return (node_id < 0) ? get_negative_node_id(node_id) : get_positive_node_id(node_id);
    }

    void clear() {
        used_ids = 0;
        positive_node_id_to_bit_id.clear();
        negative_node_id_to_bit_id.clear();
    }
};

class ReachabilityLabels {
public:
    static const int MAX_GRAPH_SIZE = 100000;
    std::bitset<MAX_GRAPH_SIZE>* labels_bits = nullptr;
    std::unordered_set<int>* labels = nullptr;
    bool using_bits;
    bool flipped = false;

    BitIndex* bit_index = nullptr;

    ReachabilityLabels(bool _using_bits, BitIndex* node_id_to_bit_id) : bit_index(node_id_to_bit_id) {
        using_bits = _using_bits;
        if (using_bits) {
            labels_bits = new std::bitset<MAX_GRAPH_SIZE>();
        } else {
            labels = new std::unordered_set<int>();
        }
    }

    ~ReachabilityLabels() {
        delete labels_bits;
        delete labels;
    }

    void set(int node_id) {
        if (using_bits) {
	    auto bit_id = bit_index->get_bit_id(node_id);
            labels_bits->set(bit_id);
        } else {
            if (flipped) {
                labels->erase(node_id);
            } else {
                labels->insert(node_id);
            }
        }
    }

    void unset(int node_id) {
        if (using_bits) {
            labels_bits->reset(bit_index->get_bit_id(node_id));
        } else {
            if (flipped) {
                labels->insert(node_id);
            } else {
                labels->erase(node_id);
            }
        }
    }

    void reset() {
        if (using_bits) {
            labels_bits->reset();
        } else {
            labels->clear();
        }
    }

    void union_with(ReachabilityLabels& target) {
        if (using_bits) {
            *labels_bits |= *target.labels_bits;
        } else {
            for (auto el : (*target.labels)) {
                set(el);
            }
        }
    }

    void substract(ReachabilityLabels& target) {
        if (using_bits) {
            std::bitset<MAX_GRAPH_SIZE> labels_bits_t = *target.labels_bits;
            labels_bits_t.flip();
            *labels_bits &= labels_bits_t;
        } else {
            std::forward_list<int> to_erase;
            for (auto& el : (*labels)) {
                if (target.labels->count(el)) {
                    to_erase.push_front(el);
                }
            }
            for (auto& el : to_erase) labels->erase(el);
        }
    }

    void mask(ReachabilityLabels& target) {
        if (using_bits) {
            *labels_bits &= *target.labels_bits;
        } else {
            std::forward_list<int> labels_to_remove;
            for (auto e1 : *labels) {
                assert(target.flipped);
                if (target.labels->count(e1)) {
                    labels_to_remove.push_front(e1);
                }
            }

            for (auto e : labels_to_remove) {
                labels->erase(e);
            }
        }
    }

    int size() {
        if (using_bits) {
            return (*labels_bits).count();
        } else {
            return (*labels).size();
        }
    }

    ReachabilityLabels& operator=(const ReachabilityLabels& other)
    {
        if (this != &other) {
            if (using_bits) {
                labels_bits = new std::bitset<MAX_GRAPH_SIZE>();
                *labels_bits = *other.labels_bits;
            } else {
                labels = new std::unordered_set<int>();
                *labels = *other.labels;
            }
        }
        return *this;
    }

    bool operator==(const ReachabilityLabels& other)
    {
        return *this == other;
    }

    int get_nodesize_sum(dagger::DAG* dag, std::unordered_map<int, int >* existing_scc = nullptr, std::unordered_set<int>* existing_singles = nullptr) {
        int size = 0;
        if (using_bits) {
            assert(existing_scc != nullptr && existing_singles != nullptr);

            int total_calls = 0;

            //check only nodes that exist in dag
            for (auto& node : *existing_scc) {
                auto v = node.first;
                total_calls++;
                if (labels_bits->test(bit_index->get_negative_node_id(v))) {
                    size += dag->getNodeSize(v);
                }
            }

            for (auto& v : *existing_singles) {
                total_calls++;
                if (labels_bits->test(bit_index->get_positive_node_id(v))) {
                    size += dag->getNodeSize(v);
                }
            }
        } else {
            for (auto it = labels->begin(); it != labels->end(); it++) {
                size += dag->getNodeSize(*it);
            }
        }
        return size;
    }

    inline void flip() {
        flipped = true;
        if (using_bits) {
            labels_bits->flip();
        }
    }
};

#endif //SATGREEDY_REACHABILITYLABELS_H
