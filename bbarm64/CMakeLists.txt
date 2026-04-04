cmake_minimum_required(VERSION 3.20)
project(bbarm64 VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

add_compile_options(-mavx2 -mfma -mbmi2)

set(SOURCES
    src/main.cpp
    src/core/cpu_context.cpp
    src/core/memory_manager.cpp
    src/core/exec_engine.cpp
    src/decoder/arm64_decoder.cpp
    src/ir/ir_builder.cpp
    src/ir/ir_optimizer.cpp
    src/backend/x86_64_emitter.cpp
    src/backend/x86_64_regalloc.cpp
    src/backend/x86_64_lower.cpp
    src/cache/translation_cache.cpp
    src/cache/block_chaining.cpp
    src/syscall/syscall_table.cpp
    src/syscall/syscall_handlers.cpp
    src/elf/elf_loader.cpp
    src/elf/dynlink.cpp
    src/threading/thread_manager.cpp
    src/threading/tls_emu.cpp
    src/util/log.cpp
    src/util/config.cpp
    src/util/profiler.cpp
)

add_executable(bbarm64 ${SOURCES})
target_include_directories(bbarm64 PRIVATE src/)

find_package(Threads REQUIRED)
target_link_libraries(bbarm64 PRIVATE Threads::Threads dl)

install(TARGETS bbarm64 DESTINATION bin)
