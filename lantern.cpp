#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <stdio.h>
#include <tuple>

#ifdef _WIN32                                                       
#define lntrnDebugBreak() __debugbreak()                                 
#elif defined __linux__                                             
#define lntrnDebugBreak() __builtin_trap()
#else
#define lntrnDebugBreak()
#endif  

#define ExprToString(expr) #expr

#define STACK_CAP 64

#define PanicOnError(err, err_type, token_pos, ...) {           \
    if(err) {                                                   \
        printf("Lantern: Error on line %i:%i: ",                 \
                std::get<0>(token_pos), std::get<1>(token_pos));\
        printf(__VA_ARGS__);                                    \
        printf("\n");                                           \
        printf("Error Type: %s | Error Code: %i\n",             \
            ExprToString(err_type), (int)err_type);             \
        lntrnDebugBreak();                                      \
    }                                                           \
}

static std::string interpretingFilepath = "";

enum class TokenType {
    Plus = 0,
    Minus,
    Multiply,
    Divide,
    Equals,
    NotEqual,
    Not,
    If,
    EndIf,
    Else,
    While,
    RunWhile,
    EndWhile,
    GreaterThan,
    LessThan,
    GreaterThanOrEqual,
    LessThanOrEqual,
    Prev,
    PushToStack,
    OperationResult,
    Print,
    PrintLine,
    MemSet,
    MemGet,
};

enum class TokenRuntimeType {
    Int = 0,
    String,
    MaxTypes
};
enum class InterpretationError {
    StackOverflow = 0,
    StackUnderflow,
    SyntaxError,
    IllegalOperation,
    IllegalInstruction,
    InvalidDataType,
    IllegalHeapAccess
};

struct Token {
    Token() = default;
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
            if(IsStringLiteral(word) && word.size() > 1) {
                addingLiteral = false;
                words.push_back(word);
                continue;
            }
            if((word.front() == '"' || word == "\"") && !addingLiteral) {
                addingLiteral = true;
                words.push_back(stringLiterals[strLiteralCount++]);
                continue;
            } 
            if(!addingLiteral) {
                words.push_back(word);
            }
            if(word.back() == '"' || word == "\"") {
                addingLiteral = false;
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

std::tuple<int32_t, int32_t> GetTokenPositionInFile(uint32_t tokenIndex) {
    std::ifstream file(interpretingFilepath);
    std::string line;
    uint32_t lineIndex = 0;
    uint32_t currentTokenIndex = 0;
    while(std::getline(file, line)) {
        uint32_t wordIndex = 0;
        lineIndex++;

        std::string word;
        std::stringstream lineSS(line);

        while(lineSS >> word) {
            wordIndex++;
            
            if(currentTokenIndex == tokenIndex) {
                return std::tuple<int32_t, int32_t>(lineIndex, wordIndex);
            }
            currentTokenIndex++;
        }
    }
    file.close();

    return std::tuple<int32_t, int32_t>(-1, -1);
}
template<typename T>
int32_t GetIndexOfNthOccurrenceOfElement(const std::vector<T>& vals, const T& searchVal, uint32_t n) { 
    uint32_t i = 0;
    int32_t occurrenceCount = 0;
    for(auto& val : vals) {
        if(val == searchVal && occurrenceCount == (int32_t)n) {
            return i;
        } else if(val == searchVal) {
            occurrenceCount++;
        }
        i++;
    }
    return -1;
}

const std::vector<Token> GenerateProgramFromFile(const std::string& filepath) {
    std::vector<Token> program;
    const std::vector<std::string> tokenWords = GetTokenWordsFromFile(filepath);

    std::vector<int> ifToElseMap;

    for(uint32_t i = 0; i < tokenWords.size(); i++) {
        std::string token = tokenWords[i];
        if(token == "if") {
            bool foundElse = false;
            for(uint32_t j = i; j < tokenWords.size(); j++) {
                if(tokenWords[j] == "else") {
                    ifToElseMap.push_back(j);
                    foundElse = true;
                    break;
                }
            }
            if(!foundElse) {
                ifToElseMap.push_back(-1);
            }
        }
    }
    uint32_t ifStatementCount = 0;
    uint32_t whileLoopCount = 0;
    for(uint32_t i = 0; i < tokenWords.size(); i++) {
        std::string token = tokenWords[i];
        if(IsStringNumber(token)) {
            int32_t data = atoi(token.c_str());
            Token tok = Token(TokenType::PushToStack);
            tok.SetData<int32_t>(data);
            tok.SetRuntimeType(TokenRuntimeType::Int);
            program.push_back(tok);
            continue;
        }
        if(IsStringLiteral(token)) {
            Token tok = Token(TokenType::PushToStack);
            token.erase(0, 1);
            token.erase(token.size() - 1);
            tok.SetData<std::string>(token);
            tok.SetRuntimeType(TokenRuntimeType::String);
            program.push_back(tok);
            continue;
        }

        if(token == "+") {
            program.push_back(Token(TokenType::Plus));
        }
        else if(token == "-") {
            program.push_back(Token(TokenType::Minus));
        }
        else if(token == "*") {
            program.push_back(Token(TokenType::Multiply));
        }
        else if(token == "/") {
            program.push_back(Token(TokenType::Divide));
        }
        else if(token == "print") {
            program.push_back(Token(TokenType::Print));
        }
        else if(token == "println") {
            program.push_back(Token(TokenType::PrintLine));
        }
        else if(token == "==") {
            program.push_back(Token(TokenType::Equals));
        }
        else if(token == "!=") {
            program.push_back(Token(TokenType::NotEqual));
        }
        else if(token == ">") {
            program.push_back(Token(TokenType::GreaterThan));
        }
        else if(token == "!") {
            program.push_back(Token(TokenType::Not));
        }
        else if(token == "<") {
            program.push_back(Token(TokenType::LessThan));
        }
        else if(token == ">=") {
            program.push_back(Token(TokenType::GreaterThanOrEqual));
        }
        else if(token == "<=") {
            program.push_back(Token(TokenType::LessThanOrEqual));
        }
        else if(token == "mset") {
            program.push_back(Token(TokenType::MemSet));
        }
        else if(token == "mget") {
            program.push_back(Token(TokenType::MemGet));
        }
        else if(token == "endi") {
            program.push_back(Token(TokenType::EndIf));
        }
        else if(token == "endw") {
            Token tok = Token(TokenType::EndWhile);
            int32_t whileTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "while", whileLoopCount - 1);
            PanicOnError(whileTokenIndex == -1, InterpretationError::SyntaxError, GetTokenPositionInFile(i), "While-End-Token without run-while-token loop.");
            tok.SetData<int32_t>(whileTokenIndex);
            program.push_back(tok);
        }
        else if(token == "run") {
            Token tok = Token(TokenType::RunWhile);
            int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endw", whileLoopCount - 1);
            PanicOnError(endTokenIndex == -1, InterpretationError::SyntaxError, GetTokenPositionInFile(i), "While loop without end token.");
            tok.SetData<int32_t>(endTokenIndex);
            program.push_back(tok);
        }
        else if(token == "while") {
            program.push_back(Token(TokenType::While));
            whileLoopCount++;
        }
        else if(token == "prev") { 
            program.push_back(Token(TokenType::Prev));
        }
        else if(token == "if") {
            int32_t elseTokenIndex = ifToElseMap[ifStatementCount];
            if(elseTokenIndex != -1) {
                Token tok = Token(TokenType::If);
                int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endi", ifStatementCount);
                PanicOnError(endTokenIndex == -1, InterpretationError::SyntaxError, GetTokenPositionInFile(i), "If statement without end token.");
                std::vector<int32_t> indices = {endTokenIndex, elseTokenIndex};
                tok.SetData<std::vector<int32_t>>(indices);
                program.push_back(tok);
            } else {
                Token tok = Token(TokenType::If);
                int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endi", ifStatementCount);
                PanicOnError(endTokenIndex == -1, InterpretationError::SyntaxError, GetTokenPositionInFile(i), "If statement without end token.");
                std::vector<int32_t> indices = {endTokenIndex};
                tok.SetData<std::vector<int32_t>>(indices);
                program.push_back(tok);
            }
            ifStatementCount++;
        }
        else if(token == "else") {  
            int32_t endTokenIndex = GetIndexOfNthOccurrenceOfElement<std::string>(tokenWords, "endi", ifStatementCount - 1);
            PanicOnError(endTokenIndex == -1, InterpretationError::SyntaxError, GetTokenPositionInFile(i), "Else statement without end token or if statement.");
    
            Token tok = Token(TokenType::Else);
            tok.SetData<int32_t>(endTokenIndex);
            program.push_back(tok);
        } else {
            PanicOnError(true, InterpretationError::IllegalInstruction, GetTokenPositionInFile(i), "Invalid instruction: %s.", token.c_str());
        }
    }
    return program;
}

void InterpreteProgram(const std::string& filepath) {
    std::vector<Token> program = GenerateProgramFromFile(filepath);
    std::vector<Token> stack;
    std::vector<Token> heap;
    heap.reserve(8);
    stack.reserve(STACK_CAP);

    for(uint32_t i = 0; i < program.size(); i++) {
        auto& token = program[i];
        PanicOnError(stack.size() >= STACK_CAP, InterpretationError::StackOverflow, GetTokenPositionInFile(i), "Stack is overflowed.");
        if(token.Type == TokenType::PushToStack) { 
                stack.push_back(token);
        }
        if(token.Type == TokenType::Plus || token.Type == TokenType::Minus
                || token.Type == TokenType::Multiply || token.Type == TokenType::Divide) {
            if(!stack.empty()) {
                Token a = stack[stack.size() - 1];
                Token b = stack[stack.size() - 2];
                if(a.RuntimeType == TokenRuntimeType::Int && b.RuntimeType == TokenRuntimeType::Int) {
                    stack.pop_back();
                    stack.pop_back();

                    int32_t result = 0;
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
                    tok.RuntimeType = TokenRuntimeType::Int;
                    stack.push_back(tok);
                } else if(a.RuntimeType == TokenRuntimeType::String && b.RuntimeType == TokenRuntimeType::String) {
                    stack.pop_back();
                    stack.pop_back();
                    
                    std::string result = "";
                    if(token.Type == TokenType::Plus) {
                        result = b.RawData<std::string>() + a.RawData<std::string>();
                    }
                    Token tok = Token(TokenType::OperationResult);
                    tok.SetData<std::string>(result);
                    tok.RuntimeType = TokenRuntimeType::String;
                    stack.push_back(tok);
                } else {
                    PanicOnError(a.RuntimeType != b.RuntimeType, InterpretationError::InvalidDataType,
                        GetTokenPositionInFile(i),
                        "Operation with different data types.");
                    PanicOnError(a.RuntimeType > TokenRuntimeType::MaxTypes || b.RuntimeType > TokenRuntimeType::MaxTypes, 
                        InterpretationError::InvalidDataType,
                        GetTokenPositionInFile(i),
                        "Operation with unknown data type.");
                }
            }
        }
        if(token.Type == TokenType::Equals || 
                token.Type == TokenType::NotEqual) {
                PanicOnError(stack.size() < 2, InterpretationError::StackUnderflow,
                        GetTokenPositionInFile(i),
                        "Equality check with less than 2 values on stack.");
                Token a = stack.back();
                stack.pop_back();
                Token b = stack.back();
                stack.pop_back();
                if(token.Type == TokenType::Equals) {
                    Token tok = Token(TokenType::OperationResult);
                    if(a.RuntimeType == TokenRuntimeType::Int && b.RuntimeType == TokenRuntimeType::Int) {
                        tok.SetData<int32_t>((a.RawData<int32_t>() == b.RawData<int32_t>()));
                        tok.RuntimeType = TokenRuntimeType::Int; 
                    } else if(a.RuntimeType == TokenRuntimeType::String && b.RuntimeType == TokenRuntimeType::String) {
                        tok.SetData<int32_t>((a.RawData<std::string>() == b.RawData<std::string>()));
                        tok.RuntimeType = TokenRuntimeType::String; 
                    } else {
                        PanicOnError(a.RuntimeType != b.RuntimeType, InterpretationError::InvalidDataType, 
                            GetTokenPositionInFile(i),
                            "Equality check with different data types.");
                        PanicOnError(a.RuntimeType > TokenRuntimeType::MaxTypes || b.RuntimeType > TokenRuntimeType::MaxTypes, 
                            InterpretationError::InvalidDataType,
                            GetTokenPositionInFile(i),
                            "Equality chek with unknown data type.");
                    }
                    stack.push_back(tok);
                }
                else {
                    Token tok = Token(TokenType::OperationResult);
                    if(a.RuntimeType == TokenRuntimeType::Int && b.RuntimeType == TokenRuntimeType::Int) {
                        tok.SetData<int32_t>((a.RawData<int32_t>() != b.RawData<int32_t>()));
                        tok.RuntimeType = TokenRuntimeType::Int; 
                    } else if(a.RuntimeType == TokenRuntimeType::String && b.RuntimeType == TokenRuntimeType::String) {
                        tok.SetData<int32_t>((a.RawData<std::string>() != b.RawData<std::string>()));
                        tok.RuntimeType = TokenRuntimeType::String; 
                    } else {
                        PanicOnError(a.RuntimeType != b.RuntimeType, InterpretationError::InvalidDataType,
                            GetTokenPositionInFile(i),
                            "Equality check with different data types.");
                        PanicOnError(a.RuntimeType > TokenRuntimeType::MaxTypes || b.RuntimeType > TokenRuntimeType::MaxTypes, 
                            InterpretationError::InvalidDataType,
                            GetTokenPositionInFile(i),
                            "Equality chek with unknown data type.");
                    }
                    stack.push_back(tok);
                } 
        }
        if(token.Type == TokenType::Prev) {
            PanicOnError(stack.size() < 2, InterpretationError::StackUnderflow, 
                    GetTokenPositionInFile(i),
                    "Tried to retrieve previous value stack with empty stack.");
            Token index = stack.back();
            stack.pop_back();

            Token prev = stack[stack.size() - 1 - index.RawData<int32_t>()];
            stack.push_back(prev);
        }
        if(token.Type == TokenType::GreaterThan ||
                token.Type == TokenType::LessThan ||
                token.Type == TokenType::GreaterThanOrEqual ||
                token.Type == TokenType::LessThanOrEqual) {
            Token a = stack[stack.size() - 1];
            Token b = stack[stack.size() - 2];
            if(a.RuntimeType == TokenRuntimeType::Int && b.RuntimeType == TokenRuntimeType::Int) {
                stack.pop_back();
                stack.pop_back();
                Token res = Token(TokenType::OperationResult);
                if(token.Type == TokenType::GreaterThan) {
                    res.SetData<int32_t>(b.RawData<int32_t>() > a.RawData<int32_t>());
                } else if(token.Type == TokenType::LessThan) {
                    res.SetData<int32_t>(b.RawData<int32_t>() < a.RawData<int32_t>());
                } else if(token.Type == TokenType::GreaterThanOrEqual) {
                    res.SetData<int32_t>(b.RawData<int32_t>() >= a.RawData<int32_t>());
                } else if(token.Type == TokenType::LessThanOrEqual) {
                    res.SetData<int32_t>(b.RawData<int32_t>() <= a.RawData<int32_t>());
                }
                res.RuntimeType = TokenRuntimeType::Int; 
                stack.push_back(res);
            } else {
                    PanicOnError(a.RuntimeType != b.RuntimeType, InterpretationError::InvalidDataType,
                        GetTokenPositionInFile(i),
                        "Size operation with different data types.");
                    PanicOnError(a.RuntimeType > TokenRuntimeType::Int || b.RuntimeType > TokenRuntimeType::Int, 
                        InterpretationError::InvalidDataType,
                        GetTokenPositionInFile(i),
                        "Size operation with non-integer data type.");
            }
        }
        if(token.Type == TokenType::If || token.Type == TokenType::RunWhile) {
            PanicOnError(stack.empty(), InterpretationError::StackUnderflow, 
                    GetTokenPositionInFile(i),
                    "If statement without condition.");
            Token val = stack.back();
            stack.pop_back();
            if(!val.RawData<int32_t>()) {
                if(token.Type == TokenType::If) {
                    if(token.RawData<std::vector<int32_t>>().size() == 1) {
                        i = token.RawData<std::vector<int32_t>>()[0];
                    } else if(token.RawData<std::vector<int32_t>>().size() == 2) {
                        i = token.RawData<std::vector<int32_t>>()[1];
                    }
                } else {
                    i = token.RawData<int32_t>();
                }
            }   
        }
        if(token.Type == TokenType::Else) {
                i = token.RawData<int32_t>();
        }
        if(token.Type == TokenType::EndWhile) {
            i = token.RawData<int32_t>();
        }
        if(token.Type == TokenType::Not) {
            PanicOnError(stack.empty() || stack[stack.size() - 1].RuntimeType != TokenRuntimeType::Int, 
                InterpretationError::StackUnderflow,
                GetTokenPositionInFile(i),
                "Used ! token without valid value on stack.");
            stack[stack.size() - 1].SetData<int32_t>(!stack[stack.size() - 1].RawData<int32_t>());
        }
        if(token.Type == TokenType::MemGet) {
            PanicOnError(stack.empty(), InterpretationError::StackUnderflow,
                GetTokenPositionInFile(i),
                "Used mget without specified index.");
            Token index = stack.back();
            PanicOnError(index.RuntimeType != TokenRuntimeType::Int, InterpretationError::InvalidDataType,
                GetTokenPositionInFile(i),
                "Non-integer value specified as index for mget.");
            PanicOnError(index.RawData<int32_t>() > (int32_t)heap.capacity(), InterpretationError::IllegalHeapAccess,
                GetTokenPositionInFile(i),
                "Specified index of mget is out of bounds of heap.");
            stack.pop_back();
            Token res = heap[index.RawData<int32_t>()];
            res.Type = TokenType::OperationResult;
            stack.push_back(res);
        }
        if(token.Type == TokenType::MemSet) {
            PanicOnError(stack.size() < 2, InterpretationError::StackUnderflow,
                    GetTokenPositionInFile(i),
                "Too few values for mset (usage: [index] [val] mset");
            Token index = stack[stack.size() - 2];
            PanicOnError(index.RuntimeType != TokenRuntimeType::Int, InterpretationError::InvalidDataType,
                GetTokenPositionInFile(i),
                "Non-integer value specified as index for mset.");
            PanicOnError(index.RawData<int32_t>() > (int32_t)heap.capacity(), InterpretationError::IllegalHeapAccess,
                GetTokenPositionInFile(i),
                "Specified index of mset is out of bounds of heap.");
            Token val = stack[stack.size() - 1];
            if(index.RawData<int32_t>() < (int32_t)heap.size()) {
                PanicOnError(val.RuntimeType != heap[index.RawData<int32_t>()].RuntimeType,
                    InterpretationError::InvalidDataType, 
                    GetTokenPositionInFile(i),
                    "Tried to set data type of heap value to different data type.");
            }
            stack.pop_back();
            stack.pop_back();
            if(heap.size() >= heap.capacity()) {
                heap.resize(heap.size() * 2);
            }
            heap[index.RawData<int32_t>()] = val;
            
        }
        if(token.Type == TokenType::Print || token.Type == TokenType::PrintLine) {
            PanicOnError(stack.empty(), InterpretationError::StackUnderflow,
                    GetTokenPositionInFile(i),
                    "Used print token without value on stack.");
            Token tok = stack.back();
            stack.pop_back();
            if(tok.RuntimeType == TokenRuntimeType::Int) {
                int32_t data = tok.RawData<int32_t>();
                if(token.Type == TokenType::PrintLine) 
                    std::cout << data << std::flush << "\n";
                else
                    std::cout << data << std::flush;
            } else if(tok.RuntimeType == TokenRuntimeType::String) {
                std::string data = tok.RawData<std::string>();
                if(token.Type == TokenType::PrintLine) 
                    std::cout << data << std::flush << "\n";
                else
                    std::cout << data << std::flush;
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
    interpretingFilepath = argv[1];
    InterpreteProgram(argv[1]);

    return EXIT_SUCCESS;
}
