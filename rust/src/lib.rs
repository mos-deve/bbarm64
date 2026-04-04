// Rust memory-safe allocator and memory management
// Bridged to C++ via cxx

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn rust_alloc(size: usize, align: usize) -> *mut u8;
        unsafe fn rust_free(ptr: *mut u8, size: usize, align: usize);
        unsafe fn rust_realloc(
            ptr: *mut u8,
            old_size: usize,
            new_size: usize,
            align: usize,
        ) -> *mut u8;
        fn rust_alloc_executable(size: usize) -> *mut u8;
        unsafe fn rust_free_executable(ptr: *mut u8, size: usize);
        unsafe fn rust_protect_executable(ptr: *mut u8, size: usize);
    }
}

use std::alloc::{self, Layout};
use std::ffi::c_void;

/// Memory-safe allocation with proper alignment
pub fn rust_alloc(size: usize, align: usize) -> *mut u8 {
    if size == 0 {
        return std::ptr::null_mut();
    }
    let layout = Layout::from_size_align(size, align).unwrap_or(Layout::new::<u8>());
    let ptr = unsafe { alloc::alloc(layout) };
    if ptr.is_null() {
        std::process::abort();
    }
    ptr
}

/// Memory-safe deallocation
pub unsafe fn rust_free(ptr: *mut u8, size: usize, align: usize) {
    if ptr.is_null() || size == 0 {
        return;
    }
    let layout = Layout::from_size_align(size, align).unwrap_or(Layout::new::<u8>());
    alloc::dealloc(ptr, layout);
}

/// Memory-safe reallocation
pub unsafe fn rust_realloc(
    ptr: *mut u8,
    old_size: usize,
    new_size: usize,
    align: usize,
) -> *mut u8 {
    if ptr.is_null() {
        return rust_alloc(new_size, align);
    }
    if new_size == 0 {
        rust_free(ptr, old_size, align);
        return std::ptr::null_mut();
    }
    let old_layout = Layout::from_size_align(old_size, align).unwrap_or(Layout::new::<u8>());
    let new_layout = Layout::from_size_align(new_size, align).unwrap_or(Layout::new::<u8>());
    let new_ptr = alloc::realloc(ptr, old_layout, new_size);
    if new_ptr.is_null() {
        std::process::abort();
    }
    new_ptr
}

/// Allocate executable memory (for JIT translated blocks)
pub fn rust_alloc_executable(size: usize) -> *mut u8 {
    use libc::{
        mmap, mprotect, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_EXEC, PROT_READ, PROT_WRITE,
    };

    let ptr = unsafe {
        mmap(
            std::ptr::null_mut(),
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0,
        )
    };

    if ptr == MAP_FAILED {
        return std::ptr::null_mut();
    }

    // Zero the memory
    unsafe { std::ptr::write_bytes(ptr as *mut u8, 0, size) };

    ptr as *mut u8
}

/// Make memory executable (after writing JIT code)
pub unsafe fn rust_protect_executable(ptr: *mut u8, size: usize) {
    use libc::{mprotect, PROT_EXEC, PROT_READ};
    mprotect(ptr as *mut c_void, size, PROT_READ | PROT_EXEC);
}

/// Free executable memory
pub unsafe fn rust_free_executable(ptr: *mut u8, size: usize) {
    use libc::munmap;
    if !ptr.is_null() && size > 0 {
        munmap(ptr as *mut c_void, size);
    }
}
