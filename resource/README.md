# ELE-OS resource policy foundation

This single-writer C++17 advisory component supports the initial work on
EOS-003, EOS-334 and EOS-339. These IDs are **partial**, not complete:
no scheduler, power HAL, ActivityManager, JobScheduler or lmkd integration
is present. No kernel replacement or handset support is implied.

The caller provides fresh CPU busy permille, memory totals in KiB, and a
trusted thermal-policy decision (false when unavailable). The CPU estimate is
an EWMA with alpha=1/4, O(1) work and storage. Memory pressure thresholds
use hysteresis and overflow-safe arithmetic. Background eligibility is
conservative when samples are absent or invalid. Callers own freshness,
thread synchronization and job deadlines. The thresholds are experimental,
not device-validated; they must not replace Android thermal limits, process
importance, user preferences or urgent work. No actuator is included.

Host verification: tests/run.sh includes deterministic policy tests covering
boundary transitions, counter limits, missing inputs and thermal denial.
The standalone test passed locally with GCC 14.2, -Wall -Wextra -Werror and
undefined-behavior sanitizer. This does not validate the Android build,
prediction accuracy, battery gains or full-system integration.

Next: connect read-only, timestamped system telemetry on the AOSP development
target. Compare forecasts with a naive baseline; measure overhead and
calibrate per device. Integrate only through supported AOSP policy interfaces.
