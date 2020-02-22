//
// Created by Alvis Logins on 2019-07-04.
//

#ifndef SATGREEDY_SOLVER_H
#define SATGREEDY_SOLVER_H

#include "json.h"
#include "head.h"
#include "argument.h"

class Solver {
public:
    json logging;
    Argument arg;
    Graph g;

    Solver(Argument arg) : arg(arg) {
        ASSERT(arg.s > 0);
        ASSERT(arg.gamma > 0 && arg.gamma < 1);
        ASSERT(arg.epsilon > 0 && arg.epsilon < 1);

        logging["gamma"] = arg.gamma;
        fs::path o = arg.dataset;
        g = Graph(arg.dataset, "graph.csv");
        logging["graph_id"] = g.get_graph_id();
    }
    ~Solver() = default;

    void save_log() {
        ofstream logf;
        std::string fn = arg.log;
        logf.open(fn);
        logf << logging.dump(4);
        logf.close();
    }
};

#endif //SATGREEDY_SOLVER_H
