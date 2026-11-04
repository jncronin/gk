#include "oled.h"
#include "i2c.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstring>

#include "semphr.h"

#include "interface/cm33_data.h"
#include "guard.h"

extern SemaphoreHandle_t m_i2c1;
extern cm33_data_kernel dk;

static bool oled_is_init = false;

static void oled_task(void *);

static unsigned int oleds_available = 0;

static TaskHandle_t oled_thread;

static const char ch1115_init[] = 
{
    0x00,       // begin command
    0xae,
    0xd5, 0x80,
    0xa8, 0x3f,
    0xd3, 0x00,
    0x40,
    0xad, 0x8b,
    0xa1,
    0xc8,
    0xda, 0x12,
    0x81, 0x7f,
    0xd9, 0x22,
    0xdb, 0x20,
    0xa4,
    0xa6
};

static const char ch1115_on[] =
{
    0x00,
    0xaf
};

static const char ch1115_off[] =
{
    0x00,
    0xae
};

static int oled_enable_int(int en);

void init_oled()
{
    xTaskCreate(oled_task, "oled", 2048, nullptr, configMAX_PRIORITIES - 1,
        &oled_thread);
}

static int oled_nres(int val)
{
    auto &i2c1 = i2c(1);
    char i2cmsg = val ? 0x80u : 0x00u;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x03, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);

    if(!val)
    {
        oled_is_init = false;
        UninterruptibleGuard ug;
        dk.sr &= ~CM33_DK_SR_OLED_ENABLED;
    }
    return (i2cret == 1) ? 0 : -1;
}

static int oled_reset()
{
    oleds_available = 0;
    oled_is_init = false;

    klog("oled: begin reset\n");

    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    // Set OLED_NRES as output
    auto &i2c1 = i2c(1);
    char i2cmsg = 0x7f;
    auto i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x07, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("oled: PCAL6426 not responding: %d\n", i2cret);
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -1;
    }

    // Pulse it

    // Set it high, low, high
    if(oled_nres(1) != 0)
    {
        klog("oled: PCAL6426 not responding");
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if(oled_nres(0) != 0)
    {
        klog("oled: PCAL6426 not responding");
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    if(oled_nres(1) != 0)
    {
        klog("oled: PCAL6426 not responding");
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    /* check for presence of a TCA9546A on the oled daughterboard */
    i2cmsg = 0;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.Write(0x70, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("oled: TCA9546A not responding: %d\n", i2cret);
        oleds_available = 0;
        oled_nres(0);
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -2;
    }

    klog("oled: oled daughterboard found, begin probing for working oleds\n");

    for(auto i = 0u; i < 8; i++)
    {
        // Enable TCA9546A channel i - it doesn't have internal registers
        i2cmsg = 1u << i;
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.Write(0x70, &i2cmsg, 1);
        xSemaphoreGive(m_i2c1);
        if(i2cret != 1)
        {
            klog("oled: TCA9546A not responding: %d\n", i2cret);
            oleds_available = 0;
            oled_nres(0);
            return -2;
        }

        // Init OLED CH1115 with retry
        for(auto j = 0u; j < 3; j++)
        {
            xSemaphoreTake(m_i2c1, portMAX_DELAY);
            i2cret = i2c1.Write(0x3c, ch1115_init, sizeof(ch1115_init));
            xSemaphoreGive(m_i2c1);
            if(i2cret != sizeof(ch1115_init))
                vTaskDelay(pdMS_TO_TICKS(10));
            else
            {
                oleds_available |= 1u << i;
                break;
            }
        }
    }

    klog("oled: initialized oleds at addresses: %u\n", oleds_available);

    oled_is_init = true;

    UninterruptibleGuard ug;
    if(oleds_available)
        dk.sr |= CM33_DK_SR_OLED_AVAILABLE;
    else
        dk.sr &= ~CM33_DK_SR_OLED_AVAILABLE;
    dk.sr &= ~CM33_DK_SR_OLED_ENABLED;

    return 0;
}

static int oled_write_screen(unsigned int scr_id, const void *scr_data, bool enable = true)
{
    // Enable TCA9546A channel i - it doesn't have internal registers
    auto &i2c1 = i2c(1);

    auto i2cmsg = 1u << scr_id;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto i2cret = i2c1.Write(0x70, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    if(i2cret != 1)
    {
        klog("oled: TCA9546A not responding: %d\n", i2cret);
        oleds_available = 0;
        oled_nres(0);
        UninterruptibleGuard ug;
        dk.sr &= ~(CM33_DK_SR_OLED_AVAILABLE | CM33_DK_SR_OLED_AVAILABLE);
        return -2;
    }

    uint8_t page_cmd[6];
    page_cmd[0] = 0x80; // begin command
    page_cmd[1] = 0xb0; // page
    page_cmd[2] = 0x80; // begin command
    page_cmd[3] = 0x00; // low column = 0
    page_cmd[4] = 0x80; // begin command
    page_cmd[5] = 0x10; // high column = 0

    for(auto i = 0u; i < 8u; i++)
    {
        // send page by page
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.Write(0x3c, page_cmd, sizeof(page_cmd));
        xSemaphoreGive(m_i2c1);
        if(i2cret != sizeof(page_cmd))
        {
            klog("oled: page_cmd: %d\n", i2cret);
            return -1;
        }

        // send the data
        auto data = (const void *)(&((const uint8_t *)scr_data)[128 * i]);
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.RegisterWrite(0x3c, (uint8_t)0x40 /* data comes next */,
            data, 128);
        xSemaphoreGive(m_i2c1);
        if(i2cret != 128)
        {
            klog("oled: data: %d\n", i2cret);
            return -1;
        }

        // update page
        page_cmd[1] = page_cmd[1] + 1;
    }

    if(enable)
    {
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.Write(0x3c, ch1115_on, sizeof(ch1115_on));
        xSemaphoreGive(m_i2c1);
    }

    return 0;
}

void oled_task(void *)
{
    /* First check for presence of oleds then power down */
    oled_reset();
    oled_nres(0);

    while(true)
    {
        uint32_t cmd;
        if(xTaskNotifyWait(0, 0, &cmd, portMAX_DELAY) == pdTRUE)
        {
            oled_enable_int(cmd ? 1 : 0);
        }
    }
}

int oled_enable_int(int en)
{
    if(!en)
    {
        return oled_nres(0);
    }

    if(!oled_is_init)
    {
        auto res_ret = oled_reset();
        if(res_ret != 0)
            return res_ret;
    }

    uint32_t scrs_to_enable = 0;
    {
        UninterruptibleGuard ug;
        scrs_to_enable = oleds_available | dk.oleds_to_enable;
    }

    int nsuccess = 0;
    for(auto i = 0u; i < 8u; i++)
    {
        if(!(scrs_to_enable & (1u << i)))
            continue;
        if(oled_write_screen(i, (const void *)dk.oled_img[i]) == 0)
            nsuccess++;
    }

    {
        UninterruptibleGuard ug;
        if(nsuccess)
            dk.sr |= CM33_DK_SR_OLED_ENABLED;
        else
            dk.sr &= ~CM33_DK_SR_OLED_ENABLED;
    }

    return nsuccess;
}

int oled_enable(int en)
{
    if(!oled_thread)
        return -1;

    xTaskNotify(oled_thread, en ? 1U : 0U, eSetValueWithOverwrite);

    return 0;
}
