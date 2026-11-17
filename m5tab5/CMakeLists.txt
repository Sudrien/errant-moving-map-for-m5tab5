cmake_minimum_required(VERSION 3.16)

# exFAT (0018) is feckless-libs' fatfs component since 0041, named in
# main/idf_component.yml: it patches this IDF's FatFs at configure time.
# CONFIG_FECKLESS_FATFS_EXFAT=n builds FAT32 only.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(errant_moving_map_for_m5tab5)

# 0043: the map itself is an ELF built against the feckless ABI, by elf/;
# this build is main/standalone.c, which brings storage up, embeds that
# ELF and loads it. libversions.h is elf/'s, with the code that includes
# it. On its own:
#   idf.py -C elf -B build-elf build

# u-errant-0007: feckless_box_libc out of this firmware's link. It is the
# part of libc a boxed app carries in its own ELF, and elf/ links it there;
# but the manifest is one for both builds, so the component manager makes
# it a requirement of main here too, and ESP-IDF links every component of
# the build into the firmware. Its stdio and strerror then met the
# toolchain's ("multiple definition of `stderr'", `fputc', `strerror'...),
# as soon as something in the firmware used them. Nothing here calls it:
# taken off the executable and off every component that names it.
idf_build_get_property(_errant_exe EXECUTABLE)
idf_build_get_property(_errant_comps BUILD_COMPONENTS)
set(_errant_targets ${_errant_exe})
foreach(_c IN LISTS _errant_comps)
    idf_component_get_property(_lib ${_c} COMPONENT_LIB)
    list(APPEND _errant_targets ${_lib})
endforeach()
foreach(_t IN LISTS _errant_targets)
    foreach(_p LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_libs ${_t} ${_p})
        if(_libs)
            list(FILTER _libs EXCLUDE REGEX "feckless_box_libc")
            set_property(TARGET ${_t} PROPERTY ${_p} "${_libs}")
        endif()
    endforeach()
endforeach()
