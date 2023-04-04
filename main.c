#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
    uint32_t high = 5;
    uint32_t low = 12;
    uint64_t combined = (((uint64_t) high) << 32) | ((uint64_t) low);
    printf("combined: %li", combined << 32);
    return 0;
}
