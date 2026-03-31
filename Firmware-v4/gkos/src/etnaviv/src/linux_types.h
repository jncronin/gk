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
#include "syscalls_int.h"
#include "_gk_ioctls.h"

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define __iomem volatile

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

typedef size_t __kernel_size_t;

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
#define BUILD_BUG_ON(x) static_assert(!(x))
#define BIT(x) (1U << (x))

#define dev_warn(d, msg, ...) klog("GPU WARN : " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_info(d, msg, ...) klog("GPU INFO : " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_dbg(d, msg, ...) klog("GPU DEBUG: " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_err(d, msg, ...) klog("GPU ERROR: " msg __VA_OPT__(,) __VA_ARGS__)

#define clamp(x, minval, maxval) (((x) > (maxval)) ? (maxval) : (((x) < (minval)) ? (minval) : (x)))

typedef uintptr_t dma_addr_t;

#define drm_etnaviv_timespec timespec
#define timespec64 timespec

#define DECLARE_BITMAP(name, count) uint64_t name[((count + 63) & ~63) / 64] = { 0 }

struct etnaviv_drm_private;
class pm_control;
struct drm_device
{
    std::unique_ptr<etnaviv_drm_private> dev_private;
};

struct etnaviv_file_private;
struct drm_file
{
    std::unique_ptr<etnaviv_file_private> driver_priv;
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

struct drm_driver
{
	int (*open)(struct drm_device *, struct drm_file *);
	void (*postclose)(struct drm_device *, struct drm_file *);
	//struct drm_gem_object (*gem_prime_import_sg_table)(struct drm_device *,
	//	struct dma_buf_attachment *, struct sg_table *);
	void (*show_fdinfo)(struct drm_printer *, struct drm_file *);
	const struct drm_ioctl_desc *ioctls;
	size_t num_ioctls;
	const char *name;
	const char *desc;
	unsigned int major;
	unsigned int minor;
};

struct device
{
	std::unique_ptr<drm_device> drm;
	std::unique_ptr<pm_control> pm;
};

extern std::atomic<uint32_t> next_context_id;
extern std::atomic<uint32_t> next_fence_id;

#define PAGE_SIZE                   65536ul
#define PAGE_MASK                   (~(PAGE_SIZE - 1))
#define offset_in_page(p)           ((unsigned long)(p) & ~PAGE_MASK)
#define access_ok(addr, size)       addr_is_valid(addr, size)

#define __user

#define _IOC_TYPECHECK(x) sizeof(x)

#include "drm.h"

#define DRM_COMMAND_BASE                0x40
#define DRM_COMMAND_END			0xA0

#define DRM_IOCTL_NR(n)                _IOC_NR(n)
#define DRM_IOCTL_TYPE(n)              _IOC_TYPE(n)
#define DRM_MAJOR       226

#define DRM_AUTH            1u
#define DRM_MASTER          2u
#define DRM_ROOT_ONLY       4u
#define DRM_RENDER_ALLOW    32u

typedef int drm_ioctl_t(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

struct drm_ioctl_desc {
	unsigned int cmd;
	unsigned int flags;
	drm_ioctl_t *func;
	const char *name;
};

#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)				\
	[DRM_IOCTL_NR(DRM_IOCTL_##ioctl) - DRM_COMMAND_BASE] = {	\
		.cmd = DRM_IOCTL_##ioctl,				\
		.flags = _flags,					\
		.func = _func,						\
		.name = #ioctl						\
	}

int drm_dev_register(drm_device &dev, int unused_val);

void *dma_alloc_wc(struct device *dev, size_t size,
				 dma_addr_t *dma_addr, gfp_t gfp);

void dma_free_wc(struct device *dev, size_t size,
			       void *cpu_addr, dma_addr_t dma_addr);

int bitmap_find_free_region(unsigned long * bitmap,
 	int bits,
 	int order);
void bitmap_release_region (unsigned long * bitmap,
 	int pos,
 	int order);

template <typename T> int order_base_2(T x)
{
	if(x == 0 || x == 1) return 0;
	return sizeof(T) * 8 - __builtin_clz(x - 1) + 1;
}

#define GFP_KERNEL 1

class reset_control
{
	public:
		virtual int Assert() = 0;
		virtual int Deassert() = 0;
};

class Etnaviv_reset_control : public reset_control
{
	public:
		int Assert();
		int Deassert();
};

static inline int reset_control_assert(reset_control &rst) { return rst.Assert(); }
static inline int reset_control_deassert(reset_control &rst) { return rst.Deassert(); }

class clk
{
	public:
		virtual int enable(uint64_t freq = ~0ULL) = 0;
		virtual int disable() = 0;
};

class Etnaviv_core_clock : public clk
{
	public:
		uint64_t freq = 800000000;

		int enable(uint64_t freq = ~0ULL);
		int disable();
};

class Etnaviv_bus_clock : public clk
{
	public:
		int enable(uint64_t);
		int disable();
};

typedef int irqreturn_t;

class pm_control
{
	public:
		virtual int enable() = 0;
		virtual int disable() = 0;
};

class Etnaviv_pm_control : public pm_control
{
	public:
		int enable();
		int disable();
};

#endif
