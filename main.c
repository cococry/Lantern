#include <stdio.h>

int main() {
    void* stackframe[64];

    int x = 5;
    char* str = "hello";
    stackframe[0] = &x;
    stackframe[1] = str;


    printf("Stackframe at 0: %i\n", *(int*)stackframe[0]);
    printf("Stackframe at 1: %s\n", (char*)stackframe[1]);
    return 0;
}