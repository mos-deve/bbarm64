# Context - bbarm64-emu v1.2 Development

## Current State (2026-04-04)

### What was completed
1. **Removed all duplications** across README.md, main.cpp, exec_engine.cpp, arm64_decoder.cpp, tls_emu.hpp, .gitignore
2. **Updated README** with FAE-Cache branding, new features
3. **FAE-Cache system** implemented with LRU eviction, hot block tracking, AOT persistence
4. **TLS support** added: TLSEmu::allocate_tls_block, AT_TLS auxv entry
5. **Android library stubs** created: libc.so.stub, libm.so.stub
6. **Fake ld.so** (dynlink.cpp) implemented with ELF loading, segment mapping, relocation processing
7. **NZCV→EFLAGS mapping** implemented with context-memory scratch (offsets 512+)
8. **Conditional branches in JIT** working via direct Jcc emission using current EFLAGS
9. **Branch offset sign-extension fix** in IR builder
10. **Decoder fixes**: SUBS, ADDS, SBFM/ASR/LSR/LSL, B.cond routing, BR/RET routing, CMN (ADDS with op1=0b101), SIMD/FP routing
11. **Syscall table**: Added close (57) mapping
12. **Shift instructions**: SAR, SHR, SHL with both immediate and CL variants
13. **SIMD decoder**: DUP, ADD, SUB, AND, OR, EOR, BIC vector instructions
14. **Stack setup**: Fixed auxv pointer, proper ARM64 stack layout

### Current test results
- `hello_simple` (static musl): ✅ JIT executes many blocks, conditional branches work, syscalls handled, but __libc_start_main returns -1 during musl init
- `hello_world_arm64` (dynamic): Crashes - needs dynamic linker support

### Active issues
- musl init fails: __libc_start_main returns -1, likely due to missing auxv entries or stack layout issues
- SIMD vector ops (VEC_ADD, VEC_SUB, etc.) emit NOP - need full AVX2 implementation
- Dynamic linking needs more work (symbol resolution stubs)
- Some instructions still missing (LDRB, LDRH, STRB, STRH, CSEL)

### Next steps
1. Debug musl init failure - check auxv entries and stack layout
2. Add more instruction coverage (LDRB, LDRH, STRB, STRH)
3. Implement full SIMD vector ops with AVX2
4. Test with dynamic hello world binary
