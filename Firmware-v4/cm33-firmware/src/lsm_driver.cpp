#include "i2c.h"
#include "LSM6DSL_ACC_GYRO_Driver.h"
#include "pins.h"
#include "clocks.h"
#include "kernel_time.h"
#include "Fusion.h"
#include "interface/cm33_data.h"

using filter_precision = float;
static kernel_time last_filter = kernel_time_invalid();
static FusionAhrs filter;
static int lsm_reset_state = 0;

// see https://github.com/stm32duino/LSM6DSL/blob/main/src/LSM6DSLSensor.cpp for an example

static bool is_init = false;

static int lsm_reset();

static const constexpr pin LSM6DSL_EN { GPIOD, 9 };
static const constexpr unsigned int addr = 0x6a;

extern cm33_data_userspace d;

void init_lsm()
{
    is_init = false;
    LSM6DSL_EN.clear();
    LSM6DSL_EN.set_as_output();

    FusionAhrsInitialise(&filter);
}

void lsm_disable()
{
    LSM6DSL_EN.clear();
    is_init = false;
    lsm_reset_state = 0;
}

int lsm_poll()
{
    if(!is_init)
    {
        auto ret = lsm_reset();
        if(ret != 0)
            return ret;
    }

    uint8_t who;
    LSM6DSL_ACC_GYRO_R_WHO_AM_I(nullptr, &who);
    if(who != 0x6a)
    {
        is_init = false;
        return -10;
    }

    LSM6DSL_ACC_GYRO_XLDA_t da;
    if(LSM6DSL_ACC_GYRO_R_XLDA(nullptr, &da) == MEMS_SUCCESS)
    {
        if(da == LSM6DSL_ACC_GYRO_XLDA_DATA_AVAIL)
        {
            auto now = clock_cur();
            auto dt = now - last_filter;
            auto fdt = (filter_precision)kernel_time_to_us(dt) / (filter_precision)1000000.0;
            last_filter = now;

            int iacc[3] = { 0 };
            int igyr[3] = { 0 };
            if(LSM6DSL_ACC_Get_AngularRate(nullptr, igyr, 0) == MEMS_SUCCESS)
            {
                //klog("lsm: gyr: %d, %d, %d\n", gyr[0], gyr[1], gyr[2]);
            }
            else
            {
                is_init = false;
                return -11;
            }
            if(LSM6DSL_ACC_Get_Acceleration(nullptr, iacc, 0) == MEMS_SUCCESS)
            {
                //klog("lsm: acc: %d, %d, %d\n", acc[0], acc[1], acc[2]);
            }
            else
            {
                is_init = false;
                return -12;
            }

            FusionVector facc {
                (float)iacc[0] / 1000.0f,
                (float)iacc[1] / 1000.0f,
                (float)iacc[2] / 1000.0f
            };
            FusionVector fgyr {
                (float)igyr[0] / 1000.0f,
                (float)igyr[1] / 1000.0f,
                (float)igyr[2] / 1000.0f
            };

            d.acc[0] = facc.array[0];
            d.acc[1] = facc.array[1];
            d.acc[2] = facc.array[2];
            d.gyr[0] = fgyr.array[0];
            d.gyr[1] = fgyr.array[1];
            d.gyr[2] = fgyr.array[2];
            
            FusionAhrsUpdateNoMagnetometer(&filter, fgyr, facc, fdt);
            auto out = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&filter));

            d.yaw = out.angle.yaw;
            d.pitch = out.angle.pitch;
            d.roll = out.angle.roll;
        }
    }
    else
    {
        is_init = false;
        return -13;
    }

    return 0;
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

int lsm_reset()
{
    // run a state machine here to handle the required 5 ms delays
    if(lsm_reset_state == 0)
    {
        FusionAhrsReset(&filter);
        
        LSM6DSL_EN.clear();
        lsm_reset_state++;
        return 1;       // wait 5 ms
    }
    else if(lsm_reset_state == 1)
    {
        LSM6DSL_EN.set();
        lsm_reset_state++;
        return 2;       // wait 5 ms
    }
    lsm_reset_state = 0;

    /* Enable register address automatically incremented during a multiple byte
        access with a serial interface. */
    if ( LSM6DSL_ACC_GYRO_W_IF_Addr_Incr( nullptr, LSM6DSL_ACC_GYRO_IF_INC_ENABLED ) == MEMS_ERROR )
    {
        return -1;
    }

    /* Enable BDU */
    if ( LSM6DSL_ACC_GYRO_W_BDU( nullptr, LSM6DSL_ACC_GYRO_BDU_BLOCK_UPDATE ) == MEMS_ERROR )
    {
        return -2;
    }

    /* FIFO mode selection */
    if ( LSM6DSL_ACC_GYRO_W_FIFO_MODE( nullptr, LSM6DSL_ACC_GYRO_FIFO_MODE_BYPASS ) == MEMS_ERROR )
    {
        return -3;
    }

    /* Output data rate selection - power down. */
    if ( LSM6DSL_ACC_GYRO_W_ODR_XL( nullptr, LSM6DSL_ACC_GYRO_ODR_XL_POWER_DOWN ) == MEMS_ERROR )
    {
        return -4;
    }

    /* Full scale selection. */
    if ( Set_X_FS( 2 ) == false )
    {
        return -5;
    }

    /* Output data rate selection - power down */
    if ( LSM6DSL_ACC_GYRO_W_ODR_G( nullptr, LSM6DSL_ACC_GYRO_ODR_G_POWER_DOWN ) == MEMS_ERROR )
    {
        return -6;
    }

    /* Full scale selection. */
    if ( Set_G_FS( 2000 ) == false )
    {
        return -7;
    }

    /* Enable sampling */
    if(LSM6DSL_ACC_GYRO_W_ODR_XL(nullptr, LSM6DSL_ACC_GYRO_ODR_XL_208Hz) == MEMS_ERROR)
    {
        return -8;
    }

    if(LSM6DSL_ACC_GYRO_W_ODR_G(nullptr, LSM6DSL_ACC_GYRO_ODR_G_208Hz) == MEMS_ERROR)
    {
        return -9;
    }

    {
    }
    is_init = true;
    return 0;
}
