.cpu cortex-a35

.section .text._ZN8Spinlock4lockEv
.global _ZN8Spinlock4lockEv
.type _ZN8Spinlock4lockEv,%function
_ZN8Spinlock4lockEv:
    // 'this' in x0, which is also address of first member
    mov x1, #1

1:
    // first non-locking test
    ldr w2, [x0]
    cbnz w2, 1b

    // then atomic semantics
    ldxr w2, [x0]
    cbz w2, 2f

    // failed to acquire
    clrex
    b 1b

2:
    // try to update
    stxr w2, w1, [x0]
    cbnz w2, 1b

    ret

.size _ZN8Spinlock4lockEv, .-_ZN8Spinlock4lockEv

.section .text._ZN8Spinlock6unlockEv
.global _ZN8Spinlock6unlockEv
.type _ZN8Spinlock6unlockEv,%function
_ZN8Spinlock6unlockEv:
    stlr xzr, [x0]
    ret
.size _ZN8Spinlock6unlockEv, .-_ZN8Spinlock6unlockEv

.section .text._ZN8Spinlock8try_lockEv
.global _ZN8Spinlock8try_lockEv
.type _ZN8Spinlock8try_lockEv,%function
_ZN8Spinlock8try_lockEv:
    mov x1, #1
    mov x3, x0
    mov x0, #0      // pre-fill fail return

    // first try non-locking
    ldr w2, [x3]
    cbnz w2, 2f

    // then atomic semantics
    ldxr w2, [x3]
    cbz w2, 1f

    // failed to acquire
    clrex
    b 2f

1:
    // try to set
    stxr w2, w1, [x3]
    cbnz w2, 2f

    // if this far then succeed
    mov x0, #1

2:
    ret
