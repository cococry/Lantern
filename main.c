#include <stdio.h>

int main() {
    int i = 0;
    while(i < 10000) {
        if(i % 3 == 0 && i % 5 == 0) {
            printf("fizzbuzz\n");
        } else if(i % 3 == 0) {
            printf("fizz\n");
        } else if(i % 5 == 0) {
            printf("buzz\n");
        }
        printf("%i\n", i);
        i++;
    }
}
