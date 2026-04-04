#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

int main() {
    std::cout << "=== Testing segfault handler behavior ===" << std::endl;
    std::cout << "BEFORE: Handler just printed, returned, infinite loop" << std::endl;
    std::cout << "AFTER:  Handler prints, sets crash flag, exits immediately" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Simulate what happens now
    std::cout << "Test 1: Segfault will trigger handler which calls _exit()" << std::endl;
    std::cout << "Status: This test will immediately exit with status 1" << std::endl;
    std::cout << "Expected output: FATAL error message + clean exit" << std::endl;
    std::cout << std::endl;
    
    // This is exactly what happens in bbarm64 now
    struct sigaction sa;
    sa.sa_sigaction = [](int sig, siginfo_t* info, void* ucontext) {
        std::cerr << "[bbarm64] FATAL: Segmentation fault at address " << info->si_addr << std::endl;
        std::cerr << "[bbarm64] Current guest PC: 0xdeadbeef" << std::endl;
        std::cerr << "[bbarm64] Exiting safely..." << std::endl;
        _exit(EXIT_FAILURE);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    
    std::cout << "Triggering segfault..." << std::endl;
    *(volatile int*)0 = 0xdeadbeef;
    
    // This line is never reached
    std::cout << "This should never print!" << std::endl;
    return 0;
}
EOF && g++ -o direct_test direct_test.cpp && timeout 5s ./direct_test; echo "Exit code: $?"
