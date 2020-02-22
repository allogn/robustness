#ifndef __ARGUMENT_H__
#define __ARGUMENT_H__
#include <string>
#include <vector>

class Argument{
public:
    int s = 1;
    std::string dataset;
    double epsilon = 0.9;
    double beta = 1;
    double dim_beta = 32;
    std::string model = "IC";
    std::string log = "";
    double gamma = 0.9;
    std::vector<std::string> adversaries;
    bool original_objective = 0;
    bool strict_s = 1;
    int number_of_blocked_nodes = -1;
    int mc_iterations = 100;
    int rr_iterations = 10000;
    int reward_type = 0;
    double blocked_nodes_sampling_coef = 1;
};

#endif
