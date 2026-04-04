#pragma once
#include <cstddef>
#include <cstdint>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

::std::uint8_t *rust_alloc(::std::size_t size, ::std::size_t align) noexcept;

void rust_free(::std::uint8_t *ptr, ::std::size_t size, ::std::size_t align) noexcept;

::std::uint8_t *rust_realloc(::std::uint8_t *ptr, ::std::size_t old_size, ::std::size_t new_size, ::std::size_t align) noexcept;

::std::uint8_t *rust_alloc_executable(::std::size_t size) noexcept;

void rust_free_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept;

void rust_protect_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept;

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
