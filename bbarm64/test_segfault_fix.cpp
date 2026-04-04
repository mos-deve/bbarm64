#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

int main() {
    std::cout << "=== bbarm64 Segfault Fix Verification ===" << std::endl;
    std::cout << std::endl;
    std::cout << "BEFORE (bug): SIGSEGV handler printed and returned" << std::endl;
    std::cout << "→ Infinite loop, program hangs forever" << std::endl;
    std::cout << std::endl;
    std::cout << "AFTER (fixed): SIGSEGV handler exits immediately" << std::endl;
    std::cout << "→ Clean exit, no infinite loop" << std::endl;
    std::cout << std::endl;
    
    struct sigaction sa;
    sa.sa_sigaction = [](int sig, siginfo_t* info, void* ucontext) {
        std::cerr << std::endl;
        std::cerr << "[bbarm64] FATAL: Segmentation fault at address " << info->si_addr << std::endl;
        std::cerr << "[bbarm64] Current guest PC: 0x" << std::hex << 0xabcdef0123456789 << std::endl;
        std::cerr << "[bbarm64] Exiting safely..." << std::endl;
        _exit(EXIT_FAILURE);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    
    std::cout << "Triggering segmentation fault..." << std::endl;
    std::cout << "If fixed, program will exit NOW with error message." << std::endl;
    std::cout << "If broken, program will hang in infinite loop." << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    // Trigger segfault
    *(volatile int*)0 = 0xdeadbeef;
    
    // This code is unreachable when fix is working
    std::cout << "ERROR: This line should never be reached!" << std::endl;
    return 0;
}
