#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define STACK_CAP 64
#define PROGRAM_CAP 1024

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])
                                      
#define PANIC_ON_ERR(cond, err_type, ...)  {                                            \
    if(cond) {                                                                          \
        printf("Lantern: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type);  \
         printf(__VA_ARGS__);                                                           \
         printf("\n");                                                                  \
    }                                                                                   \
}                                                                                       \

typedef enum {
    INST_STACK_PUSH,
    INST_STACK_PREV,
    INST_PLUS,
    INST_PRINT,
    INST_JUMP,
} Instruction;

typedef enum {
    ERR_STACK_OVERFLOW,
    ERR_STACK_UNDERFLOW,
    ERR_INVALID_JUMP,
    ERR_INVALID_STACK_ACCESS,
    ERR_ILLEGAL_INSTRUCTION,
} Error;

typedef struct {
    Instruction inst;
    int32_t data;
} Token;

typedef struct {
    int32_t stack[STACK_CAP];
    uint32_t stack_size;

    uint32_t inst_ptr;
} ProgramState;


void exec_program(ProgramState* state, Token* program, uint32_t program_size) {
    while(state->inst_ptr < program_size) {
        switch (program[state->inst_ptr].inst) {
            case INST_STACK_PUSH: {
                PANIC_ON_ERR(state->stack_size >= STACK_CAP, ERR_STACK_OVERFLOW, "Stack is overflowed");
                state->stack[state->stack_size++] = program[state->inst_ptr].data;
                break;
            }
            case INST_PLUS: {
                PANIC_ON_ERR(state->stack_size < 2, ERR_STACK_UNDERFLOW, "Too few values on stack for plus operation.");
                
                int32_t a = state->stack[state->stack_size - 1];
                int32_t b = state->stack[state->stack_size - 2];
                state->stack_size -= 2;
                state->stack[state->stack_size++] = a + b;
                break;
            }
            case INST_PRINT: {
                PANIC_ON_ERR(state->stack_size < 1, ERR_STACK_UNDERFLOW, "No value for print function on stack.");

                int32_t val = state->stack[state->stack_size - 1];
                state->stack_size--;
                printf("%i\n", val);
                break;
            }
            case INST_JUMP: {
                int32_t index = program[state->inst_ptr].data;
                PANIC_ON_ERR(index >= (int32_t)program_size || index < 0, ERR_INVALID_JUMP, "Invalid index for jump specified.");

                state->inst_ptr = index - 1;
                break;
            }
            case INST_STACK_PREV: {
                int32_t index = (state->stack_size - 1) - program[state->inst_ptr].data;
                PANIC_ON_ERR(index >= (int32_t)state->stack_size || index < 0, ERR_INVALID_STACK_ACCESS, "Invalid index for retrieving value from stack");
                state->stack[state->stack_size++] = state->stack[index];
                break;
            }
            default:
                PANIC_ON_ERR(true, ERR_ILLEGAL_INSTRUCTION, "Illegal instruction specified.");
                break;
        }
        state->inst_ptr++;
    }
}

Token program[] = {
    {.inst = INST_STACK_PUSH, .data = 0},
    {.inst = INST_STACK_PUSH, .data = 1},
    {.inst = INST_PLUS},
    {.inst = INST_STACK_PREV, .data = 0},
    {.inst = INST_PRINT},
    {.inst = INST_JUMP, .data = 1}
};
int main() {
    ProgramState program_state;
    program_state.stack_size = 0;
    program_state.inst_ptr = 0;
    exec_program(&program_state, program, ARRAY_SIZE(program));
    return 0;
}