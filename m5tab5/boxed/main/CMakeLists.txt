# SPDX-License-Identifier: MIT
#
# u-errant-0002: the boxed map's one component. The sources are ../../main's,
# the ones ../elf/ builds, by path: this component has its own manifest
# (feckless-libs' umode branch), which main/'s pinned one cannot be. Beside
# them:
#
#   absent.c       what a box does not answer -- the network, flash
#                  partitions, a GPIO interrupt -- as the app's own
#                  functions that fail the way the map already handles.
set(_m "${CMAKE_CURRENT_LIST_DIR}/../../main")

# The map's sources, as main/CMakeLists.txt lists them for the ELF. Checked
# below against that file, so one added there and not here stops the
# configure instead of leaving an import.
set(_map_srcs
    errant.c gnss.c ffread.c maptile.c maplabel.c places.c motion.c handling.c imu.c trust.c
    wifiloc.c mapset.c mapview.c tilecache.c tilesrc.c netremote.c
    portal.c portalweb.c dnsreply.c wifijoin.c launcher_import.c launcherkey.c
    sun.c uirow.c waypoints.c worldtile.c lastfix.c area.c ubx.c aop.c)

file(READ "${_m}/CMakeLists.txt" _main_cmake)
string(REGEX MATCHALL "\"[a-z0-9_]+\\.c\"" _main_srcs "${_main_cmake}")
string(REPLACE "\"" "" _main_srcs "${_main_srcs}")
list(REMOVE_DUPLICATES _main_srcs)
list(REMOVE_ITEM _main_srcs standalone.c)   # the .bin's app_main
set(_missing ${_main_srcs})
list(REMOVE_ITEM _missing ${_map_srcs})
set(_extra ${_map_srcs})
list(REMOVE_ITEM _extra ${_main_srcs})
if(_missing OR _extra)
    message(FATAL_ERROR "boxed/main/CMakeLists.txt and main/CMakeLists.txt list different sources: "
                        "not here [${_missing}], not there [${_extra}]")
endif()

set(_srcs "absent.c")
foreach(_f IN LISTS _map_srcs)
    list(APPEND _srcs "${_m}/${_f}")
endforeach()

# The two files main/CMakeLists.txt fetches into its build directory and
# embeds, fetched the same way into this one: the world at boot (0020) and
# the BMI270's configuration image (0032). Without network each is an
# empty file, and the map boots to black, or has no accelerometer.
set(ERRANT_WORLD "${CMAKE_BINARY_DIR}/world_z0.mvt.gz")
set(ERRANT_BMI270 "${CMAKE_BINARY_DIR}/bmi270_config.bin")
if(CMAKE_BUILD_EARLY_EXPANSION)
    foreach(_f "${ERRANT_WORLD}" "${ERRANT_BMI270}")
        if(NOT EXISTS "${_f}")
            file(WRITE "${_f}" "")
        endif()
    endforeach()
else()
    idf_build_get_property(_errant_py PYTHON)
    if(NOT EXISTS "${ERRANT_WORLD}.tried")
        execute_process(
            COMMAND "${_errant_py}" -I "${_m}/../tools/fetch_worldtile.py"
                    "${ERRANT_WORLD}" --base "${CONFIG_ERRANT_TILE_BASE}"
                    --pinned "${CONFIG_ERRANT_PINNED_BUILD}"
            OUTPUT_VARIABLE _errant_out ERROR_VARIABLE _errant_err)
        message(STATUS "${_errant_err}${_errant_out}")
        file(SIZE "${ERRANT_WORLD}" _errant_size)
        # src: main/maptile.c TILE_CAP, 192 KB: a larger tile cannot be drawn.
        if(_errant_size GREATER 196608)
            message(WARNING "worldtile: ${_errant_size} bytes is more than the render "
                            "scratch's 192 KB; the map will log that it would not draw")
        endif()
        file(WRITE "${ERRANT_WORLD}.tried" "")
    endif()
    if(NOT EXISTS "${ERRANT_BMI270}.tried")
        execute_process(
            COMMAND "${_errant_py}" -I "${_m}/../tools/fetch_bmi270_config.py"
                    "${ERRANT_BMI270}"
            OUTPUT_VARIABLE _errant_out ERROR_VARIABLE _errant_err)
        message(STATUS "${_errant_err}${_errant_out}")
        file(WRITE "${ERRANT_BMI270}.tried" "")
    endif()
endif()

idf_component_register(
    SRCS ${_srcs}
    INCLUDE_DIRS "." "${_m}"
    EMBED_FILES "${ERRANT_WORLD}" "${ERRANT_BMI270}"
    REQUIRES mapcore feckless_drivers feckless_graphics_handler feckless_storage_handler
             feckless_network_handler
             feckless_abi feckless_abi_storage feckless_abi_network
             feckless_box_libc            # linked into the ELF
             esp_driver_uart esp_driver_gpio esp_driver_i2c esp_timer fatfs esp_app_format
             esp_http_client mbedtls nvs_flash
             esp_http_server lwip app_update spi_flash cjson
)

# errant.c's app_main requires the box in place of the network section.
target_compile_definitions(${COMPONENT_LIB} PRIVATE ERRANT_BOXED=1)
