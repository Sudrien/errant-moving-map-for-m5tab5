cmake_minimum_required(VERSION 3.16)

# exFAT (0018) is feckless-libs' fatfs component since 0041, named in
# main/idf_component.yml: it patches this IDF's FatFs at configure time.
# CONFIG_FECKLESS_FATFS_EXFAT=n builds FAT32 only.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(errant_moving_map_for_m5tab5)

# libversions.h, the licences page's list (0037): generated from the lock,
# the BMI270 pin and the world tile's source when the build is
# configured -- after project(), which is when main does those fetches.
idf_component_get_property(main_lib main COMPONENT_LIB)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/libversions.cmake)
target_include_directories(${main_lib} PRIVATE "${LIBVERSIONS_DIR}")
