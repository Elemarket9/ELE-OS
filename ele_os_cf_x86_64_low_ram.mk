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

# Cuttlefish image with the low-RAM class switch. Same virtual device as
# ele_os_cf_x86_64. Use it to exercise the class. A Cuttlefish boot is not
# a phone result, and this product has not been built or booted.

$(call inherit-product, device/ele/os/ele_os_cf_build.mk)
$(call inherit-product, device/ele/os/products/low_ram.mk)

PRODUCT_NAME := ele_os_cf_x86_64_low_ram
PRODUCT_MODEL := ELE-OS Virtual Phone, low-RAM class
