#include "oled.h"
#include "i2c.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstring>

#include "semphr.h"

extern SemaphoreHandle_t m_i2c1;

static void oled_task(void *);

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
    0xa6,
    0xaf
};

void init_oled()
{
    xTaskCreate(oled_task, "oled", 2048, nullptr, configMAX_PRIORITIES - 1,
        nullptr);
}

int oled_write_screen(const void *scr_data)
{
    uint8_t page_cmd[6];
    page_cmd[0] = 0x80; // begin command
    page_cmd[1] = 0xb0; // page
    page_cmd[2] = 0x80; // begin command
    page_cmd[3] = 0x00; // low column = 0
    page_cmd[4] = 0x80; // begin command
    page_cmd[5] = 0x10; // high column = 0

    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    auto &i2c1 = i2c(1);
    for(auto i = 0u; i < 8u; i++)
    {
        // send page by page
        auto i2cret = i2c1.Write(0x3c, page_cmd, sizeof(page_cmd));
        klog("oled: page_cmd: %d\n", i2cret);
        if(i2cret != sizeof(page_cmd))
        {
            xSemaphoreGive(m_i2c1);
            return -1;
        }

        //vTaskDelay(pdMS_TO_TICKS(10));

        // send the data
        auto data = (const void *)(&((const uint8_t *)scr_data)[128 * i]);
        i2cret = i2c1.RegisterWrite(0x3c, (uint8_t)0x40 /* data comes next */,
            data, 128);
        klog("oled: data: %d\n", i2cret);
        if(i2cret != 128)
        {
            xSemaphoreGive(m_i2c1);
            return -1;
        }

        // update page
        page_cmd[1] = page_cmd[1] + 1;
    }
    xSemaphoreGive(m_i2c1);

    return 0;
}

static uint8_t test_data[1024];

void oled_task(void *)
{
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    // Set OLED_NRES as output
    auto &i2c1 = i2c(1);
    char i2cmsg = 0x7f;
    auto i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x07, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    klog("A: %d\n", i2cret);

    // Pulse it

    // Set it high, low, high
    i2cmsg = 0x80;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x03, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    klog("B1: %d\n", i2cret);
    vTaskDelay(pdMS_TO_TICKS(10));
    i2cmsg = 0x00;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x03, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    klog("B2: %d\n", i2cret);
    vTaskDelay(pdMS_TO_TICKS(50));
    i2cmsg = 0x80;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.RegisterWrite(0x20, (uint8_t)0x03, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    klog("B3: %d\n", i2cret);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enable TCA9546A ch1 - it doesn't have internal registers
    i2cmsg = 0xf;
    xSemaphoreTake(m_i2c1, portMAX_DELAY);
    i2cret = i2c1.Write(0x70, &i2cmsg, 1);
    xSemaphoreGive(m_i2c1);
    klog("C: %d\n", i2cret);

    // Init OLED CH1115
    while(i2cret != sizeof(ch1115_init))
    {
        xSemaphoreTake(m_i2c1, portMAX_DELAY);
        i2cret = i2c1.Write(0x3c, ch1115_init, sizeof(ch1115_init));
        xSemaphoreGive(m_i2c1);
        klog("D: %d\n", i2cret);
        if(i2cret != sizeof(ch1115_init))
            vTaskDelay(pdMS_TO_TICKS(10));
    }


    /* test data - checkerboard 2x2 */
    for(auto i = 0u; i < 1024u; i++)
    {
        test_data[i] = ((i & 0x3) < 0x2) ? 0x33u : 0xccu;
    }
    while(oled_write_screen(test_data) != 0)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
