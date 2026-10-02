#ifndef KLOG_USB_H
#define KLOG_USB_H

#include <stddef.h>

ssize_t usb_log_write(const void *buf, size_t count);

#include "usb_class.h"
void usb_log_tfer_in(usb_handle *pdev, usb_cdc_acm_status *s);

#endif
