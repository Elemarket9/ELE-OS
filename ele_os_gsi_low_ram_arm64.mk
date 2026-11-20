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

# Low-RAM class of the arm64 Treble GSI. Same system image shape as
# ele_os_gsi_arm64, plus the low-RAM switch. The name ends in _arm64 so
# AOSP's gsi_release.mk pvmfw rule, which matches TARGET_PRODUCT %_arm64,
# sees the same architecture as the plain GSI. Not built and not booted.
# Not a named phone.

$(call inherit-product, device/ele/os/ele_os_gsi_arm64.mk)
$(call inherit-product, device/ele/os/products/low_ram.mk)

PRODUCT_NAME := ele_os_gsi_low_ram_arm64
PRODUCT_MODEL := ELE-OS Treble GSI, low-RAM class
