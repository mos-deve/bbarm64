#include <stdio.h>

int main() {
    printf("This program will intentionally segfault\n");
    fflush(stdout);
    
    // Write to NULL pointer - will cause SIGSEGV
    *(volatile int*)0 = 0xdeadbeef;
    
    printf("This line should never be reached\n");
    return 0;
}
