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

#include "sg_table.h"
#include "block_allocator.h"

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

using phys_addr_t = uintptr_t;

void *memset32(void *d, uint32_t v, size_t n);

ssize_t strscpy_pad(char *dest, const char *src, size_t count);

struct list_head
{
    //struct list_head *prev;
    //struct list_head *next;
};

#define container_of(obj, result_type, member) (result_type *)((uintptr_t)(obj) - offsetof(result_type, member))

#define xstr(a) str(a)
#define str(a) #a

#define BUG() do { klog("BUG()\n"); while(true); } while(0)
#define BUG_ON(x) do { if(x) { klog("BUG_ON(%s)\n", str(x)); while(true); } } while(0)
#define WARN_ON(x) do { if(x) { klog("WARN_ON(%s)\n", str(x)); } } while(0)
#define BUILD_BUG_ON(x) static_assert(!(x))
#define BIT(x) (1U << (x))

#define IS_ALIGNED(x, al) (((x) & ((al) - 1)) == 0)
#define ALIGN_DOWN(x, al) ((x) & ~((typeof(x))(al) - 1))

#define dev_warn(d, msg, ...) klog("GPU WARN : " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_info(d, msg, ...) klog("GPU INFO : " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_dbg(d, msg, ...) klog("GPU DEBUG: " msg __VA_OPT__(,) __VA_ARGS__)
#define dev_err(d, msg, ...) klog("GPU ERROR: " msg __VA_OPT__(,) __VA_ARGS__)
#define WARN(d, msg, ...) klog("WARN: " msg __VA_OPT__(,) __VA_ARGS__)

#define SZ_1				0x00000001
#define SZ_2				0x00000002
#define SZ_4				0x00000004
#define SZ_8				0x00000008
#define SZ_16				0x00000010
#define SZ_32				0x00000020
#define SZ_64				0x00000040
#define SZ_128				0x00000080
#define SZ_256				0x00000100
#define SZ_512				0x00000200

#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_64M				0x04000000
#define SZ_128M				0x08000000
#define SZ_256M				0x10000000
#define SZ_512M				0x20000000

#define SZ_1G				0x40000000
#define SZ_2G				0x80000000

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
	const struct drm_ioctl_desc *ioctls;
	size_t num_ioctls;
	std::string name, date, desc;
	unsigned int major, minor;
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
	BlockAllocator<int> alloc =
		BlockAllocator<int>(0, 0x100000000);
};

struct drm_mm_node : public BlockAllocator<int>::BlockAddress
{
	drm_mm *mm = nullptr;
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

int drm_dev_register(std::shared_ptr<device> &dev, int unused_val);

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
void bitmap_zero (unsigned long *bitmap, int bits);
void set_bit(long nr, volatile unsigned long *addr);
void clear_bit(long nr, volatile unsigned long *addr);
unsigned long find_first_zero_bit(const unsigned long *addr,
					 unsigned long size);

template <typename T> int order_base_2(T x)
{
	if(x == 0 || x == 1) return 0;
	return sizeof(T) * 8 - __builtin_clz(x - 1);
}

#define GFP_KERNEL 		1
#define GFP_HIGHUSER	2

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
		int enable(uint64_t = 0);
		int disable();
};

class Etnaviv_reg_clock : public clk
{
	public:
		int enable(uint64_t = 0);
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

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) ((u32)(n))

#endif
