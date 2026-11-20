# ELE-OS development and merge policy

Owner direction, 2026-11-20: automatically merge compatible, verified
ELE-OS development changes into main; no repeated owner authorization
needed for individual merges. Use small reviewable changes and respect
repository protections. Do not merge known failing changes or falsely
claim an untested system feature is complete.

The 350-capability objective is the long-term scope, not evidence of
implementation. Preserve the current AOSP overlay baseline until an
explicit architecture migration is justified. Distinguish planned,
component implementation, host validation, platform integration and
device verification. Full operating system results require an actual
AOSP build and boot; host checks do not substitute for that.
