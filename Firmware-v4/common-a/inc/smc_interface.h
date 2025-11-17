#ifndef SMC_INTERFACE_H
#define SMC_INTERFACE_H

enum class SMC_Call
{
    StartupAP,
    SetPower,
};

// stop clashes with stm32mp2 headers
#ifdef GPU
#undef GPU
#endif

enum class SMC_Power_Target
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

#endif
