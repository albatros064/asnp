.segment data

  m_drivers_serial:
    .byte 0  ; bool     out_ready
    .byte 0  ; reserved
    .byte 0  ; uint8_t  in_buffer_head
    .byte 0  ; uint8_t  in_buffer_tail
    .dword 0 ; char[16] in_buffer
    .dword 0 ; .
    .dword 0 ; .
    .dword 0 ; .

.segment text

drivers_serial_init:
    adr $2, $3, m_driver_serial
    xor $0, $0
    sw  $0, 2($41) ; in_buffer_head = in_buffer_tail = 0
    ori $0, 1
    sb  $0, 0($41) ; out_ready = 1
    jr  $47

drivers_serial_interrupt:
    adr  $2, $3, m_driver_serial
    adr  $0, $1, 0x000c0010 ; i/o address
    rw   $0, $40    ; read serial status word
    mv   $1, $0     ; extract input status
    shr  $1, 8      ; .
    andi $1, 1      ; .
    xor  $4, $4
    ; if no input byte, move on
    bne  $1, $4, driver_serial_interrupt_1
    mov  $1, $0
    andi $1, 0xf    ; isolate input byte
    lb   $4, 2($41) ; load in_buffer_head
    lb   $5, 3($41) ; load in_buffer_tail
    mov  $6, $4
    addi $6, 1      ; increment head
    andi $6, 0xf    ; and wrap it to the buffer size
    ; if buffer is full, move on
    beq  $6, $5, driver_serial_interrupt_1
    addi $4, 4
    sbr  $1, $4($41)
    sb   $6, 2($41)

  drivers_serial_interrupt_1:
    shr  $0, 9      ; extract "output busy" status
    nor  $0, $0     ; invert for "output ready"
    andi $0, 1
    wb   $0, 0($41) ; and write it

    j exception_handler_exit
