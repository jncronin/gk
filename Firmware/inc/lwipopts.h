/**
 * Copyright (c) 2023 John Cronin
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define LWIP_DHCP                       1
#define LWIP_NETIF_API                  1
#define NO_SYS                          0
#define LWIP_DNS                        1
#define LWIP_ERRNO_STDINCLUDE           1
#define LWIP_POSIX_SOCKETS_IO_NAMES     0
#define TCPIP_MBOX_SIZE                 256
#define TCPIP_THREAD_STACKSIZE          2048
#define TCPIP_THREAD_PRIO               1
#define DEFAULT_RAW_RECVMBOX_SIZE       32
#define DEFAULT_TCP_RECVMBOX_SIZE       32
#define DEFAULT_ACCEPTMBOX_SIZE       32
#define DEFAULT_UDP_RECVMBOX_SIZE       32

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void sys_sl_lock(uint32_t *sl);
void sys_sl_unlock(uint32_t *sl);
uint32_t sys_disable_interrupts();
void sys_restore_interrupts(uint32_t cpsr);
#ifdef __cplusplus
}
#endif

#define SYS_ARCH_DECL_PROTECT(lev) uint32_t lev; \
    __attribute__((section(".sram4"))) static uint32_t lev##__sl

#define SYS_ARCH_PROTECT(lev) \
    lev = sys_disable_interrupts(); \
    sys_sl_lock(& lev##__sl )

#define SYS_ARCH_UNPROTECT(lev) \
    sys_sl_unlock(& lev##__sl ); \
    sys_restore_interrupts(lev)

#ifdef __cplusplus
extern "C"
#endif
int rtt_printf_wrapper(const char * format, ...);

#define LWIP_PLATFORM_ASSERT(x) do { rtt_printf_wrapper("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); __asm__ volatile ("bkpt #0\n");} while(0)
#define LWIP_PLATFORM_DIAG(x) do { rtt_printf_wrapper x; } while(0)


#endif
