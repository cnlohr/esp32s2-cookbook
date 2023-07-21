#ifndef _TUSB_HID_GAMEPAD_H_
#define _TUSB_HID_GAMEPAD_H_

#include "class/hid/hid_device.h"

void tud_gamepad_report(hid_gamepad_report_t * report);
void tud_gamepad_ns_report(hid_gamepad_ns_report_t * report);

#endif /* _TUSB_HID_GAMEPAD_H_ */