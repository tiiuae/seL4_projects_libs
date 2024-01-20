/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2024, Technology Innovation Institute
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#if defined(CONFIG_ARM_CORTEX_A7)
#define PLAT_CPU_COMPAT "arm,cortex-a7"
#elif defined(CONFIG_ARM_CORTEX_A15)
#define PLAT_CPU_COMPAT "arm,cortex-a15"
#elif defined(CONFIG_ARM_CORTEX_A35)
#define PLAT_CPU_COMPAT "arm,cortex-a35"
#elif defined(CONFIG_ARM_CORTEX_A53)
#define PLAT_CPU_COMPAT "arm,cortex-a53"
#elif defined(CONFIG_ARM_CORTEX_A55)
#define PLAT_CPU_COMPAT "arm,cortex-a55"
#elif defined(CONFIG_ARM_CORTEX_A57)
#define PLAT_CPU_COMPAT "arm,cortex-a57"
#elif defined(CONFIG_ARM_CORTEX_A72)
#define PLAT_CPU_COMPAT "arm,cortex-a72"
#else
/* Fallback to a53 */
#define PLAT_CPU_COMPAT "arm,cortex-a53"
#endif
