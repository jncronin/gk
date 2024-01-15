#ifndef OSTYPES_H
#define OSTYPES_H

enum MemRegionType
{
    AXISRAM = 0,
    SRAM = 1,
    DTCM = 2,
    SDRAM = 3
};

enum CPUAffinity
{
    Either = 3,
    M7Only = 1,
    M4Only = 2,
    PreferM7 = 7,
    PreferM4 = 11
};

enum MemRegionAccess
{
    NoAccess = 0,
    RO = 1,
    RW = 2
};


#endif
