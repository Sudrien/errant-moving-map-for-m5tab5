# 0043: the map as an ELF for esp_elf -- main/'s sources, linked
# position-independent with mapcore and cJSON linked in and every other
# call left undefined: the feckless ABI's, with its storage and network
# sections (feckless_abi, feckless_abi_storage, feckless_abi_network
# from feckless-libs-for-m5tab5). The build checks the ELF against them.
# The shape is Lothesome's, Unprofitable Mahjong's and Spurious's elf/.
#
# The top-level build runs this one (main/CMakeLists.txt) and embeds
# what it makes. On its own:
#   idf.py -C elf -B build-elf build
# writes build-elf/errant_map.app.elf.
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

# The configuration is the .bin's, copied whole when the top-level
# build passes it: struct layouts and inline functions in the headers
# depend on it -- FatFs's FIL, httpd_req_t, the HTTP client's config --
# and the ELF must agree with the
# host it runs on. On its own, the defaults.
#
# Except partitions.csv, which is relative to the project and so would
# be looked for in elf/: made absolute, here and in the defaults (the
# later file wins).
get_filename_component(_m5 "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_partitions_line "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${_m5}/partitions.csv\"")
if(ERRANT_HOST_SDKCONFIG AND SDKCONFIG)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ERRANT_HOST_SDKCONFIG}")
    file(READ "${ERRANT_HOST_SDKCONFIG}" _cfg)
    string(REGEX REPLACE "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"[^\"\n]*\""
           "${_partitions_line}" _cfg "${_cfg}")
    file(WRITE "${SDKCONFIG}.host" "${_cfg}")
    configure_file("${SDKCONFIG}.host" "${SDKCONFIG}" COPYONLY)
endif()
file(WRITE "${CMAKE_BINARY_DIR}/sdkconfig.elf.defaults" "${_partitions_line}\n")
set(SDKCONFIG_DEFAULTS "${_m5}/sdkconfig.defaults"
                       "${CMAKE_BINARY_DIR}/sdkconfig.elf.defaults")

# main is ../main, mapcore is ../components; only what main needs is built.
list(APPEND EXTRA_COMPONENT_DIRS "${_m5}/main" "${_m5}/components")
set(COMPONENTS main)

# The .bin's version, which CI takes from the release tag (version.txt
# beside the .bin's CMakeLists.txt, not here), so the ELF's own
# esp_app_desc_t says the same. On its own, IDF's usual: git describe.
if(ERRANT_PROJECT_VER)
    set(PROJECT_VER "${ERRANT_PROJECT_VER}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Tells main/CMakeLists.txt which of its two builds this is.
idf_build_set_property(ERRANT_ELF 1)
# One manifest, so one lock: the top level's.
idf_build_set_property(DEPENDENCIES_LOCK "${_m5}/dependencies.lock")

project(errant_map)

# Into the ELF, not the ABI: the original's map pipeline, and cJSON for
# M5Launcher's config.conf -- IDF's json on 5.x; on 6.x the registry's
# cjson, whose component the component manager names espressif__cjson
# (0044: a bare "cjson" has no target, and the ELF's link stopped at
# "No rule to make target '__idf_cjson/fast'"). Whichever it is, it is
# the component whose name ends so, as Lothesome finds esp-dsp.
set(ELF_COMPONENTS mapcore)
idf_build_get_property(_comps BUILD_COMPONENTS)
foreach(_c IN LISTS _comps)
    if(_c MATCHES "(^|__)c?json$")
        list(APPEND ELF_COMPONENTS "${_c}")
    endif()
endforeach()
feckless_abi_elf(errant_map)

# libversions.h, the licences page's list (0037): generated from the lock,
# the BMI270 pin and the world tile's source -- after project(), which is
# when main does those fetches. Moved here from the top-level
# CMakeLists.txt with the code that includes it.
idf_component_get_property(main_lib main COMPONENT_LIB)
include(${_m5}/cmake/libversions.cmake)
target_include_directories(${main_lib} PRIVATE "${LIBVERSIONS_DIR}")
