#include "tusb_hid_gamepad.h"
#include "descriptors_control.h"

void tud_gamepad_report(hid_gamepad_report_t * report)
{
    tud_hid_report(REPORT_ID_GAMEPAD, report, sizeof(hid_gamepad_report_t));
}

void tud_gamepad_ns_report(hid_gamepad_ns_report_t * report)
{
    tud_hid_report(0, report, sizeof(hid_gamepad_ns_report_t));
}