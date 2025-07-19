#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <functional>

class Token {
public:
    enum Type {KEYWORD, WORD, SYMBOL, DECIMAL_LITERAL, FLOATING_POINT_LITERAL, END, TEXT, IGNORE};
    Type type;
    std::string value;

    Token(Type type = END, std::string value = "");
    friend std::ostream& operator<<(std::ostream& os, const Token& token);

    inline static const std::vector<std::string> KEYWORDS = {
    "network", "variable", "probability", "type", "discrete", "table"
    };
    inline static const std::string SYMBOLS = "{}[](),;=|";
};

typedef std::pair<bool, std::string::iterator> match_result_t;
typedef std::function<match_result_t(std::string::iterator, std::string::iterator)> matcher_function_t;

class Matcher {
public:
    Token::Type type;
    matcher_function_t matchFunc;

    Matcher(Token::Type type, matcher_function_t func);
    match_result_t try_match(std::string::iterator cursor, std::string::iterator end);

    static bool isADigit(const char c);
    static bool isAlpha(const char c);
    static bool isAlphaNumeric(const char c);
    static bool isIgnoredSymbols(const char c);
    static bool matchCommentStart(const std::string::iterator it, const std::string::iterator end);
    static bool matchCommentEnd(const std::string::iterator it, const std::string::iterator end);
    static bool theWordEnds(const std::string::iterator it);


    static match_result_t ignoreMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
    static match_result_t keyWordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
    static match_result_t wordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
    static match_result_t symbolMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
    static match_result_t decimalMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
    static match_result_t floatMatcherFunc(const std::string::iterator cursor, const std::string::iterator end);
};

class Lexer {
public:
    Lexer(std::ifstream& input);
    Token getNextToken();
private:
    std::string source;
    std::string::iterator cursor;
    Matcher ignoreMatcher;
    std::vector<std::pair<Token::Type, Matcher>> matchers;
};

// AST Structures
struct Variable {
    std::string name;
    std::vector<std::string> domain;
};

struct Probability {
    std::string variable;
    std::vector<std::string> parents;
    std::vector<std::vector<double>> table;
};

struct NetworkAST {
    std::string name;
    std::vector<Variable> variables;
    std::vector<Probability> probabilities;
};

class Parser {
public:
    Parser(std::ifstream& input);
    NetworkAST parse();
private:
    Lexer lexer;
    Token current;
    NetworkAST network;

    void advance();
    Token expect(const Token::Type expectedType, const std::string expectedValue = "");
    void parseCompilationUnit();
    void parseNetworkDeclaration();
    Probability parseProbabilityDeclaration();
    Variable parseVariableDeclaration();
    std::vector<double> parseFloatingPointList();
    void parseProbabilityValuesList();
    std::vector<std::vector<double>> parseProbabilityContent();
    std::pair<std::string, std::vector<std::string>> parseProbabilityVariablesList();
    std::vector<std::string> parseVariableDiscrete();
};