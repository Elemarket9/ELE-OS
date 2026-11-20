# SPDX-License-Identifier: Apache-2.0
# Low-RAM virtual phone. Prototype makefile: no image has been built or booted.
# Same TalkBack policy as the virtual phone: the reader is not installed.
# This is not a published install image.

$(call inherit-product, device/ele/os/ele_os_cf_build.mk)

PRODUCT_NAME := ele_os_cf_x86_64_lowram
PRODUCT_MODEL := ELE-OS Virtual Phone, low-RAM prototype
PRODUCT_PRODUCT_PROPERTIES += ro.config.low_ram=true
