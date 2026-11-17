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
