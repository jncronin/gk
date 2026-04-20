.section .rodata

.global img_gk
.type img_gk, @object
.balign 64
img_gk:
.incbin "img_gk.bin"
.size img_gk, . - img_gk

.global img_provisioning1
.type img_provisioning1, @object
.balign 64
img_provisioning1:
.incbin "img_provisioning1.bin"
.size img_provisioning1, . - img_provisioning1

.global img_provisioning2
.type img_provisioning2, @object
.balign 64
img_provisioning2:
.incbin "img_provisioning2.bin"
.size img_provisioning2, . - img_provisioning2

.global img_rawsd
.type img_rawsd, @object
.balign 64
img_rawsd:
.incbin "img_rawsd.bin"
.size img_rawsd, . - img_rawsd
