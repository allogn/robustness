#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/timer/timer.hpp>

#include "rob.h"

namespace po = boost::program_options;
using namespace boost::timer;

int main(int argn, char **argv) {
    std::string dirname;
    std::string adv_file;
    std::string outputf;
    long iterations;
    int mode;
    int number_of_blocked_nodes;
    int seeds;
    double sampling;
    int lt_model;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "produce help message")
            ("dataset,d", po::value<std::string>(&dirname)->required(), "Graph dataset directory")
            ("iterations,i", po::value<long>(&iterations)->default_value(100), "Iterations of Monte-Carlo")
            ("output,o", po::value<std::string>(&outputf)->required(), "Output json file")
            ("seeds,s", po::value<int>(&seeds)->default_value(1), "Seeds")
            ("sampling,c", po::value<double>(&sampling)->default_value(1), "Sample number of blocked nodes")
            ("lt,t", po::value<int>(&lt_model)->default_value(0), "0 - IC model, 1 - LT model")
            ("blocked,l", po::value<int>(&number_of_blocked_nodes)->default_value(-1), "Number of blocked nodes")
            ("mode,m", po::value<int>(&mode)->default_value(3), "0 - Largest Component, 1 - MaxTree, 2 - FastMaxTree, 3 - DownUpTree, 4 - DynTree, 5 - DAGGER, 6 - DAGGER Bit")
            ("adversary,a", po::value<std::string>(&adv_file)->required(), "File with sorted list of adversaries");

    po::variables_map vm;
    po::store(po::parse_command_line(argn, argv, desc), vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }
    po::notify(vm);

    auto_cpu_timer timer; // prints times on destruction
    auto rob = Rob((Rob::Mode)(mode), dirname, adv_file, lt_model == 1);
    rob.run(iterations, number_of_blocked_nodes, seeds, sampling);
    rob.save_log(outputf);
    return 0;
}
