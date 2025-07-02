#include "parser.cpp"
//#include "builder.cpp"
#include <map>
#include <string>
#include <vector>

class Node {
public:
    std::string name;
    std::vector<std::string> domain;
    std::vector<std::string> parent_names;
    std::vector<std::string> children_names;

    std::vector<std::vector<double>> probability_table;

    Node(const Variable& variable) 
        : name(variable.name), domain(variable.domain) {}

    int getValueIndex(const std::string& value) const { //Non necessaria nella rete bayesiane, dato che userò un hash map per i valori.
        for (auto i = 0; i < domain.size(); ++i) {
            if (domain[i] == value) {
                return i;
            }
        }
        return -1;
    }
};

class BayesianNetwork {
public:
    std::map<std::string, Node> nodes;

    BayesianNetwork(const Network& parsed_network) {
        build(parsed_network);
    }

    Node* getNode(const std::string& name) {
        if (nodes.count(name)) {
            return &nodes.at(name);
        }
        return nullptr;
    }

private:
    void build_variables(const Network& parsed_network) {
        for (const auto& var : parsed_network.variables) {
            nodes.emplace(var.name, Node(var));
        }
    }

    void assign_probability(Node* node, const Probability& prob) {
        node->probability_table = prob.table;
        node->parent_names = prob.parents;
    }

    void assign_edges(Node* child_node, const std::vector<std::string>& parent_names) {
        for (const auto& parent_name : parent_names) {
            Node* parent_node = getNode(parent_name);
            parent_node->children_names.push_back(child_node->name);
        }
    }

    void build(const Network& parsed_network) {
        build_variables(parsed_network);

        for (const auto& prob : parsed_network.probabilities) {
            Node* child_node = getNode(prob.variable);
            
            assign_probability(child_node, prob);
            assign_edges(child_node, prob.parents);
        }
    }
};

class Factor {
public:
    std::vector<std::string> variables;
    std::map<std::string, std::vector<std::string>> domains;
    std::vector<double> values;

    Factor(std::vector<std::string> vars, BayesianNetwork& network)
        : variables(vars) {
        for (const auto& var_name : variables) {
            Node* node = network.getNode(var_name);
            if (node)
                domains[var_name] = node->domain;
            else
                throw std::runtime_error("Variable " + var_name + " not found in the Bayesian Network.");

            for(const auto& row : node->probability_table)
                values.insert(values.end(), row.begin(), row.end());
        }
    }

    // Converte un'assegnazione di valori in un indice per il vettore 'values'.
    int get_index(const std::map<std::string, std::string>& assignment) const {
        int index = 0;
        int stride = 1;
        for (int i = variables.size() - 1; i >= 0; --i) {
            const auto& var_name = variables[i];
            const auto& domain = domains.at(var_name);
            auto it = std::find(domain.begin(), domain.end(), assignment.at(var_name));
            if (it == domain.end()) return -1;
            int value_index = std::distance(domain.begin(), it);
            index += value_index * stride;
            stride *= domain.size();
        }
        return index;
    }

    // Converte un indice del vettore 'values' in un'assegnazione di valori.
    std::map<std::string, std::string> get_assignment(int index) const {
        std::map<std::string, std::string> assignment;
        int temp_index = index;
        int stride = values.size();
        for (const auto& var_name : variables) {
            const auto& domain = domains.at(var_name);
            stride /= domain.size();
            int value_index = (temp_index / stride);
            assignment[var_name] = domain[value_index];
            temp_index -= value_index * stride;
        }
        return assignment;
    }
};



Factor static variable_elimination(BayesianNetwork& network, const std::vector<std::string>& query_variables) {
    std::vector<Factor> factors;

    for(const auto & [node_name, node] : network.nodes) {
        std::vector<std::string> variables = node.parent_names;
        variables.push_back(node_name);
        Factor factor(variables, network);

        factors.push_back(factor);
    }

    Factor result = factors[0];
    for (size_t i = 1; i < factors.size(); ++i) {
        result = factor_product(result, factors[i]);
    }

    return result;
}

int main() {

    std::string filename = "BIF/cancer.bif";

    //std::cin>>filename;

    std::ifstream input(filename);    
    Parser parser(input);

    auto start = std::chrono::steady_clock::now();

    Network network = parser.parse(); //Meglio stare attenti alla restituzione per valore e non per riferimento.

    auto finish = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = finish - start;

    std::cout<<"parsing: "<<duration.count();

    start = std::chrono::steady_clock::now();

    BayesianNetwork bayesian_network(network);

    finish = std::chrono::steady_clock::now();
    duration = finish - start;

    std::cout<<", building: "<<duration.count()<<std::endl;

    variable_elimination(bayesian_network, {"Smoking"});

    return 0;
}

//g++ -O3 -Wall -Wextra -Wpedantic -o main main.cpp parser.cpp -std=c++17