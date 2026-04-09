#include "osfile.h"
#include "etnaviv/src/linux_types.h"
#include "osmutex.h"
#include "_gk_ioctls.h"
#include <cstring>
#include "drifile.h"

static_assert(sizeof(drm_version) == _IOC_SIZE(0xc0406400));
static_assert(sizeof(drm_mode_create_dumb) == _IOC_SIZE(0xc02064b2));
static_assert(sizeof(drm_mode_map_dumb) == _IOC_SIZE(0xc01064b3));

static int dri_ioctl_mode_create_dumb(drm_mode_create_dumb *m, drm_device *dev, drm_file *f);
static int dri_ioctl_mode_map_dumb(drm_mode_map_dumb *m, drm_device *dev, drm_file *f);

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
            
        case 0xc02064b2:
            // DRM_IOCTL_MODE_CREATE_DUMB
            return dri_ioctl_mode_create_dumb(reinterpret_cast<drm_mode_create_dumb *>(ptr),
                d->drm.get(), df.get());

        case 0xc01064b3:
            // DRM_IOCTL_MODE_MAP_DUMB
            return dri_ioctl_mode_map_dumb(reinterpret_cast<drm_mode_map_dumb *>(ptr),
                d->drm.get(), df.get());

    }

    auto dri_nr = _IOC_NR(nr);
    if(dri_nr >= DRM_COMMAND_BASE && dri_nr < DRM_COMMAND_END)
    {
        // this is a driver-specific ioctl
        auto driver_nr = dri_nr - DRM_COMMAND_BASE;
        if(driver_nr < d->drm->num_ioctls)
        {
            auto &cioc = d->drm->ioctls[driver_nr];
            if(cioc.param_size != _IOC_SIZE(nr))
            {
                klog("DRI_ERROR: ioctl %08x parameter size %u, expected %u\n",
                    nr, _IOC_SIZE(nr), cioc.param_size);
                return -1;
            }
#if GPU_DEBUG > 2
            klog("DRI: ioctl: %08x starting\n", nr);
#endif
            auto ret = cioc.func(d->drm.get(), ptr, df.get());
#if GPU_DEBUG > 2
            klog("DRI: ioctl: %08x returning %d\n", nr, ret);
#endif
            return ret;
        }
    }

    klog("DRI: ioctl(%x) not supported\n", nr);
    *_errno = EINVAL;
    return -1;
}

// TODO: use a member function within the the subclassed drm_device here
int etnaviv_gem_new_handle(struct drm_device *dev, struct drm_file *file,
	u32 size, u32 flags, u32 *handle);
#define ETNA_BO_WC           0x00020000				/* MT_NORMAL_NC */



int dri_ioctl_mode_create_dumb(drm_mode_create_dumb *m, drm_device *dev, drm_file *f)
{
    auto pitch = m->width * m->bpp / 8;
    // align to 256 bytes for etnaviv efficiency
    pitch = (pitch + 255U) & ~255U;
    
    auto size = m->height * pitch;
    uint32_t handle;

    auto ret = etnaviv_gem_new_handle(dev, f, size, ETNA_BO_WC, &handle);

    if(ret == 0)
    {
        klog("drm_ioctl_mode_create_dumb: %ux%ux%u - allocated buffer with size %u and handle %u\n",
            m->width, m->height, m->bpp, size, handle);
        m->pitch = pitch;
        m->handle = handle;

        return 0;
    }
    else
    {
        klog("etnaviv_gem_new_handle failed: %d\n", ret);
        return ret;
    }
}

static int dri_ioctl_mode_map_dumb(drm_mode_map_dumb *m, drm_device *dev, drm_file *f)
{
    auto obj = drm_gem_object_lookup(f, m->handle);
    if(obj)
    {
        if(!obj->vma_node)
        {
            drm_gem_create_mmap_offset(obj);
        }
        m->offset = obj->vma_node;
        klog("dri_ioctl_mode_map_dump: handle %u linked to mmap offset %llx\n", m->handle,
            m->offset);
        return 0;
    }
    else
    {
        klog("drm_ioctl_mode_map_dumb: handle not found: %u\n", m->handle);
        return 0;
    }
}
