//
// Created by Alvis Logins on 2019-06-04.
//

#ifndef SATGREEDY_DAG_H
#define SATGREEDY_DAG_H

#include "Graph.h"

namespace dagger {
    class DAG : public Graph {
    public:
        DAG() {};
        virtual int getNodeSize(int index) = 0;
        virtual int getSccId(int node_id) = 0;

        virtual int getSccSize(int node_id) {
            int scc_id = getSccId(node_id);
            return getNodeSize(scc_id);
        }

        virtual int get_active_node_size() {
            int sum = 0;
            for (auto& v: vertices) {
                sum += getNodeSize(v.first);
            }
            return sum;
        };
    };
}


#endif //SATGREEDY_DAG_H
