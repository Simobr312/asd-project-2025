#include <bits/stdc++.h>

//https://www.bnlearn.com/bnrepository/
//http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm

class Token {
    public:
        enum Type {KEYWORD, WORD, SYMBOL, DECIMAL_LITERAL, FLOATING_POINT_LITERAL, END, TEXT, IGNORE};
        Type type;
        std::string value;

        inline static const std::regex ignore_regex = std::regex(R"(^(\s+|,+|\|+|\"|/\*.*?\*/))");

        inline static const std::vector<std::pair<Type, std::regex>> Spec = {
            {KEYWORD,    std::regex(R"(^\b(network|variable|probability|property|type|discrete|default|table)\b)")},
            {WORD, std::regex(R"(^[a-zA-Z_][a-zA-Z0-9_]*)")},
            {SYMBOL,     std::regex(R"(^[{}()[\];=,`|])")},
            {DECIMAL_LITERAL,     std::regex(R"(^\d+(?!\.|[eE]))")},
            {FLOATING_POINT_LITERAL,     std::regex(R"(^((\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?))")}
        };

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

    public:
        Lexer(std::ifstream& input) 
            : source(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()),
                cursor(source.begin()) {}

        Token getNextToken() {         //I could simplify the structure by removing the window
            std::smatch match_result;

            if(cursor == source.end())
                return Token(Token::END, "");

            std::string remaining = std::string(cursor, source.end());

            std::regex_search(remaining, match_result, Token::ignore_regex);
            if(!match_result.empty()) {
                cursor += match_result.str().size();
                return getNextToken();
            }

            for(auto [type, regex] : Token::Spec) {
                std::regex_search(remaining, match_result, regex);

                if(match_result.empty())
                    continue;
                
                std::string value = match_result.str();
                cursor += value.size();
                return Token(type, value);
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

    std::string filename;

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
