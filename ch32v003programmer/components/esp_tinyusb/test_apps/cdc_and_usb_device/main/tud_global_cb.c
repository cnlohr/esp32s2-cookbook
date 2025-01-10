/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "tinyusb.h"
#include "tusb_tasks.h"
#include "test_bvalid_sig.h"
#include "test_descriptors_config.h"

// Invoked when device is mounted
void tud_mount_cb(void)
{
    /**
     * @attention Tests relying on this callback only pass on Linux USB Host!
     *
     * This callback is issued after SetConfiguration command from USB Host.
     * However, Windows issues SetConfiguration only after a USB driver was assigned to the device.
     * So in case you are implementing a Vendor Specific class, or your device has 0 interfaces, this callback is not issued on Windows host.
     */
    printf("%s\n", __FUNCTION__);
    test_bvalid_sig_mount_cb();
    test_descriptors_config_mount_cb();
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    printf("%s\n", __FUNCTION__);
    test_bvalid_sig_umount_cb();
    test_descriptors_config_umount_cb();
}
