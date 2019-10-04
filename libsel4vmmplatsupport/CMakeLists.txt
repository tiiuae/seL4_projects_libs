#
# Copyright 2019, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.8.2)

project(libsel4vmmplatsupport C)

add_compile_options(-std=gnu99)

set(sources src/device.c src/device_utils.c)

if("${KernelArch}" STREQUAL "arm")
    file(GLOB arch_sources src/arch/arm/*.c src/arch/arm/devices/*.c)
    file(GLOB plat_sources src/plat/${KernelPlatform}/*.c src/plat/${KernelPlatform}/devices/*.c)
    list(APPEND sources ${arch_sources})
    list(APPEND sources ${plat_sources})
else("${KernelArch}" STRQEQUAL "x86")
endif()

add_library(sel4vmmplatsupport STATIC EXCLUDE_FROM_ALL ${sources})
target_include_directories(sel4vmmplatsupport PUBLIC include arch_include/${KernelArch} plat_include/${KernelPlatform})
target_link_libraries(sel4vmmplatsupport muslc sel4 utils sel4vm sel4vmmcore sel4_autoconf
        sel4vm_Config)
