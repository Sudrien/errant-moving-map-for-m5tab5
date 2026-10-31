# 0043: one directory, two builds. ../elf/ sets ERRANT_ELF and builds the
# map into an ELF for esp_elf, checked against the feckless ABI -- the
# fetches below are its, since the files go into the ELF; the top level
# builds standalone.c, runs ../elf/ and embeds what it made. The shape is
# Lothesome's, Unprofitable Mahjong's and Spurious's.
idf_build_get_property(_errant_elf ERRANT_ELF)

if(NOT _errant_elf)
    idf_component_register(
        SRCS "standalone.c"
        PRIV_REQUIRES feckless_abi feckless_abi_storage feckless_abi_network nvs_flash
                      feckless_drivers feckless_storage_handler
    )
    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        include(ExternalProject)
        idf_build_get_property(_idf_path IDF_PATH)
        idf_build_get_property(_target IDF_TARGET)
        idf_build_get_property(_build_dir BUILD_DIR)
        idf_build_get_property(_sdkconfig SDKCONFIG)
        idf_build_get_property(_project_dir PROJECT_DIR)
        idf_build_get_property(_project_ver PROJECT_VER)

        set(_elf_dir "${_build_dir}/elf")
        set(_elf "${_elf_dir}/errant_map.app.elf")

        externalproject_add(errant_elf
            SOURCE_DIR "${_project_dir}/elf"
            BINARY_DIR "${_elf_dir}"
            CMAKE_ARGS -DIDF_PATH=${_idf_path}
                       -DIDF_TARGET=${_target}
                       -DSDKCONFIG=${_elf_dir}/sdkconfig
                       -DERRANT_HOST_SDKCONFIG=${_sdkconfig}
                       -DERRANT_PROJECT_VER=${_project_ver}
            CMAKE_GENERATOR "Unix Makefiles"
            BUILD_COMMAND make elf
            BUILD_ALWAYS 1
            BUILD_BYPRODUCTS "${_elf}"
            INSTALL_COMMAND ""
        )

        # elf/ checked it against the ABI as it linked it; embedding
        # waits for that.
        add_dependencies(${COMPONENT_LIB} errant_elf)

        # elf_loader's: 4-byte aligned, as esp_elf_relocate() wants it.
        elf_embed_aligned_binary(${COMPONENT_LIB} "${_elf}" BINARY)
    endif()
    return()
endif()

# cJSON, for M5Launcher's config.conf: IDF's own `json` on 5.x, the
# registry's espressif/cjson on 6.x, which moved it out of the tree
# (idf_component.yml, behind an idf_version rule). Defeatist's 6031.
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(ERRANT_JSON cjson)
else()
    set(ERRANT_JSON json)
endif()

# The world at boot (0020): tile z0/0/0 of the build the network serves,
# fetched once into the build directory and embedded. Not in the
# repository -- it is OpenStreetMap-derived (ODbL), and CLAUDE.md keeps
# that out. A build without network embeds an empty file and boots to
# black, as before. `idf.py fullclean` (or deleting the file and its
# .tried) fetches it again.
set(ERRANT_WORLD "${CMAKE_BINARY_DIR}/world_z0.mvt.gz")
if(CMAKE_BUILD_EARLY_EXPANSION)
    # The component list is still being worked out and the config is not
    # loaded; EMBED_FILES only needs the file to exist.
    if(NOT EXISTS "${ERRANT_WORLD}")
        file(WRITE "${ERRANT_WORLD}" "")
    endif()
elseif(NOT EXISTS "${ERRANT_WORLD}.tried")
    idf_build_get_property(_errant_py PYTHON)
    execute_process(
        COMMAND "${_errant_py}" -I "${CMAKE_CURRENT_LIST_DIR}/../tools/fetch_worldtile.py"
                "${ERRANT_WORLD}" --base "${CONFIG_ERRANT_TILE_BASE}"
                --pinned "${CONFIG_ERRANT_PINNED_BUILD}"
        OUTPUT_VARIABLE _errant_out ERROR_VARIABLE _errant_err)
    message(STATUS "${_errant_err}${_errant_out}")
    file(SIZE "${ERRANT_WORLD}" _errant_size)
    # src: main/maptile.c TILE_CAP, 192 KB: a larger tile cannot be drawn.
    if(_errant_size GREATER 196608)
        message(WARNING "worldtile: ${_errant_size} bytes is more than the render "
                        "scratch's 192 KB; the firmware will log that it would not draw")
    endif()
    file(WRITE "${ERRANT_WORLD}.tried" "")
endif()

# The BMI270's configuration image (0032): Bosch's, BSD-3-Clause, fetched
# once into the build directory by tools/fetch_bmi270_config.py and
# embedded, as the world tile is. Without network, an empty file: the
# firmware then has no accelerometer, and dims on touch and distance alone.
set(ERRANT_BMI270 "${CMAKE_BINARY_DIR}/bmi270_config.bin")
if(CMAKE_BUILD_EARLY_EXPANSION)
    if(NOT EXISTS "${ERRANT_BMI270}")
        file(WRITE "${ERRANT_BMI270}" "")
    endif()
elseif(NOT EXISTS "${ERRANT_BMI270}.tried")
    idf_build_get_property(_errant_py PYTHON)
    execute_process(
        COMMAND "${_errant_py}" -I "${CMAKE_CURRENT_LIST_DIR}/../tools/fetch_bmi270_config.py"
                "${ERRANT_BMI270}"
        OUTPUT_VARIABLE _errant_out ERROR_VARIABLE _errant_err)
    message(STATUS "${_errant_err}${_errant_out}")
    file(WRITE "${ERRANT_BMI270}.tried" "")
endif()

idf_component_register(
    SRCS "errant.c" "gnss.c" "ffread.c" "maptile.c" "maplabel.c" "places.c" "motion.c" "handling.c" "imu.c" "trust.c" "wifiloc.c" "mapset.c" "mapview.c" "tilecache.c" "tilesrc.c" "netremote.c"
         "portal.c" "portalweb.c" "dnsreply.c" "wifijoin.c" "launcher_import.c" "launcherkey.c"
         "sun.c" "uirow.c" "waypoints.c" "worldtile.c" "lastfix.c" "area.c" "ubx.c" "aop.c"
    INCLUDE_DIRS "."
    EMBED_FILES "${ERRANT_WORLD}" "${ERRANT_BMI270}"
    REQUIRES mapcore feckless_drivers feckless_graphics_handler feckless_storage_handler feckless_network_handler
             feckless_abi feckless_abi_storage feckless_abi_network   # 0043
             esp_driver_uart esp_driver_gpio esp_driver_i2c esp_timer fatfs esp_app_format esp_http_client mbedtls nvs_flash
             esp_http_server lwip app_update spi_flash ${ERRANT_JSON}
)
