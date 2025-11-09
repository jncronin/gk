.cpu cortex-a35

.section .data
.global gkos
.type gkos, %object
gkos:
.incbin "gkos.elf"

.size gkos,.-gkos
