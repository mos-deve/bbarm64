.global _start
_start:
    // write(1, msg, len)
    mov     x0, #1
    adr     x1, msg
    mov     x2, #13
    mov     x8, #64
    svc     #0

    // exit(0)
    mov     x0, #0
    mov     x8, #93
    svc     #0

msg:
    .ascii "Hello World!\n"
