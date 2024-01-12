.arch n16r

.segment text
    ; set interrupt handler
    lui $0, 0xff
    ori $0, 0xfb
    xor $1, $1
    ori $1, 0xff
    mov $65, $40
    ; finally, we enter user mode
    lui $0, 0xff ; enable all interrupts
    ori $0, 0x0c ; queue up 1,1 (user, interrupt enabled)
    mov $21, $0
    eret         ; and go

interrupt_handler:
    xch $0, $10
    mov $0, $20
    

    xch $47, $57 ; put $ra back in place, set up return jump
    eret         ; 
    jr $57       ; return jump
    


.org 0xfefffb
    j interrupt_handler
