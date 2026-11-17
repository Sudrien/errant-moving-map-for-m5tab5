# boxed -- the map in a feckless box

The `umode` branch's project (u-errant-0002): the map, main/'s sources,
built into an ELF for a feckless box -- RISC-V user mode, one block of
PSRAM, every call out of it an `ecall` the launcher checks. Where ../elf/
makes the trusted ELF a launcher runs as itself, this makes the one a
launcher runs boxed.

## Building

ESP-IDF 6.1, from this directory:

    idf.py set-target esp32p4
    idf.py elf                  # -> build/errant_boxed.app.elf

The build ends with two checks. `feckless_abi` holds every import to the
ABI's core list and the box and storage sections; `feckless_box_libc`
(boxcheck) holds every import to what a box answers. Both must pass; the
first build to pass them said

    -- feckless_abi: 75 symbols, all in feckless_abi_v1 + feckless_abi_box_v1 + feckless_abi_storage_v2
    -- feckless_box_libc: boxed, 75 imports, none of them in-app names, all answered by the box

and 87 of each since u-errant-0003.

The manifest (`main/idf_component.yml`) follows feckless-libs' `umode`
branch. Its `dependencies.lock` is written by the first configure and is
this project's own; the map's manifest and lock are not touched.

The world tile and the BMI270 image are fetched into this project's build
directory, as the firmware's build fetches them into its own.

## What runs, and what does not

What a box answers: the screen and touch, gfx, the SD card and USB
storage, the RTC, threads and FreeRTOS's locks; and, since u-errant-0003
(feckless-libs' u-feckless-0062), the GNSS's UART and the BMI270 on the
board's bus. So: the map drawn from PMTiles on the card or a USB drive,
following the receiver's fix or panned by touch, with its labels, places
and saved points, dimming with the accelerometer.

What it does not answer is in `main/absent.c`, as the app's own functions
that fail the way the map already copes with:

| absent | so |
|---|---|
| GPIO interrupts | no PPS count: the clock check has the fix and the RTC, not the pulse |
| the network: Wi-Fi, sockets, HTTP client and server | an offline map: no remote tiles, setup portal, Wi-Fi location or NTP |
| esp_ota, esp_partition | nothing imported from M5Launcher's settings |

Each comes out of `absent.c` when a box answers it: the box's call then
replaces it, and the checks say which are left.

## On a board

Needs a launcher that boxes apps: Avoidant's umode branch. The file goes
on the card or a USB drive as `errant_boxed.app.elf`, in any folder
Avoidant's menu browses; it runs the ELF boxed because it imports
`feckless_abi_box_v1`.

**The box may be too small.** The map's four tiles are 13.1 MB and the
world at boot 3.3 MB more; Avoidant's box is at most 16 MB
(u-avoidant-0001). The world then does not fit (`world: no PSRAM for
it`), and the map goes on without it; if a later allocation fails the
screen says `Out of memory` and what for.

What to look for in the log: `avoidant: running errant_boxed.app.elf in a
box of N KB`; `=== Errant Moving Map ===`; `gnss: PPS on GPIO51:
ESP_ERR_NOT_SUPPORTED; counting none` and `the launcher has not started
the network`, which are absent.c; `gnss: UART1 at 38400 baud, RX GPIO6,
TX GPIO7, PPS GPIO51` and the `imu:` lines, which are the box's; the
map's own lines for the card's archives; any `thread N ended: reason`
line from feckless_umode, with the pc
(`riscv32-esp-elf-addr2line -e build/errant_boxed.app.elf.debug`, built
with `-DELF_LOADER_KEEP_DEBUG_ELF=ON`).
