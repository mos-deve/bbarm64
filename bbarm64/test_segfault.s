.global _start
_start:
    // Write to NULL pointer
    str x0, [x0]
    
    // Exit (never reached)
    mov x8, #93
    mov x0, #0
    svc #0
