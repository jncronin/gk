#include "pwr.h"
#include "thread.h"
#include "scheduler.h"
#include "process.h"
#include "i2c.h"
#include "vmem.h"
#include <stm32mp2xx.h>

adouble vsys, isys, psys;
adouble t0, t1, tavg;
adouble vcell, soc, crate;

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define DTS_VMEM ((DTS_TypeDef *)PMEM_TO_VMEM(DTS_BASE))

static void *pwr_thread(void *);

static bool dts_ready = false;

void init_pwr()
{
    Schedule(Thread::Create("pwr", pwr_thread, nullptr, true, GK_PRIORITY_VHIGH, p_kernel));
}

static bool dts_write(uint32_t reg, uint32_t val)
{
    while(true)
    {
        auto sr = DTS_VMEM->TSCSDIF_SR & 3U;
        if(sr & 2U)
            return false;
        if(sr == 0U)
            break;
    }

    DTS_VMEM->TSCSDIF_CR = DTS_TSCSDIF_CR_SDIF_PROG |
        DTS_TSCSDIF_CR_SDIF_WRN |
        (reg << DTS_TSCSDIF_CR_SDIF_ADDR_Pos) |
        (val << DTS_TSCSDIF_CR_SDIF_WDATA_Pos);

    return true;
}

static void dts_reset()
{
    /* Init on-die temperature digital temperature sensors.  As per RM43.3.6, p.2335.
        Assume we are doing a reset for fault, so restart everything */
    RCC_VMEM->DTSCFGR = 0;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RCC_VMEM->DTSCFGR = RCC_DTSCFGR_DTSRST;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RCC_VMEM->DTSCFGR = RCC_DTSCFGR_DTSEN | RCC_DTSCFGR_DTSLPEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    DTS_VMEM->PVT_IER = 0U;
    DTS_VMEM->TS0_IER = DTS_TS0_IER_IRQ_EN_FAULT;
    DTS_VMEM->TS1_IER = DTS_TS1_IER_IRQ_EN_FAULT;

    DTS_VMEM->TS0ALARMA_CFGR = 0U;
    DTS_VMEM->TS0ALARMB_CFGR = 0U;
    DTS_VMEM->TS1ALARMA_CFGR = 0U;
    // DTS_VMEM->TS1ALARMB_CFGR = 0U;  // not exist?

    DTS_VMEM->TSCSMPL_CR = 0U;
    DTS_VMEM->TS0HILORESETR = 3U;
    DTS_VMEM->TS1HILORESETR = 3U;

    DTS_VMEM->TSCCLKSYNTHR = DTS_TSCCLKSYNTHR_CLK_SYTH_EN |
        (4U << DTS_TSCCLKSYNTHR_CLK_SYNTH_HOLD_Pos) |
        (3U << DTS_TSCCLKSYNTHR_CLK_SYNTH_HI_Pos) |
        (3U << DTS_TSCCLKSYNTHR_CLK_SYNTH_LO_Pos);

    if(!dts_write(5U, 256U))
        return;
    if(!dts_write(1U, 0U))
        return;
    if(!dts_write(0U, (1U << 3) | (1U << 8)))
        return;

    klog("dts: enabled\n");

    dts_ready = true;
}

void *pwr_thread(void *)
{
    auto &i2c_pwr = i2c(2);

    while(true)
    {
        // sample various power monitors
        

        // Internal digital temperature sensor
        if(!dts_ready)
        {
            dts_reset();
        }
        else
        {
            [[maybe_unused]] auto dts_cnt = DTS_VMEM->TSCSMPLCNTR;
            auto ts0_d = DTS_VMEM->TS0SDIFDATAR;
            auto ts1_d = DTS_VMEM->TS1SDIFDATAR;

            // klog("dts: cnt: %u, ts0: %u, ts1: %u\n", dts_cnt, ts0_d, ts1_d);

            /*
                G = 58.5
                H = 201.2
                J = 0
                Cal5 = 4094
            */
            auto ts0_df = (((double)ts0_d / 4094.0) - 0.5) * 201.2 + 58.5;
            auto ts1_df = (((double)ts1_d / 4094.0) - 0.5) * 201.2 + 58.5;
            t0 = ts0_df;
            t1 = ts1_df;
            tavg = (ts0_df + ts1_df) / 2.0;
        }

        // INA236A on 0x40
        const unsigned int ina236a_addr = 0x40;

        // dump id
        uint16_t ina_id[2];
        uint16_t vbus, vshunt;
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x3e, &ina_id[0], 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x3f, &ina_id[1], 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x2, &vbus, 2);
        i2c_pwr.RegisterRead(ina236a_addr, (uint8_t)0x1, &vshunt, 2);

        ina_id[0] = __builtin_bswap16(ina_id[0]);
        ina_id[1] = __builtin_bswap16(ina_id[1]);
        vbus = __builtin_bswap16(vbus);
        vshunt = __builtin_bswap16(vshunt);


        /* For bus voltage, 1 LSB = 1.6 mV
            For shunt, depends on ADCRANGE
                ADCRANGE = 0, 1 LSB = 2.5 uV
                ADCRANGE = 1, 1 LSB = 625 nV
            We have 10 mOhm as shunt
            */

        int vbus_uv = (int)vbus * 1600;
        [[maybe_unused]] int vbus_v = vbus_uv / 1000000;
        [[maybe_unused]] int vbus_fract = vbus_uv % 1000000;
        vsys = (double)vbus_uv / 1000000.0;

        // ultimately want microamps here
        //  uI = nV * 1000 / uR
        int64_t vshunt_nv = (int64_t)vshunt * 2500;
        int64_t ishunt_ua = (vshunt_nv * 1000) / 10000;
        isys = (double)ishunt_ua / 1000000.0;

        // P = V * I => uP = uV * uI * 10^-6
        int64_t pshunt_uw = (int64_t)vbus_uv * ishunt_ua / 1000000;
        [[maybe_unused]] int pshunt_w = (int)(pshunt_uw / 1000000);
        [[maybe_unused]] int pshunt_fract = (int)(pshunt_uw % 1000000);
        psys = (double)pshunt_uw / 1000000.0;

#if GK_ENABLE_PWR_DUMP
        klog("pwr: id: %x, %x, vbus: %u, vshunt: %d\n", ina_id[0], ina_id[1], vbus, vshunt);
        klog("pwr: VBUS: %d.%06d V\n", vbus_v, vbus_fract);
        klog("pwr: ISHUNT: %d uA\n", (int)ishunt_ua);
        klog("pwr: PSHUNT: %d.%06d W\n", pshunt_w, pshunt_fract);
#endif

        // MAX17048 on 0x54
        const unsigned int max_addr = 0x40;

        unsigned int vcell_i = 0, soc_i = 0, crate_i = 0;
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x02, &vcell_i, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x04, &soc_i, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x16, &crate_i, 2);

        vcell_i = __builtin_bswap16(vcell_i);
        soc_i = __builtin_bswap16(soc_i);
        crate_i = __builtin_bswap16(crate_i);
        int i_crate = (int)(int16_t)crate_i;

        vcell = (double)vcell_i * 78.125 / 1000000.0;
        soc = (double)soc_i / 256.0;
        crate = (double)i_crate * 0.208;

        char maxbuf[256];
        snprintf(maxbuf, sizeof(maxbuf) - 1,
            "pwr: vcell: %f, soc: %f, crate: %f\n", (double)vcell, (double)soc, (double)crate);
        maxbuf[sizeof(maxbuf) - 1] = 0;
        klog(maxbuf);


        uint16_t version, hibrt, config, vreset, status;
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x08, &version, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x0a, &hibrt, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x0c, &config, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x18, &vreset, 2);
        i2c_pwr.RegisterRead(max_addr, (uint8_t)0x1a, &status, 2);
        klog("pwr: version: %x, hibrt: %x, config: %x, vreset: %x, status: %x\n",
            __builtin_bswap16(version),
            __builtin_bswap16(hibrt),
            __builtin_bswap16(config),
            __builtin_bswap16(vreset),
            __builtin_bswap16(status));


        Block(clock_cur() + kernel_time_from_ms(1000));
    }
}
