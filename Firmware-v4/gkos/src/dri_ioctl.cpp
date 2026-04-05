#include "osfile.h"
#include "etnaviv/src/linux_types.h"
#include "osmutex.h"
#include "_gk_ioctls.h"
#include <cstring>
#include "drifile.h"

static_assert(sizeof(drm_version) == _IOC_SIZE(0xc0406400));



int DRIFile::Ioctl(unsigned int nr, void *ptr, size_t len, int *_errno)
{
    switch(nr)
    {
        case 0xc0406400:
            // DRM_IOCTL_VERSION
            {
                auto dv = reinterpret_cast<drm_version *>(ptr);
                dv->version_major = d->drm->major;
                dv->version_minor = d->drm->minor;
                dv->version_patchlevel = 0;
                if(dv->date && dv->date_len)
                {
                    strncpy(dv->date, d->drm->date.c_str(), dv->date_len);
                }
                if(dv->desc && dv->desc_len)
                {
                    strncpy(dv->desc, d->drm->desc.c_str(), dv->desc_len);
                }
                if(dv->name && dv->name_len)
                {
                    strncpy(dv->name, d->drm->name.c_str(), dv->name_len);
                }
                dv->date_len = d->drm->date.length() + 1;
                dv->desc_len = d->drm->desc.length() + 1;
                dv->name_len = d->drm->name.length() + 1;
            }
            return 0;            
    }

    auto dri_nr = _IOC_NR(nr);
    if(dri_nr >= DRM_COMMAND_BASE && dri_nr < DRM_COMMAND_END)
    {
        // this is a driver-specific ioctl
        auto driver_nr = dri_nr - DRM_COMMAND_BASE;
        if(driver_nr < d->drm->num_ioctls)
        {
            auto &cioc = d->drm->ioctls[driver_nr];
            return cioc.func(d->drm.get(), ptr, nullptr);
        }
    }

    klog("DRI: ioctl(%x) not supported\n", nr);
    *_errno = EINVAL;
    return -1;
}
