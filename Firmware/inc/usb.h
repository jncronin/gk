#ifndef USB_H
#define USB_H

void *usb_task(void *);
void usb_init_chip_id();
bool usb_process_start();

#endif
