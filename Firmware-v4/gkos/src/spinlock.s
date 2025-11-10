.cpu cortex-a35

.section .text._ZN8Spinlock4lockEv
.global _ZN8Spinlock4lockEv
.type _ZN8Spinlock4lockEv,%function
_ZN8Spinlock4lockEv:
    // 'this' in x0, which is also address of first member
    mov x1, #1

1:
    // atomic semantics
    ldxr w2, [x0]
    cbnz w2, 1b         // already taken.  No need to release exclusive monitor here
                        // because other core will only do a non-exclusive store of zero to release

    // try to update
    stxr w2, w1, [x0]
    cbnz w2, 1b         // failed to update.  try again

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

    ldxr w2, [x0]
    cbnz w2, 1f             // fail

    stxr w2, w1, [x0]
    cbnz w2, 1f             // fail

    // pass
    mov x0, #1
    ret

1:
    mov x0, xzr
    ret

.size _ZN8Spinlock8try_lockEv, .-_ZN8Spinlock8try_lockEv
