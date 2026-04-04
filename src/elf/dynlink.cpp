#include "dynlink.hpp"
#include "../core/memory_manager.hpp"
#include "../elf/elf_loader.hpp"
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

namespace bbarm64 {

DynLink::DynLink(MemoryManager& mem) : mem_(mem) {}

// Load and link a shared library
bool DynLink::load_library(const std::string& path) {
    fprintf(stderr, "[bbarm64] dynlink: loading %s\n", path.c_str());

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("[bbarm64] dynlink: failed to open library");
        return false;
    }

    struct stat st;
    fstat(fd, &st);

    uint8_t* data = static_cast<uint8_t*>(
        mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)
    );
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    // Parse ELF header
    if (st.st_size < sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "[bbarm64] dynlink: file too small\n");
        munmap(data, st.st_size);
        close(fd);
        return false;
    }

    const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    // Verify ELF
    if (ehdr->e_ident[EI_MAG0] != 0x7F || ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' || ehdr->e_ident[EI_MAG3] != 'F') {
        fprintf(stderr, "[bbarm64] dynlink: not an ELF\n");
        munmap(data, st.st_size);
        close(fd);
        return false;
    }

    if (ehdr->e_machine != EM_AARCH64) {
        fprintf(stderr, "[bbarm64] dynlink: not ARM64 ELF (machine: %d)\n", ehdr->e_machine);
        munmap(data, st.st_size);
        close(fd);
        return false;
    }

    // Find base address and load segments
    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + ehdr->e_phoff + i * ehdr->e_phentsize
        );
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_vaddr < min_vaddr) min_vaddr = phdr->p_vaddr;
            if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        }
    }

    uint64_t base_addr = min_vaddr & ~0xFFFULL;
    uint64_t total_size = max_vaddr - base_addr + 0x1000;

    // Map segments
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + ehdr->e_phoff + i * ehdr->e_phentsize
        );
        if (phdr->p_type != PT_LOAD) continue;

        int prot = 0;
        if (phdr->p_flags & PF_R) prot |= PROT_READ;
        if (phdr->p_flags & PF_W) prot |= PROT_WRITE;
        if (phdr->p_flags & PF_X) prot |= PROT_EXEC;

        uint64_t page_vaddr = base_addr + (phdr->p_vaddr & ~0xFFFULL);
        uint64_t page_offset = phdr->p_vaddr & 0xFFFULL;
        uint64_t map_size = (page_offset + phdr->p_memsz + 0xFFF) & ~0xFFFULL;

        uint64_t mapped = mem_.mmap(map_size, prot | PROT_WRITE, page_vaddr);
        if (!mapped) {
            fprintf(stderr, "[bbarm64] dynlink: failed to map segment at 0x%lx\n", page_vaddr);
            munmap(data, st.st_size);
            close(fd);
            return false;
        }

        if (phdr->p_filesz > 0) {
            uint8_t* host_ptr = reinterpret_cast<uint8_t*>(page_vaddr);
            memcpy(host_ptr + page_offset, data + phdr->p_offset, phdr->p_filesz);
        }

        if (phdr->p_memsz > phdr->p_filesz) {
            uint64_t bss_start = page_vaddr + page_offset + phdr->p_filesz;
            memset(reinterpret_cast<uint8_t*>(bss_start), 0, phdr->p_memsz - phdr->p_filesz);
        }

        if (prot != (prot | PROT_WRITE)) {
            mem_.mprotect(page_vaddr, map_size, prot);
        }
    }

    // Process dynamic section for relocations
    uint64_t dynamic_addr = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + ehdr->e_phoff + i * ehdr->e_phentsize
        );
        if (phdr->p_type == PT_DYNAMIC) {
            dynamic_addr = base_addr + phdr->p_vaddr;
            break;
        }
    }

    if (dynamic_addr) {
        process_relocations(base_addr, dynamic_addr);
    }

    loaded_libs_.push_back({path, base_addr});
    fprintf(stderr, "[bbarm64] dynlink: loaded %s at 0x%lx\n", path.c_str(), base_addr);

    munmap(data, st.st_size);
    close(fd);
    return true;
}

// Resolve a symbol from loaded libraries
uint64_t DynLink::resolve_symbol(const std::string& name) {
    // Stub: in a full implementation, search .dynsym of loaded libraries
    (void)name;
    return 0;
}

// Process relocations for a loaded library
bool DynLink::process_relocations(uint64_t base_addr, uint64_t dynamic_addr) {
    // Read dynamic section entries
    uint64_t rel_addr = 0, rel_size = 0, rel_ent = 0;
    uint64_t rela_addr = 0, rela_size = 0, rela_ent = 0;
    uint64_t symtab = 0, strtab = 0;

    const Elf64_Dyn* dyn = reinterpret_cast<const Elf64_Dyn*>(dynamic_addr);
    for (; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
            case DT_REL:    rel_addr = base_addr + dyn->d_un.d_ptr; break;
            case DT_RELSZ:  rel_size = dyn->d_un.d_val; break;
            case DT_RELENT: rel_ent = dyn->d_un.d_val; break;
            case DT_RELA:    rela_addr = base_addr + dyn->d_un.d_ptr; break;
            case DT_RELASZ:  rela_size = dyn->d_un.d_val; break;
            case DT_RELAENT: rela_ent = dyn->d_un.d_val; break;
            case DT_SYMTAB:  symtab = base_addr + dyn->d_un.d_ptr; break;
            case DT_STRTAB:  strtab = base_addr + dyn->d_un.d_ptr; break;
            default: break;
        }
    }

    // Process RELA relocations (most common on ARM64)
    if (rela_addr && rela_size && rela_ent) {
        size_t count = rela_size / rela_ent;
        for (size_t i = 0; i < count; i++) {
            const Elf64_Rela* rela = reinterpret_cast<const Elf64_Rela*>(rela_addr + i * rela_ent);
            uint64_t* reloc_addr = reinterpret_cast<uint64_t*>(base_addr + rela->r_offset);
            uint32_t type = ELF64_R_TYPE(rela->r_info);

            switch (type) {
                case R_AARCH64_RELATIVE:
                    *reloc_addr = base_addr + rela->r_addend;
                    break;
                case R_AARCH64_ABS64:
                    // Absolute relocation - would need symbol resolution
                    fprintf(stderr, "[bbarm64] dynlink: ABS64 reloc at 0x%lx (stub)\n",
                            (uint64_t)reloc_addr);
                    break;
                case R_AARCH64_GLOB_DAT:
                case R_AARCH64_JUMP_SLOT:
                    // GOT entries - would need symbol resolution
                    break;
                default:
                    break;
            }
        }
    }

    // Process REL relocations
    if (rel_addr && rel_size && rel_ent) {
        size_t count = rel_size / rel_ent;
        for (size_t i = 0; i < count; i++) {
            const Elf64_Rel* rel = reinterpret_cast<const Elf64_Rel*>(rel_addr + i * rel_ent);
            uint64_t* reloc_addr = reinterpret_cast<uint64_t*>(base_addr + rel->r_offset);
            uint32_t type = ELF64_R_TYPE(rel->r_info);

            if (type == R_AARCH64_RELATIVE) {
                // REL doesn't have addend, use existing value
                *reloc_addr += base_addr;
            }
        }
    }

    return true;
}

} // namespace bbarm64
