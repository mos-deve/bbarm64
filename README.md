# bbarm64-emu (Block-based ARM64 Emulator.)

**v1.2** — A high-performance ARM64-to-x86_64 dynamic binary translator with JIT compilation.
Developed by the **FAE (Fast Arm Emulation)** group.

## Overview

bbarm64-emu is an ARM64 emulator that runs ARM64 ELF binaries on x86_64 Linux hosts. It uses a **hybrid AOT + JIT** approach with the **FAE-Cache** (Fast Arm Emulation Cache) system: ARM64 basic blocks are translated to native x86_64 machine code at runtime, cached, and executed directly. When translation isn't possible (unknown instructions, syscalls), it gracefully falls back to an interpreter.

## Features

- **AOT + JIT Compilation Pipeline**: ARM64 → Decoder → IR Builder → IR Optimizer → x86_64 Emitter → Native Execution
- **FAE-Cache**: Refined translation cache with LRU eviction, block profiling, and hot-path prioritization
- **Hybrid Execution**: JIT for hot paths, interpreter fallback for edge cases
- **Syscall Interception**: Guest ARM64 syscalls are forwarded to the host Linux kernel
- **ELF Loading**: Supports statically-linked and dynamically-linked ARM64 ELF binaries
- **TLS Support**: Thread-local storage via TPIDR_EL0 → x86_64 FS segment mapping
- **NZCV → EFLAGS Mapping**: Full conditional branch support in JIT code
- **SIMD/FP via AVX2**: NEON and floating-point instructions translated to AVX2
- **Graceful Degradation**: Unknown instructions fall back to interpreter instead of crashing
- **Android Library Stubs**: Pre-wired stubs for common Android shared libraries

## Quick Start

### Building

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

**Requirements:**
- CMake 3.14+
- clang++ or g++ with C++20 support
- x86_64 Linux host
- Rust toolchain (for memory-safe allocator via cxx bridge)

### Running

```bash
./build/bbarm64 <arm64_binary> [args...]
```

**Example:**
```bash
# Run a statically-linked ARM64 hello world
./build/bbarm64 hello_world_arm64

# Run with syscall logging
BBARM64_LOG_SYSCALLS=1 ./build/bbarm64 hello_world_arm64
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `BBARM64_DUMP` | `0` | Dump translated JIT blocks to stderr |
| `BBARM64_LOG_SYSCALLS` | `0` | Log every syscall made by the guest |
| `BBARM64_LOG_SIGNALS` | `0` | Log signal handling events |
| `BBARM64_SMC_DETECT` | `1` | Enable self-modifying code detection |
| `BBARM64_MAX_BLOCK` | `64` | Maximum instructions per translated block |
| `BBARM64_CACHE_SIZE` | `64` | FAE-Cache size in megabytes |
| `BBARM64_AOT_DIR` | `~/.cache/bbarm64/aot` | AOT cache directory for persistent translations |
| `BBARM64_MODE` | `jit` | Execution mode: `jit`, `aot`, or `hybrid` |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      bbarm64-emu v1.2                        │
│                  (FAE - Fast Arm Emulation)                  │
│                                                              │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐               │
│  │  ARM64   │───▶│   IR     │───▶│ x86_64   │               │
│  │ Decoder  │    │ Builder  │    │ Emitter  │               │
│  └──────────┘    └──────────┘    └──────────┘               │
│       │                │                │                    │
│       ▼                ▼                ▼                    │
│  ┌──────────────────────────────────────────────┐           │
│  │              FAE-Cache                         │           │
│  │  ┌────────────┐  ┌────────────┐  ┌─────────┐ │           │
│  │  │ L1: In-Mem │  │ L2: AOT    │  │ L3:     │ │           │
│  │  │ Hash Table │  │ Persistent │  │ Chaining│ │           │
│  │  └────────────┘  └────────────┘  └─────────┘ │           │
│  └──────────────────────────────────────────────┘           │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────────────────────────────────┐           │
│  │           CPU Context (ARM64)                 │           │
│  │  x[0..30], sp, pc, lr, nzcv, tpidr_el0       │           │
│  │  v[0..31] (SIMD/FP via AVX2)                 │           │
│  └──────────────────────────────────────────────┘           │
│                                                              │
│  ┌──────────────────────────────────────────────┐           │
│  │           Syscall Handlers                    │           │
│  │  ARM64 syscall numbers → host Linux syscalls  │           │
│  └──────────────────────────────────────────────┘           │
│                                                              │
│  ┌──────────────────────────────────────────────┐           │
│  │           Dynamic Linker (fake ld.so)         │           │
│  │  ELF .so loading, symbol resolution, relocs   │           │
│  └──────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

### Execution Flow

1. **AOT Lookup**: Check persistent AOT cache for pre-translated block
2. **L1 Cache Lookup**: Check in-memory FAE-Cache for hot block
3. **Translate** (if cache miss): Decode → Build IR → Optimize → Emit x86_64 → Cache
4. **Execute**: Call translated block as a native function
5. **Chain**: Direct jump to next translated block (L3 chaining)
6. **Return**: Block updates `ctx.pc` and returns via `ret`
7. **Loop**: Run loop looks up next block by new `ctx.pc`
8. **Fallback**: SVC and unknown instructions handled by interpreter

## Supported Instructions

### Data Processing
| Instruction | Description | JIT | Interpreter |
|---|---|---|---|
| ADD / SUB | Add/subtract (reg + imm) | ✅ | ✅ |
| MUL | Multiply | ✅ | ✅ |
| AND / ORR / EOR | Logical operations | ✅ | ✅ |
| MOVZ / MOVK / MOVN | Move wide | ✅ | ✅ |
| MOV (reg) | Register move | ✅ | ✅ |
| ADR / ADRP | PC-relative address | ✅ | ✅ |
| CMP / TST | Compare/test | ✅ | ✅ |
| CSEL | Conditional select | ❌ | ✅ |

### Load/Store
| Instruction | Description | JIT | Interpreter |
|---|---|---|---|
| LDR / STR (imm) | Load/store with immediate offset | ✅ | ✅ |
| LDR / STR (reg) | Load/store with register offset | ✅ | ✅ |
| LDR / STR (pre/post) | Pre/post-indexed | ❌ | ✅ |
| LDRB / LDRH / STRB / STRH | Byte/halfword variants | ❌ | ✅ |
| LDP / STP | Load/store pair | ✅ | ✅ |

### Control Flow
| Instruction | Description | JIT | Interpreter |
|---|---|---|---|
| B | Unconditional branch | ✅ | ✅ |
| BL | Branch with link | ✅ | ✅ |
| BR | Branch to register | ✅ | ✅ |
| RET | Return from subroutine | ✅ | ✅ |
| B.cond | Conditional branch | ✅* | ✅ |
| CBZ / CBNZ | Compare and branch on zero | ✅* | ✅ |
| TBZ / TBNZ | Test and branch | ✅* | ✅ |

*\* Conditional branches use NZCV→EFLAGS mapping in JIT code for full native performance.*

### System
| Instruction | Description | JIT | Interpreter |
|---|---|---|---|
| NOP | No operation | ✅ | ✅ |
| DMB / DSB / ISB | Memory barriers | ✅ | ✅ |
| SVC | Supervisor call | ❌ | ✅ |

## Syscall Support

Core syscalls implemented for ARM64 → x86_64 forwarding:

| Category | Syscalls |
|---|---|
| I/O | read, write, close, openat |
| Memory | mmap, mprotect, munmap, brk, mremap |
| Process | exit, exit_group, gettid, set_tid_address |
| Threading | futex, clone, clone3, set_robust_list |
| Signals | rt_sigaction, rt_sigprocmask, rt_sigreturn |
| Misc | prctl, getrandom, newfstatat, fstat |

## Test Results

| Binary | Type | Status |
|---|---|---|
| `hello_world_arm64` | Raw ARM64 assembly | ✅ Full JIT, prints output, exits 0 |
| `hello_simple` | Statically-linked C (musl) | ✅ JIT pipeline works — decodes, translates, executes blocks with conditional branches |
| `hello_glibc_static` | Statically-linked C (glibc) | ⚠️ Partial — TLS + NZCV support, needs more init emulation |
| `hello_dynamic` | Dynamically-linked C | ⚠️ Partial — fake ld.so wired up, needs symbol resolution |

## Known Limitations

- **Dynamic linking**: Basic support via fake ld.so; complex .so chains need additional stubs and symbol resolution
- **SIMD/FP**: DUP instruction decoded and JIT-emitted; vector arithmetic ops fall back to interpreter
- **Self-modifying code**: Basic SMC detection exists but may not catch all cases
- **ARM 32-bit (AArch32)**: Not currently supported

## Roadmap

### v1.3 (Planned)
- **Full dynamic linking**: Complete .so resolution, PLT/GOT handling
- **AVX-512 support**: Wider SIMD for hosts that support it
- **Persistent AOT cache**: Disk-based translation cache for instant startup
- **Block chaining optimization**: Direct jumps between translated blocks
- **More instruction coverage**: Atomic operations, CRC, pointer authentication
- **ARM 32-bit (AArch32)**: Planned but not currently the focus

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── LICENSE                     # MIT License
├── .gitignore
├── src/
│   ├── main.cpp                # Entry point, ELF loading, stack setup
│   ├── core/
│   │   ├── cpu_context.hpp     # ARM64 CPU state (x0-x30, sp, pc, lr, nzcv, SIMD)
│   │   ├── cpu_context.cpp     # CPU context implementation
│   │   ├── exec_engine.cpp     # Main execution loop, AOT + JIT + interpreter
│   │   ├── exec_engine.hpp     # Execution engine interface
│   │   ├── memory_manager.cpp  # Guest memory management (mmap, read/write)
│   │   └── memory_manager.hpp  # Memory manager interface
│   ├── decoder/
│   │   ├── arm64_decoder.cpp   # ARM64 instruction decoder
│   │   └── arm64_decoder.hpp   # Decoder interface
│   ├── ir/
│   │   ├── ir.hpp              # Intermediate representation definitions
│   │   ├── ir_builder.cpp      # DecodedInstr → IRBlock
│   │   └── ir_optimizer.cpp    # Constant propagation, DCE, flag merging
│   ├── backend/
│   │   ├── x86_64_emitter.cpp  # x86_64 machine code generator (AVX2 support)
│   │   ├── x86_64_emitter.hpp  # Emitter interface
│   │   ├── x86_64_regalloc.cpp # Register allocation (ARM64 → x86_64)
│   │   ├── x86_64_regalloc.hpp # RegAlloc interface
│   │   ├── x86_64_lower.cpp    # IR → x86_64 lowering (NZCV→EFLAGS)
│   │   └── x86_64_lower.hpp    # Lowering interface
│   ├── cache/
│   │   ├── translation_cache.cpp # FAE-Cache: refined block cache
│   │   ├── translation_cache.hpp # Cache interface
│   │   └── block_chaining.cpp    # L3 direct block chaining
│   ├── elf/
│   │   ├── elf_loader.cpp      # ELF binary loader
│   │   ├── elf_loader.hpp      # ELF loader interface
│   │   ├── dynlink.cpp         # Dynamic linker (fake ld.so)
│   │   └── dynlink.hpp         # Dynamic linker interface
│   ├── syscall/
│   │   ├── syscall_handlers.cpp # Individual syscall implementations
│   │   ├── syscall_handlers.hpp # Syscall handler interface
│   │   ├── syscall_table.cpp    # ARM64 → x86_64 syscall number mapping
│   │   └── syscall_table.hpp    # Syscall table interface
│   ├── threading/
│   │   ├── thread_manager.cpp  # Thread management (clone, futex)
│   │   ├── thread_manager.hpp  # Thread manager interface
│   │   ├── tls_emu.cpp         # TLS emulation (TPIDR_EL0 → FS)
│   │   └── tls_emu.hpp         # TLS emulation interface
│   └── util/
│       ├── log.hpp             # Logging utilities
│       ├── log.cpp             # Logging implementation
│       ├── config.hpp          # Configuration from environment
│       ├── config.cpp          # Config implementation
│       ├── profiler.hpp        # Execution profiling
│       └── profiler.cpp        # Profiler implementation
├── android_libraries/          # Stub libraries for Android compatibility
│   ├── README.md               # Android stubs documentation
│   ├── libc.so.stub            # libc symbol stubs
│   └── libm.so.stub            # libm symbol stubs
├── rust/
│   ├── Cargo.toml              # Rust package config (bbarm64_rust)
│   ├── Cargo.lock              # Dependency lock file
│   ├── build.rs                # cxx-build bridge compiler
│   ├── src/
│   │   └── lib.rs              # Memory-safe allocator (rust_alloc, rust_free, etc.)
│   └── target/                 # Rust build output (gitignored)
├── tests/
│   ├── README.md               # Test documentation
│   ├── hello_world.s           # ARM64 assembly hello world source
│   ├── hello_asm/              # Assembled hello world binary
│   ├── hello_static/           # Statically-linked test binary
│   └── hello_dynamic/          # Dynamically-linked test binary
├── hello_world.c               # C source for hello world test
├── hello_world_arm64           # Pre-built ARM64 test binary
├── hello_simple                # Statically-linked musl test binary
├── hello_glibc_static          # Statically-linked glibc test binary
├── hello_glibc                 # Dynamically-linked glibc test binary
├── context.md                  # Development context (agent memory)
├── findings.md                 # Research findings (agent knowledge base)
└── build/                      # CMake build output (gitignored)
```
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── LICENSE                     # MIT License
├── .gitignore
├── src/
│   ├── main.cpp                # Entry point, ELF loading, stack setup
│   ├── core/
│   │   ├── cpu_context.hpp     # ARM64 CPU state (x0-x30, sp, pc, lr, nzcv, SIMD)
│   │   ├── cpu_context.cpp     # CPU context implementation
│   │   ├── exec_engine.cpp     # Main execution loop, AOT + JIT + interpreter
│   │   ├── exec_engine.hpp     # Execution engine interface
│   │   ├── memory_manager.cpp  # Guest memory management (mmap, read/write)
│   │   └── memory_manager.hpp  # Memory manager interface
│   ├── decoder/
│   │   ├── arm64_decoder.cpp   # ARM64 instruction decoder
│   │   └── arm64_decoder.hpp   # Decoder interface
│   ├── ir/
│   │   ├── ir.hpp              # Intermediate representation definitions
│   │   ├── ir_builder.cpp      # DecodedInstr → IRBlock
│   │   └── ir_optimizer.cpp    # Constant propagation, DCE, flag merging
│   ├── backend/
│   │   ├── x86_64_emitter.cpp  # x86_64 machine code generator (AVX2 support)
│   │   ├── x86_64_emitter.hpp  # Emitter interface
│   │   ├── x86_64_regalloc.cpp # Register allocation (ARM64 → x86_64)
│   │   ├── x86_64_regalloc.hpp # RegAlloc interface
│   │   ├── x86_64_lower.cpp    # IR → x86_64 lowering (NZCV→EFLAGS)
│   │   └── x86_64_lower.hpp    # Lowering interface
│   ├── cache/
│   │   ├── translation_cache.cpp # FAE-Cache: refined block cache
│   │   ├── translation_cache.hpp # Cache interface
│   │   └── block_chaining.cpp    # L3 direct block chaining
│   ├── elf/
│   │   ├── elf_loader.cpp      # ELF binary loader
│   │   ├── elf_loader.hpp      # ELF loader interface
│   │   ├── dynlink.cpp         # Dynamic linker (fake ld.so)
│   │   └── dynlink.hpp         # Dynamic linker interface
│   ├── syscall/
│   │   ├── syscall_handlers.cpp # Individual syscall implementations
│   │   ├── syscall_handlers.hpp # Syscall handler interface
│   │   ├── syscall_table.cpp    # ARM64 → x86_64 syscall number mapping
│   │   └── syscall_table.hpp    # Syscall table interface
│   ├── threading/
│   │   ├── thread_manager.cpp  # Thread management (clone, futex)
│   │   ├── thread_manager.hpp  # Thread manager interface
│   │   ├── tls_emu.cpp         # TLS emulation (TPIDR_EL0 → FS)
│   │   └── tls_emu.hpp         # TLS emulation interface
│   └── util/
│       ├── log.hpp             # Logging utilities
│       ├── log.cpp             # Logging implementation
│       ├── config.hpp          # Configuration from environment
│       ├── config.cpp          # Config implementation
│       ├── profiler.hpp        # Execution profiling
│       └── profiler.cpp        # Profiler implementation
├── android_libraries/          # Stub libraries for Android compatibility
│   ├── README.md               # Android stubs documentation
│   ├── libc.so.stub            # libc symbol stubs
│   └── libm.so.stub            # libm symbol stubs
├── rust/
│   ├── Cargo.toml              # Rust package config (bbarm64_rust)
│   ├── Cargo.lock              # Dependency lock file
│   ├── build.rs                # cxx-build bridge compiler
│   ├── src/
│   │   └── lib.rs              # Memory-safe allocator (rust_alloc, rust_free, etc.)
│   └── target/                 # Rust build output (gitignored)
├── tests/
│   ├── README.md               # Test documentation
│   ├── hello_world.s           # ARM64 assembly hello world source
│   ├── hello_asm/              # Assembled hello world binary
│   ├── hello_static/           # Statically-linked test binary
│   └── hello_dynamic/          # Dynamically-linked test binary
├── hello_world.c               # C source for hello world test
├── hello_world_arm64           # Pre-built ARM64 test binary
├── hello_simple                # Statically-linked musl test binary
├── hello_glibc_static          # Statically-linked glibc test binary
├── hello_glibc                 # Dynamically-linked glibc test binary
├── context.md                  # Development context (agent memory)
├── findings.md                 # Research findings (agent knowledge base)
└── build/                      # CMake build output (gitignored)
```

### Rust Memory-Safe Allocator

The `rust/` directory contains a memory-safe allocator bridged to C++ via [cxx](https://cxx.rs/). It provides:

- **`rust_alloc`** — Memory-safe allocation with proper alignment
- **`rust_free`** — Memory-safe deallocation
- **`rust_realloc`** — Memory-safe reallocation
- **`rust_alloc_executable`** — Allocate executable memory for JIT blocks
- **`rust_protect_executable`** — Make memory executable after writing JIT code
- **`rust_free_executable`** — Free executable memory

This ensures all JIT-translated block memory is managed safely without buffer overflows or use-after-free vulnerabilities.

## License
