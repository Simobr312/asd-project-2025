#include "parser.h"


Token::Token(Type type, std::string value)
    : type(type), value(value) {}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    const std::string typeStr[7] = {
        "KEYWORD", "WORD", "SYMBOL", "DEC_LIT", "FLOAT", "END", "TEXT"
    };
    os << "type: " << typeStr[token.type] << " \t value: " << token.value << std::endl;
    return os;
}

Matcher::Matcher(Token::Type type, matcher_function_t func)
    : type(type), matchFunc(func) {}

match_result_t Matcher::try_match(std::string::iterator cursor, std::string::iterator end) {
    return matchFunc(cursor, end);
}

typedef std::pair<bool, std::string::iterator> match_result_t;
typedef std::function<match_result_t(std::string::iterator, std::string::iterator)> matcher_function_t;

bool Matcher::isADigit(const char c) {
    return c >= '0' && c <= '9';
}

bool Matcher::isAlpha(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Matcher::isAlphaNumeric(const char c) {
    return isADigit(c) || isAlpha(c) || c == '_';
}

bool Matcher::isIgnoredSymbols(const char c) {
    return c == ' ' || c == '|' || c == ',' || c == '"' || c == '\n';
}

bool Matcher::matchCommentStart(const std::string::iterator it, const std::string::iterator end) {
    return it != end && *it == '/' && (it + 1) != end && *(it + 1) == '*';
}

bool Matcher::matchCommentEnd(const std::string::iterator it, const std::string::iterator end) {
    return it != end && *it == '*' && (it + 1) != end && *(it + 1) == '/';
}

bool Matcher::theWordEnds(const std::string::iterator it) {
    return !isAlphaNumeric(*it);
}

match_result_t Matcher::ignoreMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    while(it != end && isIgnoredSymbols(*it)) ++it;
    if (matchCommentStart(it, end)) {
        it += 2; // skip /*
        while(it != end) {
            if (matchCommentEnd(it, end)) {
                it += 2; // skip */
                break;
            }
            ++it;
        }
    }
    return {(it == cursor) ? false : true, it};
}

match_result_t Matcher::keyWordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    for(auto keyword : Token::KEYWORDS) {
        it = cursor + keyword.size();
        if(it <= end && std::string(cursor, it) == keyword && theWordEnds(it))
            return {true, it};
    }
    return {false, it};
}

match_result_t Matcher::wordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    const std::string symbols_accepted_in_word = "./_><=?-";
    while (it != end && (isAlphaNumeric(*it) || symbols_accepted_in_word.find(*it) != std::string::npos))
        ++it;
    return {true, it};
}

match_result_t Matcher::symbolMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    if(it != end && Token::SYMBOLS.find(*it) != std::string::npos) {
        ++it;
        return {true, it};
    }
    return {false, it};
}

match_result_t Matcher::decimalMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    if (!isADigit(*it))
        return {false, it};
    while (it != end && isADigit(*it))
        ++it;
    if(!isAlphaNumeric(*it))
        return {true, it};
    return {false, it};
}

match_result_t Matcher::floatMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
    auto it = cursor;
    bool seenDot = false;
    bool seenExp = false;
    if (!isADigit(*it))
        return {false, it};
    while (it != end && isADigit(*it)) ++it;
    if (it != end && *it == '.') {
        ++it;
        seenDot = true;
        while (it != end && isADigit(*it)) ++it;
    }
    if (it != end && (*it == 'e' || *it == 'E')) {
        ++it;
        seenExp = true;
        if (it != end && (*it == '+' || *it == '-')) ++it;
        while (it != end && isADigit(*it)) ++it;
    }
    if (seenDot || seenExp)
        return {true, it};
    else
        return {false, cursor};
}

Lexer::Lexer(std::ifstream& input)
    : source(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()),
      cursor(source.begin()),
      ignoreMatcher(Token::IGNORE, Matcher::ignoreMatcherFunc),
      matchers({
          {Token::SYMBOL, Matcher(Token::SYMBOL, Matcher::symbolMatcherFunc)},
          {Token::FLOATING_POINT_LITERAL, Matcher(Token::FLOATING_POINT_LITERAL, Matcher::floatMatcherFunc)},
          {Token::DECIMAL_LITERAL, Matcher(Token::DECIMAL_LITERAL, Matcher::decimalMatcherFunc)},
          {Token::KEYWORD, Matcher(Token::KEYWORD, Matcher::keyWordMatcherFunc)},
          {Token::WORD, Matcher(Token::WORD, Matcher::wordMatcherFunc)}
      }) {}

Token Lexer::getNextToken() {
    if (cursor == source.end())
        return Token(Token::END, "");

    auto [match_res, new_cursor] = ignoreMatcher.try_match(cursor, source.end());
    if (match_res == true) {
        cursor = new_cursor;
        return getNextToken();
    }

    for (auto& [type, matcher] : matchers) {
        auto [match_res, new_cursor] = matcher.try_match(cursor, source.end());
        if (match_res == true) {
            std::string value = std::string(cursor, new_cursor);
            cursor = new_cursor;
            return Token(type, value);
        }
    }

    throw std::runtime_error("Unexpected token");
}

Parser::Parser(std::ifstream& input)
    : lexer(input) {
    advance();
}

void Parser::advance() {
    current = lexer.getNextToken();
}

Token Parser::expect(const Token::Type expectedType, const std::string expectedValue) {
    if (current.type != expectedType || (!std::empty(expectedValue) && current.value != expectedValue)) {
        throw std::runtime_error("Expected " + expectedValue + ", got " + current.value);
    }
    Token token = current;
    advance();
    return token;
}

NetworkAST Parser::parse() {
    parseCompilationUnit();
    return network;
}

void Parser::parseCompilationUnit() {
    parseNetworkDeclaration();

    while(true) {
        if(current.type == Token::END)
            break;

        if(current.type == Token::KEYWORD) {
            if(current.value == "variable") {
                Variable variable = parseVariableDeclaration();
                network.variables.push_back(variable);
            }
            else if(current.value == "probability") {
                Probability probability = parseProbabilityDeclaration();
                network.probabilities.push_back(probability);
            }
        } else
            throw std::runtime_error("Expected 'variable' or 'probability', got " + current.value);
    }
}

void Parser::parseNetworkDeclaration() {
    expect(Token::KEYWORD, "network");

    Token name = expect(Token::WORD);
    network.name = name.value;

    // Ignoro inner properties per ora
    while (current.value != "}") {
        if(current.type == Token::END)
            throw std::runtime_error("Unexpected end of input in network declaration");
        advance();
    }

    expect(Token::SYMBOL, "}");
}

Variable Parser::parseVariableDeclaration() {
    Variable variable;
    expect(Token::KEYWORD, "variable");

    Token name = expect(Token::WORD);
    variable.name = name.value;

    // Ignoro inner properties per ora
    variable.domain = parseVariableDiscrete();
    return variable;
}

std::vector<std::string> Parser::parseVariableDiscrete() {
    expect(Token::SYMBOL, "{");

    expect(Token::KEYWORD, "type");
    expect(Token::KEYWORD, "discrete");
    expect(Token::SYMBOL, "[");
    int n = std::stoi(expect(Token::DECIMAL_LITERAL).value);
    std::vector<std::string> domain(n);
    expect(Token::SYMBOL, "]");
    expect(Token::SYMBOL, "{");

    domain.reserve(n);
    for (int i = 0; i < n; ++i) {
        std::string element_of_domain;

        if(current.type == Token::DECIMAL_LITERAL)
            element_of_domain = expect(Token::DECIMAL_LITERAL).value;
        else
            element_of_domain = expect(Token::WORD).value;

        domain[i] = element_of_domain;
    }

    expect(Token::SYMBOL, "}");
    expect(Token::SYMBOL, ";");

    expect(Token::SYMBOL, "}");
    return domain;
}

Probability Parser::parseProbabilityDeclaration() {
    Probability probability;
    expect(Token::KEYWORD, "probability");

    auto [variable, parents] = parseProbabilityVariablesList();
    probability.variable = variable;
    probability.parents = parents;

    std::vector<std::vector<double>> table = parseProbabilityContent();
    probability.table = table;

    return probability;
}

std::pair<std::string, std::vector<std::string>> Parser::parseProbabilityVariablesList() {
    std::string variable;
    std::vector<std::string> parents;
    expect(Token::SYMBOL, "(");

    variable = expect(Token::WORD).value;

    while(current.value != ")"){
        parents.push_back(expect(Token::WORD).value);
    }

    expect(Token::SYMBOL, ")");
    return {variable, parents};
}

std::vector<std::vector<double>> Parser::parseProbabilityContent() {
    expect(Token::SYMBOL, "{");

    std::vector<std::vector<double>> table;

    do {
        parseProbabilityValuesList();
        table.push_back(parseFloatingPointList());
    } while(current.value == "(");

    expect(Token::SYMBOL, "}");

    return table;
}

void Parser::parseProbabilityValuesList() {
    if(current.value == "table") {
        expect(Token::KEYWORD, "table");
        return;
    }
    expect(Token::SYMBOL, "(");
    do {
        advance();
        if(current.type == Token::END)
            break;
    } while(current.value != ")");
    expect(Token::SYMBOL, ")");
}

std::vector<double> Parser::parseFloatingPointList() {
    std::vector<double> floatingPointList;
    do {
        floatingPointList.push_back(std::stod(expect(Token::FLOATING_POINT_LITERAL).value));
    } while(current.value != ";");
    expect(Token::SYMBOL, ";");
    return floatingPointList;
}