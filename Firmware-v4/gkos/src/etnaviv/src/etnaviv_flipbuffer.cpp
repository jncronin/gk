/* specific for gkos - generate instructions to flip the front color buffer to the display */

#include "linux_types.h"
#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "screen.h"
#include "vmem.h"

// TODO: this should be an inherited method for the etnaviv device, rather than a member of drm_device
int drm_device::flipbuffer(std::shared_ptr<drm_gem_object> &fb)
{
    auto obj = std::static_pointer_cast<etnaviv_gem_object>(fb);

    [[maybe_unused]] auto scr_adr = screen_current().first;
    //memcpy((void *)scr_adr, (void *)PMEM_TO_VMEM_DEVICE(obj->dma_addr), obj->size);
    screen_update();

    return 0;
}