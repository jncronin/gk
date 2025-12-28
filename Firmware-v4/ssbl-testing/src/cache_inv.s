# Invalidate instruction and data caches before enabling caching

.cpu cortex-a35

.section .text.invcache
.global invcache
.type invcache,%function

invcache:
    # get level1 data
    msr csselr_el1, xzr
    isb
    mrs x0, ccsidr_el1

    ubfx x1, x0, #3, #10        // x1 = nways - 1
    add x1, x1, #1

    ubfx x2, x0, #13, #15       // x2 = nsets - 1
    add x2, x2, #1

    mov x3, xzr                 // x3 = current way
    mov x4, xzr                 // x4 = current set

    b 4f

1:
    // loop by way
    b 3f

2:
    // loop by set

    // for l0, associativity = 3 which occupies the upper 3 bits (29/30/31)
    //  line length is always 16 words so lower 4 bits
    //  set is therefore at bit 8
    //  level at bit 1
    mov x5, x3
    lsl x5, x5, #29
    mov x6, x4
    lsl x6, x6, #8
    orr x5, x5, x6
    dc isw, x5

    add x4, x4, #1

3:
    // test set
    cmp x4, x2
    b.lo 2b

    add x3, x3, #1
    mov x4, xzr

4:
    // test way
    cmp x3, x1
    b.lo 1b



    # get level2 data
    mov x0, #2
    msr csselr_el1, x0
    isb
    mrs x0, ccsidr_el1

    ubfx x1, x0, #3, #10        // x1 = nways - 1
    add x1, x1, #1

    ubfx x2, x0, #13, #15       // x2 = nsets - 1
    add x2, x2, #1

    mov x3, xzr                 // x3 = current way
    mov x4, xzr                 // x4 = current set

    b 4f

1:
    // loop by way
    b 3f

2:
    // loop by set

    // for l2, associativity = 7 which occupies the upper 4 bits (28/29/30/31)
    //  line length is always 16 words so lower 4 bits
    //  set is therefore at bit 8
    //  level at bit 1
    mov x5, x3
    lsl x5, x5, #28
    mov x6, x4
    lsl x6, x6, #8
    orr x5, x5, x6
    orr x5, x5, #2  // L2
    dc isw, x5

    add x4, x4, #1

3:
    // test set
    cmp x4, x2
    b.lo 2b

    add x3, x3, #1
    mov x4, xzr

4:
    // test way
    cmp x3, x1
    b.lo 1b


    // invalidate icache

    ic ialluis
    ret

.size invcache, .-invcache
