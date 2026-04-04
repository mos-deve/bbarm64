#include "syscall_table.hpp"
#include <cstring>

namespace bbarm64 {

int translate_syscall_nr(int arm_nr) {
    for (const auto& entry : syscall_table) {
        if (entry.arm_nr == arm_nr) {
            return entry.x86_nr;
        }
    }
    return -1;  // Unknown syscall
}

const char* get_syscall_name(int arm_nr) {
    for (const auto& entry : syscall_table) {
        if (entry.arm_nr == arm_nr) {
            return entry.name;
        }
    }
    return "unknown";
}

} // namespace bbarm64
