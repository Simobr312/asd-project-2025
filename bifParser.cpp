#include <bits/stdc++.h>

//https://www.bnlearn.com/bnrepository/
//http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm

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

    public:
        Matcher() = default;
        ~Matcher() = default;

    virtual std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) = 0;
};

class IgnoreMatcher : public Matcher {
    private:
        bool isIgnoredSymbols(const char c) {
            return c == ' ' || c == '|' || c == ',' || c == '"' || c == '\n';
        }

        bool matchCommentStart(const std::string::iterator it, const std::string::iterator end) {
            return it != end && *it == '/' && (it + 1) != end && *(it + 1) == '*';
        }

        bool matchCommentEnd(const std::string::iterator it, const std::string::iterator end) {
            return it != end && *it == '*' && (it + 1) != end && *(it + 1) == '/';
        }

    public:
        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
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
};

class KeywordMatcher : public Matcher { //R"(^\b(network|variable|probability|property|type|discrete|default|table)\b)" 
    private:
        const std::vector<std::string> keywords;
    public:
        KeywordMatcher(const std::vector<std::string>& keywords) 
            : keywords(keywords) {}

        bool theWordEnds(const std::string::iterator it) {
            return !isAlphaNumeric(*it);
        }

        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
            auto it = cursor;
            for(auto keyword : keywords) {
                it = cursor + keyword.size();
                if(it <= end && std::string(cursor, it) == keyword && theWordEnds(it))
                        return {true, it};
            }
            return {false, it};
        }
};

//The regular expression of the WORD token from the documentation I found is R"(^[a-zA-Z_][a-zA-Z0-9_]*)",
//but for the seek of the project, in the data given by the professor it is useful to consider WORD even strings
//which do not start with an alphabetical character.
class WordMatcher : public Matcher { 
    public:
        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
            auto it = cursor;

            while (it != end && isAlphaNumeric(*it))  // letters or digits
                ++it;

            return {true, it};
        }
};

class CharSetMatcher : public Matcher { //  R"(^[{}()[\];=,`|])"
    private:
        const std::string ALLOWED;
    public:
        CharSetMatcher(const std::string& allowed)
            : ALLOWED(allowed) {}

        bool isAllowed(const char c) {
            return ALLOWED.find(c) != std::string::npos;
        }

        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
            auto it = cursor;

            if(it != end && isAllowed(*it)) {
                ++it;
                return {true, it};
            }

            return {false, it};
        }
};

class DecimalMatcher : public Matcher { //R"(^\d+(?!\.|[eE]))"
    public:
        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
            auto it = cursor;

            if (!isADigit(*it))
                return {false, it};

            while (it != end && isADigit(*it))
                ++it;

            if(!isAlphaNumeric(*it)) //making sure is not the start of a word.
                return {true, it};
            return {false, it};
        }
};

class FloatMatcher : public Matcher { //R"(^((\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?))"
    public:
        std::pair<bool, std::string::iterator> try_match(const std::string::iterator cursor, const std::string::iterator end) override {
            auto it = cursor;

            if (!isADigit(*it))
                return {false, it};

            while (it != end && isADigit(*it))
                ++it;

            if (it == end || *it != '.')  // must have decimal point for float
                return {false, it};

            ++it; // '.'

            if (it == end || !isADigit(*it))
                return {false, it};

            while (it != end && isADigit(*it))
                ++it;
            
            if(it != end && *it == 'e') {
                ++it;
                if(it != end && (*it == '+' || *it == '-'))
                    ++it;
            }

            while (it != end && isADigit(*it))
                ++it;

            return {true, it};
        }
};


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
};

class Lexer {
    private:
        std::string source;
        std::string::iterator cursor;

        std::unique_ptr<Matcher> ignoreMatcher;
        std::vector<std::pair<Token::Type, std::unique_ptr<Matcher>>> matchers;

        inline static const std::vector<std::string> KEYWORDS = {
            "network", "variable", "probability", "table", "type", "discrete", "default", "property"
        };

    public:
        Lexer(std::ifstream& input) 
        : source(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()),
          cursor(source.begin()) {

            ignoreMatcher = std::make_unique<IgnoreMatcher>();

            matchers.push_back({Token::SYMBOL, std::make_unique<CharSetMatcher>("[{}()[];=,`|]")});
            matchers.push_back({Token::FLOATING_POINT_LITERAL, std::make_unique<FloatMatcher>()}); 
            matchers.push_back({Token::DECIMAL_LITERAL, std::make_unique<DecimalMatcher>()}); //and so decimal after floating
            matchers.push_back({Token::KEYWORD, std::make_unique<KeywordMatcher>(KEYWORDS)});
            //Only after I matched DECIMAL LITERAL and KEYWORD I can try WORD, because WORD contains the others.
            matchers.push_back({Token::WORD, std::make_unique<WordMatcher>()});
        }

        Token getNextToken() {
            if(cursor == source.end())
                return Token(Token::END, "");

            auto [match_res, new_cursor] = ignoreMatcher->try_match(cursor, source.end());

            if(match_res == true) {
                cursor = new_cursor;
                return getNextToken();
            }

            for(auto& [type, matcher] : matchers) {
                auto [match_res, new_cursor] = matcher->try_match(cursor, source.end());
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
            //std::cout<<current<<std::endl;
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
        }

        void parseProababilityValuesList() {
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
                parseProababilityValuesList();
                table.push_back(parseFloatingPointList());
            }while(current.value == "(");
            
            expect(Token::SYMBOL, "}");

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
            probability.parents = parents;
            
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
            
            for (int i = 0; i < n; ++i) {
                std::string value;

                if(current.type == Token::DECIMAL_LITERAL) //The grammar doesn't allow decimal literal, but the data do
                    value = expect(Token::DECIMAL_LITERAL).value;
                else
                    value = expect(Token::WORD).value;

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

            // I'm  ignoring inner properties for now, so I'm not implementing the NetworkContent production
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
                        std::cout<<"variable: "<<variable.name<<std::endl;
                        network.variables.push_back(variable);
                    }
                    else if(current.value == "probability") {
                        Probability probability = parseProbabilityDeclaration();
                        std::cout<<"probability: "<<probability.variable<<std::endl;
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

    std::cout<<"duration: "<<duration.count();

    return 0;
}

//g++ -O3 -Wall -Wextra -Wpedantic -o bifParser bifParser.cpp