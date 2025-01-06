#include "i2c.h"
#include "LSM6DSL_ACC_GYRO_Driver.h"
#include "scheduler.h"
#include "thread.h"
#include "SEGGER_RTT.h"
#include <cmath>
#include "tilt.h"
#include "process.h"

extern Process *focus_process;

// see https://github.com/stm32duino/LSM6DSL/blob/main/src/LSM6DSLSensor.cpp for an example

static bool _init_lsm();
static SRAM4_DATA volatile bool is_running = false;
static SRAM4_DATA volatile bool needs_calib = false;
static SRAM4_DATA volatile float calib_y = 0;
static bool Set_X_FS(float fullScale);
static bool Set_G_FS(float fullScale);
static SRAM4_DATA BinarySemaphore s_en;

// d6
static const constexpr unsigned int addr = 0x6a;

static const constexpr unsigned int f_samp = 25;    // tilt sample frequency
static const constexpr int fsr_deg = 80;            // +/- maximum tilt
static const constexpr int deadzone_deg = 10;       // +/- deadzone
static const constexpr int move_deg = 2;            // number of degrees needed to move to report a new sample
extern int tilt_pos[2];

Process p_tilt;

void *lsm_thread(void *param)
{
    bool is_init = false;
    bool last_running = false;
    float last_x = 0;
    float last_y = 0;

    while(true)
    {
        if(is_running)
        {
            last_running = true;
            while(!is_init)
            {
                is_init = _init_lsm();

                if(is_init)
                {
                    // check whoami
                    uint8_t who;
                    LSM6DSL_ACC_GYRO_R_WHO_AM_I(nullptr, &who);
                    {
                        klog("lsm: whoami: %x\n", (uint32_t)who);
                    }
                    if(who != 0x6a)
                        is_init = false;
                    else
                    {
                        // enable
                        //LSM6DSL_ACC_GYRO_W_ODR_G(nullptr, LSM6DSL_ACC_GYRO_ODR_G_104Hz);
                        LSM6DSL_ACC_GYRO_W_ODR_XL(nullptr, LSM6DSL_ACC_GYRO_ODR_XL_104Hz);
                    }
                }
                else
                {
                    Block(clock_cur() + kernel_time::from_ms(1000));
                }
            }

            int acc[3] = { 0 };
            if(LSM6DSL_ACC_Get_Acceleration(nullptr, acc, 0) == MEMS_SUCCESS)
            {
                // calc pitch and roll
                auto fx = (float)acc[0];
                auto fy = (float)acc[1];
                auto fz = (float)acc[2];
                auto g = sqrtf(fx * fx + fy * fy + fz * fz);
                auto pitch = 180.0f * asinf(fx / g) / (float)M_PI;
                auto roll = 180.0f * atan2f(fy, fz) / (float)M_PI;
                auto x = -pitch;
                auto y = -roll;

                if(needs_calib)
                {
                    calib_y = y;
                }

                y -= calib_y;

                if(std::abs(x) < (float)deadzone_deg) x = 0;
                if(std::abs(y) < (float)deadzone_deg) y = 0;

                auto joy_x = x * 32767.0f / (float)fsr_deg;
                auto joy_y = y * 32767.0f / (float)fsr_deg;
                tilt_pos[0] = (int)rint(joy_x);
                tilt_pos[1] = (int)rint(joy_y);

                bool to_report = false;
                if(needs_calib)
                {
                    to_report = true;
                    needs_calib = false;
                    last_x = x;
                    last_y = y;
                }
                else
                {
                    if(std::abs(x - last_x) >= (float)move_deg)
                    {
                        last_x = x;
                        to_report = true;
                    }
                    if(std::abs(y - last_y) >= (float)move_deg)
                    {
                        last_y = y;
                        to_report = true;
                    }
                }

                if(to_report)
                {
                    focus_process->HandleTiltEvent(joy_x, joy_y);

                    {
                        klog("lsm6dsl: %d,%d\n", (int)joy_x, (int)joy_y);
                    }
                }
            }

            Block(clock_cur() + kernel_time::from_ms(1000 / f_samp));
        }
        else
        {
            if(last_running)
            {
                LSM6DSL_ACC_GYRO_W_ODR_XL(nullptr, LSM6DSL_ACC_GYRO_ODR_XL_POWER_DOWN);
                last_running = false;
            }
            s_en.Wait(clock_cur() + kernel_time::from_ms(1000));
        }
    }
}

void tilt_enable(bool en)
{
    if(en)
    {
        is_running = true;
        needs_calib = true;
        s_en.Signal();
    }
    else
    {
        is_running = false;
    }
}

bool _init_lsm()
{
    /* Enable register address automatically incremented during a multiple byte
        access with a serial interface. */
    if ( LSM6DSL_ACC_GYRO_W_IF_Addr_Incr( nullptr, LSM6DSL_ACC_GYRO_IF_INC_ENABLED ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 0\n");
        }
        return false;
    }

    /* Enable BDU */
    if ( LSM6DSL_ACC_GYRO_W_BDU( nullptr, LSM6DSL_ACC_GYRO_BDU_BLOCK_UPDATE ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 1\n");
        }
        return false;
    }

    /* FIFO mode selection */
    if ( LSM6DSL_ACC_GYRO_W_FIFO_MODE( nullptr, LSM6DSL_ACC_GYRO_FIFO_MODE_BYPASS ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 2\n");
        }
        return false;
    }

    /* Output data rate selection - power down. */
    if ( LSM6DSL_ACC_GYRO_W_ODR_XL( nullptr, LSM6DSL_ACC_GYRO_ODR_XL_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 3\n");
        }
        return false;
    }

    /* Full scale selection. */
    if ( Set_X_FS( 2.0f ) == false )
    {
        {
            klog("lsm6dsl: init failed 4\n");
        }
        return false;
    }

    /* Output data rate selection - power down */
    if ( LSM6DSL_ACC_GYRO_W_ODR_G( nullptr, LSM6DSL_ACC_GYRO_ODR_G_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 5\n");
        }
        return false;
    }

    /* Full scale selection. */
    if ( Set_G_FS( 2000.0f ) == false )
    {
        {
            klog("lsm6dsl: init failed 6\n");
        }
        return false;
    }

    {
        klog("lsm6dsl: init success\n");
    }

    return true;
}

bool Set_X_FS(float fullScale)
{
    LSM6DSL_ACC_GYRO_FS_XL_t new_fs;

    new_fs = ( fullScale <= 2.0f ) ? LSM6DSL_ACC_GYRO_FS_XL_2g
            : ( fullScale <= 4.0f ) ? LSM6DSL_ACC_GYRO_FS_XL_4g
            : ( fullScale <= 8.0f ) ? LSM6DSL_ACC_GYRO_FS_XL_8g
            :                         LSM6DSL_ACC_GYRO_FS_XL_16g;

    if ( LSM6DSL_ACC_GYRO_W_FS_XL( nullptr, new_fs ) == MEMS_ERROR )
    {
        return false;
    }

    return true;
}

/**
 * @brief  Set LSM6DSL Gyroscope full scale
 * @param  fullScale the full scale to be set
 * @retval LSM6DSL_STATUS_OK in case of success, an error code otherwise
 */
bool Set_G_FS(float fullScale)
{
    LSM6DSL_ACC_GYRO_FS_G_t new_fs;

    if ( fullScale <= 125.0f )
    {
        if ( LSM6DSL_ACC_GYRO_W_FS_125( nullptr, LSM6DSL_ACC_GYRO_FS_125_ENABLED ) == MEMS_ERROR )
        {
            return false;
        }
    }
    else
    {
        new_fs = ( fullScale <=  245.0f ) ? LSM6DSL_ACC_GYRO_FS_G_245dps
                : ( fullScale <=  500.0f ) ? LSM6DSL_ACC_GYRO_FS_G_500dps
                : ( fullScale <= 1000.0f ) ? LSM6DSL_ACC_GYRO_FS_G_1000dps
                :                            LSM6DSL_ACC_GYRO_FS_G_2000dps;

        if ( LSM6DSL_ACC_GYRO_W_FS_125( nullptr, LSM6DSL_ACC_GYRO_FS_125_DISABLED ) == MEMS_ERROR )
        {
            return false;
        }
        if ( LSM6DSL_ACC_GYRO_W_FS_G( nullptr, new_fs ) == MEMS_ERROR )
        {
            return false;
        }
    }

    return true;
}

extern "C" uint8_t LSM6DSL_IO_Write(void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite)
{
    return i2c_register_write(addr, WriteAddr, pBuffer, nBytesToWrite) > 0 ? 0 : 1;
}

extern "C" uint8_t LSM6DSL_IO_Read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead)
{
    return i2c_register_read(addr, ReadAddr, pBuffer, nBytesToRead) > 0 ? 0 : 1;
}

void init_tilt()
{
    p_tilt.stack_preference = STACK_PREFERENCE_SDRAM_RAM_TCM;
    p_tilt.argc = 0;
    p_tilt.argv = nullptr;
    p_tilt.brk = 0;
    p_tilt.code_data = InvalidMemregion();
    p_tilt.cwd = "/";
    p_tilt.default_affinity = PreferM4;
    p_tilt.for_deletion = false;
    p_tilt.heap = InvalidMemregion();
    p_tilt.name = "tilt";
    p_tilt.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_tilt.open_files[i] = nullptr;
    p_tilt.screen_h = 480;
    p_tilt.screen_w = 640;
    memcpy(p_tilt.p_mpu, mpu_default, sizeof(mpu_default));

    Schedule(Thread::Create("tilt", lsm_thread, nullptr, true, GK_PRIORITY_HIGH, p_tilt,
        CPUAffinity::PreferM4));
}
