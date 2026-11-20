/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include "ele_pressure_policy.h"
int main(void)
{
    struct ele_pressure_policy p = { ELE_NORMAL, 0 };
    ele_pressure_update(&p, 151); assert(p.level == ELE_NORMAL);
    ele_pressure_update(&p, 150); assert(p.level == ELE_ELEVATED);
    ele_pressure_update(&p, 71); assert(p.level == ELE_ELEVATED);
    ele_pressure_update(&p, 70); assert(p.level == ELE_CRITICAL);
    for (int i = 0; i < 10; i++) ele_pressure_update(&p, 99);
    assert(p.level == ELE_CRITICAL && p.recovery == 0);
    ele_pressure_update(&p, 100); ele_pressure_update(&p, 100);
    ele_pressure_update(&p, 99); assert(p.recovery == 0);
    for (int i = 0; i < 3; i++) ele_pressure_update(&p, 100);
    assert(p.level == ELE_ELEVATED);
    for (int i = 0; i < 10; i++) ele_pressure_update(&p, 199);
    assert(p.level == ELE_ELEVATED);
    for (int i = 0; i < 3; i++) ele_pressure_update(&p, 200);
    assert(p.level == ELE_NORMAL);
    ele_pressure_update(&p, 0); assert(p.level == ELE_CRITICAL);
    for (int i = 0; i < 6; i++) ele_pressure_update(&p, 1001);
    assert(p.level == ELE_NORMAL);
    return 0;
}
