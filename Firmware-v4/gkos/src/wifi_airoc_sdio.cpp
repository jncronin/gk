#include "cyhal.h"
#include "cyhal_gpio.h"
#include "cyhal_sdio.h"
#include "logger.h"
#include "sdif.h"
#include "pins.h"
#include "vmem.h"
#include "gic.h"

#define GPIOA_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE))
#define EXTI1_VMEM ((EXTI_TypeDef *)PMEM_TO_VMEM(EXTI1_BASE))

static const constexpr pin M2_WIFI_WAKE { GPIOA_VMEM, 6 };
static void SDMMC2_IRQHandler(exception_regs *, uint64_t);
static bool iten = false;

#define CY_FAIL CY_RSLT_CREATE(CY_RSLT_TYPE_FATAL, CY_RSLT_MODULE_ABSTRACTION_HAL, 0)

void cyhal_sdio_enable_event(cyhal_sdio_t *obj, cyhal_sdio_event_t event, uint8_t intr_priority, bool enable)
{
    if(event == CYHAL_SDIO_CARD_INTERRUPT)
    {
        if(enable)
        {
            iten = true;
            sdmmc[1].iface->MASK |= SDMMC_MASK_SDIOITIE;
            gic_set_handler(229, SDMMC2_IRQHandler);
            gic_set_target(229, GIC_ENABLED_CORES);
            gic_set_enable(229);
        }
        else
        {
            iten = false;
            sdmmc[1].iface->MASK &= ~SDMMC_MASK_SDIOITIE;
        }
        return;
    }
    klog("cyhal_sdio_enable_event(..., %u, %u, %s)\n", event, intr_priority,
        enable ? "enable" : "disable");
}

static cyhal_sdio_event_callback_t sdio_cb = nullptr;
static void *sdio_cb_arg = nullptr;

static void airoc_card_int()
{
    sdmmc[1].iface->MASK &= ~SDMMC_MASK_SDIOITIE;
    if(sdio_cb)
        sdio_cb(sdio_cb_arg, cyhal_sdio_event_t::CYHAL_SDIO_CARD_INTERRUPT);
}

void cyhal_sdio_register_callback(cyhal_sdio_t *obj, cyhal_sdio_event_callback_t callback, void *callback_arg)
{
    sdmmc[1].card_interrupt_handler = airoc_card_int;
    sdio_cb = callback;
    sdio_cb_arg = callback_arg;
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
    /* check consistency between argument, direction and length parameters */
    bool is_block = (argument & (1U << 27)) != 0;
    bool is_write = (argument & (1U << 31)) != 0;
    unsigned int byte_block_count = argument & 0x1ffU;

    //klog("cyhal_sdio_bulk_transfer(..., %u, %x, %p, %u)\n",
    //    direction, argument, data, length);

    if(is_write && direction != cyhal_sdio_transfer_type_t::CYHAL_SDIO_XFER_TYPE_WRITE)
    {
        klog("cyhal_sdio_bulk_transfer(..., %u, %x, %p, %u) - RW mismatch\n",
            direction, argument, data, length);
        return CY_FAIL;
    }
    if(length != (uint16_t)(byte_block_count * (is_block ? 64U : 1U)))
    {
        klog("cyhal_sdio_bulk_transfer(..., %u, %x, %p, %u) - length mismatch\n",
            direction, argument, data, length);
        return CY_FAIL;
    }

    uintptr_t _addr = (uintptr_t)data;
    bool can_dma = ((_addr & 63) == 0) && ((length & 63) == 0);
    if(can_dma && is_write)
    {
        // TODO: clean cache
        // TODO: get physical address of buffer
        can_dma = false;
    }
    
    sdmmc[1].iface->DCTRL = 0;
    sdmmc[1].iface->DLEN = length;
    sdmmc[1].iface->DCTRL = 
        (is_write ? 0UL : SDMMC_DCTRL_DTDIR) |
        (is_block ? ((6U << SDMMC_DCTRL_DBLOCKSIZE_Pos) |       // TODO: check block size
            (0U << SDMMC_DCTRL_DTMODE_Pos)) :
            (1UL << SDMMC_DCTRL_DTMODE_Pos)) |
        SDMMC_DCTRL_SDIOEN;
    sdmmc[1].iface->DTIMER = 0xffffffffU;
    sdmmc[1].iface->CMD = 0;
    sdmmc[1].iface->CMD = SDMMC_CMD_CMDTRANS;
    if(iten)
    {
        sdmmc[1].iface->MASK |= SDMMC_MASK_SDIOITIE;
    }
    else
    {
        sdmmc[1].iface->MASK &= ~SDMMC_MASK_SDIOITIE;
    }

    if(can_dma)
    {
        sdmmc[1].iface->IDMABASER = (uint32_t)_addr;
        sdmmc[1].iface->IDMACTRL = SDMMC_IDMA_IDMAEN;
    }
    else
    {
        sdmmc[1].iface->IDMACTRL = 0;
    }

    auto ret = sdmmc[1].sd_issue_command(53, SDIF::resp_type::R5, argument, response, true);
    if(ret != 0)
    {
        // fail
        return CY_FAIL;
    }

    if(!can_dma)
    {
        // polling transfer
        sdmmc[1].iface->MASK &= ~SDMMC_MASK_DATAENDIE;

        while(length)
        {
            if(is_write)
            {
                while(sdmmc[1].iface->STA & SDMMC_STA_TXFIFOF);
                sdmmc[1].iface->FIFO = *data++;
            }
            else
            {
                while(sdmmc[1].iface->STA & SDMMC_STA_RXFIFOE);
                *(uint32_t *)data++ = sdmmc[1].iface->FIFO;
            }

            length -= std::min(length, (uint16_t)4U);
        }

        while(!(sdmmc[1].iface->STA & SDMMC_STA_DATAEND))
        {
            if(!is_write)
                (void)sdmmc[1].iface->FIFO;
        }
        sdmmc[1].iface->ICR = SDMMC_STA_DATAEND;

        return CY_RSLT_SUCCESS;
    }
    else
    {
        // idma transfer
        klog("idma not implemented\n");
    }

    klog("cyhal_sdio_bulk_transfer(..., %u, %x, %p, %u)\n",
        direction, argument, data, length);
    return CY_FAIL;
}

cy_rslt_t cyhal_gpio_init(cyhal_gpio_t pin, cyhal_gpio_direction_t direction, cyhal_gpio_drive_mode_t drive_mode, bool init_val)
{
    // only handle M2_WIFI_WAKE as input
    if(pin == 0x6 && direction == cyhal_gpio_direction_t::CYHAL_GPIO_DIR_INPUT)
    {
        M2_WIFI_WAKE.set_as_input();
        return CY_RSLT_SUCCESS;
    }

    klog("cyhal_gpio_init(%u, %u, %u, %s)\n",
        pin, direction, drive_mode, init_val ? "true" : "false");
    return CY_FAIL;
}

static cyhal_gpio_callback_data_t M2_WIFI_WAKE_cb = { 0 };

static void exti1_6_handler(exception_regs *, uint64_t)
{
    auto rise = (EXTI1_VMEM->RPR1 & (1U << 6)) != 0;
    auto fall = (EXTI1_VMEM->FPR1 & (1U << 6)) != 0;
	EXTI1_VMEM->RPR1 = (1U << 6);
    EXTI1_VMEM->FPR1 = (1U << 6);

    klog("exti1_6\n");

    if(M2_WIFI_WAKE_cb.callback)
    {
        M2_WIFI_WAKE_cb.callback(M2_WIFI_WAKE_cb.callback_arg,
            (cyhal_gpio_event_t)
            ((rise ? cyhal_gpio_event_t::CYHAL_GPIO_IRQ_RISE : 0) |
            (fall ? cyhal_gpio_event_t::CYHAL_GPIO_IRQ_FALL : 0)));
    }
}

void cyhal_gpio_register_callback(cyhal_gpio_t pin, cyhal_gpio_callback_data_t* callback_data)
{
    // only handle GPIOA,6
    if(pin == 0x6)
    {
        M2_WIFI_WAKE_cb = *callback_data;
    }
    else
    {
        klog("cyhal_gpio_register_callback\n");
    }
}

void cyhal_gpio_enable_event(cyhal_gpio_t pin, cyhal_gpio_event_t event, uint8_t intr_priority, bool enable)
{
    if(pin == 0x6)
    {
        EXTI1_VMEM->EXTICR[1] = (EXTI1_VMEM->EXTICR[1] & ~EXTI_EXTICR2_EXTI6_Msk) |
            (0U << EXTI_EXTICR2_EXTI6_Pos);

        if(enable)
        {
            switch(event)
            {
                case cyhal_gpio_event_t::CYHAL_GPIO_IRQ_BOTH:
                    EXTI1_VMEM->RTSR1 |= (1U << 6);
                    EXTI1_VMEM->FTSR1 |= (1U << 6);
                    break;
                case cyhal_gpio_event_t::CYHAL_GPIO_IRQ_RISE:
                    EXTI1_VMEM->RTSR1 |= (1U << 6);
                    EXTI1_VMEM->FTSR1 &= ~(1U << 6);
                    break;
                case cyhal_gpio_event_t::CYHAL_GPIO_IRQ_FALL:
                    EXTI1_VMEM->RTSR1 &= ~(1U << 6);
                    EXTI1_VMEM->FTSR1 |= (1U << 6);
                    break;
                case cyhal_gpio_event_t::CYHAL_GPIO_IRQ_NONE:
                    EXTI1_VMEM->RTSR1 &= ~(1U << 6);
                    EXTI1_VMEM->FTSR1 &= ~(1U << 6);
                    break;
            }

            gic_set_handler(306, exti1_6_handler);
            gic_set_target(306, GIC_ENABLED_CORES);
            gic_set_enable(306);
        }
        else
        {
            EXTI1_VMEM->RTSR1 &= ~(1U << 6);
            EXTI1_VMEM->FTSR1 &= ~(1U << 6);
        }
    }
    klog("cyhal_gpio_register_event(..., %u, %u, %s)\n",
        event, intr_priority, enable ? "enable" : "disable");
}

void cyhal_gpio_free(cyhal_gpio_t pin)
{
    klog("cyhal_gpio_free\n");
}

void SDMMC2_IRQHandler(exception_regs *, uint64_t)
{
    sdmmc[1].IRQ();
}
