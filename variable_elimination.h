#pragma once

#include "parser.h"

#include <memory>
#include <set>

class Node;

struct CPT {
    std::vector<Node*> parents; 
    std::vector<double> table;
};

class Node {
public:
    std::string name;
    std::vector<std::string> domain;
    CPT cpt;
    
    std::vector<Node*> children;

    Node(const std::string& name, const std::vector<std::string>& domain);

    size_t getCardinality() const;
};

/*
    A factor is a table which maps combinations of variable values to probabilities.
    In this program a factor will be used to represent the intermediate CPT of the operations done to marginalize a variable.
    For example, a factor could represent
        - variables = ["A", "B"]
        - cardinalities = {"A": 2, "B": 3} // A has 2 values, B has 3 values
        - values = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
*/
struct Factor {
    std::vector<std::string> variables;
    std::vector<double> values;
    std::map<std::string, size_t> cardinalities; //number of elements in the domain for each variable
    std::map<std::string, size_t> strides;

    Factor(const std::vector<std::string>& vars, const std::map<std::string, size_t>& cards);

    std::map<std::string, size_t> getAssignment(size_t index) const;
    void initialise_indexes();
    size_t getIndex(const std::map<std::string, size_t>& assignment) const;
    double getValue(const std::map<std::string, size_t>& assignment) const;

    void normalize();
};

class BayesianNetwork {
private:
    std::map<std::string, std::unique_ptr<Node>> nodes; //Unique pointer are because Node are heavy

    private:
        void build(const NetworkAST& parsedNetwork);
    
        /*
        This function is a breath-first search to find all relevant variables in the network.
        It starts from the query variable and explores all its parents.
        I am not sure the bfs is the best "order" to traverse the network.
        */
        std::set<std::string> getRelevantVariables(const std::string& queryVar, const std::map<std::string, std::unique_ptr<Node>>& nodes);
        std::vector<Factor> buildInitialFactors(const std::set<std::string>& relevantVars);

        /*
        This is the heart of the calculateMarginal function.
        1. Take the factors that contains variables to eliminate
        2. Compute the product of them
        3. Sum-Out

        Example:
            f1: ["A", "B"] [0.1, 0.2, 0.3, 0.4]
            f2: ["B", "C"] [0.5, 0.6, 0.7, 0.8]
            queryVar = "A"
            So variables to eliminate are "B", "C"

            Elimination of "C":
                1. Only f2 contains C
                2. Sum-Out "C" from f2 to f2'
            f2': ["B"] [1.1, 1.5]

            Elimination of "B"
                1. f1 and f2' contain "B"
                2. Compute the product f1 and f2'
                3. Sum-out "B"
            result: ["A"] [0.33, 0.67]

        */
        std::vector<Factor> eliminateVariables(std::vector<Factor> factors, const std::string& queryVariableName);
        Factor combineNormalizeFactors(const std::vector<Factor>& factors);
public:
    BayesianNetwork(const NetworkAST& parsedNetwork);

    const Node* getNode(const std::string& name) const;

    /*
    This function "merges" two factors into one.
    For example:
        f1: ["A", "B"] [0.1, 0.2, 0.3, 0.4]
        f2: ["B", "C"] [0.5, 0.6, 0.7, 0.8]
     f1*f2: ["A", "B", "C"]
        It maps, for example, the assignment (A=0, B=1, C=0) to the value
            f1(A=0, B=1)*f2(B=1, C=0) = 0.2 * 0.7 = 0.14  
    */
    Factor factorProduct(const Factor& f1, const Factor& f2);

    /*
    This function sums out a variable from a factor.
    It creates a new factor with the variable removed and sums over all its values.
    For example:
        f: ["A", "B"]   [0.1, 0.2, 0.3, 0.4]
        r: ["A"]        [0.1 + 0.2, 0.3 + 0.4] = [0.3, 0.7]

    */
    Factor factorSumOut(const Factor& factor, const std::string& varToSumOut);

    //This is the main function for the implementation of the algorithm
    Factor calculateMarginal(const std::string& queryVariableName);

    //This function is only for debug
    void printFactor(const Factor& f, const std::string& label = "");
};