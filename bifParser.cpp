#include <bits/stdc++.h>

#define LOG_DEBUG(msg) \
    if (debugMode) std::cerr << "[DEBUG] " << msg << std::endl;

const bool debugMode = true;

//https://www.bnlearn.com/bnrepository/
//http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm

class Token {
    public:
        enum Type {KEYWORD, WORD, SYMBOL, DECIMAL_LITERAL, FLOATING_POINT_LITERAL, END, TEXT, IGNORE};
        Type type;
        std::string value;

        Token(Type type = END, std::string value = "")
            : type(type), value(value) {};

        friend std::ostream& operator<<(std::ostream& os, const Token& token) {
            const std::string typeStr[7] = {
                "KEYWORD", "WORD", "SYMBOL", "DEC_LIT", "FLOAT", "END", "TEXT"
            };

            os<<"type: "<< typeStr[token.type]<<" \t value: "<<token.value<<std::endl;
            return os;
        }

        inline static const std::vector<std::string> KEYWORDS = {
            "network", "variable", "probability", "table", "type", "discrete", "default", "property"
        };

        inline static const std::string SYMBOLS = "[{}()[];=,`|]";
};

typedef std::pair<bool, std::string::iterator> match_result_t;
typedef std::function<match_result_t(std::string::iterator, std::string::iterator)> matcher_function_t;
class Matcher {
    protected:
        static bool isADigit(const char c) {
            return c >= '0' && c <= '9';
        }

        static bool isAlpha(const char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        static bool isAlphaNumeric(const char c) { //The underscore is not allowed by the documentation, but it is needed for the data.
            return isADigit(c) || isAlpha(c) || c == '_';
        }

        static bool isIgnoredSymbols(const char c) {
            return c == ' ' || c == '|' || c == ',' || c == '"' || c == '\n';
        }

        static bool matchCommentStart(const std::string::iterator it, const std::string::iterator end) {
            return it != end && *it == '/' && (it + 1) != end && *(it + 1) == '*';
        }

        static bool matchCommentEnd(const std::string::iterator it, const std::string::iterator end) {
            return it != end && *it == '*' && (it + 1) != end && *(it + 1) == '/';
        }

        static bool theWordEnds(const std::string::iterator it) {
            return !isAlphaNumeric(*it);
        }

    public:
        Token::Type type;
        matcher_function_t matchFunc;

        Matcher(Token::Type type, matcher_function_t func)
            : type(type), matchFunc(func) {}

        match_result_t try_match(std::string::iterator cursor, std::string::iterator end) {
            return matchFunc(cursor, end);
        }

        static match_result_t ignoreMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
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

        //R"(^\b(network|variable|probability|property|type|discrete|default|table)\b)"
        static match_result_t keyWordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
            auto it = cursor;
            for(auto keyword : Token::KEYWORDS) {
                it = cursor + keyword.size();
                if(it <= end && std::string(cursor, it) == keyword && theWordEnds(it))
                        return {true, it};
            }
            return {false, it};
        }


        //The regular expression of the WORD token from the documentation I found is R"(^[a-zA-Z_][a-zA-Z0-9_]*)",
        //but for the seek of the project, in the data given by the professor it is useful to consider WORD even strings
        //which do not start with an alphabetical character.
        static match_result_t wordMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
            auto it = cursor;

            while (it != end && isAlphaNumeric(*it))  // letters or digits
                ++it;

            return {true, it};
        }

        //  R"(^[{}()[\];=,`|])"
        static match_result_t symbolMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
            auto it = cursor;

            if(it != end && Token::SYMBOLS.find(*it) != std::string::npos) {
                ++it;
                return {true, it};
            }

            return {false, it};
        }   

        static match_result_t decimalMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
            auto it = cursor;

            if (!isADigit(*it))
                return {false, it};

            while (it != end && isADigit(*it))
                ++it;

            if(!isAlphaNumeric(*it)) //making sure is not the start of a word.
                return {true, it};
            return {false, it};
        }


        //R"(^\d+(?!\.|[eE]))"
        static match_result_t floatMatcherFunc(const std::string::iterator cursor, const std::string::iterator end) {
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
};  


class Lexer {
    private:
        std::string source;
        std::string::iterator cursor;

        Matcher ignoreMatcher;
        std::vector<std::pair<Token::Type, Matcher>> matchers;

    public:
        Lexer(std::ifstream& input) 
        : source(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()),
          cursor(source.begin()),
           ignoreMatcher(Token::IGNORE, Matcher::ignoreMatcherFunc),
           matchers({ //The order is foundamental, because some regular expression cointains others
                {Token::SYMBOL, Matcher(Token::SYMBOL, Matcher::symbolMatcherFunc)},
                {Token::FLOATING_POINT_LITERAL,  Matcher(Token::FLOATING_POINT_LITERAL, Matcher::floatMatcherFunc)},
                {Token::DECIMAL_LITERAL,        Matcher(Token::DECIMAL_LITERAL, Matcher::decimalMatcherFunc)},
                {Token::KEYWORD,Matcher(Token::KEYWORD, Matcher::keyWordMatcherFunc)},
                {Token::WORD,   Matcher(Token::WORD, Matcher::wordMatcherFunc)}
            }) {
            }

        Token getNextToken() {
            if(cursor == source.end())
                return Token(Token::END, "");

            auto [match_res, new_cursor] = ignoreMatcher.try_match(cursor, source.end());
            if(match_res == true) {
                cursor = new_cursor;
                return getNextToken();
            }

            for(auto& [type, matcher] : matchers) {
                auto [match_res, new_cursor] = matcher.try_match(cursor, source.end());
                if(match_res == true) {
                    std::string value = std::string(cursor, new_cursor);
                    cursor = new_cursor;
                    return Token(type, value);
                }
            }

            throw std::runtime_error("Unexpected token");
        } 
};

//
// Abstract Syntax Tree
//
struct Variable {
    std::string name;
    std::vector<std::string> values;
};

struct Probability {
    std::string variable;
    std::vector<std::string> parents;
    std::vector<std::vector<double>> table;
};

struct Network {
    std::string name;
    std::vector<Variable> variables;
    std::vector<Probability> probabilities;
};

class Parser {
    private:
        Lexer lexer;
        Token current;

        Network network;

        void advance() {
            current = lexer.getNextToken();
           LOG_DEBUG("Current token: " + current.value);
        }

        Token expect(const Token::Type expectedType, const std::string expectedValue = "") {
            if(current.type != expectedType || (!std::empty(expectedValue) && current.value != expectedValue)) {
                throw std::runtime_error("Expected " + expectedValue + ", got " + current.value);
            }
            Token token = current;
            advance();
            return token;
        }

    public:
        Parser(std::ifstream& input)
            :  lexer(input){
                advance();
        }

        Network parse() {
            parseCompilationUnit();
            return network;
        }

    private:

        std::vector<double> parseFloatingPointList() {
            std::vector<double> floatingPointList;
            do {
                floatingPointList.push_back(std::stod(expect(Token::FLOATING_POINT_LITERAL).value));
            } while(current.value != ";");
            expect(Token::SYMBOL, ";");
            return floatingPointList;
        }consid

        void parseProbabilityValuesList() {
            if(current.value == "table") {
                expect(Token::KEYWORD, "table");
                return;
            }
            expect(Token::SYMBOL, "(");
                do { //I'm skipping this data because until now I don't need them
                    advance();
                    if(current.type == Token::END)
                        break;
                } while(current.value != ")");
            expect(Token::SYMBOL, ")");
        }

        std::vector<std::vector<double>> parseProbabilityContent() {
            expect(Token::SYMBOL, "{");

            std::vector<std::vector<double>> table;

            do {
                parseProbabilityValuesList();
                table.push_back(parseFloatingPointList());
            }while(current.value == "(");
            
            expect(Token::SYMBOL, "}");consid

            return table;
        }

        std::pair<std::string, std::vector<std::string>> parseProbabilityVariablesList() {
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

        Probability parseProbabilityDeclaration() {
            Probability probability;
            expect(Token::KEYWORD, "probability");

            auto [variable, parents] = parseProbabilityVariablesList();
            probability.variable = variable;
            probability.parents = parentsconsid;
            
            std::vector<std::vector<double>> table = parseProbabilityContent();
            probability.table = table;

            return probability;
        }

        std::vector<std::string> parseVariableDiscrete() {
            expect(Token::SYMBOL, "{");

            expect(Token::KEYWORD, "type");
            expect(Token::KEYWORD, "discrete");
            expect(Token::SYMBOL, "[");
            int n = std::stoi(expect(Token::DECIMAL_LITERAL).value);
            std::vector<std::string> values(n);
            expect(Token::SYMBOL, "]");
            expect(Token::SYMBOL, "{");
            
            values.reserve(n);
            for (int i = 0; i < n; ++i) {
                std::string value;

                if(current.type == Token::DECIMAL_LITERAL) //The grammar doesn't allow decimal literal, but the data do
                    value = expect(Token::DECIMAL_LITERAL).value;
                else
                    value = expect(Token:consid:WORD).value;

                values[i] = value;
            }

            expect(Token::SYMBOL, "}");
            expect(Token::SYMBOL, ";");

            expect(Token::SYMBOL, "}");
            return values;
        }

        Variable parseVariableDeclaration() {
            Variable variable;
            expect(Token::KEYWORD, "variable");

            Token name = expect(Token::WORD);
            variable.name = name.value;

            //I'm ignoring inner properties for now, and so the parseVariableContent() production
            variable.values = parseVariableDiscrete();
            return variable;
        }

        void parseNetworkDeclaration() {
            expect(Token::KEYWORD, "network");

            Token name = expect(Token::WORD);
            network.name = name.value;

            // I'm  ignoring inn#define LOG_DEBUG(msg) \
    if (debugMode) std::cerr << "[DEBUG] " << msg << std::endl;, so I'm not implementing the NetworkContent production
            while (current.value != "}") {
                if(current.type == Token::END)
                    throw std::runtime_error("Unexpected end of input in network declaration");
                advance();
            }

            expect(Token::SYMBOL, "}");
        }

        void parseCompilationUnit() {
            parseNetworkDeclaration();

            while(true) {
                if(current.type == Token::END)
                    break;

                if(current.type == Token::KEYWORD) {
                    if(current.value == "variable") {
                        Variable variable = parseVariableDeclaration();
                        log_debug("Parsed variable: " + variable.name);
                        network.variables.push_back(variable);
                    }
                    else if(current.value == "probability") {
                        Probability probability = parseProbabilityDeclaration();
                        LOG_DEBUG("Parsed probability for variable: " + probability.variable);
                        network.probabilities.push_back(probability);
                    }
                } else
                    throw std::runtime_error("Expected 'variable' or 'probability', got " + current.value);
            }         
        }
};

int main() {

    std::string filename = "BIF/munin1.bif";

    std::cin>>filename;

    std::ifstream input(filename);    
    Parser parser(input);

    const auto start = std::chrono::steady_clock::now();

    parser.parse();

    const auto finish = std::chrono::steady_clock::now();
    const std::chrono::duration<double> duration = finish - start;

    log_debug("Parsing completed in " + std::to_string(duration.count()) + " seconds");

    return 0;
}

//g++ -O3 -Wall -Wextra -Wpedantic -o bifParser bifParser.cpp