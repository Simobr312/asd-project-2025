#include <bits/stdc++.h>

//https://www.bnlearn.com/bnrepository/
//http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm

class Matcher {
    protected:
        std::string::iterator start;
        std::string::iterator it;

        static bool isADigit(char c) {
            return c >= '0' && c <= '9';
        }

        static bool isAlpha(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        static bool isAlphaNumeric(char c) {
            return isADigit(c) || isAlpha(c) || c == '_';
        }

    public:
        Matcher() = default;
        ~Matcher() = default;

        virtual bool match(const std::string::iterator cursor, const std::string::iterator end) = 0;

        std::string::iterator getIterator() {
            return it;
        }

        std::string getValue() {
            return std::string(start, it);
        }
};

/*
inline static const std::vector<std::pair<Type, std::regex>> Spec = {
        {KEYWORD,    std::regex()},
        {WORD, std::regex()},
        {SYMBOL,     std::regex()},
        {DECIMAL_LITERAL,     std::regex()},
        {FLOATING_POINT_LITERAL,     std::regex()}
    };
*/

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
        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            it = cursor;
        
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
            
            return (it == cursor) ? false : true;
        }
};

class KeywordMatcher : public Matcher { //R"(^\b(network|variable|probability|property|type|discrete|default|table)\b)" 
    public:
        inline static const std::vector<std::string> KEYWORDS = {
            "network", "variable", "probability", "table", "type", "discrete", "default", "property"
        };

        bool areEqual(const std::string a, const std::string b) {
            return a.compare(b) == 0;
        }

        bool theWordEnds(const std::string::iterator it) {
            return !isAlphaNumeric(*it);
        }

        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            for(auto keyword : KEYWORDS) {
                it = cursor + keyword.size();
                if(it <= end && areEqual(keyword, std::string(cursor, it)) 
                    && theWordEnds(it))
                        return true;
            }
            return false;
        }
};


class WordMatcher : public Matcher { //R"(^[a-zA-Z_][a-zA-Z0-9_]*)"
    public:
        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            it = cursor;

            if (it == end || !isAlpha(*it))  // word starts with a letter
                return false;

            while (it != end && isAlphaNumeric(*it))  // letters or digits
                ++it;

            return true;
        }
};

class SymbolMatcher : public Matcher { //  R"(^[{}()[\];=,`|])" 
    public:
        inline static const std::string SYMBOLS = "[{}()[];=,`|]";

        bool isASymbol(const char c) {
            return SYMBOLS.find(c) != std::string::npos;
        }

        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            it = cursor;


            if(it != end && isASymbol(*it)) {
                ++it;
                return true;
            }

            return false;
        }
};

class DecimalMatcher : public Matcher { //R"(^\d+(?!\.|[eE]))"
    public:
        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            it = cursor;

            if (!isADigit(*it))
                return false;

            while (it != end && isADigit(*it))
                ++it;

            return true;

            return false;
        }
};

class FloatMatcher : public Matcher { //R"(^((\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?))"
    public:
        bool match(const std::string::iterator cursor, const std::string::iterator end) override {
            start = cursor;
            it = cursor;

            if (!isADigit(*it))
                return false;

            while (it != end && isADigit(*it))
                ++it;

            if (it == end || *it != '.')  // must have decimal point for float
                return false;

            ++it; // '.'

            if (it == end || !isADigit(*it))
                return false;

            while (it != end && isADigit(*it))
                ++it;
            
            if(it != end && *it == 'e') {
                ++it;
                if(it != end && (*it == '+' || *it == '-'))
                    ++it;
            }

            while (it != end && isADigit(*it))
                ++it;

            return true;
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

    public:
        Lexer(std::ifstream& input) 
        : source(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()),
          cursor(source.begin()) {

            ignoreMatcher = std::make_unique<IgnoreMatcher>();

            matchers.push_back({Token::SYMBOL, std::make_unique<SymbolMatcher>()});
            matchers.push_back({Token::KEYWORD, std::make_unique<KeywordMatcher>()});
            matchers.push_back({Token::WORD, std::make_unique<WordMatcher>()}); // I'm making sure word come after keyword
            matchers.push_back({Token::FLOATING_POINT_LITERAL, std::make_unique<FloatMatcher>()}); 
            matchers.push_back({Token::DECIMAL_LITERAL, std::make_unique<DecimalMatcher>()}); //and so decimal after floating
        }

        Token getNextToken() {
            std::smatch match_result;

            if(cursor == source.end())
                return Token(Token::END, "");

            if(ignoreMatcher->match(cursor, source.end())) {
                cursor = ignoreMatcher->getIterator();
                return getNextToken();
            }

            for(auto& [type, matcher] : matchers) {
                if(matcher->match(cursor, source.end())) {
                    cursor = matcher->getIterator();
                    return Token(type, matcher->getValue());
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
                do { //I'm skipping this data because untile now I don't need them
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

        std::pair<std::string, std::vector<std::string>> parseProabilityVariablesList() {
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

            auto [variable, parents] = parseProabilityVariablesList();
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
                std::string value = expect(Token::WORD).value;
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
                } else {
                    throw std::runtime_error("Expected 'variable' or 'probability', got " + current.value);
                }
            }
            
        }
};

int main() {

    std::string filename = "BIF/pathfinder.bif";

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
