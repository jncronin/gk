#ifndef PMIC_H
#define PMIC_H

#include <cstdint>

struct pmic_vreg
{
    enum _type { Buck, LDO, RefDDR };
    _type type;
    int id;
    bool is_enabled;
    int mv = 0;
    enum _mode { HP, LP, CCM, Normal, Bypass, SinkSource };
    _mode mode = Normal;
    bool is_alt = false;
};

// stop clashes with stm32mp2 headers
#ifdef GPU
#undef GPU
#endif

enum class PMIC_Power_Target
{
    CPU,
    GPU,
    Core,
    SDCard,
    SDCard_IO,
    SDIO_IO,
    Flash,
    Audio,
    USB
};

uint8_t pmic_read_register(uint8_t reg);
pmic_vreg pmic_get_buck(int id, bool alt = false);
pmic_vreg pmic_get_ldo(int id, bool alt = false);
pmic_vreg pmic_get_refddr(bool alt = false);
void pmic_dump(const pmic_vreg &v);
void pmic_dump();
void pmic_set(const pmic_vreg &v);
int pmic_set_power(PMIC_Power_Target target, unsigned int voltage_mv);
void pmic_switchoff();
void pmic_reset();
void init_pmic();

#endif
