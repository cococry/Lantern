#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#define STACK_CAP 64
#define PROGRAM_CAP 1024
         
#define PANIC_ON_ERR(cond, err_type, ...)  {                                            \
    if(cond) {                                                                          \
        printf("Lantern: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type);  \
         printf(__VA_ARGS__);                                                           \
         printf("\n");                                                                  \
    }                                                                                   \
}                                                                                       \

typedef enum {
    INST_STACK_PUSH, INST_STACK_PREV,
    INST_PLUS, INST_MINUS, INST_MUL, INST_DIV,
    INST_EQ,INST_NEQ, 
    INST_GT,INST_LT,INST_GEQ,INST_LEQ,
    INST_IF,INST_ELSE,INST_ENDIF,
    INST_WHILE, INST_RUN_WHILE, INST_END_WHILE,
    INST_PRINT,
    INST_JUMP,
} Instruction;

typedef enum {
    ERR_STACK_OVERFLOW,
    ERR_STACK_UNDERFLOW,
    ERR_INVALID_JUMP,
    ERR_INVALID_STACK_ACCESS,
    ERR_ILLEGAL_INSTRUCTION,
    ERR_SYNTAX_ERROR,
} Error;

typedef struct {
    Instruction inst;
    int32_t data;
} Token;

typedef struct {
    int32_t stack[STACK_CAP];
    int32_t stack_size;

    uint32_t inst_ptr;
} ProgramState;

bool is_str_int(const char* str) {
    for(uint32_t i = 0; i < strlen(str); i++) {
        if(!isdigit(str[i])) return false;
    }
    return true;
}
Token* load_program_from_file(const char* filepath, uint32_t* program_size) {
    FILE* file_count_words;
    file_count_words = fopen(filepath, "r");
    if(!file_count_words) {
        printf("Lantern: [Error]: Cannot read file '%s'.\n", filepath);
        return NULL;
    }
    uint32_t words_in_file = 1;
    char ch;
    bool on_word = false;
    while ((ch = fgetc(file_count_words)) != EOF) {
        if(ch == ' ' || ch == '\t' || ch == '\0' || ch == '\n') {
            if (on_word) {
                on_word = false;
                words_in_file++;
            }
        } else {
            on_word = true;
        }
    }
    fclose(file_count_words);

    *program_size = words_in_file;
    
    FILE* file;
    file = fopen(filepath, "r");

    Token* program = malloc(sizeof(Token) * words_in_file);
    char word[128];
    uint32_t i = 0;
    while(fscanf(file, "%s", word) != EOF) {
        if(is_str_int(word)) {
            program[i] = (Token){ .inst = INST_STACK_PUSH, .data = atoi(word) };
            i++;
            continue;
        }
        if(strcmp(word, "prev") == 0) {
            program[i] = (Token){ .inst = INST_STACK_PREV };
        } else if(strcmp(word, "+") == 0) {
            program[i] = (Token){ .inst = INST_PLUS };
        } else if(strcmp(word, "-") == 0) {
            program[i] = (Token){ .inst = INST_MINUS };
        } else if(strcmp(word, "*") == 0) {
            program[i] = (Token){ .inst = INST_MUL };
        } else if(strcmp(word, "/") == 0) {
            program[i] = (Token){ .inst = INST_DIV };
        } else if(strcmp(word, "*") == 0) {
            program[i] = (Token){ .inst = INST_MUL };
        } else if(strcmp(word, "==") == 0) {
            program[i] = (Token){ .inst = INST_EQ };
        } else if(strcmp(word, ">") == 0) {
            program[i] = (Token){ .inst = INST_GT };
        } else if(strcmp(word, "<") == 0) {
            program[i] = (Token){ .inst = INST_LT };
        } else if(strcmp(word, ">=") == 0) {
            program[i] = (Token){ .inst = INST_GEQ };
        } else if(strcmp(word, "<=") == 0) {
            program[i] = (Token){ .inst = INST_LEQ };
        } else if(strcmp(word, "if") == 0) {
            program[i] = (Token){ .inst = INST_IF };
        } else if(strcmp(word, "else") == 0) {
            program[i] = (Token){ .inst = INST_ELSE };
        } else if(strcmp(word, "endi") == 0) {
            program[i] = (Token){ .inst = INST_ENDIF };
        } else if(strcmp(word, "print") == 0) {
            program[i] = (Token){ .inst = INST_PRINT };
        } else if(strcmp(word, "while") == 0) {
            program[i] = (Token){ .inst = INST_WHILE };
        } else if(strcmp(word, "run") == 0) {
            program[i] = (Token){ .inst = INST_RUN_WHILE };
        } else if(strcmp(word, "endw") == 0) {
            program[i] = (Token){ .inst = INST_END_WHILE };
        } else if(strcmp(word, "jmp") == 0) {
            program[i] = (Token){ .inst = INST_JUMP };
        } else {
            PANIC_ON_ERR(true, ERR_SYNTAX_ERROR, "Syntax Error: Invalid Token '%s'.", word);
        }
        i++;
    }
    fclose(file);  

    return program;
}

void exec_program(ProgramState* state, Token* program, uint32_t program_size) {
    for(uint32_t i = 0; i < program_size; i++) {
        if(program[i].inst == INST_ELSE) {
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst == INST_ENDIF) {
                    program[i].data = j;
                    break;
                }
            }     
        }
        if(program[i].inst == INST_END_WHILE) {
            program[i].data = -1;
            for(int32_t j = i - 1; j >= 0; j--) {
                if(program[j].inst == INST_WHILE) {
                    program[i].data = j;
                    break;
                }
            }
            PANIC_ON_ERR(program[i].data == -1, ERR_SYNTAX_ERROR, "While-end token without while condition.");
        }
        if(program[i].inst == INST_RUN_WHILE) {
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst == INST_END_WHILE) {
                    program[i].data = j;
                    break;
                }
            }
            PANIC_ON_ERR(program[i].data == -1, ERR_SYNTAX_ERROR, "While condtion without end token.");
        }
        if(program[i].inst == INST_IF) {
            program[i].data = -1;
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst == INST_ELSE) {
                    program[i].data = j;
                    break;
                }
            }
            if(program[i].data == -1) {
                for(uint32_t k = i; k < program_size; k++) {
                    if(program[k].inst == INST_ENDIF) {
                        program[i].data = k;
                        break;
                    }
                }
            }
            PANIC_ON_ERR(program[i].data == -1, ERR_SYNTAX_ERROR, "If without endif");
        }
    }
    while(state->inst_ptr < program_size) {
        if(program[state->inst_ptr].inst == INST_RUN_WHILE) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for while condition specified.");
            int32_t cond = state->stack[state->stack_size - 1];
            state->stack_size -= 1;
            if(!cond) {
                Token tok = program[state->inst_ptr];
                state->inst_ptr = tok.data + 1;
                if((uint32_t)tok.data == program_size)
                    break;
            }
        }
        if(program[state->inst_ptr].inst == INST_END_WHILE) {
            Token tok = program[state->inst_ptr];
            state->inst_ptr = tok.data;
        }
        if (program[state->inst_ptr].inst == INST_STACK_PUSH) {
            PANIC_ON_ERR(state->stack_size >= STACK_CAP, ERR_STACK_OVERFLOW, "Stack is overflowed");
            state->stack[state->stack_size++] = program[state->inst_ptr].data;
        }
        if(program[state->inst_ptr].inst == INST_PLUS || program[state->inst_ptr].inst == INST_MINUS ||
            program[state->inst_ptr].inst == INST_DIV || program[state->inst_ptr].inst == INST_MUL) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW,
                         "Too few values on stack for arithmetic operator.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            if (program[state->inst_ptr].inst == INST_PLUS)
                state->stack[state->stack_size++] = b + a;
            else if (program[state->inst_ptr].inst == INST_MINUS)
                state->stack[state->stack_size++] = b - a;
            else if (program[state->inst_ptr].inst == INST_MUL)
                state->stack[state->stack_size++] = b * a;
            else if (program[state->inst_ptr].inst == INST_DIV)
                state->stack[state->stack_size++] = b / a;
        }
        if(program[state->inst_ptr].inst == INST_PRINT) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for print function on stack.");
            int32_t val = state->stack[state->stack_size - 1];
            state->stack_size -= 1;
            printf("%i\n", val);
        }
        if(program[state->inst_ptr].inst == INST_JUMP) {
            int32_t index = program[state->inst_ptr].data;
            PANIC_ON_ERR(index >= (int32_t)program_size || index < 0, ERR_INVALID_JUMP, "Invalid index for jump specified.");
            state->inst_ptr = index - 1;
        }
        if(program[state->inst_ptr].inst == INST_STACK_PREV) {
            state->stack_size--;
            int32_t index = (state->stack_size - 1) - program[state->inst_ptr].data;
            PANIC_ON_ERR(index >= (int32_t)state->stack_size || index < 0, ERR_INVALID_STACK_ACCESS, "Invalid index for retrieving value from stack");
            state->stack[state->stack_size++] = state->stack[index];
        }
        if(program[state->inst_ptr].inst == INST_EQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for equality check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(a == b);
        }
        if(program[state->inst_ptr].inst == INST_NEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for equality check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(a != b);
        }
        if(program[state->inst_ptr].inst == INST_GT) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for greather-than check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(b > a);
        }
        if(program[state->inst_ptr].inst == INST_LT) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for less-than check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(b < a);
        }
        if(program[state->inst_ptr].inst == INST_GEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for greather-than-equal check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(b >= a);
        }
        if(program[state->inst_ptr].inst == INST_LEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for less-than-equal check specified.");
            int32_t a = state->stack[state->stack_size - 1];
            int32_t b = state->stack[state->stack_size - 2];
            state->stack_size -= 2;
            state->stack[state->stack_size++] = (int32_t)(b <= a);
        }
        if(program[state->inst_ptr].inst == INST_IF) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for if check specified.");
            int32_t cond = state->stack[state->stack_size - 1];
            state->stack_size -= 1;
            if(!cond) {
                Token tok = program[state->inst_ptr];
                state->inst_ptr = tok.data; 
            } 
        }
        if(program[state->inst_ptr].inst == INST_ELSE) {
            Token tok = program[state->inst_ptr];
            state->inst_ptr = tok.data;
        }
        state->inst_ptr++;
    }
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Lantern: [Error]: Too few arguments specified. Usage: ./lantern <filepath>\n");
        return 1;
    }
    if(argc > 2) {
        printf("Lantern: [Error]: Too many arguments specified. Usage: ./lantern <filepath>\n");
        return 1;
    }
    uint32_t program_size;
    Token* program = load_program_from_file(argv[1], &program_size);
    ProgramState program_state;
    program_state.stack_size = 0;
    program_state.inst_ptr = 0;

    exec_program(&program_state, program, program_size);
    return 0;
}
