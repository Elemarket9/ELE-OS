/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ELE_PRESSURE_POLICY_H
#define ELE_PRESSURE_POLICY_H

enum ele_pressure_level { ELE_NORMAL, ELE_ELEVATED, ELE_CRITICAL };
struct ele_pressure_policy {
    enum ele_pressure_level level;
    unsigned int recovery;
};

/* Available memory in permille. Escalate immediately, recover after 3 samples.
 * This policy only classifies pressure; it never kills tasks or changes VM knobs.
 */
static inline void ele_pressure_update(struct ele_pressure_policy *p,
                                       unsigned int available)
{
    enum ele_pressure_level target;

    if (available > 1000)
        available = 1000;
    if (available <= 70)
        target = ELE_CRITICAL;
    else if (available <= 150)
        target = ELE_ELEVATED;
    else
        target = ELE_NORMAL;

    if (target > p->level) {
        p->level = target;
        p->recovery = 0;
    } else if ((p->level == ELE_CRITICAL && available >= 100) ||
               (p->level == ELE_ELEVATED && available >= 200)) {
        if (++p->recovery >= 3) {
            /* Recover one level at a time, even after a sudden improvement. */
            p->level = p->level == ELE_CRITICAL ? ELE_ELEVATED : ELE_NORMAL;
            p->recovery = 0;
        }
    } else {
        p->recovery = 0;
    }
}
#endif
