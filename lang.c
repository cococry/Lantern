#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define true 1
#define false 0
typedef unsigned char bool;

char* trim_white_space(char *str) {
  char* end;

  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  
    return str;

  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';

  return str;
}
 
bool is_str_num(const char* str) {
	for(uint32_t i = 0; str[i] != '\0'; i++) {
		if(isdigit(str[i]) == false)
			return false;
	}
	return true;
}

typedef enum {
	token_type_plus = 0,
	token_type_minus,
	token_type_print,
	token_type_push_to_stack,
	token_type_number_result
} token_type; 

typedef struct {
	token_type type;
	void* data;
} token;

typedef struct {
	uint32_t size, cap;
	token* tokens;
} token_stack;

token_stack token_stack_create(uint32_t cap) {
	token_stack stack;
	
	stack.size = 0;
	stack.cap = cap;
	stack.tokens = (token*)malloc(cap * sizeof(token));
	return stack;
}  

void token_stack_push(token_stack* stack, token tok) {
	if(stack->size > stack->cap) {
		token* tmp_tokens = stack->tokens;
		free(stack->tokens);
		stack->tokens = (token*)malloc(stack->cap * 2 * sizeof(token));
		stack->tokens = tmp_tokens;
	} 
	else {
		printf("Pushed %i to stack at %i\n", (int)*(int*)tok.data, stack->size);
		stack->tokens[stack->size++] = tok;
	}
}

token token_stack_pop(token_stack* stack) {
	stack->size--;
	token tok = stack->tokens[stack->size];
	token* tmp_stack = stack->tokens;
	free(stack->tokens);
	stack->tokens = (token*)malloc(stack->size * sizeof(token));
	for(uint32_t i = 0; i < stack->size; i++) {
		stack->tokens[i] = tmp_stack[i];
	}
	printf("%i\n", (int)*(int*)tok.data); 
	return tok;
}

void token_stack_delete(token_stack* stack) {
	if(stack->tokens != NULL)
		free(stack->tokens);
}

int32_t read_program_file(const char* filepath, char** tokens) {
	FILE* file = fopen(filepath,"r");		
	char arr[100][50]; 
	int32_t i = 0;
	while(true) { 
		char ch = (char)fgetc(file); 
		int32_t ch_i = 0; 
		while(ch != ' ' && !feof(file)){	 
			arr[i][ch_i++] = ch;		 
			ch = (char)fgetc(file); 
		} 
		arr[i][ch_i] = 0;		 
		if(feof(file)){	 
			break; 
		} 
		i++; 
	} 
	for(int32_t j = 0;j <= i; j++){
		trim_white_space(arr[j]);
		tokens[j] = arr[j];
	}
	return i;
}

void simulate_program(const char* filepath) {
	token* program = malloc(sizeof(token) * 16);
	char** tokens = malloc(512);
	int32_t token_count;
	{
		token_count = read_program_file(filepath, tokens);
		for(uint32_t i = 0; i <= token_count; i++) {
			if(is_str_num(tokens[i])) {
				int32_t data = atoi(tokens[i]);	
				program[i] = (token){token_type_push_to_stack, &data};
			}
			else if(strcmp(tokens[i], "+") == 0) {
				program[i] = (token){token_type_plus, NULL};
			}
			else if(strcmp(tokens[i], "-") == 0) {
				program[i] = (token){token_type_minus, NULL};
			}
			else if(strcmp(tokens[i], "print") == 0) {
				program[i] = (token){token_type_print, NULL};
			} 
		}
	}
	token_stack stack = token_stack_create(token_count);
	for(uint32_t i = 0; i <= token_count; i++) {
		printf("Iterating: %s\n", tokens[i]);
		if(program[i].type == token_type_push_to_stack){
			token_stack_push(&stack, program[i]);
			printf("token_type_push_to_stack %i\n", (int)*(int*)program[i].data);
		}		
		else if(program[i].type == token_type_plus) {
			token a = token_stack_pop(&stack);
			token b = token_stack_pop(&stack);
			int32_t data = (int32_t)*(int32_t*)a.data + (int32_t)*(int32_t*)b.data;	
			token_stack_push(&stack, (token){token_type_number_result, &data}),
			printf("token_type_plus\n");
		}			
		else if(program[i].type == token_type_minus) {
			token a = token_stack_pop(&stack);
			token b = token_stack_pop(&stack);
			int32_t data = (int32_t)*(int32_t*)a.data - (int32_t)*(int32_t*)b.data;	
			token_stack_push(&stack, (token){token_type_number_result, &data});
			printf("token_type_minus\n");
		}					
		else if(program[i].type == token_type_print) {
			int* data = (int*)token_stack_pop(&stack).data;
			printf("%i\n", *data);
			printf("token_type_print\n");
		} 
	}
	token_stack_delete(&stack);
}



int main(int argc, char** argv) {
	if(argc > 2) {
		printf("Too many arguments supplied.\n");
		printf("Usage: lantern <filepath>\n");
		return 1;
	}
	else if(argc < 2) {
		printf("Too few arguments supplied.\n");
		printf("Usage: lantern <filepath>\n");
		return 1;
	}
	simulate_program(argv[1]);
	return 0;
}
