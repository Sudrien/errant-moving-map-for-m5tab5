/* SPDX-License-Identifier: MIT
 *
 * 0043: the .bin's app_main(). The map is the ELF that ../elf/ built
 * from errant.c and the rest, embedded in this firmware, and this loads
 * it the way a launcher would load the published copy: through the
 * feckless ABI, with its storage and network sections
 * (feckless_abi_storage, feckless_abi_network), which this firmware
 * links.
 *
 * What a launcher brings up before it runs an app is brought up here,
 * in Avoidant's order: NVS; the bus and expanders, which hold the USB
 * host's power; the USB host before storage, which registers mass
 * storage as a class with it; storage; then the port, which powers
 * USB-A. The map finds the card and the drive mounted and NVS up, and
 * starts the network itself. Its own board inits return ESP_OK when run
 * again.
 */
#include <stdint.h>

#include "esp_log.h"
#include "feckless_abi.h"
#include "nvs_flash.h"
#include "storage.h"
#include "storage_io.h"
#include "tab5io.h"
#include "usbhost.h"

extern const uint8_t elf_start[] asm("_binary_errant_map_app_elf_start");
extern const uint8_t elf_end[]   asm("_binary_errant_map_app_elf_end");

void app_main(void)
{
    /* Not erased on a failure, as the map's own net_start() had it: a
     * program that wipes the saved networks because it could not read
     * them is worse than one that runs without them. */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK)
        ESP_LOGE("standalone", "nvs_flash_init: %s; no saved networks this boot",
                 esp_err_to_name(err));

    ESP_ERROR_CHECK(tab5io_init());
    ESP_ERROR_CHECK(usbhost_init(tab5io_exp2()));
    storage_io_init();
    static const storage_usb_t usb = {
        .register_class = usbhost_register_class,
        .set_power      = usbhost_set_power,
        .powered        = usbhost_powered,
    };
    ESP_ERROR_CHECK(storage_init(&usb));
    usbhost_start();

    int ret = feckless_abi_run(elf_start, (size_t)(elf_end - elf_start));
    ESP_LOGE("standalone", "the map did not start: %d", ret);
}
