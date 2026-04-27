#ifndef DRM_DEVICE_H
#define DRM_DEVICE_H

#include <memory>

class device;
typedef std::shared_ptr<device> (*get_drm_device_t)(int);

int drm_dev_register(get_drm_device_t get_func, int unused_val);

#endif
