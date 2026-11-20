# SPDX-License-Identifier: Apache-2.0
# Low-RAM Treble GSI. Same generic ARM64 system image and the same screen
# reader, with ro.config.low_ram set. Prototype makefile: no image has been
# built or booted, and no install image is published.

$(call inherit-product, device/ele/os/products/ele_os_gsi_arm64.mk)

PRODUCT_NAME := ele_os_gsi_arm64_lowram
PRODUCT_MODEL := ELE-OS Treble GSI, low-RAM prototype
PRODUCT_PRODUCT_PROPERTIES += ro.config.low_ram=true
