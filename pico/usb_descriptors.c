#include <tusb.h>

// Device descriptor
static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x2E8A, // Raspberry Pi (example VID)
    .idProduct          = 0x000A, // Generic CDC
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// Configuration descriptor with single CDC
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF  0x81
#define EPNUM_CDC_OUT    0x02
#define EPNUM_CDC_IN     0x82

static uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // CDC: Notification EP, Data EP OUT & IN
    TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_fs_configuration;
}

// Strings
static char const * string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, // 0: English (0x0409)
    "EVAbits",
    "RP2040 AVR ISP",
    "0001",
    "CDC"
};

static uint16_t _desc_str[32];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        _desc_str[1] = (uint16_t) ((uint8_t const *)string_desc_arr[0])[0] | ((uint16_t)((uint8_t const *)string_desc_arr[0])[1] << 8);
        // first byte is length (in 2-byte units) and descriptor type
        _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
        return _desc_str;
    }

    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) {
        return NULL;
    }

    const char* str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);
    if (chr_count > 31) chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);
    return _desc_str;
}
