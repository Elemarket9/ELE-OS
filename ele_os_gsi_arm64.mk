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

# Treble GSI for many phones. No phone maker and no named handset.
# android-16.0.0_r1 build/make/target/product/aosp_arm64.mk says its system
# image is the arm64 GSI (64-bit userspace, 64-bit binder, system-as-root,
# VNDK). That file only inherits gsi_release.mk when TARGET_PRODUCT is
# aosp_arm64, so this product inherits gsi_release.mk itself.
#
# The same aosp_arm64.mk header says it is geared toward a US-centric
# emulator build. Locales are unchanged here.
#
# gsi_release.mk at that tag sets PRODUCT_SHIPPING_API_LEVEL. This file
# does not override it. A 32-bit GSI is not defined; add one only when a
# porter has a device that needs it.
#
# Not built. Not booted. Not a low-RAM product: flagship and mid-range
# Treble phones use this target. The vendor image, kernel, and firmware
# stay on the phone.

$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_arm64.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/gsi_release.mk)

PRODUCT_NAME := ele_os_gsi_arm64
PRODUCT_DEVICE := generic_arm64
PRODUCT_BRAND := ELE-OS
PRODUCT_MANUFACTURER := ELE-OS
PRODUCT_MODEL := ELE-OS Treble GSI

# Same public AOSP test key as the Cuttlefish prototype. Not a release key.
PRODUCT_DEFAULT_DEV_CERTIFICATE := build/make/target/product/security/testkey
PRODUCT_SYSTEM_PROPERTIES += \
    ro.eleos.keys=test \
    ro.eleos.prototype=1

PRODUCT_PACKAGES += ele-status ele-resource EleShare
PRODUCT_PACKAGES_DEBUG += ele-store

$(call inherit-product, device/ele/os/products/common.mk)
