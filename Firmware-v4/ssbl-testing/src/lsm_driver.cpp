#include "i2c.h"
#include "LSM6DSL_ACC_GYRO_Driver.h"
#include "logger.h"
#include "pins.h"
#include "clocks.h"
#include "vmem.h"
#include "ESKF.h"
#include "kernel_time.h"

using filter_precision = float;
static IMU_EKF::ESKF<filter_precision> filter;
static bool filter_init = false;
static kernel_time last_filter = kernel_time_invalid();

// see https://github.com/stm32duino/LSM6DSL/blob/main/src/LSM6DSLSensor.cpp for an example

static bool is_init = false;

static void lsm_reset();

static const constexpr pin LSM6DSL_EN { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOD_BASE) , 9 };
static const constexpr unsigned int addr = 0x6a;

void init_lsm()
{
    is_init = false;
    filter_init = false;
    LSM6DSL_EN.clear();
    LSM6DSL_EN.set_as_output();
    LSM6DSL_EN.set();
}

void lsm_poll()
{
    if(!is_init)
    {
        lsm_reset();
    }

    uint8_t who;
    LSM6DSL_ACC_GYRO_R_WHO_AM_I(nullptr, &who);
    if(who != 0x6a)
    {
        klog("lsm: whoami: %x\n", (uint32_t)who);
        is_init = false;
        return;
    }

    LSM6DSL_ACC_GYRO_XLDA_t da;
    if(LSM6DSL_ACC_GYRO_R_XLDA(nullptr, &da) == MEMS_SUCCESS)
    {
        if(da == LSM6DSL_ACC_GYRO_XLDA_DATA_AVAIL)
        {
            if(filter_init)
            {
                auto now = clock_cur();
                auto dt = now - last_filter;
                auto fdt = (filter_precision)kernel_time_to_us(dt) * (filter_precision)1000000.0;
                filter.predict(fdt);
                last_filter = now;
            }

            int acc[3] = { 0 };
            if(LSM6DSL_ACC_Get_AngularRate(nullptr, acc, 0) == MEMS_SUCCESS)
            {
                klog("lsm: gyr: %d, %d, %d\n", acc[0], acc[1], acc[2]);

                if(filter_init)
                {
                    filter.correctGyr((filter_precision)acc[0] / (filter_precision)1000000.0,
                        (filter_precision)acc[1] / (filter_precision)1000000.0,
                        (filter_precision)acc[2] / (filter_precision)1000000.0);
                }
            }
            else
            {
                is_init = false;
                filter_init = false;
                return;
            }
            if(LSM6DSL_ACC_Get_Acceleration(nullptr, acc, 0) == MEMS_SUCCESS)
            {
                klog("lsm: acc: %d, %d, %d\n", acc[0], acc[1], acc[2]);

                if(filter_init)
                {
                    filter.correctAcc((filter_precision)acc[0] / (filter_precision)1000000.0,
                        (filter_precision)acc[1] / (filter_precision)1000000.0,
                        (filter_precision)acc[2] / (filter_precision)1000000.0);
                }
                else
                {
                    filter.initWithAcc((filter_precision)acc[0] / (filter_precision)1000000.0,
                        (filter_precision)acc[1] / (filter_precision)1000000.0,
                        (filter_precision)acc[2] / (filter_precision)1000000.0);
                    filter_init = true;
                }
            }
            else
            {
                is_init = false;
                filter_init = false;
                return;
            }

            if(filter_init)
            {
                filter_precision out[3];
                filter.getAttitude(out[0], out[1], out[2]);

                // for want of klog("%f")...
                int roll = (int)out[0];
                int roll_frac = (int)(out[0] * (filter_precision)1000000.0) - (roll * 1000000);
                int pitch = (int)out[1];
                int pitch_frac = (int)(out[1] * (filter_precision)1000000.0) - (pitch * 1000000);
                int yaw = (int)out[2];
                int yaw_frac = (int)(out[2] * (filter_precision)1000000.0) - (yaw * 1000000);

                klog("lsm: flt: %d.%06d, %d.%06d, %d.%06d\n",
                    roll, roll_frac, pitch, pitch_frac, yaw, yaw_frac);
            }
        }
    }
    else
    {
        is_init = false;
        return;
    }
}

bool Set_X_FS(int fullScale)
{
    LSM6DSL_ACC_GYRO_FS_XL_t new_fs;

    new_fs = ( fullScale <= 2 ) ? LSM6DSL_ACC_GYRO_FS_XL_2g
            : ( fullScale <= 4 ) ? LSM6DSL_ACC_GYRO_FS_XL_4g
            : ( fullScale <= 8 ) ? LSM6DSL_ACC_GYRO_FS_XL_8g
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
bool Set_G_FS(int fullScale)
{
    LSM6DSL_ACC_GYRO_FS_G_t new_fs;

    if ( fullScale <= 125 )
    {
        if ( LSM6DSL_ACC_GYRO_W_FS_125( nullptr, LSM6DSL_ACC_GYRO_FS_125_ENABLED ) == MEMS_ERROR )
        {
            return false;
        }
    }
    else
    {
        new_fs = ( fullScale <=  245 ) ? LSM6DSL_ACC_GYRO_FS_G_245dps
                : ( fullScale <=  500 ) ? LSM6DSL_ACC_GYRO_FS_G_500dps
                : ( fullScale <= 1000 ) ? LSM6DSL_ACC_GYRO_FS_G_1000dps
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
    auto &i2c1 = i2c(1);
    return i2c1.RegisterWrite(addr, WriteAddr, pBuffer, nBytesToWrite) > 0 ? 0 : 1;
}

extern "C" uint8_t LSM6DSL_IO_Read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead)
{
    auto &i2c1 = i2c(1);
    return i2c1.RegisterRead(addr, ReadAddr, pBuffer, nBytesToRead) > 0 ? 0 : 1;
}

void lsm_reset()
{
    filter_init = false;

    LSM6DSL_EN.clear();
    udelay(5000);
    LSM6DSL_EN.set();
    udelay(5000);

    /* Enable register address automatically incremented during a multiple byte
        access with a serial interface. */
    if ( LSM6DSL_ACC_GYRO_W_IF_Addr_Incr( nullptr, LSM6DSL_ACC_GYRO_IF_INC_ENABLED ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 0\n");
        }
        return;
    }

    /* Enable BDU */
    if ( LSM6DSL_ACC_GYRO_W_BDU( nullptr, LSM6DSL_ACC_GYRO_BDU_BLOCK_UPDATE ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 1\n");
        }
        return;
    }

    /* FIFO mode selection */
    if ( LSM6DSL_ACC_GYRO_W_FIFO_MODE( nullptr, LSM6DSL_ACC_GYRO_FIFO_MODE_BYPASS ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 2\n");
        }
        return;
    }

    /* Output data rate selection - power down. */
    if ( LSM6DSL_ACC_GYRO_W_ODR_XL( nullptr, LSM6DSL_ACC_GYRO_ODR_XL_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 3\n");
        }
        return;
    }

    /* Full scale selection. */
    if ( Set_X_FS( 2 ) == false )
    {
        {
            klog("lsm6dsl: init failed 4\n");
        }
        return;
    }

    /* Output data rate selection - power down */
    if ( LSM6DSL_ACC_GYRO_W_ODR_G( nullptr, LSM6DSL_ACC_GYRO_ODR_G_POWER_DOWN ) == MEMS_ERROR )
    {
        {
            klog("lsm6dsl: init failed 5\n");
        }
        return;
    }

    /* Full scale selection. */
    if ( Set_G_FS( 2000 ) == false )
    {
        {
            klog("lsm6dsl: init failed 6\n");
        }
        return;
    }

    /* Enable sampling */
    if(LSM6DSL_ACC_GYRO_W_ODR_XL(nullptr, LSM6DSL_ACC_GYRO_ODR_XL_26Hz) == MEMS_ERROR)
    {
        klog("lsm6dsl: init failed 7\n");
        return;
    }

    if(LSM6DSL_ACC_GYRO_W_ODR_G(nullptr, LSM6DSL_ACC_GYRO_ODR_G_26Hz) == MEMS_ERROR)
    {
        klog("lsm6dsl: init failed 7\n");
        return;
    }

    {
        klog("lsm6dsl: init success\n");
    }
    is_init = true;
}
