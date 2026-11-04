#ifndef CM33_INTERFACE_H
#define CM33_INTERFACE_H

void init_cm33_interface();
int cm33_set_tilt(bool en);
int cm33_set_touch(bool en);
int cm33_set_left_stick_mouse(bool en);
int cm33_set_right_stick_mouse(bool en);
int cm33_set_tilt_stick_mouse(bool en);
int cm33_set_left_stick_4way(bool en);
int cm33_set_right_stick_4way(bool en);
int cm33_set_tilt_stick_4way(bool en);
int cm33_set_throttle_stick_mouse(bool en);
int cm33_set_throttle_stick_detent(bool en, int ndetents);
int cm33_set_oled(bool en);
int cm33_set_oled_scr(int scr_id, bool en);
void *cm33_get_oled_buffer(int scr_id);

#endif
