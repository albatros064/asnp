.data
; scratch space - reset to zero when done
Z0: .word 0
Z1: .word 0
Z2: .word 0
T0: .word 0 ; names here are arbitrary. they could all be Zx
T1: .word 0

; synthesized instruction bits - there must be no real function calls between the start and end of
;   a synthesized instruction, so we don't use a stack. should be reset to zero when done
AR0: .word 0 ; "argument" for synthesized instructions
AR1: .word 0 ; '
RET: .word 0 ; "return" for synthesized instructions

; constants - do not write to these...
POS1: .word  1 ; 0x0001
NEG1: .word -1 ; 0xffff
C15:  .word 15 ; 0x000f

.text
oper_AND:
    subleq C15,T1                 ; set our loop counter to -15
                                  ;   (we'll count up and stop the loop at 0 => 1)
  oper_AND_top:
    subleq AR0,Z0,oper_AND_unset0 ; eg: -9 (10111) => 9 (01001) top bit was set, so we're _not_
                                  ;   negative. fall through to check the other value. (1? bit)
                                  ;   if top bit was not set, skip working increment (0 bit)
    subleq AR1,Z1,oper_AND_unset1 ; same deal here. fall through to increment working var (1 bit),
                                  ;   building it up one bit at a time. otherwise, skip (0 bit)
    subleq NEG1,T0                ; set bit in working var
    subleq Z2,Z2,oper_AND_unset1
  oper_AND_unset0:
    subleq b,Z1                   ; we got here because we don't have a set bit on the first value,
                                  ;   but we still need to do the shift on the second value, so we
                                  ;   start that process here.
  oper_AND_unset1:
    subleq Z0,a                   ; finish a += a (a <<= 1)
    subleq Z1,b                   ; finish b += b (b <<= 1)
    subleq Z0,Z0                  ; clear zeros
    subleq Z1,Z1                  ; '
    subleq T0,Z0                  ; start T0 += T0
    subleq Z0,T0                  ;  finish T0 += T0 (T0 <<= 1)
    subleq Z0,Z0                  ; clear zero
    subleq NEG1,T1,oper_AND_top   ; increment loop counter, and loop if we're still <= 0
                                  ;   you argue this hurts branch prediction... what prediction?
  oper_AND_exit:
    subleq T0,Z0                  ; negate our working value
    subleq Z0,RET                 ; and put it where it needs to go (may not need to be RET)
    subleq Z0,Z0                  ; re-zero
    subleq Z1,Z1                  ; '
    subleq T0,T0                  ; '
    subleq T1,T1                  ; '
