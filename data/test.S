.arch n16r

.segment kmem
fun_times:
    .dword 987654
welcome:
    .dword 123456

.segment ktext
.include "../data/bios/test.S"
    j fun_times
    mov $40, $51
    xch $1, $10
