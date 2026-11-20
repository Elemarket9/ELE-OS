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

# Low-RAM phone class. This is the low-end and older class, the one aimed
# at giving those phones a current system image. It is not a named handset
# and not a phone-maker build. Flagship and mid-range products must not
# inherit this file.
#
# UNMEASURED. ro.config.low_ram is AOSP's switch. lmkd uses it to pick its
# own low-RAM behavior rather than its high-performance behavior. This file
# does not replace those defaults, does not read MemTotal, and does not
# claim a RAM or battery delta. No image has been built.
#
# The property is a system property. A Treble GSI does not replace the
# phone's vendor image, so a vendor property set here would not be the one
# that phone boots with.
#
# Not Android Go. build/make/target/product/go_defaults.mk at
# android-16.0.0_r1 also pulls gms_mainline_go release maps. This file
# does not inherit it.

PRODUCT_SYSTEM_PROPERTIES += \
    ro.config.low_ram=true

$(call inherit-product-if-exists, device/ele/os/products/memory_low_ram.mk)
