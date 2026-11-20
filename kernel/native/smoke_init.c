/* SPDX-License-Identifier: GPL-2.0-only */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <unistd.h>

static int read_value(const char *path, char *buf, size_t len)
{
    int fd = open(path, O_RDONLY);
    ssize_t n;
    if (fd < 0)
        return -1;
    n = read(fd, buf, len - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
    return 0;
}

int main(void)
{
    struct utsname u;
    char buf[512], level[16];
    unsigned int available;
    unsigned long long samples, transitions;
    int ok = 0;
    if (mount("proc", "/proc", "proc", 0, NULL) ||
        mount("sysfs", "/sys", "sysfs", 0, NULL)) {
        perror("mount");
        goto done;
    }
    if (uname(&u) || !strstr(u.release, "-eleos"))
        goto done;
    if (read_value("/sys/kernel/ele_kernel/pressure", buf, sizeof(buf)) ||
        sscanf(buf, "level=%15s available_permille=%u samples=%llu transitions=%llu",
               level, &available, &samples, &transitions) != 4 ||
        (strcmp(level, "normal") && strcmp(level, "elevated") &&
         strcmp(level, "critical")) || available > 1000 || !samples ||
        transitions > samples)
        goto done;
    printf("ELE pressure: %s", buf);
    if (read_value("/proc/sys/kernel/unprivileged_bpf_disabled", buf, sizeof(buf)) ||
        (buf[0] != '1' && buf[0] != '2'))
        goto done;
    if (read_value("/proc/pressure/memory", buf, sizeof(buf)) ||
        !strstr(buf, "some") || !strstr(buf, "full"))
        goto done;
    ok = 1;
done:
    puts(ok ? "ELE_KERNEL_SMOKE_PASS" : "ELE_KERNEL_SMOKE_FAIL");
    fflush(stdout);
    sync();
    reboot(RB_POWER_OFF);
    for (;;)
        pause();
}
