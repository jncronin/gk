#include "i2c.h"
#include "LSM6DSL_ACC_GYRO_Driver.h"
#include "scheduler.h"
#include "thread.h"
#include "SEGGER_RTT.h"
#include <cmath>

// see https://github.com/stm32duino/LSM6DSL/blob/main/src/LSM6DSLSensor.cpp for an example

static bool init_lsm();
static SRAM4_DATA volatile bool is_running = true;
static bool Set_X_FS(float fullScale);
static bool Set_G_FS(float fullScale);

// d6
static const constexpr unsigned int addr = 0x6a;

extern Spinlock s_rtt;

void *lsm_thread(void *param)
{
    bool is_init = false;
    while(true)
    {
        if(is_running)
        {
            while(!is_init)
            {
                is_init = init_lsm();

                if(is_init)
                {
                    // check whoami
                    uint8_t who;
                    LSM6DSL_ACC_GYRO_R_WHO_AM_I(nullptr, &who);
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "lsm: whoami: %x\n", (uint32_t)who);
                    }
                    if(who != 0x6a)
                        is_init = false;
                    else
                    {
                        // enable
                        LSM6DSL_ACC_GYRO_W_ODR_G(nullptr, LSM6DSL_ACC_GYRO_ODR_G_104Hz);
                        LSM6DSL_ACC_GYRO_W_ODR_XL(nullptr, LSM6DSL_ACC_GYRO_ODR_XL_104Hz);
                    }
                }
            }

            int acc[3] = { 0 }, gy[3] = { 0 };
            LSM6DSL_ACC_Get_Acceleration(nullptr, acc, 0);
            LSM6DSL_ACC_Get_AngularRate(nullptr, gy, 0);

            LSM6DSL_ACC_GYRO_ODR_G_t godr;
            LSM6DSL_ACC_GYRO_R_ODR_G(nullptr, &godr);

            // calc pitch and roll
            auto fx = (float)acc[0];
            auto fy = (float)acc[1];
            auto fz = (float)acc[2];
            auto g = sqrtf(fx * fx + fy * fy + fz * fz);
            auto pitch = 180.0f * asinf(fx / g) / (float)M_PI;
            auto roll = 180.0f * atan2f(fy, fz) / (float)M_PI;

            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "lsm6dsl: %d,%d,%d  %d,%d,%d\n", acc[0], acc[1], acc[2],
                    gy[0], gy[1], gy[2]);
                SEGGER_RTT_printf(0, "lsm6dsl: godr: %d\n", (int)godr);
                SEGGER_RTT_printf(0, "lsm6sdl: pitch %s, roll %s, g %s\n",
                    std::to_string(pitch).c_str(),
                    std::to_string(roll).c_str(),
                    std::to_string(g).c_str());
            }

            Block(clock_cur() + kernel_time::from_ms(100));


        }
        else
        {
            // not running
            Block(clock_cur() + kernel_time::from_ms(1000));
        }
    }
}

bool init_lsm()
{
    /* Enable register address automatically incremented during a multiple byte
        access with a serial interface. */
    if ( LSM6DSL_ACC_GYRO_W_IF_Addr_Incr( nullptr, LSM6DSL_ACC_GYRO_IF_INC_ENABLED ) == MEMS_ERROR )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 0\n");
        }
        return false;
    }

    /* Enable BDU */
    if ( LSM6DSL_ACC_GYRO_W_BDU( nullptr, LSM6DSL_ACC_GYRO_BDU_BLOCK_UPDATE ) == MEMS_ERROR )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 1\n");
        }
        return false;
    }

    /* FIFO mode selection */
    if ( LSM6DSL_ACC_GYRO_W_FIFO_MODE( nullptr, LSM6DSL_ACC_GYRO_FIFO_MODE_BYPASS ) == MEMS_ERROR )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 2\n");
        }
        return false;
    }

    /* Output data rate selection - power down. */
    if ( LSM6DSL_ACC_GYRO_W_ODR_XL( nullptr, LSM6DSL_ACC_GYRO_ODR_XL_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 3\n");
        }
        return false;
    }

    /* Full scale selection. */
    if ( Set_X_FS( 2.0f ) == false )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 4\n");
        }
        return false;
    }

    /* Output data rate selection - power down */
    if ( LSM6DSL_ACC_GYRO_W_ODR_G( nullptr, LSM6DSL_ACC_GYRO_ODR_G_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 5\n");
        }
        return false;
    }

    /* Full scale selection. */
    if ( Set_G_FS( 2000.0f ) == false )
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "lsm6dsl: init failed 6\n");
        }
        return false;
    }

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "lsm6dsl: init success\n");
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
    return i2c_register_write(addr, WriteAddr, pBuffer, nBytesToWrite) == 0 ? 0 : 1;
}

extern "C" uint8_t LSM6DSL_IO_Read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead)
{
    return i2c_register_read(addr, ReadAddr, pBuffer, nBytesToRead) == 0 ? 0 : 1;
}

