#include "parser.h"
#include "variable_elimination.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <queue>
#include <set>

#define DEBUG 0

Node::Node(const std::string& name, const std::vector<std::string>& domain)
    : name(name), domain(domain) {}

size_t Node::getCardinality() const {
    return domain.size();
}

Factor::Factor(const std::vector<std::string>& vars, const std::map<std::string, size_t>& cards)
    : variables(vars), cardinalities(cards) {
    initialise_indexes();
}

std::map<std::string, size_t> Factor::getAssignment(size_t index) const {
    std::map<std::string, size_t> assignment;
    size_t temp_index = index;
    for (const auto& var : variables) {
        const size_t stride = strides.at(var);
        assignment[var] = temp_index / stride;
        temp_index %= stride;
    }
    return assignment;
}

void Factor::initialise_indexes() {
    size_t current_stride = 1;
    for (auto it = variables.rbegin(); it != variables.rend(); ++it) {
        const std::string& var = *it;
        strides[var] = current_stride;
        current_stride *= cardinalities.at(var);
    }
    values.resize(current_stride, 0.0);
}

size_t Factor::getIndex(const std::map<std::string, size_t>& assignment) const {
    size_t index = 0;
    for (const auto& var : variables) {
        index += strides.at(var) * assignment.at(var);
    }
    return index;
}

double Factor::getValue(const std::map<std::string, size_t>& assignment) const {
    return values[getIndex(assignment)];
}

void Factor::normalize() {
    double total = 0;
    for (const auto& val : values)
        total += val;
    if (total > 0)
        for (auto& val : values)
            val /= total;
}

BayesianNetwork::BayesianNetwork(const NetworkAST& parsedNetwork) {
    build(parsedNetwork);
}

const Node* BayesianNetwork::getNode(const std::string& name) const {
    auto it = nodes.find(name);
    if (it == nodes.end()) {
        return nullptr;
    }
    return it->second.get();
}

void BayesianNetwork::build(const NetworkAST& parsedNetwork) {
    for (const auto& var : parsedNetwork.variables)
        nodes[var.name] = std::make_unique<Node>(var.name, var.domain);

    for (const auto& prob : parsedNetwork.probabilities) {
        Node* currentNode = nodes.at(prob.variable).get();
        for (const auto& parentName : prob.parents) {
            Node* parentNode = nodes.at(parentName).get();
            currentNode->cpt.parents.push_back(parentNode);
            parentNode->children.push_back(currentNode);
        }
        for (const auto& row : prob.table)
            currentNode->cpt.table.insert(currentNode->cpt.table.end(), row.begin(), row.end());
    }
}

void BayesianNetwork::printFactor(const Factor& f, const std::string& label) {
    if (!label.empty()) std::cout << "\n=== Factor: " << label << " ===\n";
    std::cout << "Variables: ";
    for (const auto& var : f.variables) std::cout << var << " ";
    std::cout << "\nValues:\n";
    for (size_t i = 0; i < f.values.size(); ++i) {
        auto assignment = f.getAssignment(i);
        std::cout << "  ";
        for (const auto& var : f.variables) {
            const Node* queryNode = getNode(var);
            std::cout << var << "=" << queryNode->domain[assignment[var]] << " ";
        }
        std::cout << "-> " << f.values[i] << "\n";
    }
}

std::set<std::string> BayesianNetwork::getRelevantVariables(const std::string& queryVar, const std::map<std::string, std::unique_ptr<Node>>& nodes) {
    std::set<std::string> visited;
    std::queue<std::string> to_visit;
    to_visit.push(queryVar);

    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();

        if (visited.count(current)) continue;
        visited.insert(current);

        const Node* node = nodes.at(current).get();
        for (const Node* parent : node->cpt.parents)
            to_visit.push(parent->name);
    }
    return visited;
}

Factor BayesianNetwork::factorProduct(const Factor& f1, const Factor& f2) {
    std::set<std::string> vars_set;
    vars_set.insert(f1.variables.begin(), f1.variables.end());
    vars_set.insert(f2.variables.begin(), f2.variables.end());

    std::vector<std::string> new_vars(vars_set.begin(), vars_set.end());
    std::map<std::string, size_t> new_cards;
    for (const auto& var : new_vars)
        if (f1.cardinalities.count(var)) new_cards[var] = f1.cardinalities.at(var);
        else new_cards[var] = f2.cardinalities.at(var);

    Factor result(new_vars, new_cards);

    for (size_t i = 0; i < result.values.size(); ++i) {
        std::map<std::string, size_t> full_assignment = result.getAssignment(i);
        std::map<std::string, size_t> assignment1, assignment2;
        for (const auto& var : f1.variables) assignment1[var] = full_assignment.at(var);
        for (const auto& var : f2.variables) assignment2[var] = full_assignment.at(var);
        result.values[i] = f1.getValue(assignment1) * f2.getValue(assignment2);
    }
    return result;
}

Factor BayesianNetwork::factorSumOut(const Factor& factor, const std::string& varToSumOut) {
    std::vector<std::string> new_vars;
    std::map<std::string, size_t> new_cards;

    for (const auto& var : factor.variables) {
        if (var != varToSumOut) {
            new_vars.push_back(var);
            new_cards[var] = factor.cardinalities.at(var);
        }
    }

    Factor result(new_vars, new_cards);
    size_t varToSumOut_cardinality = factor.cardinalities.at(varToSumOut);

    for (size_t i = 0; i < result.values.size(); ++i) {
        auto partial_assignment = result.getAssignment(i);
        double sum = 0.0;
        for (size_t j = 0; j < varToSumOut_cardinality; ++j) {
            partial_assignment[varToSumOut] = j;
            sum += factor.getValue(partial_assignment);
        }
        result.values[i] = sum;
    }
    return result;
}

std::vector<Factor> BayesianNetwork::buildInitialFactors(const std::set<std::string>& relevantVars) {
    std::vector<Factor> factors;
    for (const auto& pair : nodes) {
        const std::string& nodeName = pair.first;
        if (!relevantVars.count(nodeName)) continue;

        const Node* node = pair.second.get();
        std::vector<std::string> factor_vars;
        std::map<std::string, size_t> factor_cards;

        for (const auto* parent : node->cpt.parents) {
            factor_vars.push_back(parent->name);
            factor_cards[parent->name] = parent->getCardinality();
        }
        factor_vars.push_back(node->name);
        factor_cards[node->name] = node->getCardinality();

        Factor f(factor_vars, factor_cards);
        f.values = node->cpt.table;

        if (DEBUG) printFactor(f, "Initial factor for " + node->name);

        factors.push_back(f);
    }
    return factors;
}

std::vector<Factor> BayesianNetwork::eliminateVariables(std::vector<Factor> factors, const std::string& queryVariableName) {
    for (const auto& pair : nodes) {
        const std::string& var_to_eliminate = pair.first;
        if (var_to_eliminate == queryVariableName) continue;

        std::vector<Factor> factors_with_var;
        std::vector<Factor> remaining_factors;

        for (const auto& f : factors) {
            bool found = (std::find(f.variables.begin(), f.variables.end(), var_to_eliminate) != f.variables.end());
            if (found) factors_with_var.push_back(f);
            else remaining_factors.push_back(f);
        }

        if (factors_with_var.empty()) continue;

        Factor product = factors_with_var[0];
        for (size_t i = 1; i < factors_with_var.size(); ++i)
            product = factorProduct(product, factors_with_var[i]);

        if (DEBUG) printFactor(product, "Product before summing out " + var_to_eliminate);

        Factor summed_out = factorSumOut(product, var_to_eliminate);

        if (DEBUG) printFactor(summed_out, "After summing out " + var_to_eliminate);

        factors = remaining_factors;
        factors.push_back(summed_out);
    }
    return factors;
}

Factor BayesianNetwork::combineNormalizeFactors(const std::vector<Factor>& factors) {
    Factor final_factor = factors[0];
    for (size_t i = 1; i < factors.size(); ++i) {
        final_factor = factorProduct(final_factor, factors[i]);
    }

    if (DEBUG) printFactor(final_factor, "Final unnormalized factor");

    final_factor.normalize();

    if (DEBUG) printFactor(final_factor, "Final normalized factor");

    return final_factor;
}

Factor BayesianNetwork::calculateMarginal(const std::string& queryVariableName) {
    if (DEBUG)
        std::cout << "\n[DEBUG] Starting marginal computation for variable: " << queryVariableName << "\n";

    std::set<std::string> relevantVars = getRelevantVariables(queryVariableName, nodes);

    std::vector<Factor> factors = buildInitialFactors(relevantVars);
    factors = eliminateVariables(factors, queryVariableName);
    return combineNormalizeFactors(factors);
}