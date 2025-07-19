#include "parser.h"
#include "variable_elimination.h"

#include <chrono>

//https://www.bnlearn.com/bnrepository/
//http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm

bool isQueryVariableInNetwork(const NetworkAST& network, const std::string& queryVariableName) {
    for (const auto& variable : network.variables)
        if (variable.name == queryVariableName)
            return true;
    return false;
}

int main(int argc, char* argv[]) {
    std::string filename;
    std::string queryVariableName;

    if(argc < 3) {
        std::cout << "Usage: ./main <filename> <query_variable>\n";
        return 1;
    }

    filename = argv[1];
    queryVariableName = argv[2];

    std::ifstream input(filename);    

    if(!input.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 1;
    }

    Parser parser(input);

    auto start = std::chrono::steady_clock::now();

    NetworkAST parsed_network = parser.parse(); //Meglio stare attenti alla restituzione per valore e non per riferimento.

    auto finish = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = finish - start;

    std::cout<<"Parsing took: "<<duration.count()<< std::endl;

    if(!isQueryVariableInNetwork(parsed_network, queryVariableName)) {
        std::cerr << "Query variable not found in the network." << std::endl;
        return 1;
    }

    BayesianNetwork bn(parsed_network);

    start = std::chrono::steady_clock::now();

    Factor marginal = bn.calculateMarginal(queryVariableName);
    
    finish = std::chrono::steady_clock::now();
    duration = finish - start;

    std::cout << "\nMarginal distribution for " << queryVariableName << ":" << std::endl;

    const Node* queryNode = bn.getNode(queryVariableName);
    for (size_t i = 0; i < marginal.values.size(); ++i) {
        std::map<std::string, size_t> assignment = marginal.getAssignment(i);
        std::string valueName = queryNode->domain[assignment[queryVariableName]];
        std::cout << "P(" << queryVariableName << " = " << valueName << ") = " 
                  << marginal.values[i] << "\n";
    }
    std::cout<< std::endl;

    std::cout << "Marginal computation took: " << duration.count() << " seconds." << std::endl;
}

//g++ -O3 -Wall -Wextra -Wpedantic -o main parser.cpp variable_elimination.cpp -std=c++17