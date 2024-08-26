#!/bin/sh

# usage:
#   just kernel: ./interpret-profile.sh
#   with userspace program: ./interpret-profile.sh <exec_file> <load_addr>

if [ "$#" -eq 2 ]; then
    xxd -e -c 4 trace.bin | awk -v laddr="$2" '{_laddr = strtonum(laddr); addr = strtonum("0x" $2); if(addr >= _laddr) printf("%x\n", addr - _laddr);}' | arm-none-eabi-addr2line -f -e $1 > trace.txt
    xxd -e -c 4 trace.bin | awk -v laddr="$2" '{_laddr = strtonum(laddr); addr = strtonum("0x" $2); if(addr < _laddr) printf("%x\n", addr);}' | arm-none-eabi-addr2line -f -e ../build/gk.elf >> trace.txt
elif [ "$#" -eq 0 ]; then
    xxd -e -c 4 trace.bin | awk '{addr = strtonum("0x" $2); printf("%x\n", addr);}' | arm-none-eabi-addr2line -f -e ../build/gk.elf >> trace.txt
else
    echo "Usage: ./interpret-profile.sh [exec_file load_addr]"
    return
fi

Rscript interpret-profile.R

