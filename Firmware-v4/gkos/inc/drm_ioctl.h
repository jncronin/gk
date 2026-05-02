#ifndef DRM_IOCTL_H
#define DRM_IOCTL_H

#include <memory>
#include "_gk_ioctls.h"

class drm_device;
class drm_file;

typedef int drm_ioctl_t(drm_device *dev, void *data,
			std::shared_ptr<drm_file> &file_priv);

struct drm_ioctl_desc {
	unsigned int cmd;
	unsigned int flags;
	drm_ioctl_t *func;
	const char *name;
	size_t param_size;
};

#define DRM_COMMAND_BASE                0x40
#define DRM_COMMAND_END			0xA0

#define DRM_IOCTL_NR(n)                _IOC_NR(n)
#define DRM_IOCTL_TYPE(n)              _IOC_TYPE(n)
#define DRM_MAJOR       226

#define DRM_AUTH            1u
#define DRM_MASTER          2u
#define DRM_ROOT_ONLY       4u
#define DRM_RENDER_ALLOW    32u

#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)				\
	[DRM_IOCTL_NR(DRM_IOCTL_##ioctl) - DRM_COMMAND_BASE] = {	\
		.cmd = DRM_IOCTL_##ioctl,				\
		.flags = _flags,					\
		.func = _func,						\
		.name = #ioctl						\
	}

#endif
