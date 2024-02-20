#include "tusb.h"
#include "stm32h7xx.h"
#include "mpuregions.h"

#define USB_VID     0xdeed
#define USB_PID     0x474b
#define USB_BCD     0x0200
#define DEVICE_BCD  0x0100

#define TUSB_DATA __attribute__((section(".tusb_data")))
#define TUSB_BSS __attribute__((section(".tusb_bss")))

// device descriptor
TUSB_DATA tusb_desc_device_t desc_device =
{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = 0,  // defined per-interface
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = DEVICE_BCD,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 1,
};

uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

// Conf descriptor
enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_ETH,
    ITH_NUM_ETH_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define EPNUM_MSC_OUT     0x03
#define EPNUM_MSC_IN      0x83

#define EPNUM_ETH_NOTIF   0x84
#define EPNUM_ETH_OUT     0x05
#define EPNUM_ETH_IN      0x85

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_RNDIS_DESC_LEN)

TUSB_DATA uint8_t desc_fs_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 500),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_ETH, 5, EPNUM_ETH_NOTIF, 8, EPNUM_ETH_OUT, EPNUM_ETH_IN, CFG_TUD_NET_ENDPOINT_SIZE)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_fs_configuration;
}

// String descriptors
TUSB_DATA char const *string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 },      // supported language English
    "JC",
    "gk",
    "123456",                           // use chip ID here
    "Terminal",
    "GKNetwork"
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
TUSB_BSS static uint16_t _desc_str[32];
TUSB_BSS static char chip_id_str[32];

TUSB_DATA static char hexchars[] = "0123456789ABCDEF";

static void u8_to_hex(char *dest, uint32_t v)
{
  *dest++ = hexchars[(v >> 8) & 0xf];
  *dest = hexchars[v & 0xf];
}
static void u32_to_hex(char *dest, uint32_t v)
{
  u8_to_hex(&dest[0], (v >> 24) & 0xff);
  u8_to_hex(&dest[2], (v >> 16) & 0xff);
  u8_to_hex(&dest[4], (v >> 8) & 0xff);
  u8_to_hex(&dest[6], v & 0xff);
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    if(index == 3)
    {
        SetMPUForCurrentThread(MPUGenerate(UID_BASE, 32, 7, false, RO, NoAccess, DEV_S));
        // use 96-bit chip ID instead
        u32_to_hex(&chip_id_str[0], *(volatile uint32_t *)UID_BASE);
        u32_to_hex(&chip_id_str[8], *(volatile uint32_t *)(UID_BASE + 4));
        u32_to_hex(&chip_id_str[16], *(volatile uint32_t *)(UID_BASE + 8));
        chip_id_str[24] = 0;

        str = chip_id_str;
    }

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}