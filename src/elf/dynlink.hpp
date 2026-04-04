#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bbarm64 {

class MemoryManager;

// Dynamic linker for ARM64 .so files
class DynLink {
public:
    DynLink(MemoryManager& mem);

    // Load and link a shared library
    bool load_library(const std::string& path);

    // Resolve a symbol
    uint64_t resolve_symbol(const std::string& name);

    // Process relocations for a loaded library
    bool process_relocations(uint64_t base_addr, uint64_t dynamic_addr);

private:
    MemoryManager& mem_;
    struct LoadedLib {
        std::string path;
        uint64_t base_addr;
    };
    std::vector<LoadedLib> loaded_libs_;
};

} // namespace bbarm64
