# bbarm64-emu

**v1.1** — A high-performance ARM64-to-x86_64 dynamic binary translator with JIT compilation.

## Overview

bbarm64-emu is an ARM64 emulator that runs ARM64 ELF binaries on x86_64 Linux hosts. It uses a **hybrid JIT + interpreter** approach: ARM64 basic blocks are translated to native x86_64 machine code at runtime, cached, and executed directly. When translation isn't possible (unknown instructions, syscalls, conditional branches), it gracefully falls back to an interpreter.

## Features

- **JIT Compilation Pipeline**: ARM64 → Decoder → IR Builder → IR Optimizer → x86_64 Emitter → Native Execution
- **Translation Cache**: Translated blocks are cached for reuse, avoiding re-translation overhead
- **Hybrid Execution**: JIT for hot paths, interpreter fallback for edge cases
- **Syscall Interception**: Guest ARM64 syscalls are forwarded to the host Linux kernel
- **ELF Loading**: Supports statically-linked ARM64 ELF binaries with proper segment mapping
- **Graceful Degradation**: Unknown instructions fall back to interpreter instead of crashing

## Quick Start

### Building

```bash
cd bbarm64
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

**Requirements:**
- CMake 3.14+
- clang++ or g++ with C++20 support
- x86_64 Linux host

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
| `BBARM64_CACHE_SIZE` | `64` | Translation cache size in megabytes |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      bbarm64-emu v1.1                        │
│                                                              │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐               │
│  │  ARM64   │───▶│   IR     │───▶│ x86_64   │               │
│  │ Decoder  │    │ Builder  │    │ Emitter  │               │
│  └──────────┘    └──────────┘    └──────────┘               │
│       │                │                │                    │
│       ▼                ▼                ▼                    │
│  ┌──────────────────────────────────────────────┐           │
│  │           Translation Cache                   │           │
│  │  (caches translated blocks by guest PC)       │           │
│  └──────────────────────────────────────────────┘           │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────────────────────────────────┐           │
│  │           CPU Context (ARM64)                 │           │
│  │  x[0..30], sp, pc, lr, nzcv, tpidr_el0       │           │
│  └──────────────────────────────────────────────┘           │
│                                                              │
│  ┌──────────────────────────────────────────────┐           │
│  │           Syscall Handlers                    │           │
│  │  ARM64 syscall numbers → host Linux syscalls  │           │
│  └──────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

### Execution Flow

1. **Fetch**: Read ARM64 instruction at `ctx.pc`
2. **Cache Lookup**: Check if block is already translated
3. **Translate** (if cache miss): Decode → Build IR → Optimize → Emit x86_64 → Cache
4. **Execute**: Call translated block as a native function
5. **Return**: Block updates `ctx.pc` and returns via `ret`
6. **Loop**: Run loop looks up next block by new `ctx.pc`
7. **Fallback**: SVC and unknown instructions handled by interpreter

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
| B.cond | Conditional branch | ❌* | ✅ |
| CBZ / CBNZ | Compare and branch on zero | ❌* | ✅ |
| TBZ / TBNZ | Test and branch | ❌* | ✅ |

*\* Conditional branches stop the JIT block; the interpreter handles the actual condition check.*

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
| `hello_simple` | Statically-linked C (musl) | ✅ Full JIT, prints output, exits 0 |
| `hello_glibc_static` | Statically-linked C (glibc) | ⚠️ Partial JIT — executes through libc init, crashes during complex setup |

## Known Limitations

- **Dynamic linking**: Dynamically-linked binaries (those requiring ld-linux-aarch64.so) are not yet supported
- **Conditional branches in JIT**: Conditional branches terminate the JIT block and fall back to the interpreter for the actual condition check
- **NZCV flags**: ARM64 condition flags are not fully mapped to x86_64 EFLAGS in JIT code
- **TLS**: Thread-local storage setup is not implemented, which blocks complex glibc programs
- **SIMD/FP**: NEON and floating-point instructions are not yet supported
- **Self-modifying code**: Basic SMC detection exists but may not catch all cases

## Roadmap

### v1.2 (Planned)
- **Improved glibc compatibility**: TLS setup, proper auxv, robust list initialization
- **Refined translation layer**: Better register allocation, reduced register spilling
- **Block chaining**: Direct jumps between translated blocks to eliminate run loop overhead
- **NZCV → EFLAGS mapping**: Full conditional branch support in JIT code
- **More instruction coverage**: SIMD/FP (NEON), atomic operations, CRC instructions
- **ARM 32-bit (AArch32)**: Planned but not currently the focus

## Project Structure

```
bbarm64/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── src/
│   ├── main.cpp                # Entry point, ELF loading, stack setup
│   ├── core/
│   │   ├── cpu_context.hpp     # ARM64 CPU state (x0-x30, sp, pc, lr, nzcv)
│   │   ├── exec_engine.cpp     # Main execution loop, JIT + interpreter
│   │   └── memory_manager.cpp  # Guest memory management (mmap, read/write)
│   ├── decoder/
│   │   └── arm64_decoder.cpp   # ARM64 instruction decoder
│   ├── ir/
│   │   ├── ir.hpp              # Intermediate representation definitions
│   │   ├── ir_builder.cpp      # DecodedInstr → IRBlock
│   │   └── ir_optimizer.cpp    # Constant propagation, DCE, flag merging
│   ├── backend/
│   │   ├── x86_64_emitter.cpp  # x86_64 machine code generator
│   │   ├── x86_64_regalloc.cpp # Register allocation (ARM64 → x86_64)
│   │   └── x86_64_lower.cpp    # IR → x86_64 lowering
│   ├── cache/
│   │   └── translation_cache.cpp # Block cache with LRU eviction
│   ├── elf/
│   │   └── elf_loader.cpp      # ELF binary loader
│   ├── syscall/
│   │   ├── syscall_handlers.cpp # Individual syscall implementations
│   │   └── syscall_table.cpp    # ARM64 → x86_64 syscall number mapping
│   └── util/
│       ├── log.hpp             # Logging utilities
│       ├── config.hpp          # Configuration from environment
│       └── profiler.hpp        # Execution profiling
└── build/                      # Build output
```

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
