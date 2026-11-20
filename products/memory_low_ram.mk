#
# Copyright 2026 The ELE-OS Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Memory build flags for the low-RAM phone class only. products/low_ram.mk
# inherits this file. Flagship and mid-range products do not.
#
# UNMEASURED. No Cuttlefish image, no Treble GSI, and no handset has been
# built or timed. Nothing here is a measured result, and no phone is named.
#
# The three assignments below are the non-numeric flags in AOSP
# android-16.0.0_r1 build/make/target/product/go_defaults_common.mk.
# This file does not inherit that makefile, and it does not inherit
# go_defaults.mk. That second file also pulls gms_mainline_go release
# maps. This is not an Android Go product and not a certification.
#
# AOSP's comment on the compiler filter says it profiles the system server
# and wifi service to reduce RAM and storage. ELE-OS has not measured
# either one. AOSP's comment on minimizing Java debug info says it shrinks
# the system image and leaves less JDWP local-variable information, and
# that stack traces are unaffected. The size delta is not measured.
# Skipping libartd is AOSP's "Do not generate libartd."
#
# Not set, on purpose. lmkd keeps the defaults it already has for the
# ro.config.low_ram mode selected by the class fragment:
# - ro.lmk.upgrade_pressure, ro.lmk.downgrade_pressure, and the other
#   ro.lmk.* overrides in go_defaults_common.prop
# - dalvik.vm.heapgrowthlimit and dalvik.vm.heapsize
# - pm.dexopt.shared. The AOSP comment on that prop says quicken trades
#   extra CPU and battery for RAM
# - MALLOC_LOW_MEMORY. In go_defaults_common.mk the comment says "outside
#   of eng builds" and the conditional enables the allocator on eng
#   builds. Those two disagree, so this tree copies neither
# - zram and other swap. The kernel and vendor image stay with Cuttlefish
#   upstream or with the phone

ELE_OS_MEMORY_POLICY := low-ram-aosp-build-flags-unmeasured

PRODUCT_SYSTEM_SERVER_COMPILER_FILTER := speed-profile
PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false
PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true
