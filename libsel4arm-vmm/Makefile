#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := libsel4arm-vmm.a

CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/mach/$(MACH)/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/plat/$(PLAT)/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/arch/$(ARCH)/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/images/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/devices/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/devices/arm/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/devices/$(PLAT)/*.c))

# Header files/directories this library provides
HDRFILES := $(wildcard ${SOURCE_DIR}/include/*) \
            $(wildcard ${SOURCE_DIR}/arch_include/$(ARCH)/*) \
            $(wildcard ${SOURCE_DIR}/plat_include/$(PLAT)/*) \
            $(wildcard ${SOURCE_DIR}/mach_include/$(MACH)/*) 

include $(SEL4_COMMON)/common.mk
