# bbarm64-emu — Block-Based ARM64 Emulator for x86_64 Linux

## Project Vision

A high-performance, user-mode dynamic binary translator that runs ARM64 (AArch64) Linux
binaries on x86_64 Linux hosts. The core design philosophy is **block-based translation with
an aggressive caching system** — translate each basic block of ARM64 code into native x86_64
machine code once, cache it, and chain blocks together for near-native execution speed.

**Direction:** ARM64 guest → x86_64 host (the reverse of Box64/FEX-Emu)

---

## 1. Competitive Landscape & Design Inspiration

### Existing Projects (x86→ARM direction)
| Project | Approach | Language | Key Strength |
|---------|----------|----------|--------------|
| **Box64** | Multi-pass dynarec, no IR | C | Simple, fast compilation, good gaming perf |
| **FEX-Emu** | IR-based (frontend→IR→backend) | C++ | Optimizations on IR, broad x86 feature support |
| **QEMU TCG** | IR-based (TCG ops) | C | Portable, mature, supports all architectures |
| **Rosetta 2** | AOT + JIT hybrid, persistent cache | Proprietary | Near-native performance, install-time translation |

### What We Learn From Each
- **From Box64:** Multi-pass approach for register allocation and flag propagation. The
  dynablock concept — count instructions, allocate memory, generate code.
- **From FEX-Emu:** IR as an optimization layer. The IR allows dead-code elimination,
  constant folding, and instruction combining before backend emission.
- **From QEMU TCG:** Translation Block (TB) abstraction, direct block chaining,
  self-modifying code detection via page write-protection.
- **From Rosetta 2:** Persistent on-disk translation cache for instant startup on
  subsequent runs. Translate each instruction only once (instruction cache benefit).

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        bbarm64-emu                               │
│                                                                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌───────────┐  │
│  │ ARM64    │───▶│   IR     │───▶│  x86_64  │───▶│  Exec     │  │
│  │ Decoder  │    │ Builder  │    │ Backend  │    │  Engine   │  │
│  └──────────┘    └──────────┘    └──────────┘    └───────────┘  │
│       │               │               │               │          │
│       ▼               ▼               ▼               ▼          │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Translation Cache Layer                      │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────────────┐  │   │
│  │  │ L1: In-Mem │  │ L2: Disk   │  │ L3: Block Chaining │  │   │
│  │  │ Hash Table │  │ Persistent │  │ Direct Jumps       │  │   │
│  │  └────────────┘  └────────────┘  └────────────────────┘  │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Linux Syscall Wrapper Layer                   │   │
│  │  ARM64 syscall numbers → x86_64 syscall numbers           │   │
│  │  Signal handling, TLS, mmap, futex, epoll, etc.           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              ELF Loader & Dynamic Linker                   │   │
│  │  Parse ARM64 ELF, load .so files, resolve relocations     │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Core Subsystems

### 3.1 ARM64 Decoder (Frontend)

**Responsibility:** Decode ARM64 binary instructions into an internal representation.

**Key Design Decisions:**
- Decode one instruction at a time, but group into **basic blocks** (sequences ending
  at branches, calls, returns, or page boundaries)
- Maximum block size: configurable, default 64 instructions (balances compilation
  overhead vs. optimization opportunity)
- Handle both A64 (base) and SIMD/FP (Advanced SIMD / NEON) instruction encodings

**Instruction Categories to Support (phased):**
1. **Phase 1 — Core Integer:** ADD, SUB, AND, ORR, EOR, LSL, LSR, ASR, ROR, MOV,
   CMP, TST, NOP, BR, BL, RET, B, B.cond, CBZ, CBNZ, TBZ, TBNZ
2. **Phase 2 — Load/Store:** LDR, STR, LDRB, STRB, LDRH, STRH, LDRSW, LDUR, STUR,
   LDP, STP, LDAXR, STLR, memory barriers (DMB, DSB, ISB)
3. **Phase 3 — SIMD/NEON:** FADD, FSUB, FMUL, FDIV, FCMP, FMOV, vector ops,
   conversions, saturating ops
4. **Phase 4 — System:** MRS, MSR, CLREX, HINT, PAC (pointer auth — may need to NOP)
5. **Phase 5 — Crypto:** AES, SHA, PMULL (hardware crypto → x86 AES-NI/SHA mapping)

### 3.2 Intermediate Representation (IR)

**Why an IR instead of direct emission (Box64 style)?**
- Enables cross-instruction optimization (dead code elimination, constant propagation)
- Makes the backend swappable (could target other hosts later)
- Allows optimization passes that significantly reduce emitted instruction count
- Critical for handling ARM64→x86_64 semantic gaps (flags, condition codes)

**IR Design — SSA-based, inspired by FEX-Emu but simplified:**

```c
// Each IR instruction:
struct IRInstruction {
    IROpcode    op;          // ADD, SUB, LOAD, STORE, BRANCH, CALL, etc.
    uint8_t     dest;        // Destination virtual register
    uint8_t     src1;        // Source virtual register 1
    uint8_t     src2;        // Source virtual register 2 or immediate index
    IRFlags     flags;       // Sets NZCV? Uses NZCV? Memory barrier?
    uint32_t    imm;         // Inline immediate for small constants
};
```

**IR Opcodes (core set):**
- `IR_ADD`, `IR_SUB`, `IR_AND`, `IR_OR`, `IR_XOR`, `IR_SHL`, `IR_SHR`, `IR_SAR`
- `IR_MUL`, `IR_DIV`, `IR_UDIV`, `IR_NEG`, `IR_NOT`
- `IR_LOAD_8/16/32/64`, `IR_STORE_8/16/32/64`
- `IR_LOAD_PAIR`, `IR_STORE_PAIR` (for LDP/STP)
- `IR_CMP`, `IR_TST` (sets condition flags)
- `IR_CMOV` (conditional move — maps to ARM64 CSEL, x86 CMOVcc)
- `IR_BRANCH_COND` (conditional branch)
- `IR_CALL`, `IR_RET`, `IR_JMP`
- `IR_SEXT_8/16/32`, `IR_ZEXT_8/16/32`
- `IR_BSWAP`, `IR_REV` (byte reversal)
- `IR_FADD`, `IR_FSUB`, `IR_FMUL`, `IR_FDIV`, `IR_FCMP`
- `IR_VECTOR_ADD`, `IR_VECTOR_MUL`, etc. (SIMD)
- `IR_BARRIER` (memory ordering)
- `IR_SYSCALL`, `IR_GET_CONTEXT_REG`, `IR_SET_CONTEXT_REG`

**IR Optimization Passes:**
1. **Constant folding:** Evaluate constant expressions at translation time
2. **Dead code elimination:** Remove instructions whose results are never used
3. **Common subexpression elimination:** Deduplicate repeated computations
4. **Flag optimization:** Merge redundant flag computations (critical for ARM64→x86)
5. **Instruction combining:** e.g., `ADD x0, x1, #1` followed by `CMP x0, #0` →
   combine into `INC + TEST`
6. **Register coalescing:** Reduce virtual register count to fit x86_64 physical regs

### 3.3 x86_64 Backend (Code Generator)

**Responsibility:** Lower IR to native x86_64 machine code.

**Register Mapping Strategy:**

ARM64 has 31 GPRs (X0-X30) + SP. x86_64 has 16 GPRs (RAX-R15).

```
ARM64 Reg  →  x86_64 Reg   |  Purpose
───────────────────────────┼─────────────────────────────
X0-X7      →  RDI, RSI, RDX, RCX, R8, R9, R10, R11
             (also syscall args)
X8         →  R12          |  Indirect result / syscall number
X9-X15     →  R13-R15, [spill]
X16-X17    →  [spill]      |  IP0, IP1 (intra-procedure temps)
X18        →  [reserved]   |  Platform register (usually unused on Linux)
X19-X28    →  [spill]      |  Callee-saved
X29 (FP)   →  RBP          |  Frame pointer
X30 (LR)   →  [spill]      |  Link register
SP         →  RSP          |  Stack pointer
PC         →  [emulated]   |  Program counter (in CPU context)
NZCV       →  [emulated]   |  Condition flags (in CPU context)
```

**Register Allocation Strategy:**
- Use a **hybrid approach**: direct mapping for hot registers + spill slots for cold ones
- Keep the most frequently used ARM64 registers in x86_64 physical registers
- Use x86_64's R8-R15 extensively (they're the extra registers ARM64 doesn't have
  natively but we can use as spill slots)
- For blocks with heavy register pressure, use the stack with `PUSH`/`POP` pairs

**Condition Flags (NZCV → EFLAGS):**

This is the **hardest semantic gap**. ARM64 NZCV (Negative, Zero, Carry, oVerflow)
doesn't map 1:1 to x86_64 EFLAGS.

```
ARM64 NZCV:          x86_64 EFLAGS:
  N = result[63]       SF = result[63]     ← Direct match
  Z = result == 0      ZF = result == 0    ← Direct match
  C = carry out        CF = carry/borrow   ← Direct match (for ADD/SUB)
  V = signed overflow  OF = signed overflow ← Direct match (for ADD/SUB)
```

**Good news:** For ADD/SUB/CMP, the flags map directly! x86's ADD/SUB/CMP set
SF/ZF/CF/OF in the same way ARM64 sets N/Z/C/V.

**The challenge:** ARM64 has many instructions that DON'T set flags, while x86
instructions often DO set flags. We must:
1. Use `LAHF`/`SAHF` to save/restore flags around non-flag-setting x86 instructions
2. Or better: track flag liveness in the IR and only materialize flags when needed
3. Use `CMOVcc` for conditional operations instead of branching when possible
4. For shift instructions: ARM64 sets C from the last bit shifted out; x86 sets CF
   similarly for single-bit shifts but differently for multi-bit → needs emulation

**Flag Materialization Strategy:**
- Track which flags are "live" after each IR instruction
- Only emit flag-setting variants of x86 instructions when flags are needed downstream
- Use `SETcc` + `MOVZX` to materialize individual flags into GPRs when needed
- For complex flag computations (e.g., after shifts), emit inline helper sequences

### 3.4 Translation Cache System (The Key Differentiator)

This is where bbarm64-emu aims to excel. Three-tier caching:

#### L1: In-Memory Hash Table (Hot Cache)
```c
struct TranslationCache {
    // Hash table: ARM64 PC → translated block info
    struct BlockEntry {
        uint64_t    arm_pc;           // Original ARM64 address
        uint64_t    host_ptr;         // Pointer to x86_64 code
        uint32_t    arm_block_size;   // Number of ARM64 instructions
        uint32_t    host_code_size;   // Size of generated x86_64 code
        uint64_t    cpu_state_mask;   // Which CPU state bits this block depends on
        uint32_t    hit_count;        // For profiling / optimization decisions
        uint32_t    chain_targets[2]; // Direct chain targets (slot 0, 1)
    };

    // Hash table with open addressing
    BlockEntry*   entries;
    uint32_t      capacity;
    uint32_t      count;

    // Executable memory pool (mmap with PROT_READ|PROT_WRITE|PROT_EXEC)
    uint8_t*      code_pool;
    size_t        code_pool_size;
    size_t        code_pool_used;
};
```

**Hash function:** `hash = (pc ^ (pc >> 12) ^ (pc >> 24)) & (capacity - 1)`
**Capacity:** Power of 2, starts at 64K entries, grows dynamically
**Eviction:** LRU based on hit_count + age; least-used blocks are freed first

#### L2: Persistent Disk Cache (Warm Cache)
- On first run: translate blocks, store in L1
- On shutdown (or periodically): serialize L1 cache to disk
- On startup: load disk cache into L1, skip translation for cached blocks
- Cache file format:
  ```
  [Header: magic "BBARM64C", version, build_hash, entry_count]
  [Entry: arm_pc, block_size, host_code_size, host_code_bytes...]
  [Entry: ...]
  [Index: sorted by arm_pc for fast lookup]
  ```
- Cache invalidation: hash the ARM64 binary (ELF + .so files), invalidate if changed
- Cache location: `~/.cache/bbarm64-emu/<binary_hash>.cache`

**Benefits:**
- Near-instant startup on second run (no translation overhead)
- Rosetta 2's key advantage — we replicate it for Linux
- Particularly valuable for large applications (games, databases)

#### L3: Direct Block Chaining (Zero-Overhead Transitions)
- After translating a block, patch its exit to jump directly to the next block
- Avoids returning to the main dispatch loop for sequential execution
- Uses x86_64 relative `JMP` or `CALL` for direct chaining
- Two chain slots per block (for conditional branches: taken/not-taken)
- If target block not yet translated, chain points to a "lookup and translate" stub

```
Block A (ARM64 PC 0x1000):
  ... translated x86_64 code ...
  CMP ...           ; was ARM64 B.cond
  JNE  chain_slot_0 ; if not taken, go to next sequential block
  JMP  chain_slot_1 ; if taken, go to branch target

chain_slot_0: JMP 0x00000000  ; patched later to point to Block B
chain_slot_1: JMP 0x00000000  ; patched later to point to Block C
```

### 3.5 Execution Engine

**Entry Point:**
```c
// Called when entering emulated code
void DynaRun(CPUContext* ctx, uint64_t arm_pc) {
    BlockEntry* block = CacheLookup(arm_pc);
    if (!block) {
        block = TranslateBlock(arm_pc);
        CacheInsert(arm_pc, block);
    }
    // Jump to translated code via function pointer
    typedef void (*HostCodeFunc)(CPUContext*);
    ((HostCodeFunc)block->host_ptr)(ctx);
}
```

**Block Prologue/Epilogue:**
- Prologue: Save x86_64 callee-saved registers, load ARM64 context into x86_64 regs
- Epilogue: Save ARM64 context back, restore x86_64 callee-saved registers, return

**Signal Handling:**
- Install SIGSEGV handler to catch invalid memory accesses
- Map host PC back to ARM64 PC for accurate exception reporting
- Handle self-modifying code detection (see §3.6)

### 3.6 Self-Modifying Code Detection

ARM64 Linux programs rarely self-modify, but we must handle it correctly:

1. **Page write-protection:** When generating code for a page, `mprotect` it as read-only
2. **SIGSEGV on write:** If the guest writes to a translated page, catch the signal
3. **Invalidate:** Remove all blocks from that page from the cache
4. **Re-protect:** Make the page writable again, let the write proceed
5. **Re-translate:** Next execution will re-translate the modified code

### 3.7 Linux Syscall Wrapper Layer

ARM64 and x86_64 Linux have **different syscall numbers** and sometimes different
calling conventions:

| Aspect | ARM64 | x86_64 |
|--------|-------|--------|
| Syscall instruction | `SVC #0` | `syscall` |
| Syscall number reg | X8 | RAX |
| Arg 1-6 | X0-X5 | RDI, RSI, RDX, R10, R8, R9 |
| Return | X0 | RAX |
| Error indication | X0 = -errno | RAX = -errno (via CF) |

**Syscall Translation Table:**
```c
struct SyscallEntry {
    int arm_nr;       // ARM64 syscall number
    int x86_nr;       // x86_64 syscall number
    const char* name; // For debugging
    SyscallHandler handler; // Custom handler if needed (not just pass-through)
};
```

**Syscalls needing custom handlers:**
- `mmap`, `mprotect`, `munmap` — need to handle ARM64 page size expectations
- `futex` — critical for threading, must be fast
- `clone`, `clone3` — thread creation, register mapping
- `rt_sigaction`, `rt_sigreturn` — signal handling
- `tls` operations — `set_tid_address`, `set_robust_list`
- `epoll`, `io_uring` — I/O multiplexing
- `prctl` — various process control operations

### 3.8 ELF Loader & Dynamic Linker

**Responsibilities:**
1. Parse ARM64 ELF headers, validate architecture (EM_AARCH64)
2. Load segments into memory with correct permissions
3. Process dynamic relocations (R_AARCH64_ABS64, R_AARCH64_RELATIVE, etc.)
4. Load dependent `.so` files (also ARM64)
5. Resolve symbols from host x86_64 libraries where possible
6. Set up the initial stack (argc, argv, envp, auxv)
7. Transfer control to the entry point

**Key Challenge:** ARM64 `.so` files can't be loaded directly by the x86_64 dynamic
linker. We must:
- Implement our own ELF loader for ARM64 shared libraries
- Wrap host x86_64 library calls (like Box64 does with its wrapper system)
- Handle TLS (Thread-Local Storage) differences between architectures

---

## 4. Implementation Phases

### Phase 0: Foundation (Weeks 1-2)
- [ ] Project scaffolding: build system (CMake), directory structure
- [ ] CPU context structure (ARM64 registers, x86_64 state)
- [ ] Memory management: virtual memory map, page table emulation
- [ ] Basic ARM64 instruction decoder (core integer instructions only)
- [ ] x86_64 code emitter (raw byte emission with relocation support)
- [ ] Simple test harness: decode an ARM64 instruction, verify

### Phase 1: Core Dynarec (Weeks 3-6)
- [ ] IR data structures and builder
- [ ] IR → x86_64 backend for integer instructions
- [ ] Translation cache (L1 in-memory hash table)
- [ ] Block prologue/epilogue generation
- [ ] Direct block chaining (L3)
- [ ] Basic execution engine
- [ ] Test: Run a simple ARM64 "hello world" (no syscalls, just compute)

### Phase 2: Syscall & ELF Support (Weeks 7-10)
- [ ] ELF loader for ARM64 binaries
- [ ] Syscall translation layer (core syscalls: read, write, exit, mmap, brk)
- [ ] Dynamic linker for ARM64 `.so` files
- [ ] Signal handling infrastructure
- [ ] Test: Run ARM64 binaries that use libc

### Phase 3: Advanced Instructions (Weeks 11-14)
- [ ] Load/store exclusive (LDXR/STXR) → x86_64 LOCK CMPXCHG
- [ ] Memory barriers (DMB, DSB, ISB) → x86_64 MFENCE/LFENCE/SFENCE
- [ ] SIMD/NEON support (basic vector ops)
- [ ] Floating point support
- [ ] Test: Run ARM64 programs using math libraries

### Phase 4: Performance Optimization (Weeks 15-18)
- [ ] Persistent disk cache (L2)
- [ ] IR optimization passes (constant folding, DCE, CSE)
- [ ] Register allocation improvements
- [ ] Flag liveness analysis and optimization
- [ ] Profiling infrastructure
- [ ] Benchmark against QEMU-user and Box64 (reverse direction)

### Phase 5: Completeness & Compatibility (Weeks 19-24)
- [ ] Full SIMD/NEON coverage
- [ ] Crypto instructions (AES, SHA)
- [ ] Pointer authentication (PAC) handling
- [ ] Thread support (clone, futex, TLS)
- [ ] Test suite: run ARM64 Linux test binaries
- [ ] Compatibility testing with real ARM64 applications

---

## 5. Key Technical Challenges & Solutions

### Challenge 1: Register Pressure
**Problem:** ARM64 has 31 GPRs, x86_64 has 16. Spilling to memory is expensive.
**Solution:**
- Aggressive register allocation in the IR phase
- Keep only live registers in physical registers
- Use x86_64's full register set (R8-R15) as spill slots
- Profile-guided: track which ARM64 regs are most used per block, prioritize those

### Challenge 2: Condition Flags
**Problem:** ARM64 NZCV doesn't always map cleanly to x86 EFLAGS.
**Solution:**
- For ADD/SUB/CMP/TST: flags map directly, use x86's native flag-setting
- For other instructions: track flag liveness, materialize only when needed
- Use `SETcc` instructions to extract individual flags
- For complex cases (shifts, logical ops): emit inline flag computation sequences

### Challenge 3: Memory Ordering
**Problem:** ARM64 has weak memory ordering; x86_64 has strong (TSO) ordering.
**Solution:**
- x86_64's TSO is stricter than ARM64's model, so most ARM64 code works correctly
- DMB/DSB barriers can often be NOPs on x86_64 (x86 already provides the ordering)
- ISB (instruction sync barrier) → flush any pending translation state
- For acquire/release semantics: use x86 LOCK prefix where needed

### Challenge 4: SIMD Width Mismatch
**Problem:** ARM64 NEON has 32 × 128-bit registers; x86_64 SSE has 16 × 128-bit,
AVX has 16 × 256-bit, AVX-512 has 32 × 512-bit.
**Solution:**
- Target SSE2 as the baseline (available on all x86_64 CPUs)
- Use AVX/AVX2 if detected at runtime (wider registers = fewer spills)
- Map ARM64 V0-V31 to x86_64 XMM0-XMM15 + spill slots
- For AVX-512 hosts: use the extra registers (ZMM16-ZMM31) for ARM64 V16-V31

### Challenge 5: Startup Performance
**Problem:** First run requires translating every block, causing slow startup.
**Solution:**
- Persistent disk cache (L2) — translate once, load forever
- Lazy translation: only translate blocks that are actually executed
- Profile-guided: prioritize translating hot paths first
- Pre-translate known entry points (main, init functions)

### Challenge 6: Self-Modifying Code
**Problem:** Translated code becomes stale if the guest modifies its own code.
**Solution:**
- Page write-protection via `mprotect`
- SIGSEGV handler to detect writes to translated pages
- Invalidate affected blocks, re-translate on next execution
- This is well-understood (QEMU does it); the overhead is minimal for non-SMC code

---

## 6. Project Structure

```
bbarm64-emu/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.c                  # Entry point, argument parsing
│   ├── cpu/
│   │   ├── cpu_context.h       # ARM64 register state definition
│   │   ├── cpu_context.c       # Context save/restore
│   │   ├── decoder.c           # ARM64 instruction decoder
│   │   ├── decoder.h           # Decoded instruction structure
│   │   └── flags.c             # NZCV flag emulation helpers
│   ├── ir/
│   │   ├── ir.h                # IR instruction definitions
│   │   ├── ir_builder.c        # Build IR from decoded instructions
│   │   ├── ir_optimize.c       # IR optimization passes
│   │   └── ir_validate.c       # IR validation (debug builds)
│   ├── backend/
│   │   ├── x86_64_emit.c       # x86_64 code emitter
│   │   ├── x86_64_emit.h       # Emitter API
│   │   ├── x86_64_regalloc.c   # Register allocation
│   │   ├── x86_64_lower.c      # IR → x86_64 lowering
│   │   └── x86_64_prolog.c     # Block prologue/epilogue
│   ├── cache/
│   │   ├── translation_cache.h # Cache data structures
│   │   ├── translation_cache.c # L1 in-memory cache
│   │   ├── disk_cache.c        # L2 persistent disk cache
│   │   └── block_chain.c       # L3 direct block chaining
│   ├── exec/
│   │   ├── exec_engine.c       # Main execution loop
│   │   ├── signals.c           # Signal handling
│   │   └── smc_detect.c        # Self-modifying code detection
│   ├── syscall/
│   │   ├── syscall_table.h     # ARM64 → x86_64 syscall mapping
│   │   ├── syscall_table.c     # Syscall number translation
│   │   ├── syscall_handlers.c  # Custom syscall implementations
│   │   └── futex.c             # Futex (critical for threading)
│   ├── elf/
│   │   ├── elf_loader.c        # ARM64 ELF parsing and loading
│   │   ├── elf_loader.h
│   │   ├── dynlink.c           # Dynamic linker for .so files
│   │   └── reloc.c             # Relocation processing
│   └── util/
│       ├── memory.c            # Virtual memory management
│       ├── memory.h
│       ├── log.c               # Logging infrastructure
│       ├── log.h
│       ├── profiler.c          # Performance profiling
│       └── config.c            # Configuration / environment variables
├── tests/
│   ├── test_decoder.c          # Decoder unit tests
│   ├── test_ir.c               # IR builder/optimizer tests
│   ├── test_backend.c          # Backend emission tests
│   ├── test_cache.c            # Translation cache tests
│   └── arm64_bins/             # Pre-compiled ARM64 test binaries
│       ├── hello.S
│       ├── add_test.S
│       └── branch_test.S
└── tools/
    ├── gen_syscall_table.py    # Generate syscall mapping from kernel headers
    └── disasm_arm64.c          # Standalone ARM64 disassembler (debug tool)
```

---

## 7. Technology Choices

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | **C11** | Performance, portability, matches Box64/QEMU style, no runtime overhead |
| Build System | **CMake** | Industry standard, good IDE support, cross-platform |
| ARM64 Decoding | **Custom** | Full control, no external dependencies, can optimize for our use case |
| x86_64 Emission | **Custom** | Direct control over instruction encoding, no assembler dependency |
| Testing | **Custom + ARM64 GCC cross-compiler** | Compile ARM64 test binaries, run through emulator, verify output |
| Profiling | **Custom counters + perf integration** | Lightweight, no external dependencies, generates perf-compatible maps |

**Deliberately NOT using:**
- LLVM: Too heavy, slow startup, overkill for our use case
- Capstone/Unicorn: External dependencies, slower than custom decoder
- QEMU codebase: Too complex, TCG IR is not ideal for our optimization goals

---

## 8. Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Translation speed | >1M ARM64 instr/sec | Fast enough that translation isn't the bottleneck |
| Runtime speed | 30-60% of native | Comparable to Box64/FEX-Emu in their respective directions |
| Cache hit rate | >95% (after warmup) | Most blocks should be found in cache |
| Startup time (cold) | <500ms for simple binaries | Fast enough for CLI tools |
| Startup time (warm) | <50ms for cached binaries | Near-instant with disk cache |
| Memory overhead | <100MB for typical apps | Reasonable for desktop use |

---

## 9. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Flag emulation complexity | High | High | Start with simple approach, optimize later; use IR flag tracking |
| Register pressure on x86_64 | Medium | Medium | Aggressive register allocation; use all 16 x86_64 GPRs |
| SIMD compatibility gaps | Medium | High | Start with SSE2 baseline, add AVX later; fall back to scalar for edge cases |
| Syscall compatibility | High | High | Start with core syscalls, expand based on real binary needs |
| Self-modifying code edge cases | Low | Medium | QEMU's approach is well-tested; follow their model |
| Thread safety | Medium | High | Start single-threaded, add threading support incrementally |

---

## 10. Next Steps (If Proceeding)

1. **Set up the project repository** with the directory structure above
2. **Implement the ARM64 decoder** for core integer instructions (Phase 0)
3. **Build the x86_64 code emitter** with a simple test (emit `MOV RAX, 42; RET`)
4. **Create the CPU context structure** and basic save/restore
5. **Write the first integration test**: decode `ADD X0, X1, X2`, emit x86_64 equivalent,
   verify the result matches expected behavior

---

## Appendix A: ARM64 → x86_64 Instruction Mapping Reference

### Integer Arithmetic
| ARM64 | x86_64 | Notes |
|-------|--------|-------|
| `ADD Xd, Xn, Xm` | `ADD Rd, Rn, Rm` | Flags map directly if `S` suffix |
| `ADD Xd, Xn, #imm` | `ADD Rd, Rn, imm` | Limited to sign-extended 12-bit imm |
| `SUB Xd, Xn, Xm` | `SUB Rd, Rn, Rm` | Flags map directly if `S` suffix |
| `MUL Xd, Xn, Xm` | `IMUL Rd, Rn, Rm` | 64-bit multiply |
| `UDIV Xd, Xn, Xm` | `DIV Rm` | Requires RAX/RDX setup |
| `SDIV Xd, Xn, Xm` | `IDIV Rm` | Requires RAX/RDX setup |

### Logical & Shift
| ARM64 | x86_64 | Notes |
|-------|--------|-------|
| `AND Xd, Xn, Xm` | `AND Rd, Rm` | |
| `ORR Xd, Xn, Xm` | `OR Rd, Rm` | |
| `EOR Xd, Xn, Xm` | `XOR Rd, Rm` | |
| `LSL Xd, Xn, #imm` | `SHL Rd, imm` | CF maps for single-bit |
| `LSR Xd, Xn, #imm` | `SHR Rd, imm` | CF maps for single-bit |
| `ASR Xd, Xn, #imm` | `SAR Rd, imm` | CF maps for single-bit |
| `ROR Xd, Xn, #imm` | `ROR Rd, imm` | |

### Branch
| ARM64 | x86_64 | Notes |
|-------|--------|-------|
| `B label` | `JMP rel32` | Direct branch |
| `B.cond label` | `Jcc rel32` | Conditional, NZCV must be current |
| `BL label` | `CALL rel32` | Save LR (X30) |
| `BR Xn` | `JMP Rn` | Indirect branch |
| `RET` | `RET` | Return to LR |
| `CBZ Xn, label` | `TEST Rn, Rn; JZ` | Compare and branch on zero |
| `CBNZ Xn, label` | `TEST Rn, Rn; JNZ` | Compare and branch on non-zero |

### Load/Store
| ARM64 | x86_64 | Notes |
|-------|--------|-------|
| `LDR Xd, [Xn, #imm]` | `MOV Rd, [Rn + imm]` | |
| `STR Xd, [Xn, #imm]` | `MOV [Rn + imm], Rd` | |
| `LDRB Wd, [Xn, #imm]` | `MOVZX Rd, BYTE [Rn + imm]` | Zero-extend |
| `LDRSB Xd, [Xn, #imm]` | `MOVSX Rd, BYTE [Rn + imm]` | Sign-extend |
| `LDP Xd, Xe, [Xn, #imm]` | `MOV Rd, [Rn+imm]; MOV Re, [Rn+imm+8]` | Load pair |
| `STP Xd, Xe, [Xn, #imm]` | `MOV [Rn+imm], Rd; MOV [Rn+imm+8], Re` | Store pair |

---

## Appendix B: ARM64 Syscall Number Differences (Selected)

| Syscall | ARM64 NR | x86_64 NR | Notes |
|---------|----------|-----------|-------|
| read | 63 | 0 | |
| write | 64 | 1 | |
| open | 1024 (openat) | 2 (open) | ARM64 uses openat only |
| close | 57 | 3 | |
| mmap | 222 | 9 | |
| brk | 214 | 12 | |
| exit | 93 | 60 | |
| clone | 220 | 56 | Arg order differs! |
| futex | 98 | 202 | |

---

*Document created: 2026-04-03*
*Status: Planning phase — not yet implemented*
