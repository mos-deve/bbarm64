#include <cstddef>
#include <cstdint>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

extern "C" {
::std::uint8_t *cxxbridge1$194$rust_alloc(::std::size_t size, ::std::size_t align) noexcept;

void cxxbridge1$194$rust_free(::std::uint8_t *ptr, ::std::size_t size, ::std::size_t align) noexcept;

::std::uint8_t *cxxbridge1$194$rust_realloc(::std::uint8_t *ptr, ::std::size_t old_size, ::std::size_t new_size, ::std::size_t align) noexcept;

::std::uint8_t *cxxbridge1$194$rust_alloc_executable(::std::size_t size) noexcept;

void cxxbridge1$194$rust_free_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept;

void cxxbridge1$194$rust_protect_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept;
} // extern "C"

::std::uint8_t *rust_alloc(::std::size_t size, ::std::size_t align) noexcept {
  return cxxbridge1$194$rust_alloc(size, align);
}

void rust_free(::std::uint8_t *ptr, ::std::size_t size, ::std::size_t align) noexcept {
  cxxbridge1$194$rust_free(ptr, size, align);
}

::std::uint8_t *rust_realloc(::std::uint8_t *ptr, ::std::size_t old_size, ::std::size_t new_size, ::std::size_t align) noexcept {
  return cxxbridge1$194$rust_realloc(ptr, old_size, new_size, align);
}

::std::uint8_t *rust_alloc_executable(::std::size_t size) noexcept {
  return cxxbridge1$194$rust_alloc_executable(size);
}

void rust_free_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept {
  cxxbridge1$194$rust_free_executable(ptr, size);
}

void rust_protect_executable(::std::uint8_t *ptr, ::std::size_t size) noexcept {
  cxxbridge1$194$rust_protect_executable(ptr, size);
}
