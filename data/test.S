.arch n16r

.segment kmem

.segment ktext
    adr $0, $1, exception_handler
    mov $65, $40
    lui $0, 0xff
    ori $0, 0x0c
    mov $21, $0
    eret

loop:
    j loop
    j loop
    j loop

exception_handler:
    adr $0, $1, 0x000c0010
    lw  $0, 0($40)
    mov  $1, $0
    shr  $1, 8
    andi $1, 1
    xor  $4, $4
    bne  $1, $4, _exit
    mov  $1, $0
    andi $1, 0xf
    sb   $1, 0($40)
_exit:
    mov  $47, $67
    eret
    jr $47
