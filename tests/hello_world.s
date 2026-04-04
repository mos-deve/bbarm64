.global _start
_start:
    mov x0, #1          // stdout
    adr x1, msg         // message address
    mov x2, #13         // length
    mov x8, #64         // sys_write (ARM64)
    svc #0

    mov x0, #0          // exit code
    mov x8, #93         // sys_exit (ARM64)
    svc #0

msg:
    .ascii "Hello World!\n"
