#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

libs-$(CONFIG_LIB_SEL4_ARM_VMM) += libsel4arm-vmm

libsel4arm-vmm: common $(libc) libutils libsel4utils libsel4vka libsel4vspace
libsel4arm-vmm: libsel4simple libplatsupport libsel4vchan

ifeq ($(CONFIG_LIB_USB),y)
libsel4arm-vmm: libusbdrivers
endif

