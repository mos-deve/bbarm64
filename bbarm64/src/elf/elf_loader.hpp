#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace bbarm64 {

class MemoryManager;
class CPUContext;

// ELF header info
struct ELFInfo {
    uint64_t entry_point;
    uint64_t phdr_addr;
    uint64_t phdr_offset;
    uint16_t phnum;
    uint16_t phentsize;
    uint64_t base_addr;
    bool is_dynamic;
    uint64_t dynamic_addr;
    std::string interp;  // Dynamic linker path
};

// Program header
struct ELFProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

// ELF loader
class ELFLoader {
public:
    ELFLoader(MemoryManager& mem);

    // Load an ARM64 ELF binary
    bool load(const std::string& path, ELFInfo& info);

    // Load a shared library
    bool load_library(const std::string& path, ELFInfo& info, uint64_t base_hint = 0);

    // Get program headers
    const std::vector<ELFProgramHeader>& program_headers() const { return phdrs_; }

    // Parse ELF from memory buffer
    bool parse(const uint8_t* data, size_t size, ELFInfo& info);

private:
    MemoryManager& mem_;
    std::vector<ELFProgramHeader> phdrs_;

    bool load_segments(const uint8_t* elf_data, size_t elf_size, const ELFInfo& info);
    bool load_interp(const std::string& path, std::string& out);
};

} // namespace bbarm64
