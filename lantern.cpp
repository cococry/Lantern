#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <sstream>
#include <assert.h>

enum class TokenType {
    Plus = 0,
    Minus,
    Multiply,
    Divide,
    Equals,
    NotEqual,
    If,
    EndIf,
    Else,
    While,
    RunWhile,
    EndWhile,
    GreaterThan,
    LessThan,
    Prev,
    PushToStack,
    OperationResult,
    Print,
};

enum class TokenRuntimeType {
    Int = 0,
    String
};
struct Token {
    Token(TokenType type, const std::shared_ptr<void>& data = nullptr) 
        : Data(data), Type(type) {
    }
    template<typename T>
    void SetData(T value) {
        Data = std::make_shared<T>(value);
    }
    void SetRuntimeType(TokenRuntimeType runtimeType) {
        RuntimeType = runtimeType;
    }
    
    template<typename T>
    T RawData() {
        return *std::static_pointer_cast<T>(Data);
    }

    std::shared_ptr<void> Data;
    TokenType Type;
    TokenRuntimeType RuntimeType;
};

bool IsStringLiteral(const std::string& str) {
    return str.front() == '"' && str.back() == '"';
}


const std::vector<std::string> GetTokenWordsFromFile(const std::string& filepath) {
    std::string line;
    std::vector<std::string> words;

    std::ifstream file(filepath);
    if(!file) {
        std::cout << "Lantern: Failed to open file at '" << filepath << "'\n";
        return {};
    }
    while(std::getline(file, line)) {
        bool foundStrLiteralBegin = false;
        std::string strLiteral = "";
        std::vector<std::string> stringLiterals;
        for(char c : line) {
            if(c == '"' && foundStrLiteralBegin) {
                strLiteral += c;
                stringLiterals.push_back(strLiteral);
                strLiteral = "";
                foundStrLiteralBegin = false;
                continue;
            }
            if(c == '"' && !foundStrLiteralBegin) {
                foundStrLiteralBegin = true;
            }
            if(foundStrLiteralBegin) {
                strLiteral += c;
            }
        }
        std::stringstream lineSS(line);
        std::string word;
        uint32_t strLiteralCount = 0;
        bool addingLiteral = false;
        while(lineSS >> word) {
            if(IsStringLiteral(word)) {
                words.push_back(word);
                strLiteralCount++;
            } else {
                if(word.front() == '"' && word.size() > 1) {
                    words.push_back(stringLiterals[strLiteralCount++]);         
                    addingLiteral = true;
                } else if(word.find('"') < word.size() && addingLiteral) {
                    addingLiteral = false;
                }
            }
            if(!(word.find('"') < word.size()) && !addingLiteral) {
                words.push_back(word);
            }
        }
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

template<typename T>
int32_t GetIndexOfNthOccurrenceOfElement(const std::vector<T>& vals, const T& search_val, uint32_t n) { 
    uint32_t i = 0;
    int32_t occurrence_count = 0;
    for(auto& val : vals) {
        if(val == search_val && occurrence_count == n) {
            return i;
        } else if(val == search_val) {
            occurrence_count++;
        }
        i++;
    }
    return -1;
}

const std::vector<Token> GenerateProgramFromFile(const std::string& filepath) {
    std::vector<Token> program;
    const std::vector<std::string> tokenWords = GetTokenWordsFromFile(filepath);

    uint32_t if_statement_count = 0;
    uint32_t while_loop_count = 0;
    for(auto token : tokenWords) {
        if(IsStringNumber(token)) {
            int32_t data = atoi(token.c_str());
            Token tok = Token(TokenType::PushToStack);
            tok.SetData<int32_t>(data);
            tok.SetRuntimeType(TokenRuntimeType::Int);
            program.push_back(tok);
        }
        if(IsStringLiteral(token)) {
            Token tok = Token(TokenType::PushToStack);
            token.erase(0, 1);
            token.erase(token.size() - 1);
            tok.SetData<std::string>(token);
            tok.SetRuntimeType(TokenRuntimeType::String);
            program.push_back(tok);
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
        if(token == "==") {
            program.push_back(Token(TokenType::Equals));
        }
        if(token == "!=") {
            program.push_back(Token(TokenType::NotEqual));
        }
        if(token == ">") {
            program.push_back(Token(TokenType::GreaterThan));
        }
        if(token == "<") {
            program.push_back(Token(TokenType::LessThan));
        }
        if(token == "endi") {
            program.push_back(Token(TokenType::EndIf));
        }
        if(token == "endw") {
            Token tok = Token(TokenType::EndWhile);
            int32_t whileTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "while", while_loop_count - 1);
            assert(whileTokenIndex != -1 && "While-End-Token without run-while-token loop.");
            tok.SetData<int32_t>(whileTokenIndex);
            program.push_back(tok);
        }
        if(token == "run") {
            Token tok = Token(TokenType::RunWhile);
            int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endw", while_loop_count - 1);
            assert(endTokenIndex != -1 && "While loop without end token.");
            tok.SetData<int32_t>(endTokenIndex + 1);
            program.push_back(tok);
        }
        if(token == "while") {
            program.push_back(Token(TokenType::While));
            while_loop_count++;
        }
        if(token == "prev") { 
            program.push_back(Token(TokenType::Prev));
        }
        if(token == "if") {
            int32_t elseTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "else", if_statement_count);
            if(elseTokenIndex != -1) {
                Token tok = Token(TokenType::If);
                tok.SetData<int32_t>(elseTokenIndex);
                program.push_back(tok);
            } else {
                int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endi", if_statement_count);
                assert(endTokenIndex != -1 && "If statement without end token.");
                Token tok = Token(TokenType::If);
                tok.SetData<int32_t>(endTokenIndex);
                program.push_back(tok);
                if_statement_count++;
            }
        }
        if(token == "else") {  
            int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endi", if_statement_count);
            assert(endTokenIndex != -1 && "Else statement without end token or if statement.");

            Token tok = Token(TokenType::Else);
            tok.SetData<int32_t>(endTokenIndex);
            program.push_back(tok);
        }

    }
    return program;
}

void InterpreteProgram(const std::string& filepath) {
    std::vector<Token> program = GenerateProgramFromFile(filepath);
    std::vector<Token> stack = {};
    for(uint32_t i = 0; i < program.size(); i++) {
        auto& token = program[i];
        if(token.Type == TokenType::PushToStack) { 
                stack.push_back(token);
        }
        if(token.Type == TokenType::Plus || token.Type == TokenType::Minus
                || token.Type == TokenType::Multiply || token.Type == TokenType::Divide) {
            if(!stack.empty()) {
                if(token.RuntimeType == TokenRuntimeType::Int) {
                    Token a = stack.back();
                    stack.pop_back();
                    Token b = stack.back();
                    stack.pop_back();

                    int32_t result;
                    if(token.Type == TokenType::Plus)
                        result = a.RawData<int32_t>() + b.RawData<int32_t>();
                    else if(token.Type == TokenType::Minus)
                        result = b.RawData<int32_t>() - a.RawData<int32_t>();
                    else if(token.Type == TokenType::Multiply)
                        result = a.RawData<int32_t>() * b.RawData<int32_t>();
                    else if(token.Type == TokenType::Divide)
                        result = b.RawData<int32_t>() / a.RawData<int32_t>();
                    Token tok = Token(TokenType::OperationResult); 
                    tok.SetData<int32_t>(result);
                    stack.push_back(tok);
                } else {
                    Token a = stack.back();
                    stack.pop_back();
                    Token b = stack.back();
                    stack.pop_back();
                    
                    std::string result;
                    if(token.Type == TokenType::Plus) {
                        result = b.RawData<std::string>() + a.RawData<std::string>();
                    }
                    Token tok = Token(TokenType::OperationResult);
                    tok.SetData<std::string>(result);
                    std::cout << result << "\n";
                    stack.push_back(tok);
                }
            }
        }
        if(token.Type == TokenType::Equals || 
                token.Type == TokenType::NotEqual) {
                Token a = stack.back();
                stack.pop_back();
                Token b = stack.back();
                stack.pop_back();
                if(token.Type == TokenType::Equals) {
                    Token tok = Token(TokenType::OperationResult);
                    if(token.RuntimeType == TokenRuntimeType::Int) {
                        tok.SetData<int32_t>((a.RawData<int32_t>() == b.RawData<int32_t>()));
                    } else if(token.RuntimeType == TokenRuntimeType::String) {
                        tok.SetData<int32_t>((a.RawData<std::string>() == b.RawData<std::string>()));
                    }
                    stack.push_back(tok);
                }
                else {
                    Token tok = Token(TokenType::OperationResult);
                    if(token.RuntimeType == TokenRuntimeType::Int) {
                    tok.SetData<int32_t>((a.RawData<int32_t>() != b.RawData<int32_t>()));
                    } else if(token.RuntimeType == TokenRuntimeType::String) {
                        tok.SetData<int32_t>((a.RawData<std::string>() != b.RawData<std::string>()));
                    }
                    stack.push_back(tok);
                } 
        }
        if(token.Type == TokenType::Prev) {
            Token index = stack.back();
            stack.pop_back();
            Token prev = stack[stack.size() - index.RawData<int32_t>() - 1];
            stack.push_back(prev);
        }
        if(token.Type == TokenType::GreaterThan ||
                token.Type == TokenType::LessThan) {
            if(token.RuntimeType == TokenRuntimeType::Int) {
                Token a = stack.back();
                stack.pop_back();
                Token b = stack.back();
                stack.pop_back();

                Token res = Token(TokenType::OperationResult);
                if(token.Type == TokenType::GreaterThan) {
                    res.SetData<int32_t>(b.RawData<int32_t>() > a.RawData<int32_t>());
                }
                else
                    res.SetData<int32_t>(b.RawData<int32_t>() < a.RawData<int32_t>());
                stack.push_back(res);
            }
        }
        if(token.Type == TokenType::If) {
            Token val = stack.back();
            stack.pop_back();
            if(!val.RawData<int32_t>()) {
                i = token.RawData<int32_t>();
            }
        }
        if(token.Type == TokenType::EndWhile) {
            i = token.RawData<int32_t>();
        }
        if(token.Type == TokenType::RunWhile) {
            Token val = stack.back();
            stack.pop_back();

            if(!val.RawData<int32_t>()) {
                if(token.RawData<int32_t>() > program.size()) 
                    break;
                i = token.RawData<int32_t>();
            }
        }
        if(token.Type == TokenType::Else) {
            i = token.RawData<int32_t>() - 1;
        }
        if(token.Type == TokenType::Print) {
            if(!stack.empty()) {
                Token tok = stack.back();
                if(tok.RuntimeType == TokenRuntimeType::Int) {
                    int32_t data = tok.RawData<int32_t>();
                    stack.pop_back();
                    std::cout << data << "\n";
                } else {
                    std::string data = tok.RawData<std::string>();
                    stack.pop_back();
                    std::cout << data << "\n";
                }
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
