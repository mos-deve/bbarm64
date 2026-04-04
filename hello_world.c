#include <unistd.h>

int main(void) {
    write(1, "Hello from C!\n", 14);
    return 0;
}
