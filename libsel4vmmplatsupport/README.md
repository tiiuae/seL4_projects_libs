<!--
     Copyright 2019, Data61
     Commonwealth Scientific and Industrial Research Organisation (CSIRO)
     ABN 41 687 119 230.

     This software may be distributed and modified according to the terms of
     the BSD 2-Clause license. Note that NO WARRANTY is provided.
     See "LICENSE_BSD2.txt" for details.

     @TAG(DATA61_BSD)
-->

# libsel4vmmplatsupport

A library containing various VMM utilities and drivers that can be
used to construct a Guest VM on a supported platform. This library makes use of 'libsel4vm' to implement VMM
support.
This is a consolidated library composed of libraries previously known as (but now deprecated) 'libsel4vmm' (x86),
'libsel4arm-vmm' (arm), libsel4vmmcore and libsel4pci.

Reference implementations using this library are:
* CAmkES VM (x86) - https://github.com/seL4/camkes-vm
* CAmkES ARM VM (arm) - https://github.com/SEL4PROJ/camkes-arm-vm
