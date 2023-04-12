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
#define MAX_WORD_SIZE 256

#define PANIC_ON_ERR(cond, err_type, ...)  {                                            \
    if(cond) {                                                                          \
        printf("Lantern: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type);  \
         printf(__VA_ARGS__);                                                           \
         printf("\n");                                                                  \
    }                                                                                   \
}                                                                                       \

typedef enum {
    INST_STACK_PUSH, INST_STACK_PREV, 
    INST_PLUS, INST_MINUS, INST_MUL, INST_DIV, INST_MOD,
    INST_EQ,INST_NEQ, 
    INST_GT,INST_LT,INST_GEQ,INST_LEQ,
    INST_LOGICAL_AND, INST_LOGICAL_OR,
    INST_IF,INST_ELSE, INST_ELIF, INST_THEN, INST_ENDIF,
    INST_WHILE, INST_RUN_WHILE, INST_END_WHILE,
    INST_PRINT, INST_PRINTLN,
    INST_JUMP,
    INST_ADD_VAR_TO_STACKFRAME, INST_ASSIGN, INST_VAR_USAGE, INST_VAR_REASSIGN,
    INST_HEAP_ALLOC, INST_HEAP_FREE, INST_PTR_GET_I, INST_PTR_SET_I,
    INST_INT_TYPE, INST_STR_TYPE,
    INST_MACRO, INST_MACRO_DEF, INST_END_MACRO, INST_MACRO_USAGE
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
    ERR_INVALID_DATA_TYPE,
    ERR_ILLEGAL_INSTRUCTION,
    ERR_SYNTAX_ERROR,
    ERR_INVALID_PTR,
} Error;

typedef struct {
    size_t data;
    VariableType var_type;
    bool heap_ptr;
} RuntimeValue;

typedef struct {
    void* data;
    VariableType var_type;
} HeapValue;

typedef struct {
    Instruction inst;
    RuntimeValue val;
} Token;

typedef struct {
    RuntimeValue val;
    uint32_t frame_index;
} StackFrameValue;

typedef struct {
    RuntimeValue stack[STACK_CAP];
    int32_t stack_size;

    HeapValue* heap;
    uint32_t heap_size;

    StackFrameValue stackframe[STACKFRAME_CAP];
    uint32_t stackframe_size;
    uint32_t stackframe_index;

    uint32_t inst_ptr;
    uint32_t program_size;

    uint32_t call_positions[STACK_CAP];
    uint32_t call_positions_count;

    uint32_t macro_positions[PROGRAM_CAP];
    uint32_t macro_count;
    bool found_solution_for_if_block;
} ProgramState;

bool 
is_str_int(const char* str) {
    for(uint32_t i = 0; i < strlen(str); i++) {
        if(!isdigit(str[i])) return false;
    }
    return true;
}

bool 
is_str_literal(char* str) {
  return (str[0] == '"' && str[strlen(str) - 1] == '"' && strlen(str) > 1);
}

bool 
is_str_macro_usage(char* str) {
    return str[0] == '$';
}

bool
is_str_var_name(const char* str) {
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

bool
int_array_contains(int32_t* arr, uint32_t arr_size, int32_t val) {
    for(uint32_t i = 0; i < arr_size; i++) {
        if(arr[i] == val) return true;
    }
    return false;
}

void 
strip_char_from_str(char c, char* str) {
    uint32_t len = strlen(str);
    for(uint32_t i = 0; i < len; i++) {
        if(str[i] == c) {
            for(uint32_t j = i; j < len; j++) {
                str[j] = str[j+1];
            }
            len--;
            i--;
        }
    }
}

uint32_t
get_word_count_in_file(FILE* file) {
    uint32_t words_in_file = 0;
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
    return words_in_file;
}

void 
clear_current_stackframe(ProgramState* state) {
  for(int32_t i = state->stackframe_size; i >= 0; i--) {
      if(state->stackframe[i].frame_index != state->stackframe_index) break;
      state->stackframe[i].val.data = SIZE_MAX;
      state->stackframe[i].val.heap_ptr = false; 
      state->stackframe_size--;
  }
  state->stackframe_index--;
}

void 
heap_free(ProgramState* state, uint32_t pos) {
    free(state->heap[pos].data);
    state->heap_size--;
}

uint32_t 
heap_alloc(ProgramState* state, size_t size, VariableType type) {
    state->heap[state->heap_size].data = malloc(size);
    state->heap[state->heap_size++].var_type = type;
    return state->heap_size - 1; 
}

RuntimeValue
stack_top(ProgramState* state) {
    return state->stack[state->stack_size - 1]; 
}

RuntimeValue
stack_pop(ProgramState* state) {
    state->stack_size--; 
    return state->stack[state->stack_size];
}

RuntimeValue
stack_peak(ProgramState* state, uint32_t index) {
    return state->stack[state->stack_size - index];
}

void 
stack_push(ProgramState* state, RuntimeValue val) {
    state->stack_size++;
    state->stack[state->stack_size - 1] = val;
}

Token* 
load_program_from_file(const char* filepath, uint32_t* program_size, ProgramState* state) {
    FILE* file;
    file = fopen(filepath, "r");
    if(!file) {
        printf("Lantern: [Error]: Cannot read file '%s'.\n", filepath);
        return NULL;
    }
    uint32_t words_in_file = get_word_count_in_file(file);
    rewind(file);    

    char literals[words_in_file][MAX_WORD_SIZE];
    uint32_t literal_count = 0;
    {
        char line[MAX_WORD_SIZE];
        bool removed_word_from_literal = false;
        while (fgets(line, sizeof(line), file)) {
            char literal[MAX_WORD_SIZE] = "";
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
                    } else if(line[i] != ' ') 
                        removed_word_from_literal = false;
                }
                if(line[i] == '"' && on_literal) {
                    on_literal = false;
                    literal[literal_char_count] = '\0';
                    literal_char_count = 0;
                    char literal_cpy[MAX_WORD_SIZE];
                    strcpy(literal_cpy, literal);
                    strcpy(literals[literal_count++], literal_cpy);
                }
            }
        }
    }
    *program_size = words_in_file;
    rewind(file);
    
    Token* program = malloc(sizeof(Token) * words_in_file);

    char word[MAX_WORD_SIZE];
    uint32_t i = 0;
    bool on_literal = false;
    uint32_t literal_token_count = 0;

    char variable_names[MAX_WORD_SIZE][STACKFRAME_CAP];
    uint32_t variable_stackframe_indices[STACKFRAME_CAP];
    uint32_t variable_count = 0;
    uint32_t virtual_stackframe_index = 0;
    
    uint32_t crossreferenced_tokens[*program_size];
    uint32_t crossreferenced_token_count = 0;

    bool on_comment = false;

    char macro_names[MAX_WORD_SIZE][PROGRAM_CAP];
    uint32_t macro_names_count = 0;
    while(fscanf(file, "%s", word) != EOF) {
        //printf("%s\n", word);
        if(strcmp("#", word) == 0 && !on_comment) {
            on_comment = true;
        } else if(strcmp("#>", word) == 0 && on_comment) {
            on_comment = false;
            continue;
        }
        if(on_comment) continue;

        if((!on_literal && word[0] == '"') || (!on_literal && is_str_literal(word))) {
            on_literal = !is_str_literal(word);

            char* literal_cpy = malloc(MAX_WORD_SIZE);
            strcpy(literal_cpy, literals[literal_token_count]);
            strip_char_from_str('"', literal_cpy);

            state->heap[state->heap_size].data = literal_cpy;
            state->heap[state->heap_size].var_type = VAR_TYPE_STR;
            program[i] = (Token){ .inst = INST_STACK_PUSH };
            program[i].val.data = state->heap_size;
            program[i].val.var_type = VAR_TYPE_STR;
            program[i].val.heap_ptr = true;

            state->heap_size++;
            literal_token_count++;
            i++;
            continue;
        }
        if(on_literal && word[strlen(word) - 1] == '"') {
            on_literal = false;
            continue;
        }
        if(on_literal) {
            continue;
        }
        if(is_str_int(word)) {
            program[i] = (Token){ .inst = INST_STACK_PUSH };
            program[i].val.data = atoi(word);
            program[i].val.var_type = VAR_TYPE_INT;
            program[i].val.heap_ptr = false;
            i++;
            continue;
        }
        if(is_str_macro_usage(word)) {
            memmove(word, word+1, strlen(word));
            for(uint32_t j = 0; j < macro_names_count; j++) {
                if(strcmp(macro_names[j], word) == 0) {
                    program[i] = (Token){ .inst = INST_MACRO_USAGE, .val.data = state->macro_positions[j] };
                    break;
                }
            }
            i++;
            continue;
        }
        
        if(i > 0) {
            if(program[i - 1].inst == INST_MACRO) {
                strcpy(macro_names[macro_names_count++], word);
                i++;
                continue;
            }
        }
        if(strcmp(word, "prev") == 0) {
            program[i] = (Token){ .inst = INST_STACK_PREV };
        } else if(strcmp(word, "=") == 0) {
            program[i] = (Token){ .inst = INST_ASSIGN };
        } else if(strcmp(word, "+") == 0) {
            program[i] = (Token){ .inst = INST_PLUS };
        } else if(strcmp(word, "-") == 0) {
            program[i] = (Token){ .inst = INST_MINUS };
        } else if(strcmp(word, "*") == 0) {
            program[i] = (Token){ .inst = INST_MUL };
        } else if(strcmp(word, "%") == 0) {
            program[i] = (Token){ .inst = INST_MOD };
        } else if(strcmp(word, "/") == 0) {
            program[i] = (Token){ .inst = INST_MUL };
        } else if(strcmp(word, "==") == 0) {
            program[i] = (Token){ .inst = INST_EQ };
        } else if(strcmp(word, "!=") == 0) {
            program[i] = (Token){ .inst = INST_NEQ };
        } else if(strcmp(word, ">") == 0) {
            program[i] = (Token){ .inst = INST_GT };
        } else if(strcmp(word, "<") == 0) {
            program[i] = (Token){ .inst = INST_LT };
        } else if(strcmp(word, ">=") == 0) {
            program[i] = (Token){ .inst = INST_GEQ };
        } else if(strcmp(word, "<=") == 0) {
            program[i] = (Token){ .inst = INST_LEQ };
        } else if(strcmp(word, "and") == 0) {
            program[i] = (Token){ .inst = INST_LOGICAL_AND };
        } else if(strcmp(word, "or") == 0) {
            program[i] = (Token){ .inst = INST_LOGICAL_OR };
        } else if(strcmp(word, "if") == 0) {
            virtual_stackframe_index++;
            program[i] = (Token){ .inst = INST_IF };
        } else if(strcmp(word, "else") == 0) {
            program[i] = (Token){ .inst = INST_ELSE };
        } else if(strcmp(word, "end") == 0) {
            for(int32_t j = i; j >= 0; j--) {
                bool skip_token = false;
                for(uint32_t k = 0; k < crossreferenced_token_count; k++) {
                    if(crossreferenced_tokens[k] != (uint32_t)j) continue;
                    skip_token = true;
                    break;
                }
                if(skip_token) continue;

                if(program[j].inst == INST_IF) {
                    program[i] = (Token) { .inst = INST_ENDIF };
                    virtual_stackframe_index--;
                    crossreferenced_tokens[crossreferenced_token_count++] = j;
                    break;
                } else if(program[j].inst == INST_WHILE) {
                    program[i] = (Token){ .inst = INST_END_WHILE };
                    program[i].val.data = j;
                    virtual_stackframe_index--;
                    crossreferenced_tokens[crossreferenced_token_count++] = j;
                    break;
                } else if(program[j].inst == INST_MACRO) {
                    program[i] = (Token){ .inst = INST_END_MACRO };
                    crossreferenced_tokens[crossreferenced_token_count++] = j;
                    break;
                }
            } 
        } else if(strcmp(word, "then") == 0) {
            program[i] = (Token){ .inst = INST_THEN };
        }else if(strcmp(word, "elif") == 0) {
            program[i] = (Token){ .inst = INST_ELIF };
        } else if(strcmp(word, "print") == 0) {
            program[i] = (Token){ .inst = INST_PRINT };
        } else if(strcmp(word, "while") == 0) {
            program[i] = (Token){ .inst = INST_WHILE };
        } else if(strcmp(word, "run") == 0) {
            virtual_stackframe_index++;
            program[i] = (Token){ .inst = INST_RUN_WHILE };
        } else if(strcmp(word, "println") == 0) {
            program[i] = (Token){ .inst = INST_PRINTLN };
        } else if(strcmp(word, "jmp") == 0) {
            program[i] = (Token){ .inst = INST_JUMP };
        } else if(strcmp(word, "alloc") == 0) {
            program[i] = (Token){ .inst = INST_HEAP_ALLOC };
        } else if(strcmp(word, "free") == 0) {
            program[i] = (Token){ .inst = INST_HEAP_FREE };
        } else if(strcmp(word, "pget") == 0) {
            program[i] = (Token){ .inst = INST_PTR_GET_I };
        } else if(strcmp(word, "pset") == 0) {
            program[i] = (Token){ .inst = INST_PTR_SET_I };
        } else if(strcmp(word, "int") == 0) {
            program[i] = (Token){ .inst = INST_INT_TYPE };
        } else if(strcmp(word, "str") == 0) {
            program[i] = (Token){ .inst = INST_STR_TYPE };
        } else if(strcmp(word, "macro") == 0) {
            program[i] = (Token){ .inst = INST_MACRO };
        } else if(strcmp(word, "def") == 0) {
            program[i] = (Token){ .inst = INST_MACRO_DEF };
            state->macro_positions[state->macro_count++] = i;
        } else {
            if(is_str_var_name(word)) {
                if(program[i - 1].inst == INST_ASSIGN) {
                    PANIC_ON_ERR(i < 2, ERR_SYNTAX_ERROR, "Assigning variable to nothing."); 
                    bool re_assigning = false;
                    for(uint32_t j = 0; j < variable_count; j++) {
                        if(strcmp(variable_names[j], word) == 0) {
                            int32_t stackframe_index = -1;
                            for(uint32_t k = 0; k < variable_count; k++) {
                                if(strcmp(variable_names[k], word) != 0) continue;
                                stackframe_index = k;
                                break; 
                            }
                            re_assigning = true;
                            program[i] = (Token){ .inst = INST_VAR_REASSIGN };
                            program[i].val.data = stackframe_index;
                            break;
                        }    
                    }
                    if(!re_assigning) {
                        strcpy(variable_names[variable_count], word);
                        variable_stackframe_indices[variable_count++] = virtual_stackframe_index;
                        program[i] = (Token){ .inst = INST_ADD_VAR_TO_STACKFRAME};
                    }
                    i++;
                    continue;
                } 
                int32_t stackframe_index = -1;
                for(uint32_t i = 0; i < variable_count; i++) {
                    if(strcmp(variable_names[i], word) != 0 || variable_stackframe_indices[i] > virtual_stackframe_index) continue;
                    stackframe_index = i;
                    break;
                }
                PANIC_ON_ERR(stackframe_index == -1, ERR_SYNTAX_ERROR, "Undeclared identifier '%s'.", word);
                program[i] = (Token){ .inst = INST_VAR_USAGE };
                program[i].val.data = stackframe_index;
                i++;
                continue;
            
            }
            PANIC_ON_ERR(true, ERR_SYNTAX_ERROR, "Syntax Error: Invalid Token '%s'.", word);
        }
        i++;
    }
    rewind(file);
    fclose(file); 

    return program;
}

void
crossreference_tokens(Token* program, uint32_t program_size) {
    uint32_t crossreferenced_whiles[program_size];
    uint32_t crossreferenced_whiles_count = 0;
    for(uint32_t i = 0; i < program_size; i++) {
        if(program[i].inst == INST_RUN_WHILE) {
            if(int_array_contains((int32_t*)crossreferenced_whiles, crossreferenced_whiles_count, i))
                continue;
            uint32_t while_index = 0;
            uint32_t end_while_index = 0;
            while_index++;
            for(uint32_t j = i + 1; j < program_size; j++) {
                if(program[j].inst == INST_END_WHILE && (while_index - 1) == end_while_index) {
                    program[i].val.data = j;
                    crossreferenced_whiles[crossreferenced_whiles_count++] = j;
                    break;
                } else if(program[j].inst == INST_END_WHILE && (while_index - 1) != end_while_index) {
                    end_while_index++;
                }
                if(program[j].inst == INST_RUN_WHILE) {
                    while_index++;
                }
            }
        }
        if(program[i].inst == INST_ELSE) {
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst != INST_ENDIF) continue;
                program[i].val.data = j; 
                break;
            }     
        }
        if(program[i].inst == INST_IF || program[i].inst == INST_THEN) {
            program[i].val.data = -1;
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst != INST_ELIF) continue;
                program[i].val.data = j - 1;
                break;
            }
            if(program[i].val.data == (size_t)-1) {
                for(uint32_t j = i; j < program_size; j++) {
                    if(program[j].inst != INST_ELSE) continue;
                    program[i].val.data = j;
                    break;
                }
            }
            if(program[i].val.data == (size_t)-1) {
                for(uint32_t j = i; j < program_size; j++) {
                    if(program[j].inst != INST_ENDIF) continue;
                    program[i].val.data = j;
                    break;
                }
            }
            PANIC_ON_ERR(program[i].val.data == (size_t)-1, ERR_SYNTAX_ERROR, "If without endif");
        }
        if(program[i].inst == INST_MACRO) {
            for(uint32_t j = i; j < program_size; j++) {
                if(program[j].inst != INST_END_MACRO) continue;
                program[i].val.data = j;
                break;
            }
        }
    }
}
void 
exec_program(ProgramState* state, Token* program, uint32_t program_size) {
    crossreference_tokens(program, program_size);
    while(state->inst_ptr < program_size) {
        //printf("instruction: %i\n", state->inst_ptr);
        Token* current_token = &program[state->inst_ptr];
        if(current_token->inst == INST_RUN_WHILE) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for while condition specified.");
            PANIC_ON_ERR(stack_top(state).var_type != VAR_TYPE_INT, ERR_INVALID_DATA_TYPE,
                "Invalid data type for while condition.");

            int32_t cond = stack_pop(state).data;
            if(!cond) 
                state->inst_ptr = current_token->val.data;
            else 
                state->stackframe_index++;
        }
        if(current_token->inst == INST_END_WHILE) {
            uint32_t while_index = current_token->val.data;
            state->inst_ptr = while_index;
            clear_current_stackframe(state);
        } 
        if(current_token->inst == INST_VAR_USAGE) {
            state->stack[state->stack_size++] = state->stackframe[current_token->val.data].val;
        }
        if(current_token->inst == INST_ADD_VAR_TO_STACKFRAME) {
            state->stackframe[state->stackframe_size] = (StackFrameValue){ .frame_index = state->stackframe_index };
            state->stackframe[state->stackframe_size++].val = stack_pop(state);
        }
        if(current_token->inst == INST_VAR_REASSIGN) {
            state->stackframe[current_token->val.data].val = stack_pop(state);
        }
        if (current_token->inst == INST_STACK_PUSH) {
            PANIC_ON_ERR(state->stack_size >= STACK_CAP, ERR_STACK_OVERFLOW, "Stack is overflowed");
            stack_push(state, current_token->val);
        }
        if(current_token->inst == INST_PLUS || current_token->inst == INST_MINUS ||
            current_token->inst == INST_DIV || current_token->inst == INST_MUL || 
            current_token->inst == INST_MOD) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW,
                         "Too few values on stack for arithmetic operator.");
            
            RuntimeValue val_a = stack_peak(state, 1);
            RuntimeValue val_b = stack_peak(state, 2);
            if(val_a.var_type == VAR_TYPE_INT && val_b.var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                if (current_token->inst == INST_PLUS)
                    state->stack[state->stack_size].data = b + a;
                else if (current_token->inst == INST_MINUS)
                    state->stack[state->stack_size].data = b - a;
                else if (current_token->inst == INST_MUL)
                    state->stack[state->stack_size].data = b * a;
                else if (current_token->inst == INST_DIV)
                    state->stack[state->stack_size].data = b / a;
                else if(current_token->inst == INST_MOD)
                    state->stack[state->stack_size].data = b % a;
                state->stack[state->stack_size++].var_type = VAR_TYPE_INT;
            } else if(val_a.var_type == VAR_TYPE_STR && val_b.var_type == VAR_TYPE_STR) {
                if(current_token->inst == INST_PLUS) {
                    char* a = state->heap[stack_pop(state).data].data;
                    char* b = state->heap[stack_pop(state).data].data;
                    state->heap[state->heap_size].data = strcat(b, a);
                    state->heap[state->heap_size].var_type = VAR_TYPE_STR;
                    stack_push(state, (RuntimeValue) { .heap_ptr = true, .data = state->heap_size, .var_type = VAR_TYPE_STR });
                    state->heap_size++;
                }
            }
        }
        if(current_token->inst == INST_PRINT || current_token->inst == INST_PRINTLN) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for print function on stack.");
            bool heap_ptr = stack_top(state).heap_ptr;
            if(!heap_ptr) {
                int32_t val = stack_pop(state).data;
                if(current_token->inst == INST_PRINTLN)
                    printf("%i\n", val);
                else
                    printf("%i", val);
            } else { 
                if(state->heap[stack_top(state).data].var_type == VAR_TYPE_STR) {
                    char* val = state->heap[stack_pop(state).data].data;
                    if(current_token->inst == INST_PRINTLN)
                        printf("%s\n", val);
                    else
                        printf("%s", val);
                }
            }
        }
        if(current_token->inst == INST_JUMP) {
            int32_t index = stack_pop(state).data;
            PANIC_ON_ERR(index >= (int32_t)program_size || 
                         index < 0, ERR_INVALID_JUMP, "Invalid index for jump specified.");
            state->inst_ptr = index;
            continue;
        }
        if(current_token->inst == INST_STACK_PREV) {
            state->stack_size--;
            int32_t index = (state->stack_size - 1) - current_token->val.data;
            PANIC_ON_ERR(index >= (int32_t)state->stack_size || 
                         index < 0, ERR_INVALID_STACK_ACCESS, "Invalid index for retrieving value from stack");
            stack_push(state, state->stack[index]);
        }
        if(current_token->inst == INST_EQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for equality check specified.");
            
            RuntimeValue val_a = stack_peak(state, 1);
            RuntimeValue val_b = stack_peak(state, 2);
            if(val_a.var_type == VAR_TYPE_INT && val_b.var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(a == b), .var_type = VAR_TYPE_INT });
            } else if(val_a.var_type == VAR_TYPE_STR && val_b.var_type == VAR_TYPE_STR) {
                char* a = state->heap[stack_pop(state).data].data;
                char* b = state->heap[stack_pop(state).data].data;
                stack_push(state, (RuntimeValue){ .data = strcmp(a, b) == 0, .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_NEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for equality check specified.");
            RuntimeValue val_a = stack_peak(state, 1);
            RuntimeValue val_b = stack_peak(state, 2);
            if(val_a.var_type == VAR_TYPE_INT && val_b.var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(a != b), .var_type = VAR_TYPE_INT });
            } else if(val_a.var_type == VAR_TYPE_STR && val_b.var_type == VAR_TYPE_STR) {
                char* a = state->heap[stack_pop(state).data].data;
                char* b = state->heap[stack_pop(state).data].data;
                stack_push(state, (RuntimeValue){ .data = strcmp(a, b) != 0, .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_GT) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for greather-than check specified.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b > a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_LT) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for less-than check specified.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b < a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_GEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for greather-than-equal check specified.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b >= a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_LEQ) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values for less-than-equal check specified.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b <= a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_LOGICAL_OR) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_OVERFLOW, "Too few values for logical or operation.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b || a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_LOGICAL_AND) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_OVERFLOW, "Too few values for logical or operation.");
            if(stack_peak(state, 1).var_type == VAR_TYPE_INT && stack_peak(state, 2).var_type == VAR_TYPE_INT) {
                int32_t a = stack_pop(state).data;
                int32_t b = stack_pop(state).data;
                stack_push(state, (RuntimeValue){ .data = (int32_t)(b && a), .var_type = VAR_TYPE_INT });
            }
        }
        if(current_token->inst == INST_ELSE) {
            state->inst_ptr = current_token->val.data;
        }

        if(current_token->inst == INST_ENDIF) {
            clear_current_stackframe(state);
        }

        if(current_token->inst == INST_IF || current_token->inst == INST_THEN) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for if check specified.");
            PANIC_ON_ERR(stack_top(state).var_type != VAR_TYPE_INT, ERR_INVALID_DATA_TYPE, 
                "Invalid data type for if condition.");
            int32_t cond = stack_pop(state).data;
            if(!cond) {
                if(current_token->inst == INST_IF)
                    state->found_solution_for_if_block = false;
                state->inst_ptr = current_token->val.data; 
            } else {
                if(current_token->inst == INST_IF) 
                    state->found_solution_for_if_block = true;
                if(state->found_solution_for_if_block && current_token->inst == INST_THEN) {
                    for(uint32_t j = state->inst_ptr; j <= program_size; j++) {
                        if(program[j].inst != INST_ENDIF) continue;
                        state->inst_ptr = j;
                        break;
                    }
                } else {
                    state->stackframe_index++;
                    if(current_token->inst == INST_THEN) {
                        state->found_solution_for_if_block = true;
                    }
                }
            }
        }
        if(current_token->inst == INST_HEAP_ALLOC) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for size of memory allocation specified.");
            PANIC_ON_ERR(program[state->inst_ptr - 1].inst != INST_STR_TYPE &&
                program[state->inst_ptr - 1].inst != INST_INT_TYPE, ERR_INVALID_DATA_TYPE, "Invalid data type for allocating block");

            VariableType type = VAR_TYPE_INT;
            if(program[state->inst_ptr - 1].inst == INST_INT_TYPE)
                type = VAR_TYPE_INT;
            else if(program[state->inst_ptr - 1].inst == INST_STR_TYPE)
                type = VAR_TYPE_STR;

            uint32_t heap_ptr = heap_alloc(state, stack_pop(state).data, type); 
            stack_push(state, (RuntimeValue){ .data = heap_ptr, .heap_ptr = true, .var_type = VAR_TYPE_INT });
        }
        if(current_token->inst == INST_HEAP_FREE) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No pointer for free operation specified.");
            PANIC_ON_ERR(!stack_top(state).heap_ptr, ERR_INVALID_PTR, "Trying to free stack based value.");
            PANIC_ON_ERR(stack_top(state).data > state->heap_size, ERR_INVALID_PTR, 
                "Invalid pointer for free.");
            
            heap_free(state, stack_pop(state).data);
        }
        if(current_token->inst == INST_PTR_GET_I) {
            PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Not enough values for pget specified.");
            RuntimeValue heap_index = stack_pop(state);
            RuntimeValue data_index = stack_pop(state);
            PANIC_ON_ERR(!heap_index.heap_ptr, ERR_INVALID_PTR, "Trying to pget with stack based value.");
            PANIC_ON_ERR(heap_index.data > state->heap_size, ERR_INVALID_PTR, 
                "Invalid pointer for pget.");

            switch (state->heap[heap_index.data].var_type) {
                case VAR_TYPE_INT: {
                    size_t* heap_data = (size_t*)state->heap[heap_index.data].data;
                    stack_push(state, (RuntimeValue) { 
                        .data = heap_data[data_index.data], 
                        .var_type = state->heap[heap_index.data].var_type, 
                        .heap_ptr = false });
                    break;
                }
                case VAR_TYPE_STR: {
                    char** heap_data = (char**)state->heap[heap_index.data].data;
                    char* data_at_index = heap_data[data_index.data];
                    state->heap[state->heap_size++] = (HeapValue){ .data = data_at_index, .var_type = VAR_TYPE_STR };
                    stack_push(state, (RuntimeValue) {
                        .data =  state->heap_size - 1,
                        .heap_ptr = true,
                        .var_type = VAR_TYPE_STR
                    });
                    break;
                }
            }
        } 
        if(current_token->inst == INST_PTR_SET_I) {
            PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "Not enough values for pset specified.");

            RuntimeValue heap_index = stack_pop(state);
            RuntimeValue data_index = stack_pop(state);
            RuntimeValue val = stack_pop(state);

            PANIC_ON_ERR(!heap_index.heap_ptr, ERR_INVALID_PTR, "Trying to pset with stack based value.");
            PANIC_ON_ERR(heap_index.data > state->heap_size, ERR_INVALID_PTR, 
                "Invalid pointer for pset.");
            PANIC_ON_ERR(val.var_type != state->heap[heap_index.data].var_type, 
                ERR_INVALID_DATA_TYPE, "Assigning value of pointer to different data type");

            switch (state->heap[heap_index.data].var_type) {
                case VAR_TYPE_INT: {
                    size_t* heap_data = (size_t*)state->heap[heap_index.data].data;
                    heap_data[data_index.data] = val.data;
                    break;
                }
                case VAR_TYPE_STR: {       
                    char** heap_data = (char**)state->heap[heap_index.data].data;
                    heap_data[data_index.data] = state->heap[val.data].data;
                    break;
                }
            }
        }
        if(current_token->inst == INST_MACRO_USAGE) {
            state->call_positions[state->call_positions_count++] = state->inst_ptr;
            state->inst_ptr = current_token->val.data;
        }
        if(current_token->inst == INST_END_MACRO) {
            state->inst_ptr = state->call_positions[state->call_positions_count - 1];
            state->call_positions_count--;
        }
        if(current_token->inst == INST_MACRO) {
            state->inst_ptr = current_token->val.data;
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
    ProgramState program_state;
    program_state.stack_size = 0;
    program_state.heap_size = 0;
    program_state.stackframe_size = 0;
    program_state.inst_ptr = 0;
    program_state.heap = malloc(1024 * 1024 /* TODO: Dynamic Heap */);
    program_state.stackframe_index = 0;
    Token* program = load_program_from_file(argv[1], &program_size, &program_state);
    program_state.program_size = program_size;
    exec_program(&program_state, program, program_size);
    free(program_state.heap);
    free(program);
    return 0;
} 
