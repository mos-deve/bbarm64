#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <stdint.h>

struct CPUContext {
    uint32_t state_flags;
    static constexpr uint32_t STATE_CRASHED = (1u << 2);
    uint64_t pc;
    CPUContext() : state_flags(0), pc(0xdeadbeef) {}
};

class ExecEngine {
public:
    static ExecEngine* s_instance;
    CPUContext ctx;
    
    ExecEngine() {
        s_instance = this;
    }
    
    static void signal_handler(int sig, siginfo_t* info, void* ucontext) {
        std::cerr << "[bbarm64] FATAL: Segmentation fault at address " << info->si_addr << std::endl;
        std::cerr << "[bbarm64] Current guest PC: 0x" << std::hex << s_instance->ctx.pc << std::endl;
        
        if (s_instance) {
            s_instance->ctx.state_flags |= CPUContext::STATE_CRASHED;
        }
        
        std::cerr << "[bbarm64] Exiting safely..." << std::endl;
        _exit(EXIT_FAILURE);
    }
    
    void run() {
        struct sigaction sa;
        sa.sa_sigaction = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, nullptr);
        
        std::cout << "Running execution loop..." << std::endl;
        while (!(ctx.state_flags & CPUContext::STATE_CRASHED)) {
            // Simulate execution that will segfault
            *(volatile int*)0 = 0xdeadbeef;
        }
        std::cout << "Loop exited cleanly" << std::endl;
    }
};

ExecEngine* ExecEngine::s_instance = nullptr;

int main() {
    ExecEngine engine;
    engine.run();
    return 0;
}
EOF && g++ -o test_handler test_handler.cpp && echo "Running test..." && ./test_handler
