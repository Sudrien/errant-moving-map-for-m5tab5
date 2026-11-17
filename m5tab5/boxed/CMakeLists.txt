# SPDX-License-Identifier: MIT
#
# u-errant-0002: the map as an ELF for a feckless box -- RISC-V user mode,
# every call out of it an ecall the launcher checks. The same sources as
# ../elf/ (main/'s), built against feckless-libs' umode branch with its own
# manifest and lock, so nothing here moves the pins a release is built
# from. See README.md.
#
#     idf.py set-target esp32p4
#     idf.py elf                  ->  build/errant_boxed.app.elf
#
# `elf` is elf_loader's target; `idf.py build` would go on to link a
# firmware, which this is not.
cmake_minimum_required(VERSION 3.16)

get_filename_component(_m5 "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# The map's configuration, so the sources compile as the map's do, and its
# partition table by full path (the defaults name it relative to the
# project, which is not this one).
#
# u-errant-0004: and CONFIG_ESP_HTTPS_SERVER_ENABLE, as a launcher has it.
# feckless_abi_network (whose header errant.c includes) warns at configure
# when it is off, because a launcher without it cannot link; this is an
# app and links no server, so the warning was only noise -- but the map's
# sdkconfig.defaults predates that check, and saying it here keeps this
# project's configure quiet without touching the map's own defaults.
file(WRITE "${CMAKE_BINARY_DIR}/sdkconfig.boxed.defaults"
     "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${_m5}/partitions.csv\"\n"
     "CONFIG_ESP_HTTPS_SERVER_ENABLE=y\n")
set(SDKCONFIG_DEFAULTS "${_m5}/sdkconfig.defaults"
                       "${CMAKE_BINARY_DIR}/sdkconfig.boxed.defaults")

# mapcore is ../components; main is this project's own (main/), which
# lists ../main's sources.
list(APPEND EXTRA_COMPONENT_DIRS "${_m5}/components")
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(errant_boxed)

# Into the ELF, as ../elf/ puts them: the original's map pipeline, and
# cJSON by whatever the component manager names it (0044).
set(ELF_COMPONENTS mapcore)
idf_build_get_property(_comps BUILD_COMPONENTS)
foreach(_c IN LISTS _comps)
    if(_c MATCHES "(^|__)c?json$")
        list(APPEND ELF_COMPONENTS "${_c}")
    endif()
endforeach()
# feckless_abi_box_elf() links feckless_box_libc in as well, and fails the
# build on any import a box does not answer (boxcheck).
feckless_abi_box_elf(errant_boxed)

# libversions.h, the licences page's list (0037), from this project's lock.
idf_component_get_property(main_lib main COMPONENT_LIB)
include(${_m5}/cmake/libversions.cmake)
target_include_directories(${main_lib} PRIVATE "${LIBVERSIONS_DIR}")
