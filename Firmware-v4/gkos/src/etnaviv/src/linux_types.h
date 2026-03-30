#ifndef LINUX_TYPES_H
#define LINUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

#include <vector>
#include <list>
#include <osmutex.h>
#include <atomic>

#include <memory>

#include "logger.h"

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))

#define DRM_DEBUG klog

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u8 __le8;
typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;

typedef u8 __u8;
typedef u16 __u16;
typedef u32 __u32;
typedef u64 __u64;

typedef s8 __s8;
typedef s16 __s16;
typedef s32 __s32;
typedef s64 __s64;

typedef unsigned int gfp_t;

using atomic_t = std::atomic<unsigned int>;
using kref = std::atomic<unsigned int>;

using phys_addr_t = uintptr_t;

struct list_head
{
    struct list_head *prev;
    struct list_head *next;
};

#define container_of(obj, result_type, member) (result_type *)((uintptr_t)(obj) - offsetof(result_type, member))

#define xstr(a) str(a)
#define str(a) #a

#define BUG() do { klog("BUG()\n"); while(true); } while(0);
#define BUG_ON(x) do { if(x) { klog("BUG_ON(%s)\n", str(x)); while(true); } } while(0);

typedef uintptr_t dma_addr_t;

#define drm_etnaviv_timespec timespec
#define timespec64 timespec

#define DECLARE_BITMAP(name, count) uint64_t name[(count) / 64]

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

struct drm_sched_entity
{

};

struct drm_sched_job
{

};

struct drm_mm
{

};

struct drm_mm_node
{

};

struct drm_gpu_scheduler
{

};

#endif
