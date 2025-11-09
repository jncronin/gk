.cpu cortex-a35

.section .text.quick_clear_64
.global quick_clear_64
.type quick_clear_64, %function

quick_clear_64:
    add x1, x0, x1      // x1 = end of data

    b 2f

1:
    stp xzr, xzr, [x0], #16
    stp xzr, xzr, [x0], #16
    stp xzr, xzr, [x0], #16
    stp xzr, xzr, [x0], #16

2:
    cmp x0, x1
    b.lo 1b

    ret

.size quick_clear_64,.-quick_clear_64



.section .text.quick_copy_64
.global quick_copy_64
.type quick_copy_64, %function

quick_copy_64:
    add x2, x0, x2      // x2 = end of dest data

    b 2f
1:
    ldp x3, x4, [x1], #16
    stp x3, x4, [x0], #16
    ldp x5, x6, [x1], #16
    stp x5, x6, [x0], #16
    ldp x7, x8, [x1], #16
    stp x7, x8, [x0], #16
    ldp x9, x10, [x1], #16
    stp x9, x10, [x0], #16

2:
    cmp x0, x1
    b.lo 1b

    ret

.size quick_copy_64,.-quick_copy_64
