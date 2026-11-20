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

# Battery policy for every ELE-OS phone class, not for one named phone.
# UNMEASURED. No battery run on Cuttlefish or on a handset exists.
#
# This fragment sets no screen timeout, Doze constant, CPU governor, or
# I/O scheduler. AOSP's platform defaults remain. A Treble GSI does not
# replace the phone's kernel, so a governor assigned here would not be
# the one that runs on the phone.
#
# pm.dexopt.shared is not set. AOSP's go_defaults_common.prop at
# android-16.0.0_r1 says the quicken filter trades extra CPU and battery
# for RAM. The low-RAM memory fragment refuses that filter too.
#
# An always-on assistant listener and a recovery check-in client are not
# packages in this tree. This file does not add them.

ELE_OS_BATTERY_POLICY := platform-default-unmeasured
