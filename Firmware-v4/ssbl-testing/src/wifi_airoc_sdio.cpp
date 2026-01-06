#include "cyhal.h"
#include "cyhal_gpio.h"
#include "cyhal_sdio.h"
#include "logger.h"
#include "sdif.h"

#define CY_FAIL CY_RSLT_CREATE(CY_RSLT_TYPE_FATAL, CY_RSLT_MODULE_ABSTRACTION_HAL, 0)

void cyhal_sdio_enable_event(cyhal_sdio_t *obj, cyhal_sdio_event_t event, uint8_t intr_priority, bool enable)
{
    if(event == CYHAL_SDIO_CARD_INTERRUPT)
    {
        if(enable)
        {
            sdmmc[1].iface->MASK |= SDMMC_MASK_SDIOITIE;
        }
        else
        {
            sdmmc[1].iface->MASK &= ~SDMMC_MASK_SDIOITIE;
        }
        return;
    }
    klog("cyhal_sdio_enable_event(..., %u, %u, %s)\n", event, intr_priority,
        enable ? "enable" : "disable");
}

void cyhal_sdio_register_callback(cyhal_sdio_t *obj, cyhal_sdio_event_callback_t callback, void *callback_arg)
{
    klog("cyhal_sdio_register_callback\n");
}

cy_rslt_t cyhal_sdio_send_cmd(cyhal_sdio_t *obj, cyhal_sdio_transfer_type_t direction, cyhal_sdio_command_t command, uint32_t argument, uint32_t* response)
{
    if(command == 52)
    {
        // allow command 52, disallow others
        return sdmmc[1].sd_issue_command(command, SDIF::resp_type::R5, argument, response) == 0 ? CY_RSLT_SUCCESS : CY_FAIL;

    }
    klog("cyhal_sdio_send_cmd(%u)\n", command);
    return CY_FAIL;
}

cy_rslt_t cyhal_sdio_bulk_transfer(cyhal_sdio_t *obj, cyhal_sdio_transfer_type_t direction, uint32_t argument, const uint32_t* data, uint16_t length, uint32_t* response)
{
    klog("cyhal_sdio_bulk_transfer(..., %u, %x, %p, %u)\n",
        direction, argument, data, length);
    return CY_FAIL;
}

cy_rslt_t cyhal_gpio_init(cyhal_gpio_t pin, cyhal_gpio_direction_t direction, cyhal_gpio_drive_mode_t drive_mode, bool init_val)
{
    klog("cyhal_gpio_init\n");
    return CY_FAIL;
}

void cyhal_gpio_register_callback(cyhal_gpio_t pin, cyhal_gpio_callback_data_t* callback_data)
{
    klog("cyhal_gpio_register_callback\n");
}

void cyhal_gpio_enable_event(cyhal_gpio_t pin, cyhal_gpio_event_t event, uint8_t intr_priority, bool enable)
{
    klog("cyhal_gpio_register_event(..., %u, %u, %s)\n",
        event, intr_priority, enable ? "enable" : "disable");
}

void cyhal_gpio_free(cyhal_gpio_t pin)
{
    klog("cyhal_gpio_free\n");
}

