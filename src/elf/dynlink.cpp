#include "dynlink.hpp"
#include "../core/memory_manager.hpp"
#include <cstdio>

namespace bbarm64 {

DynLink::DynLink(MemoryManager& mem) : mem_(mem) {}

bool DynLink::load_library(const std::string& path) {
    fprintf(stderr, "[bbarm64] dynlink: loading %s (stub)\n", path.c_str());
    loaded_libs_.push_back({path, 0});
    return true;
}

uint64_t DynLink::resolve_symbol(const std::string& name) {
    (void)name;
    return 0;  // Stub
}

bool DynLink::process_relocations(uint64_t base_addr, uint64_t dynamic_addr) {
    (void)base_addr;
    (void)dynamic_addr;
    return true;  // Stub
}

} // namespace bbarm64
