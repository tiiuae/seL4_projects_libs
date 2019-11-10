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

project(libsel4vm C)

set(configure_string "")

add_compile_options(-std=gnu99)

set(sources src/boot.c src/guest_vspace.c src/guest_ram.c src/arch/${KernelArch}/boot.c
    src/guest_memory.c src/guest_memory_utils.c src/vm.c src/arch/${KernelArch}/vm.c
    src/guest_vcpu_fault.c src/arch/${KernelArch}/guest_vcpu_fault_arch.c
    src/arch/${KernelArch}/guest_irq_controller.c)

if("${KernelArch}" STREQUAL "arm")
    config_option(
        LibSel4ArmVMMVchanSupport
        LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
        "Support vchan connections"
        DEFAULT
        OFF
    )

    add_config_library(sel4vm "${configure_string}")

    file(
        GLOB
            old_arm_sources
            sel4-arm-vmm/src/*.c
            sel4-arm-vmm/src/arch/arm/*/*.c
            sel4-arm-vmm/src/images/*.c
            sel4-arm-vmm/src/devices/*.c
            sel4-arm-vmm/src/plat/${KernelPlatform}/*/*.c
    )
    set(arm_sources src/arch/arm/mem_abort.c src/arch/arm/syscalls.c src/arch/arm/vgic/vgic.c
        src/arch/arm/guest_arm_context.c src/arch/arm/fault.c src/arch/arm/${KernelSel4Arch}/fault.c)
    list(APPEND sources ${arm_sources})
    list(APPEND sources ${old_arm_sources})
    add_library(sel4vm STATIC EXCLUDE_FROM_ALL ${sources})
    target_include_directories(
        sel4vm
        PUBLIC
            include
            arch_include/${KernelArch}
            sel4-arm-vmm/include
    )

    target_include_directories(sel4vm PRIVATE src/arch/arm/${KernelSel4Arch})
    target_include_directories(sel4vm PUBLIC sel4_arch_include/${KernelSel4Arch})
    target_link_libraries(
        sel4vm
        muslc
        sel4
        sel4simple
        utils
        sel4utils
        sel4vka
        sel4vspace
        sel4allocman
        sel4dma
        platsupport
        sel4_autoconf
        sel4vm_Config
        usbdrivers_Config
    )
    if(LibSel4ArmVMMVchanSupport)
        target_link_libraries(sel4vm sel4vchan)
    endif()
    if(LibUSB)
        target_link_libraries(sel4vm usbdrivers)
    endif()
elseif("${KernelArch}" STREQUAL "x86")
    config_option(LibSel4VMMVESAFrameBuffer VMM_VESA_FRAMEBUFFER "Expose VESA Frame Buffer
        Support for mapping the VESA frame buffer frames into the guest OS.
        This option is useful for working around non-working video drivers otherwise,
        due to missing features or such." DEFAULT ON)

    config_option(LibSel4VMMDebug LIB_VMM_DEBUG "Enable VMM debug output" DEFAULT ON)

    config_string(LibSel4VMMDebugLevel LIB_VMM_DEBUG_LEVEL "VMM Debug Output
        Debug verbosity level.
        5 levels for debug messages:
           0: Always printout
           1: Main entry point in a module
           2: 2nd level entry point in a module
           3: Main entry point of a function
           4: Details inside a function" DEFAULT 3 UNQUOTE)

    config_option(
        LibSel4VMVMXTimerDebug
        LIB_VM_VMX_TIMER_DEBUG
        "Use VMX Pre-Emption timer for debugging
        Will cause a regular vmexit to happen based on VMX pre-emption
        timer. At each exit the guest state will be printed out. This
        can be used to aid debugging when running a guest causes nothing
        to appear to happen"
        DEFAULT
        OFF
    )

    config_string(
        LibSel4VMVMXTimerTimeout
        LIB_VM_VMX_TIMER_TIMEOUT
        "Cycles between VMX timer pre-emptions
        Cycles between timer exits. Time spent loading the VMCS context
        count, so setting this too low may result in the guest making
        no progress"
        DEFAULT
        4000
        DEPENDS
        "LibSel4VMVMXTimerDebug"
    )

    config_option(LibSel4VMMIgnoreEPTViolation VMM_IGNORE_EPT_VIOLATION "Ignore EPT Violations
        If set then EPT faults will be ignored and the guest will be resumed" DEFAULT OFF)

    mark_as_advanced(
        LibSel4VMMVESAFrameBuffer
        LibSel4VMMDebug
        LibSel4VMMDebugLevel
        LibSel4VMMVMXTimerDebug
        LibSel4VMMVMXTimerTimeout
        LibSel4VMMIgnoreEPTViolation
    )

    add_config_library(sel4vm "${configure_string}")

    file(
        GLOB
            x86_sources
            sel4-x86-vmm/src/vmm/*.c
            sel4-x86-vmm/src/platform/*.c
            sel4-x86-vmm/src/driver/*.c
            sel4-x86-vmm/src/manager/*.c
            src/arch/${KernelArch}/processor/*.c
    )
    list(APPEND sources ${x86_sources} src/arch/${KernelArch}/ioports.c src/arch/${KernelArch}/i8259/i8259.c
        src/arch/${KernelArch}/guest_x86_context.c src/arch/${KernelArch}/vmcs.c src/arch/${KernelArch}/debug.c
        src/arch/${KernelArch}/interrupt.c src/arch/${KernelArch}/vmcall.c src/arch/${KernelArch}/ept.c
        src/arch/${KernelArch}/halt.c src/arch/${KernelArch}/vmx_timer.c)
    add_library(sel4vm STATIC EXCLUDE_FROM_ALL ${sources})

    target_include_directories(
        sel4vm
        PUBLIC include arch_include/${KernelArch} sel4-x86-vmm/include
    )
    target_link_libraries(
        sel4vm
        PUBLIC
            muslc
            sel4
            sel4utils
            pci
            sel4allocman
            ethdrivers
            platsupport
            sel4vm_Config
    )
endif()

target_include_directories(sel4vm PRIVATE src src/arch/${KernelArch})
