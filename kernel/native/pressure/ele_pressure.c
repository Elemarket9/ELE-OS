// SPDX-License-Identifier: GPL-2.0-only
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kobject.h>
#include <linux/math64.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include "ele_pressure_policy.h"

static DEFINE_MUTEX(snapshot_lock);
static struct kobject *ele_kobj;
static struct ele_pressure_policy policy;
static unsigned int available_permille;
static u64 samples, transitions;
static void sample_pressure(struct work_struct *work);
static DECLARE_DELAYED_WORK(pressure_work, sample_pressure);
static const char * const names[] = { "normal", "elevated", "critical" };

static void update_snapshot(void)
{
    struct sysinfo info;
    long available = si_mem_available();
    unsigned int ratio;
    enum ele_pressure_level previous;

    si_meminfo(&info);
    if (!info.totalram)
        return;
    ratio = div64_u64((u64)min_t(unsigned long, max_t(long, available, 0),
                                info.totalram) * 1000, info.totalram);
    mutex_lock(&snapshot_lock);
    previous = policy.level;
    ele_pressure_update(&policy, ratio);
    available_permille = ratio;
    samples++;
    if (previous != policy.level)
        transitions++;
    mutex_unlock(&snapshot_lock);
}

static void sample_pressure(struct work_struct *work)
{
    update_snapshot();
    schedule_delayed_work(&pressure_work, msecs_to_jiffies(1000));
}

static ssize_t pressure_show(struct kobject *kobj, struct kobj_attribute *attr,
                             char *buf)
{
    ssize_t len;

    mutex_lock(&snapshot_lock);
    len = sysfs_emit(buf, "level=%s available_permille=%u samples=%llu transitions=%llu\n",
                     samples ? names[policy.level] : "unknown", available_permille,
                     (unsigned long long)samples, (unsigned long long)transitions);
    mutex_unlock(&snapshot_lock);
    return len;
}
static struct kobj_attribute pressure_attr = __ATTR_RO(pressure);

static int __init ele_pressure_init(void)
{
    int ret;

    ele_kobj = kobject_create_and_add("ele_kernel", kernel_kobj);
    if (!ele_kobj)
        return -ENOMEM;
    update_snapshot();
    ret = sysfs_create_file(ele_kobj, &pressure_attr.attr);
    if (ret) {
        kobject_put(ele_kobj);
        return ret;
    }
    schedule_delayed_work(&pressure_work, msecs_to_jiffies(1000));
    return 0;
}

static void __exit ele_pressure_exit(void)
{
    cancel_delayed_work_sync(&pressure_work);
    sysfs_remove_file(ele_kobj, &pressure_attr.attr);
    kobject_put(ele_kobj);
}
module_init(ele_pressure_init);
module_exit(ele_pressure_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ELE-Kernel hysteretic memory pressure telemetry");
