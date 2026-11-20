# SPDX-License-Identifier: Apache-2.0
# Virtual phone lunch target. Inherits the TalkBack policy and does not
# install the reader. This file does not set ro.config.low_ram.

$(call inherit-product, device/ele/os/ele_os_cf_x86_64.mk)
$(call inherit-product, device/ele/os/talkback/ele_talkback.mk)
