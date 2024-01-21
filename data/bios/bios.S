.arch n16r

.segment data

; KERNEL MEMORY
m_kernel_exception_memory:
    .dword 0 ; $40
    .dword 0 ; $41
    .dword 0 ; $42
    .dword 0 ; $43
    .dword 0 ; $44
    .dword 0 ; $45
    .dword 0 ; $46
    .dword 0 ; $47
    .dword 0 ; $60
    .dword 0 ; $65
    .dword 0 ; $66
    .dword 0 ; $67


; DRIVER MEMORY

.segment text
    ; set interrupt handler
    adr $0, $1, exception_handler
    mov $65, $40

  ; serial driver init
    jal driver_serial_init

    lui $0, 0xff
    ori $0, 0x0c
    mov $0, $20
    eret

    ; eventually, we build our command interpreter
    ; maybe something like wozmon?
idle_loop:
    j idle_loop

exception_handler:
    ; hold $0,$1 and kernel load store address
    xch  $40, $50
    adr  $0, $1, m_kernel_exception_memory
    xch  $45, $40
    ; store first-tier registers so we have breathing room
    sw   $0, 20($45) ; store stack pointer
    sw   $1, 22($45) ; .
    mov  $40, $44
    sw   $0, 16($45) ; store global pointer
    sw   $1, 18($45) ; .
    mov  $40, $46
    sw   $0, 24($45) ; store frame pointer
    sw   $1, 26($45) ; .
    mov  $40, $47
    sw   $0, 28($45) ; store return address
    sw   $1, 30($45) ; .
    mov  $40, $50
    sw   $0,  0($45)
    sw   $1,  2($45)
    sw   $2,  4($45)
    sw   $3,  6($45)
    sw   $4,  8($45)
    sw   $5, 10($45)
    sw   $6, 12($45)
    sw   $7, 14($45)
    

    mov  $0,  $20   ; grab cause
    andi $0,  0x3c  ;  isolate exception
    xor  $1,  $1
    xch  $40, $47
    ; $47 should now contain an offset into our jump table
    adr  $0, $1, _exception_vectors
    add  $47, $40
    jr   $47        ;  jump to the target

    ; 16 jump targets for individual exception handlers
  _exception_vectors:
    j interrupt_handler        ; int
    j exception_handler_exit
    j exception_handler_exit
    j exception_handler_exit
    j exception_handler_exit   ; addrl
    j exception_handler_exit   ; addrs
    j exception_handler_exit   ; ibus
    j exception_handler_exit   ; dbus
    j syscall_handler          ; syscall
    j exception_handler_exit   ; bkpt
    j reserved_handler         ; ri
    j exception_handler_exit
    j exception_handler_exit   ; ovf
    j exception_handler_exit
    j exception_handler_exit
    j exception_handler_exit

interrupt_handler:
    xor  $1, $1
    mov  $0, $20
    mov  $2, $0
    shr  $2, 0x8
    andi $2, 0x4
    bne  $2, $1, drivers_serial_interrupt

syscall_handler:
reserved_handler:
exception_handler_exit
    mov $47, $67 ; 
    xch $40, $50
    xch $47, $57 ; put $ra back, set up return jump
    eret
    jr $57       ; return jump (in eret protection slot)
    

.include "drivers/serial.S"
