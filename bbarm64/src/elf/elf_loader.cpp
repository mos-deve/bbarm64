#include "elf_loader.hpp"
#include "../core/memory_manager.hpp"
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>

namespace bbarm64 {

ELFLoader::ELFLoader(MemoryManager& mem) : mem_(mem) {}

bool ELFLoader::load(const std::string& path, ELFInfo& info) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("[bbarm64] Failed to open ELF");
        return false;
    }

    struct stat st;
    fstat(fd, &st);

    uint8_t* data = static_cast<uint8_t*>(
        ::mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)
    );
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    bool result = parse(data, st.st_size, info);
    if (result) {
        result = load_segments(data, st.st_size, info);
    }

    ::munmap(data, st.st_size);
    close(fd);
    return result;
}

bool ELFLoader::load_library(const std::string& path, ELFInfo& info, uint64_t base_hint) {
    (void)base_hint;
    return load(path, info);
}

bool ELFLoader::parse(const uint8_t* data, size_t size, ELFInfo& info) {
    if (size < sizeof(Elf64_Ehdr)) return false;

    const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    // Verify ELF magic
    if (ehdr->e_ident[EI_MAG0] != 0x7F || ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' || ehdr->e_ident[EI_MAG3] != 'F') {
        fprintf(stderr, "[bbarm64] Not an ELF file\n");
        return false;
    }

    // Verify ARM64
    if (ehdr->e_machine != EM_AARCH64) {
        fprintf(stderr, "[bbarm64] Not an ARM64 ELF (machine: %d)\n", ehdr->e_machine);
        return false;
    }

    // Verify 64-bit
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "[bbarm64] Not a 64-bit ELF\n");
        return false;
    }

    info.entry_point = ehdr->e_entry;
    info.phdr_offset = ehdr->e_phoff;
    info.phnum = ehdr->e_phnum;
    info.phentsize = ehdr->e_phentsize;
    info.is_dynamic = false;
    info.dynamic_addr = 0;
    info.base_addr = 0;

    // Find the lowest vaddr for base
    uint64_t min_vaddr = UINT64_MAX;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + ehdr->e_phoff + i * ehdr->e_phentsize
        );
        if (phdr->p_type == PT_LOAD && phdr->p_vaddr < min_vaddr) {
            min_vaddr = phdr->p_vaddr;
        }
    }
    info.base_addr = min_vaddr;

    // Parse program headers
    phdrs_.clear();
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + ehdr->e_phoff + i * ehdr->e_phentsize
        );

        ELFProgramHeader ph;
        ph.type = phdr->p_type;
        ph.flags = phdr->p_flags;
        ph.offset = phdr->p_offset;
        ph.vaddr = phdr->p_vaddr;
        ph.paddr = phdr->p_paddr;
        ph.filesz = phdr->p_filesz;
        ph.memsz = phdr->p_memsz;
        ph.align = phdr->p_align;
        phdrs_.push_back(ph);

        if (phdr->p_type == PT_DYNAMIC) {
            info.is_dynamic = true;
            info.dynamic_addr = phdr->p_vaddr;
        }

        if (phdr->p_type == PT_INTERP) {
            const char* interp = reinterpret_cast<const char*>(data + phdr->p_offset);
            info.interp = interp;
        }
    }

    return true;
}

bool ELFLoader::load_segments(const uint8_t* elf_data, size_t elf_size, const ELFInfo& info) {
    (void)elf_size;
    (void)info;

    for (const auto& ph : phdrs_) {
        if (ph.type != PT_LOAD) continue;

        int prot = 0;
        if (ph.flags & PF_R) prot |= PROT_READ;
        if (ph.flags & PF_W) prot |= PROT_WRITE;
        if (ph.flags & PF_X) prot |= PROT_EXEC;

        // Align vaddr down to page boundary
        uint64_t page_vaddr = ph.vaddr & ~(4095ULL);
        uint64_t page_offset = ph.vaddr & 4095ULL;
        uint64_t map_size = (page_offset + ph.memsz + 4095ULL) & ~4095ULL;

        // Map with write permission initially so we can copy data
        int load_prot = prot | PROT_WRITE;

        // Allocate memory from memory manager
        uint64_t mapped = mem_.mmap(map_size, load_prot, page_vaddr);
        if (!mapped) {
            fprintf(stderr, "[bbarm64] Failed to map segment at 0x%lx (size 0x%lx)\n",
                    page_vaddr, map_size);
            return false;
        }

        // Copy segment data from ELF file
        if (ph.filesz > 0) {
            uint8_t* host_ptr = reinterpret_cast<uint8_t*>(page_vaddr);
            memcpy(host_ptr + page_offset, elf_data + ph.offset, ph.filesz);
        }

        // Zero BSS area (memsz > filesz)
        if (ph.memsz > ph.filesz) {
            uint64_t bss_start = page_vaddr + page_offset + ph.filesz;
            size_t bss_size = ph.memsz - ph.filesz;
            memset(reinterpret_cast<uint8_t*>(bss_start), 0, bss_size);
        }

        // Zero the padding before the segment (from page start to page_offset)
        if (page_offset > 0) {
            memset(reinterpret_cast<uint8_t*>(page_vaddr), 0, page_offset);
        }

        // Now set the correct permissions
        if (prot != load_prot) {
            mem_.mprotect(page_vaddr, map_size, prot);
        }
    }

    return true;
}

bool ELFLoader::load_interp(const std::string& path, std::string& out) {
    (void)path;
    (void)out;
    return false;
}

} // namespace bbarm64
