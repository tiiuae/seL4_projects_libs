/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once


#ifdef CONFIG_LIB_PCI_DEBUG

    #define DPRINTF(lvl, ...) \
        do{ \
            if(lvl <= 0 || lvl < CONFIG_LIB_PCI_DEBUG_LEVEL){ \
                printf("%s:%d | ", __func__, __LINE__); \
                printf(__VA_ARGS__); \
            } \
        }while(0)

    #define drun(lvl, cmd) \
        do{ \
            if(lvl < LIB_PCI_DEBUG_LEVEL){ \
                cmd; \
            } \
        }while(0)

#else /* CONFIG_LIB_PCI_DEBUG */

    #define DPRINTF(lvl, ...) do{ /* nothing */ }while(0)
    #define drun() do{ /* nothing */ }while(0)

#endif /* CONFIG_LIB_PCI_DEBUG */


