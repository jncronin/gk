#include "osfile.h"
#include "etnaviv/src/linux_types.h"
#include "osmutex.h"
#include <vector>

static Spinlock sl_dri;
static std::vector<std::shared_ptr<device>> drm_devs;

DRIFile::DRIFile()
{
    type = FT_DRI;
    klog("DRI: opened\n");
}

ssize_t DRIFile::Write(const char *, size_t, int *_errno)
{
    *_errno = EINVAL;
    return -1;
}

ssize_t DRIFile::Read(char *, size_t, int *_errno)
{
    *_errno = EINVAL;
    return -1;
}

int DRIFile::ReadDir(dirent *de, int *_errno)
{
    if(dt != dir)
    {
        *_errno = ENOTDIR;
        return -1;
    }

    CriticalGuard cg(sl_dri);
    if(dir_iter >= drm_devs.size() * 2)
    {
        return 0;
    }

    bool is_render = false;
    auto act_iter = dir_iter;
    if(dir_iter >= drm_devs.size())
    {
        is_render = true;
        act_iter = dir_iter - drm_devs.size();
    }

    //auto &cdev = drm_devs[act_iter];

    de->d_type = DT_CHR;
    de->d_ino = 0;
    de->d_off = 0;
    de->d_reclen = 0;

    if(is_render)
    {
        snprintf(de->d_name, sizeof(de->d_name) - 1, "renderD%u", act_iter + 128);
    }
    else
    {
        snprintf(de->d_name, sizeof(de->d_name) - 1, "card%u", act_iter);
    }
    de->d_name[sizeof(de->d_name) - 1] = 0;

    dir_iter++;

    return 1;
}

int DRIFile::Fstat(struct stat *buf, int *_errno)
{
    *buf = { 0 };
    if(dt == dri_type::dir)
    {
        buf->st_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        return 0;
    }
    buf->st_mode = S_IFCHR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    if(dt == dri_type::card)
    {
        buf->st_rdev = (DRM_MAJOR << 8) | dir_iter;
    }
    else
    {
        buf->st_rdev = (DRM_MAJOR << 8) | (dir_iter + 128);
    }

    return 0;
}

int drm_dev_register(std::shared_ptr<device> &dev, int)
{
    CriticalGuard cg(sl_dri);
    auto dev_id = drm_devs.size();
    drm_devs.push_back(dev);
    cg.unlock();

    klog("DRM: registered device %u: %s\n", dev_id, dev->drm->name.c_str());
    return 0;
}

int dri_open(const std::string &fname, PFile *f, bool for_read, bool for_write)
{
    bool is_render = false;
    unsigned int dev_id;

    /* parse cardX/renderDX to is_render,dev_id */
    if(fname.starts_with("card"))
    {
        if(fname.length() == 4)
        {
            // invalid
            return -1;
        }

        char *endptr;
        dev_id = std::strtoul(fname.substr(4).c_str(), &endptr, 10);
        if(*endptr != 0)
        {
            // invalid
            return -1;
        }
    }
    else if(fname.starts_with("renderD"))
    {
        if(fname.length() == 7)
        {
            // invalid
            return -1;
        }

        char *endptr;
        dev_id = std::strtoul(fname.substr(7).c_str(), &endptr, 10);
        if(*endptr != 0)
        {
            // invalid
            return -1;
        }
        dev_id -= 128;
        is_render = true;
    }
    else
    {
        // invalid
        return -1;
    }

    klog("dri_open: dev_id: %u, is_render: %s\n", dev_id, is_render ? "true" : "false");

    CriticalGuard cg(sl_dri);
    if(dev_id >= drm_devs.size())
    {
        // invalid
        return -1;
    }

    auto cdev = drm_devs[dev_id];
    cg.unlock();

    auto nfile = std::make_shared<DRIFile>();
    nfile->d = cdev;
    nfile->dev_name = cdev->drm->name;
    nfile->dir_iter = dev_id;
    nfile->dt = is_render ? DRIFile::dri_type::render : DRIFile::dri_type::card;
    *f = nfile;

    return 0;
}
