#ifndef CM33_INTERFACE_H
#define CM33_INTERFACE_H

void init_cm33_interface();
int cm33_set_tilt(bool en);
int cm33_set_touch(bool en);
int cm33_set_left_stick_mouse(bool en);
int cm33_set_right_stick_mouse(bool en);
int cm33_set_tilt_stick_mouse(bool en);
int cm33_set_throttle_stick_mouse(bool en);
int cm33_set_throttle_stick_detent(bool en, int ndetents);

#endif
