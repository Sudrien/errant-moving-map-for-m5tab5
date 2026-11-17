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
 * -- 0046 -- the network up to be asked for (feckless_net_start(), feckless-libs'
 * 6005), before the port, so USB Ethernet's class is installed with the
 * rest. Its own board inits return ESP_OK when run again.
 *
 * u-errant-0005: the map is built for a box, so it runs in one here as on
 * Avoidant's umode branch -- RISC-V user mode, in one block of PSRAM the
 * PMP grants it and nothing else, every call outside it answered by
 * feckless_box_calls -- and not through feckless_abi_run(), which would
 * refuse it by the name feckless_abi_box_v1. This file is the whole of
 * the launcher: it brings the board up, makes the box and waits. The
 * network is still started, as a launcher starts it, though a box cannot
 * ask for it yet (absent.c).
 */
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "feckless_net.h"
#include "feckless_abi.h"
#include "feckless_box_calls.h"
#include "feckless_umode_idf.h"
#include "feckless_umode_load.h"
#include "gfx.h"
#include "lcd.h"
#include "nvs_flash.h"
#include "storage.h"
#include "storage_io.h"
#include "tab5io.h"
#include "touch.h"
#include "usbhost.h"

extern const uint8_t elf_start[] asm("_binary_errant_map_app_elf_start");
extern const uint8_t elf_end[]   asm("_binary_errant_map_app_elf_end");

/* u-errant-0005: the box. The largest free PSRAM block less a margin for
 * this firmware -- 2 MB, Avoidant's, measured after the panel's
 * framebuffer is taken -- and at most 24 MB, Avoidant's ceiling
 * (u-avoidant-0005). The map wants about 19 MB: four tiles at
 * 13.1 MB, the world at boot 3.3 MB, scratch about 2 MB (u-errant-0001). */
#define BOX_MARGIN      (2u << 20)
#define BOX_MAX         (24u << 20)
#define BOX_STACK       (64 * 1024)         /* app_main's, in the box */
#define BOX_CORE        (1)                 /* not the drivers' (feckless_umode_load.h) */

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
    /* 0046: Defeatist's list of saved networks, which the map has always
     * read ("defeatist"), so a network joined in either is known here. */
    const feckless_net_start_t net = {
        .exp2                  = tab5io_exp2(),
        .usb_register_class    = usbhost_register_class,
        .usb_set_config_select = usbhost_set_config_select,
        .nvs_ns                = "defeatist",
    };
    feckless_net_start(&net);
    usbhost_start();

    /* u-errant-0005: the panel, gfx and touch up here too, on core 0, as a
     * launcher has them before an app runs (Lothesome's u-lothesome-0004):
     * the panel's interrupts -- its DMA restart, every frame -- are then
     * core 0's, and the map runs on core 1, where its windows cannot hold
     * them off. Its own inits find them up and return ESP_OK. Before the
     * box is sized, so the framebuffer is not counted as free. */
    esp_lcd_panel_handle_t panel = NULL;
    if (lcd_init(&panel) != ESP_OK || gfx_init(panel, LCD_H_RES, LCD_V_RES) != ESP_OK) {
        ESP_LOGE("standalone", "the panel did not come up");
        return;
    }
    touch_init(tab5io_bus(), LCD_H_RES, LCD_V_RES);     /* the map copes without */

    /* The stack guard's interrupt out of every core's routing first, which
     * a window needs. */
    feckless_umode_guard_unroute();
    const size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    size_t box = big > BOX_MARGIN ? big - BOX_MARGIN : 0;
    if (box > BOX_MAX) box = BOX_MAX;
    box &= ~(size_t)63;
    uint32_t n;
    const feckless_umode_call_t *calls = feckless_box_calls_v1(&n);
    feckless_umode_app_t *app = NULL;
    err = feckless_umode_load(elf_start, (size_t)(elf_end - elf_start), calls, n,
                              (uint32_t)box, BOX_STACK, &app);
    if (err != ESP_OK) {
        ESP_LOGE("standalone", "the map would not load in a box of %u KB: %s",
                 (unsigned)(box / 1024), esp_err_to_name(err));
        return;
    }
    ESP_LOGI("standalone", "the map in a box of %u KB", (unsigned)(box / 1024));
    /* On core 1, with the board brought up here on core 0: a window holds
     * interrupts off on its own core, and the panel's are on this one. */
    const feckless_umode_end_t end = feckless_umode_app_run_on(app, BOX_CORE);
    ESP_LOGE("standalone", "the map ended: reason %d, %s at 0x%08x", (int)end.reason,
             end.reason == FECKLESS_UMODE_END_FAULT ? feckless_umode_cause_name(end.mcause) : "-",
             (unsigned)end.pc);
    feckless_umode_unload(app);
    feckless_box_calls_end();
}
