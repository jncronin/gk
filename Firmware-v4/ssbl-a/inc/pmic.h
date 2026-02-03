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

uint8_t pmic_read_register(uint8_t reg);
void pmic_write_register(uint8_t addr, uint8_t val);
pmic_vreg pmic_get_buck(int id, bool alt = false);
pmic_vreg pmic_get_ldo(int id, bool alt = false);
pmic_vreg pmic_get_refddr(bool alt = false);
void pmic_dump(const pmic_vreg &v);
void pmic_dump();
void pmic_dump_status();
void pmic_set(const pmic_vreg &v);

#endif
