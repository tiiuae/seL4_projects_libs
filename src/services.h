/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Author: alex.kroh@nicta.com.au */

#ifndef _SERVICES_H_
#define _SERVICES_H_

#include <stdlib.h>

#if defined(PLAT_IMX6) || defined(IMX6)
#define DMA_MINALIGN_BYTES 32
#elif defined(PLAT_EXYNOS4)
#define DMA_MINALIGN_BYTES 32
#elif defined(PLAT_EXYNOS5)
#define DMA_MINALIGN_BYTES 32
#else
#warning Unknown platform. DMA alignment defaulting to 32 bytes.
#endif

#define _malloc malloc
#define _free   free

#endif /* _SERVICES_H_ */
