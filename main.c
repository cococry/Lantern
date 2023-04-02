#include <stdio.h>

int main() {
    void* data;

    {
        int x = 5;
        data = &x;
    }
    printf("Data is: %i\n", *(int*)data);
    return 0;
}