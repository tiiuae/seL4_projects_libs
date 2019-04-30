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

project(libsel4pci C)

add_compile_options(-std=gnu99)

file(GLOB sources src/*.c src/arch/${KernelArch}/*.c)

add_library(sel4pci STATIC EXCLUDE_FROM_ALL ${sources})
target_include_directories(sel4pci PUBLIC include arch_include/${KernelArch})
target_link_libraries(sel4pci sel4 pci muslc platsupport ethdrivers sel4vm sel4_autoconf)
