#ifndef USB_H
#define USB_H

void *usb_task(void *);
void usb_init_chip_id();
bool usb_process_start();

#include "osqueue.h"
extern FixedQueue<uint32_t, 32> usb_queue;

// messages to send to usb system
#define USB_MESSAGE_INT             0x1
#define USB_MESSAGE_VBUS_DOWN       0x2
#define USB_MESSAGE_VBUS_UP         0x3
#define USB_MESSAGE_LOG_UPDATE      0x4

#endif
