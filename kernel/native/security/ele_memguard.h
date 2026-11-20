/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_ELE_MEMGUARD_H
#define _LINUX_ELE_MEMGUARD_H
#include <linux/mman.h>

/* Enforce per-mapping W^X, including effective protection from personality.
 * This is not a guarantee of exclusive physical-page aliases, nor CFI.
 */
static inline bool ele_memguard_denies(unsigned long prot)
{
    return (prot & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC);
}
#endif
