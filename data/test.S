.arch n16r

.data
.byte -128
.byte
.byte 128
.byte 255

.text
.org 0xfe0001
    j oofa
    mov $2,$6
    beq $0,$2, doofa

oofa:
    mov $3,$2
    add $3,$2
doofa:
    addu $4, $3

.byte 0x55
.byte 0xaa
