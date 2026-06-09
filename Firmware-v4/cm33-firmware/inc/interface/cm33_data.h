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
    volatile cm33_joystick throttle, throttle_raw;
    volatile cm33_joystick tilt_raw;
};

static_assert(sizeof(cm33_data_userspace) == 26*4);

struct cm33_joy_calib
{
    int16_t left, right, top, bottom, middle_x, middle_y, digital_dz, analog_dz;
};

struct cm33_data_kernel
{
    volatile uint32_t sr;
    volatile uint32_t cr;

    volatile cm33_joy_calib joy_a_calib, joy_b_calib, tilt_calib, throttle_calib;

    volatile uint32_t rb_size;
    volatile uint32_t rb_w_ptr;
    volatile uint32_t rb_r_ptr;
    volatile uint32_t rb_paddr;
};

static_assert(sizeof(cm33_data_kernel) == 22*4);

// status register
#define CM33_DK_SR_READY            1
#define CM33_DK_SR_FAIL             2
#define CM33_DK_SR_TILT_ENABLE      4
#define CM33_DK_SR_OUTPUT_ENABLE    8
#define CM33_DK_SR_OVERFLOW         16
#define CM33_DK_SR_TOUCH_ENABLE     32
#define CM33_DK_SR_LEFT_STICK_MOUSE     64
#define CM33_DK_SR_RIGHT_STICK_MOUSE    128
#define CM33_DK_SR_TILT_STICK_MOUSE     256
#define CM33_DK_SR_THROTTLE_STICK_MOUSE 512
#define CM33_DK_SR_THROTTLE_STICK_DETENT_SHIFT      10
#define CM33_DK_SR_THROTTLE_STICK_DETENT_MASK       0x3c00
#define CM33_DK_SR_LEFT_STICK_4WAY      0x4000
#define CM33_DK_SR_RIGHT_STICK_4WAY     0x8000
#define CM33_DK_SR_TILT_STICK_4WAY      0x10000

// commands
#define CM33_DK_CMD_TILT_ENABLE     1
#define CM33_DK_CMD_TILT_DISABLE    2
#define CM33_DK_CMD_TOUCH_ENABLE    3
#define CM33_DK_CMD_TOUCH_DISABLE   4
#define CM33_DK_CMD_SET_LEFT_STICK_MOUSE        5
#define CM33_DK_CMD_CLEAR_LEFT_STICK_MOUSE      6
#define CM33_DK_CMD_SET_RIGHT_STICK_MOUSE       7
#define CM33_DK_CMD_CLEAR_RIGHT_STICK_MOUSE     8
#define CM33_DK_CMD_SET_TILT_STICK_MOUSE       9
#define CM33_DK_CMD_CLEAR_TILT_STICK_MOUSE     10
#define CM33_DK_CMD_SET_THROTTLE_STICK_MOUSE    11
#define CM33_DK_CMD_CLEAR_THROTTLE_STICK_MOUSE  12
#define CM33_DK_CMD_THROTTLE_STICK_DETENT       13
#define CM33_DK_CMD_THROTTLE_STICK_DETENT_END   (CM33_DK_CMD_THROTTLE_STICK_DETENT + 15)
#define CM33_DK_CMD_LEFT_STICK_8WAY             29
static_assert(CM33_DK_CMD_LEFT_STICK_8WAY == CM33_DK_CMD_THROTTLE_STICK_DETENT_END + 1);
#define CM33_DK_CMD_LEFT_STICK_4WAY             30
#define CM33_DK_CMD_RIGHT_STICK_8WAY            31
#define CM33_DK_CMD_RIGHT_STICK_4WAY            32
#define CM33_DK_CMD_TILT_STICK_8WAY             33
#define CM33_DK_CMD_TILT_STICK_4WAY             34

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
#define CM33_DK_MSG_MOUSEMOVE       (0xaU << 24)

#endif
