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

# No Google software in an ELE-OS image: no Play Services, no Play Store,
# no sandboxed Play, and no microG. The names are
# products/no_google_packages.txt.
#
# AOSP inherit-product only concatenates PRODUCT_PACKAGES. android-16.0.0_r1
# build/make/core/product.mk has no remove list, so assigning
# PRODUCT_PACKAGES := $(filter-out ...) in this fragment does not see
# packages inherited from another makefile. The gate that runs in this
# repository is tests/host-debloat.sh. It rejects a module or package from
# the list if this tree adds it.
#
# UNMEASURED. Keeping those packages out is a compatibility and privacy
# choice. It is not a recorded RAM, storage, or battery delta.

ELE_OS_IMAGE_SOFTWARE := no-google-services
