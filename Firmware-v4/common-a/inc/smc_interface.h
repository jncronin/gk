#ifndef SMC_INTERFACE_H
#define SMC_INTERFACE_H

enum SMC_Call
{
    StartupAP,
    SetPower,
};

enum SMC_Power_Target
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
