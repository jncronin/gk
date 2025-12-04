#include "thread.h"
#include "gk_conf.h"
#include "vmem.h"
#include "usb_device.h"
#include "tusb.h"
#include <vector>

#define UID_BASE  PMEM_TO_VMEM(0x44000014)

#define USB_VID     0xdeed
#define USB_PID     (0x474b + (GK_ENABLE_NETWORK) + 2 * (GK_ENABLE_USB_MASS_STORAGE))
#define USB_BCD     0x0200
#define DEVICE_BCD  0x0100

// device descriptor
#include "usb_descriptors.h"

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + \
  TUD_CDC_DESC_LEN * (GK_ENABLE_USB_CDC) + \
  TUD_RNDIS_DESC_LEN * (GK_ENABLE_NETWORK) + \
  TUD_MSC_DESC_LEN * (GK_ENABLE_USB_MASS_STORAGE) + \
  TUD_CDC_DESC_LEN * (GK_LOG_USB))

constexpr std::vector<uint8_t> usb_str_encode(const std::string &str)
{
  std::vector<uint8_t> ret;
  ret.push_back(str.length() * 2 + 2);
  ret.push_back(USB_DESC_TYPE_STRING);
  for(auto c : str)
  {
    ret.push_back(c);
    ret.push_back(0);
  }
  return ret;
}

const uint8_t device_desc[] =
{
	USB_LEN_DEV_DESC, /* bLength */
	USB_DESC_TYPE_DEVICE, /* bDescriptorType */
	0x00, /* bcdUSB */
	0x02, /* version */
	0x00, /* bDeviceClass */
	0x00, /* bDeviceSubClass */
	0x00, /* bDeviceProtocol */
	USB_MAX_EP0_SIZE, /* bMaxPacketSize */
	LOBYTE(USB_VID), /* idVendor */
	HIBYTE(USB_VID), /* idVendor */
	LOBYTE(USB_PID), /* idVendor */
	HIBYTE(USB_PID), /* idVendor */
	0x00, /* bcdDevice rel. 4.00 */
	0x04,
	USBD_IDX_MFC_STR, /* Index of manufacturer string */
	USBD_IDX_PRODUCT_STR, /* Index of product string */
	USBD_IDX_SERIAL_STR, /* Index of serial number string */
	USBD_MAX_NUM_CONFIGURATION /* bNumConfigurations */
}; /* USB_DeviceDescriptor */

const uint8_t desc_fs_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 500),

#if GK_ENABLE_NETWORK
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_ETH, 5, EPNUM_ETH_NOTIF, 8, EPNUM_ETH_OUT, EPNUM_ETH_IN, CFG_TUD_NET_ENDPOINT_SIZE),
#endif
#if GK_ENABLE_USB_CDC
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif
#if GK_LOG_USB
    TUD_CDC_DESCRIPTOR(ITF_NUM_LOG_CDC, 4, EPNUM_CDC_LOG_NOTIF, 8, EPNUM_CDC_LOG_OUT, EPNUM_CDC_LOG_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif

#if GK_ENABLE_USB_MASS_STORAGE
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 6, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
#endif
};

static const uint8_t usb_qualifier_desc_buf[] = {
	USB_LEN_DEV_QUALIFIER_DESC,
	USB_DESC_TYPE_DEVICE_QUALIFIER,
	0x00,
	0x02,
	0x00,
	0x00,
	0x00,
	0x40,
	0x01,
	0x00,
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_fs_configuration;
}

// String descriptors
char const *string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 },      // supported language English
    "JC",
    "gk",
    "123456",                           // use chip ID here
    "Terminal",
    "GKNetwork",
    "GKDrive"
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
static uint16_t _desc_str[32] = { 0 };
static char chip_id_str[32] = { 0 };
static std::vector<uint8_t> chip_id_buf;

static const char hexchars[] = "0123456789ABCDEF";

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

void usb_init_chip_id()
{
  u32_to_hex(&chip_id_str[0], *(volatile uint32_t *)UID_BASE);
  u32_to_hex(&chip_id_str[8], *(volatile uint32_t *)(UID_BASE + 4));
  u32_to_hex(&chip_id_str[16], *(volatile uint32_t *)(UID_BASE + 8));
  chip_id_str[24] = 0;

  std::string s(chip_id_str);
  chip_id_buf = usb_str_encode(s);
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

static uint8_t *usb_device_desc(uint16_t *length)
{
  *length = sizeof(device_desc);
//  klog("usb_device_desc: length: %u, data: %p\n", *length, device_desc);
  return (uint8_t *)device_desc;
}

static uint8_t *usb_config_desc(uint16_t *length)
{
  *length = sizeof(desc_fs_configuration);
//  klog("usb_config_desc: length: %u, data: %p\n", *length, desc_fs_configuration);
  return (uint8_t *)desc_fs_configuration;
}

static const uint8_t usb_lang_id_desc_buf[] = {
	USB_LEN_LANGID_STR_DESC,
	USB_DESC_TYPE_STRING,
	0x09, 0x04
};

uint8_t *usb_lang_id_desc(uint16_t *length)
{
  *length = sizeof(usb_lang_id_desc_buf);
//  klog("usb_lang_id_desc: length: %u, data: %p\n", *length, usb_lang_id_desc_buf);
  return (uint8_t *)usb_lang_id_desc_buf;
}

const std::vector<uint8_t> usb_manufacturer_buf = usb_str_encode("JC");
const std::vector<uint8_t> usb_product_buf = usb_str_encode("GKV4");
const std::vector<uint8_t> usb_interface_buf = usb_str_encode("GKV4");
const std::vector<uint8_t> usb_config_str_buf = usb_str_encode("GKV4");

const std::vector<uint8_t> usb_user_strs[] = 
{
  usb_str_encode("Terminal"),
  usb_str_encode("GKNetwork"),
  usb_str_encode("GKDrive")
};
constexpr auto n_user_strs = sizeof(usb_user_strs) / sizeof(usb_user_strs[0]);

uint8_t *usb_manufacturer_desc(uint16_t *length)
{
  *length = usb_manufacturer_buf.size();
//  klog("usb_manufacturer_desc: length: %u, data: %p\n", *length, usb_manufacturer_buf.data());
  return (uint8_t *)usb_manufacturer_buf.data();
}

uint8_t *usb_product_desc(uint16_t *length)
{
  *length = usb_product_buf.size();
//  klog("usb_product_desc: length: %u, data: %p\n", *length, usb_product_buf.data());
  return (uint8_t *)usb_product_buf.data();
}

uint8_t *usb_serial_desc(uint16_t *length)
{
  *length = chip_id_buf.size();
//  klog("usb_serial_desc: length: %u, data: %p\n", *length, chip_id_buf.data());
  return (uint8_t *)chip_id_buf.data();
}

static uint8_t *usb_interface_desc(uint16_t *length)
{
  *length = usb_interface_buf.size();
//  klog("usb_interface_desc: length: %u, data: %p\n", *length, usb_interface_buf.data());
  return (uint8_t *)usb_interface_buf.data();
}

static uint8_t *usb_get_usr_desc(uint8_t index, uint16_t *length)
{
  if(index >= n_user_strs)
    return nullptr;
  *length = usb_user_strs[index].size();
//  klog("usb_get_usr_desc(%u): length: %u, data: %p\n", index, *length, usb_user_strs[index].data());
  return (uint8_t *)usb_user_strs[index].data();
}

static uint8_t *usb_config_str_desc(uint16_t *length)
{
  *length = usb_config_str_buf.size();
//  klog("usb_config_str_desc: length: %u, data: %p\n", *length, usb_config_str_buf.data());
  return (uint8_t *)usb_config_str_buf.data();
}

static uint8_t *usb_get_qualifier_desc(uint16_t *length)
{
  *length = sizeof(usb_qualifier_desc_buf);
//  klog("usb_get_qualifier_desc: length: %u, data: %p\n", *length, usb_qualifier_desc_buf);
  return (uint8_t *)usb_qualifier_desc_buf;
}

extern "C" const usb_desc usb_desc_callback
{
  .get_device_desc = usb_device_desc,
  .get_lang_id_desc = usb_lang_id_desc,
	.get_manufacturer_desc = usb_manufacturer_desc,
	.get_product_desc = usb_product_desc,
	.get_serial_desc = usb_serial_desc,
	.get_configuration_desc = usb_config_str_desc,
	.get_interface_desc = usb_interface_desc,
	.get_usr_desc = usb_get_usr_desc,
	.get_config_desc = usb_config_desc,
	.get_device_qualifier_desc = usb_get_qualifier_desc,
	.get_other_speed_config_desc = nullptr,
};
