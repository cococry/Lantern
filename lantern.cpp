#include <iostream>
#include <vector>
#include <string>
#include <fstream>

enum class TokenType {
    Plus = 0,
    Minus,
    Multiply,
    Divide,
    Equals,
    PushToStack,
    OperationResult,
    Print,
};

struct Token {
    Token(TokenType type, int32_t data = -1) 
        : Data(data), Type(type) {
    }
    int32_t Data;
    TokenType Type;
};

const std::vector<std::string> GetTokenWordsFromFile(const std::string& filepath) {
    std::string word;
    std::vector<std::string> words;
    std::ifstream file(filepath);
    if(!file) {
        std::cout << "Lantern: Failed to open file at '" << filepath << "'\n";
        return {};
    }
    while(file >> word) {
        words.push_back(word);
    }

    file.close();
    
    return words;
}

bool IsStringNumber(const std::string& str) {
    std::string::const_iterator it = str.begin();
    while(it != str.end() && std::isdigit(*it))
        it++;
    return !str.empty() && it == str.end();
}

const std::vector<Token> GenerateProgramFromFile(const std::string& filepath) {
    std::vector<Token> program;
    const std::vector<std::string> tokenWords = GetTokenWordsFromFile(filepath);

    for(auto& token : tokenWords) {
        if(IsStringNumber(token)) {
            int32_t data = atoi(token.c_str());
            program.push_back(Token(TokenType::PushToStack, data));
        }
        if(token == "+") {
            program.push_back(Token(TokenType::Plus));
        }
        if(token == "-") {
            program.push_back(Token(TokenType::Minus));
        }
        if(token == "*") {
            program.push_back(Token(TokenType::Multiply));
        }
        if(token == "/") {
            program.push_back(Token(TokenType::Divide));
        }
        if(token == "print") {
            program.push_back(Token(TokenType::Print));
        }
        if(token == "=") {
            program.push_back(Token(TokenType::Equals));
        }
    }
    return program;
}

void InterpreteProgram(const std::string& filepath) {
    const std::vector<Token> program = GenerateProgramFromFile(filepath);
    std::vector<Token> stack = {};
    for(uint32_t i = 0; i < program.size(); i++) {
        auto& token = program[i];
        if(token.Type == TokenType::PushToStack) {
                stack.push_back(token);
        }
        if(token.Type == TokenType::Plus || token.Type == TokenType::Minus
                || token.Type == TokenType::Multiply || token.Type == TokenType::Divide) {
            if(!stack.empty()) {
                Token a = stack.back();
                stack.pop_back();
                Token b = stack.back();
                stack.pop_back();

                int32_t result;
                if(token.Type == TokenType::Plus)
                    result = a.Data + b.Data;
                else if(token.Type == TokenType::Minus)
                    result = b.Data - a.Data;
                else if(token.Type == TokenType::Multiply)
                    result = a.Data * b.Data;
                else if(token.Type == TokenType::Divide)
                    result = b.Data / a.Data;
                stack.push_back(Token(TokenType::OperationResult, result));
            }
        }
        if(token.Type == TokenType::Equals) {
            Token a = stack.back();
            stack.pop_back();
            Token b = stack.back();
            stack.pop_back();

            stack.push_back(Token(TokenType::OperationResult, (a.Data == b.Data) ? 1 : 0));
        }
        if(token.Type == TokenType::Print) {
                if(!stack.empty()) {
                    int32_t data = stack.back().Data;
                    stack.pop_back();
                    std::cout << data << "\n";
                }
        } 
    }
}

int main(int argc, char** argv) {
    if(argc > 2) {
        std::cout << "Lantern: Too many arguments suplied.\n";
        std::cout << "Usage: lantern <filepath>\n";
        return EXIT_FAILURE;
    }

    if(argc < 2) {
        std::cout << "Lantern: Too few arguments suplied.\n";
        std::cout << "Usage: lantern <filepath>\n";
        return EXIT_FAILURE;
    }
    InterpreteProgram(argv[1]);

    return EXIT_SUCCESS;
}
