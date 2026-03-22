#ifndef LINUX_TYPES_H
#define LINUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8 __le8;
typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;

typedef uintptr_t dma_addr_t;

#define drm_etnaviv_timespec timespec
#define timespec64 timespec

struct drm_device
{

};

struct drm_file
{

};

struct drm_gem_object
{

};

struct iosys_map
{

};

#endif
