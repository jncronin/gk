#ifndef CM33_DATA_H
#define CM33_DATA_H

#include <stdint.h>

// the following structure is exposed to userspace, and placed in SRAM1
struct cm33_joystick
{
    int16_t x, res0, y, res1;
};

struct cm33_data_userspace
{
    volatile uint32_t keystate;
    volatile cm33_joystick joy_a, joy_b, joy_tilt;
    volatile float yaw, pitch, roll;
    volatile float acc[3], gyr[3];
    volatile cm33_joystick joy_a_raw, joy_b_raw;    // for calibration purposes
};

static_assert(sizeof(cm33_data_userspace) == 20*4);

struct cm33_joy_calib
{
    int16_t left, right, top, bottom, middle_x, middle_y, res0, res1;
};

struct cm33_data_kernel
{
    volatile uint32_t sr;
    volatile uint32_t cr;

    volatile cm33_joy_calib joy_a_calib, joy_b_calib;
    float tilt_zero;

    volatile uint32_t rb_size;
    volatile uint32_t rb_w_ptr;
    volatile uint32_t rb_r_ptr;
    volatile uint32_t rb_paddr;
};

static_assert(sizeof(cm33_data_kernel) == 15*4);

// status register
#define CM33_DK_SR_READY            1
#define CM33_DK_SR_FAIL             2
#define CM33_DK_SR_TILT_ENABLE      4
#define CM33_DK_SR_OUTPUT_ENABLE    8
#define CM33_DK_SR_OVERFLOW         16
#define CM33_DK_SR_TOUCH_ENABLE     32

// commands
#define CM33_DK_CMD_TILT_ENABLE     1
#define CM33_DK_CMD_TILT_DISABLE    2
#define CM33_DK_CMD_TOUCH_ENABLE    3
#define CM33_DK_CMD_TOUCH_DISABLE   4

// messages
#define CM33_DK_MSG_MASK            (0xffU << 24)
#define CM33_DK_MSG_CONTENTS        (0xffffffU)
#define CM33_DK_MSG_PRESS           (0x1U << 24)
#define CM33_DK_MSG_RELEASE         (0x2U << 24)
#define CM33_DK_MSG_LONGPRESS       (0x3U << 24)
#define CM33_DK_MSG_REPEAT          (0x4U << 24)
#define CM33_DK_MSG_LOGEND          (0x5U << 24)
#define CM33_DK_MSG_LOG             (0x6U << 24)
#define CM33_DK_MSG_TOUCHPRESS      (0x7U << 24)
#define CM33_DK_MSG_TOUCHMOVE       (0x8U << 24)
#define CM33_DK_MSG_TOUCHRELEASE    (0x9U << 24)

#endif
