.cpu cortex-a35

.align 16
.section .rodata.ddr4_fw
.type _binary_ddr4_pmu_train_bin,%object
.global _binary_ddr4_pmu_train_bin
_binary_ddr4_pmu_train_bin:
.incbin "../../stm32-ddr-phy-binary/stm32mp2/ddr4_pmu_train.bin"
.size _binary_ddr4_pmu_train_bin, .-_binary_ddr4_pmu_train_bin
.align 16

.align 16
.section .rodata.ddr3_fw
.type _binary_ddr3_pmu_train_bin,%object
.global _binary_ddr3_pmu_train_bin
_binary_ddr3_pmu_train_bin:
.incbin "../../stm32-ddr-phy-binary/stm32mp2/ddr3_pmu_train.bin"
.size _binary_ddr3_pmu_train_bin, .-_binary_ddr3_pmu_train_bin
.align 16

.align 16
.section .rodata.lpddr4_fw
.type _binary_lpddr4_pmu_train_bin,%object
.global _binary_lpddr4_pmu_train_bin
_binary_lpddr4_pmu_train_bin:
.incbin "../../stm32-ddr-phy-binary/stm32mp2/lpddr4_pmu_train.bin"
.size _binary_lpddr4_pmu_train_bin, .-_binary_lpddr4_pmu_train_bin
.align 16
