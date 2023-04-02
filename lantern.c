#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#define STACK_CAP 64
#define STACKFRAME_CAP 256
#define PROGRAM_CAP 1024
         
#define PANIC_ON_ERR(cond, err_type, ...)  {                                            \
    if(cond) {                                                                          \
        printf("Lantern: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type);  \
         printf(__VA_ARGS__);                                                           \
         printf("\n");                                                                  \
    }                                                                                   \
}                                                                                       \

typedef enum {
    INST_STACK_PUSH, INST_STACK_PREV, INST_STACK_PUSH_HEAP_PTR,
    INST_PLUS, INST_MINUS, INST_MUL, INST_DIV,
    INST_EQ,INST_NEQ, 
    INST_GT,INST_LT,INST_GEQ,INST_LEQ,
    INST_IF,INST_ELSE,INST_ENDIF,
    INST_WHILE, INST_RUN_WHILE, INST_END_WHILE,
    INST_PRINT, INST_PRINTLN,
    INST_JUMP,
    INST_ADD_VAR_TO_STACKFRAME, INST_VAR_ASSIGN, INST_VAR_USAGE
} Instruction;

typedef enum {
    VAR_TYPE_INT,
    VAR_TYPE_STR
} VariableType;

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
    size_t data;
} Token;

typedef struct {
    size_t data;
    bool heap_ptr;
    uint32_t frame_index;
} StackFrameValue;

typedef struct {
    int32_t stack[STACK_CAP];
    int32_t stack_size;
    void** heap;
    uint32_t heap_size;
    StackFrameValue stackframe[STACKFRAME_CAP];
    uint32_t stackframe_size;
    uint32_t stackframe_index;
    uint32_t inst_ptr;
    uint32_t program_size;
} ProgramState;

bool is_str_int(const char* str) {
    for(uint32_t i = 0; i < strlen(str); i++) {
        if(!isdigit(str[i])) return false;
    }
    return true;
}
bool is_str_var_name(const char* str) {
    if(is_str_int(str)) {
        return false;
    }
    if(isdigit(str[0])) {
        return false;
    }
    for(uint32_t i = 0; i < strlen(str); i++) {
        if(str[i] == '!' || str[i] == '@' || str[i] == '#' || str[i] == '$'
            || str[i] == '%' || str[i] == '^' || str[i] == '&' || str[i] == '*'
            || str[i] == '(' || str[i] == ')' || str[i] == '-' || str[i] == '{'
            || str[i] == '}' || str[i] == '[' || str[i] == ']' || str[i] == ':'
            || str[i] == ';' || str[i] == '"' || str[i] == '\'' || str[i] == '<'
            || str[i] == '>' || str[i] == '.' || str[i] == '/' || str[i] == '?'
            || str[i] == '~' || str[i] == '`') {
            return false;
        }
    }
    return true;
}

void clear_current_stackframe(ProgramState* state, Token* program, uint32_t program_size) {
    uint32_t i;
    uint32_t cleared_values = 0;
    for(i = state->stackframe_size; i > 0; i--) {
        if(state->stackframe[i].frame_index != state->stackframe_index) break;
        state->stackframe[i].data = SIZE_MAX;
        state->stackframe[i].heap_ptr = false; 
        state->stackframe_size--;
        cleared_values++;     
    }
    if(cleared_values != 0) {
        for(uint32_t i = 0; i < program_size; i++) {
            if(program[i].inst == INST_VAR_USAGE) {
                program[i].data -= cleared_values;
            }
        }
    }
}

Token* load_program_from_file(const char* filepath, uint32_t* program_size, ProgramState* state) {
    FILE* file;
    file = fopen(filepath, "r");
    if(!file) {
        printf("Lantern: [Error]: Cannot read file '%s'.\n", filepath);
        return NULL;
    }
    uint32_t words_in_file = 1;
    char ch;
    bool on_word = false;
    while ((ch = fgetc(file)) != EOF) {
        if(ch == ' ' || ch == '\t' || ch == '\0' || ch == '\n') {
            if (on_word) {
                on_word = false;
                words_in_file++;
            }
        } else {
            on_word = true;
        }
    }

    rewind(file);    
    char line[256];
    char** literals = malloc(256 * (*program_size));
    bool removed_word_from_literal = false;
    uint32_t literal_count = 0;
    while (fgets(line, sizeof(line), file)) {
        char literal[256] = "";
        bool on_literal = false;
        uint32_t literal_char_count = 0;
        for(uint32_t i = 0; i < strlen(line); i++) {
            if(line[i] == '"' && !on_literal) {
                on_literal = true;
                literal[literal_char_count++] = line[i];
                continue;
            }
            if(on_literal) {
                literal[literal_char_count++] = line[i];
                if(line[i] == ' ' && !removed_word_from_literal) {
                    removed_word_from_literal = true;
                    words_in_file--;
                } else if(line[i] != ' ') {
                    removed_word_from_literal = false;
                }
            }
            if(line[i] == '"' && on_literal) {
                on_literal = false;
                literal[literal_char_count] = '\0';
                literal_char_count = 0;
                char* literal_cpy = malloc(256);
                strncpy(literal_cpy, literal, 256);
                literals[literal_count++] = literal_cpy;
            }
        }
    }
    *program_size = words_in_file;
    rewind(file);

    Token* program = malloc(sizeof(Token) * words_in_file);

    char word[128];
    uint32_t i = 0;
    bool on_literal_token = false;
    uint32_t literal_token_count = 0;

    char variable_names[256][STACKFRAME_CAP];
    uint32_t variable_stackframe_indices[STACKFRAME_CAP];
    uint32_t variable_count = 0;
    uint32_t virtual_stackframe_index = 0;
    while(fscanf(file, "%s", word) != EOF) {
        if((!on_literal_token && word[0] == '"') || (!on_literal_token && word[0] == '"' && word[strlen(word) - 1] == '"' && strlen(word) > 1)) {
            if(!on_literal_token && word[0] == '"' && word[strlen(word) - 1] == '"' && strlen(word) > 1) {
                on_literal_token = false;
            } else {
                on_literal_token = true;
            }
            char* literal_cpy = malloc(256);
            strcpy(literal_cpy, literals[literal_token_count]);
            uint32_t len = strlen(literal_cpy);
            for(uint32_t i = 0; i < len; i++) {
                if(literal_cpy[i] == '"') {
                    for(uint32_t j = i; j < len; j++) {
                        literal_cpy[j] = literal_cpy[j+1];
                    }
                    len--;
                    i--;
                }
            }
            state->heap[state->heap_size] = literal_cpy;
            program[i] = (Token){ .inst = INST_STACK_PUSH_HEAP_PTR, .data = state->heap_size};
            state->heap_size++;
            literal_token_count++;
            i++;
            continue;
        }
        if(on_literal_token && word[strlen(word) - 1] == '"') {
            on_literal_token = false;
            continue;
        }
        if(on_literal_token) {
            continue;
        }
        if(is_str_int(word)) {
            program[i] = (Token){ .inst = INST_STACK_PUSH, .data = atoi(word) };
            i++;
            continue;
        }
        if(strcmp(word, "prev") == 0) {
            program[i] = (Token){ .inst = INST_STACK_PREV };
        } else if(strcmp(word, "=") == 0) {
            program[i] = (Token){ .inst = INST_VAR_ASSIGN };
        } else if(strcmp(word, "+") == 0) {
            program[i] = (Token){ .inst = INST_PLUS };
        } else if(strcmp(word, "-") == 0) {
            program[i] = (Token){ .inst = INST_MINUS };
        } else if(strcmp(word, "*") == 0) {
            program[i] = (Token){ .inst = INST_MUL };
        } else if(strcmp(word, "/") == 0) {
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
            virtual_stackframe_index++;
            program[i] = (Token){ .inst = INST_IF };
        } else if(strcmp(word, "else") == 0) {
            program[i] = (Token){ .inst = INST_ELSE };
        } else if(strcmp(word, "endi") == 0) {
            virtual_stackframe_index--;
            program[i] = (Token){ .inst = INST_ENDIF };
        } else if(strcmp(word, "print") == 0) {
            program[i] = (Token){ .inst = INST_PRINT };
        } else if(strcmp(word, "while") == 0) {
            program[i] = (Token){ .inst = INST_WHILE };
        } else if(strcmp(word, "run") == 0) {
            virtual_stackframe_index++;
            program[i] = (Token){ .inst = INST_RUN_WHILE };
        } else if(strcmp(word, "endw") == 0) {
            virtual_stackframe_index--;
            program[i] = (Token){ .inst = INST_END_WHILE };
        } else if(strcmp(word, "println") == 0) {
            program[i] = (Token){ .inst = INST_PRINTLN };
        } else if(strcmp(word, "jmp") == 0) {
            program[i] = (Token){ .inst = INST_JUMP };
        } else {
            if(is_str_var_name(word)) {
                if(i >= 2) {
                    if(program[i - 1].inst == INST_VAR_ASSIGN) {
                        PANIC_ON_ERR(i < 2, ERR_SYNTAX_ERROR, "Assigning variable to nothing.");
                        program[i] = (Token){ .inst = INST_ADD_VAR_TO_STACKFRAME};
                        strcpy(variable_names[variable_count], word);
                        variable_stackframe_indices[variable_count++] = virtual_stackframe_index;
                        i++;
                        continue;
                    } 
                }
                int32_t stackframe_index = -1;
                for(uint32_t i = 0; i < variable_count; i++) {
                    if(strcmp(variable_names[i], word) == 0 && variable_stackframe_indices[i] == virtual_stackframe_index) {
                        stackframe_index = i;
                        break;
                    }
                }
                PANIC_ON_ERR(stackframe_index == -1, ERR_SYNTAX_ERROR, "Undeclared identifier '%s'.", word);
                program[i] = (Token){ .inst = INST_VAR_USAGE, .data = stackframe_index};
                i++;
                continue;
            
            }
            PANIC_ON_ERR(true, ERR_SYNTAX_ERROR, "Syntax Error: Invalid Token '%s'.", word);
        }
        i++;
    }
    fclose(file); 

    free(literals);
    return program;
}

void heap_free(ProgramState* state, uint32_t pos) {
    free(state->heap[pos]);
    state->heap_size--;
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
            PANIC_ON_ERR(program[i].data == (size_t)-1, ERR_SYNTAX_ERROR, "While-end token without while condition.");
        }
        if(program[i].inst == INST_RUN_WHILE) {
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst == INST_END_WHILE) {
                    program[i].data = j;
                    break;
                }
            }
            PANIC_ON_ERR(program[i].data == (size_t)-1, ERR_SYNTAX_ERROR, "While condtion without end token.");
        }
        if(program[i].inst == INST_IF) {
            program[i].data = -1;
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst == INST_ELSE) {
                    program[i].data = j;
                    break;
                }
            }
            if(program[i].data == (size_t)-1) {
                for(uint32_t k = i; k < program_size; k++) {
                    if(program[k].inst == INST_ENDIF) {
                        program[i].data = k;
                        break;
                    }
                }
            }
            PANIC_ON_ERR(program[i].data == (size_t)-1, ERR_SYNTAX_ERROR, "If without endif");
        }
    }
    while(state->inst_ptr < program_size) {
        if(program[state->inst_ptr].inst == INST_VAR_USAGE) {
            if(state->stackframe[program[state->inst_ptr].data].heap_ptr) {
                program[state->inst_ptr].inst = INST_STACK_PUSH_HEAP_PTR;
            } else {
                program[state->inst_ptr].inst = INST_STACK_PUSH;
            }
            program[state->inst_ptr].data = state->stackframe[program[state->inst_ptr].data].data;
        }
        if(program[state->inst_ptr].inst == INST_ADD_VAR_TO_STACKFRAME) {
            state->stackframe[state->stackframe_size++] = (StackFrameValue){ .data = program[state->inst_ptr - 2].data,
                .heap_ptr = (program[state->inst_ptr - 2].inst == INST_STACK_PUSH_HEAP_PTR ? true : false), .frame_index = state->stackframe_index};   
        }
        if(program[state->inst_ptr].inst == INST_VAR_ASSIGN) {
            state->stack_size--;
        }
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
            else {
                state->stackframe_index++;
            }
        }
        if(program[state->inst_ptr].inst == INST_END_WHILE) {
            Token tok = program[state->inst_ptr];
            state->inst_ptr = tok.data;
            state->stackframe_index--;
            clear_current_stackframe(state, program, program_size);
        }
        if (program[state->inst_ptr].inst == INST_STACK_PUSH || program[state->inst_ptr].inst == INST_STACK_PUSH_HEAP_PTR) {
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
        if(program[state->inst_ptr].inst == INST_PRINT || program[state->inst_ptr].inst == INST_PRINTLN) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for print function on stack.");
            bool heap_ptr = false;
            for(uint32_t i = state->inst_ptr; i > 0; i--) {
                if(program[i].inst == INST_STACK_PUSH || program[i].inst == INST_STACK_PUSH_HEAP_PTR) {
                    heap_ptr = (program[i].inst == INST_STACK_PUSH) ? false : true;
                    break;
                }
            }
            if(!heap_ptr) {
                int32_t val = state->stack[state->stack_size - 1];
                state->stack_size -= 1;
                if(program[state->inst_ptr].inst == INST_PRINTLN)
                    printf("%i\n", val);
                else
                    printf("%i", val);
            } else {
                char* val = state->heap[program[state->inst_ptr - 1].data];
                state->stack_size -= 1;
                if(program[state->inst_ptr].inst == INST_PRINTLN)
                    printf("%s\n", val);
                else
                    printf("%s", val);
            }
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
            } else {
                state->stackframe_index++;
            }
        }
        if(program[state->inst_ptr].inst == INST_ENDIF) {
            state->stackframe_index--;
            clear_current_stackframe(state, program, program_size);
        }
        if(program[state->inst_ptr].inst == INST_ELSE) {
            Token tok = program[state->inst_ptr];
            state->inst_ptr = tok.data;
        }
        //printf("Stack size: %i\n", state->stack_size);
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
    ProgramState program_state;
    program_state.stack_size = 0;
    program_state.heap_size = 0;
    program_state.stackframe_size = 0;
    program_state.inst_ptr = 0;
    program_state.heap = malloc(1024 /* TODO: Dynamic Heap */);
    program_state.stackframe_index = 0;
    Token* program = load_program_from_file(argv[1], &program_size, &program_state);
    program_state.program_size = program_size;
    exec_program(&program_state, program, program_size);
    free(program_state.heap);
    return 0;
}