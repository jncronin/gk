#include "syscalls_int.h"
#include "cm33_data.h"

extern cm33_joy_calib input_joy_calib[2];

int syscall_joystick_calib(unsigned int axis_pair, int left, int right,
    int top, int bottom, int middle_x, int middle_y, int *_errno)
{
    klog("joystick: calib: axis_pair: %u, left: %d, right: %d, top: %d, bottom: %d, middle_x: %d, middle_y: %d\n",
        axis_pair, left, right, top, bottom, middle_x, middle_y);

    //joystick: calib: axis_pair: 1, left: -32764, right: 19552, top: 20868, bottom: -32768, middle_x: -1200, middle_y: -1008
    
    if(axis_pair >= 2)
    {
        *_errno = EINVAL;
        return -1;
    }

    if((left < -32768) || (left > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if((right < -32768) || (right > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if((top < -32768) || (top > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if((bottom < -32768) || (bottom > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if((middle_x < -32768) || (middle_x > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if((middle_y < -32768) || (middle_y > 32767))
    {
        *_errno = EINVAL;
        return -1;
    }

    if(left > right)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(left > middle_x)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(middle_x > right)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(bottom > top)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(bottom > middle_y)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(middle_y > top)
    {
        *_errno = EINVAL;
        return -1;
    }

    klog("joystick: acceptable\n");
    input_joy_calib[axis_pair].left = (int16_t)left;
    input_joy_calib[axis_pair].right = (int16_t)right;
    input_joy_calib[axis_pair].top = (int16_t)top;
    input_joy_calib[axis_pair].bottom = (int16_t)bottom;
    input_joy_calib[axis_pair].middle_x = (int16_t)middle_x;
    input_joy_calib[axis_pair].middle_y = (int16_t)middle_y;

    return 0;
}
