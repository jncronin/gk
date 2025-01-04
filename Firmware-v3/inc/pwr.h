#ifndef PWR_H
#define PWR_H

extern "C" int pwr_disable_regulators();
int pwr_set_vos_high();
double pwr_get_vdd();
void *pwr_monitor_thread(void *p);

struct pwr_status
{
    double vdd;
    double vbat;
    double state_of_charge;
    double charge_rate;
    double time_until_full_empty;
    bool is_charging;
    bool is_full;
    bool battery_present;
    bool vreg_overtemperature;
    bool vreg_undervoltage;
    bool vreg_pgood;
};

void pwr_get_status(pwr_status *stat);

#endif
