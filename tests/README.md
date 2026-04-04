# bbarm64 Test Binaries

Pre-built ARM64 test binaries for verifying bbarm64-emu functionality.

## Test Binaries

| File | Type | Description | Status |
|---|---|---|---|
| `hello_asm` | Raw ARM64 assembly | Minimal hello world using syscalls | ✅ Works |
| `hello_static` | Statically-linked C | C program compiled with `-static` | ⚠️ Partial (glibc init issue) |
| `hello_dynamic` | Dynamically-linked C | C program linked against host libc | ❌ Not supported yet |

## Rebuilding

```bash
# Assembly binary
aarch64-linux-gnu-as -o hello_asm.o hello_world.s
aarch64-linux-gnu-ld -e _start -o hello_asm hello_asm.o

# Static C binary
aarch64-linux-gnu-gcc -static -o hello_static ../hello_world.c

# Dynamic C binary
aarch64-linux-gnu-gcc -o hello_dynamic ../hello_world.c
```

## Running Tests

```bash
# From the repo root
./build/bbarm64 tests/hello_asm
./build/bbarm64 tests/hello_static
```
