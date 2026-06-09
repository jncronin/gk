#ifndef DRM_DEVICE_H
#define DRM_DEVICE_H

#include <string>
#include "drm_ioctl.h"
#include "drm_scheduler.h"

class pm_control
{
	public:
		virtual int enable() = 0;
		virtual int disable() = 0;
};

class clk
{
	public:
		virtual int enable(uint64_t freq = ~0ULL) = 0;
		virtual int disable() = 0;
};

class reset_control
{
	public:
		virtual int Assert() = 0;
		virtual int Deassert() = 0;
};

class drm_device
{
    public:
        std::unique_ptr<pm_control> pm;
		std::unique_ptr<DRMScheduler> dsched;

        const struct drm_ioctl_desc *ioctls;
        size_t num_ioctls;
        std::string name, date, desc;
        unsigned int major, minor;

		virtual int suspend();
		virtual int resume();

		unsigned int suspend_timeout_ms = 3000;

        virtual ~drm_device() = default;
};

int drm_dev_register(drm_device *, int unused_val);

#endif
