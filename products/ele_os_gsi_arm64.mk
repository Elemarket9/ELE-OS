# SPDX-License-Identifier: Apache-2.0
# Device-neutral Treble GSI for ELE-OS. Generic ARM64 system image.
# No phone maker and no per-device tree. The TalkBack policy is inherited
# and the reader is not installed.
# Prototype: this product has not been lunched, built, or booted.
#
# The inherit list follows build/make/target/product/aosp_arm64.mk at
# android-16.0.0_r1. That file applies gsi_release.mk only when
# TARGET_PRODUCT is aosp_arm64, so this product includes it directly.

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)
$(call inherit-product-if-exists, packages/modules/Virtualization/apex/product_packages.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/board/generic_arm64/device.mk)

AB_OTA_UPDATER := true
AB_OTA_PARTITIONS ?= system

PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS := relaxed
MODULE_BUILD_FROM_SOURCE ?= true
$(call inherit-product, $(SRC_TARGET_DIR)/product/gsi_release.mk)
PRODUCT_SOONG_DEFINED_SYSTEM_IMAGE := aosp_system_image
USE_SOONG_DEFINED_SYSTEM_IMAGE := true
PRODUCT_USE_SOONG_NOTICE_XML := true

PRODUCT_NAME := ele_os_gsi_arm64
PRODUCT_DEVICE := generic_arm64
PRODUCT_BRAND := ELE-OS
PRODUCT_MODEL := ELE-OS Treble GSI
PRODUCT_NO_BIONIC_PAGE_SIZE_MACRO := true

$(call inherit-product, device/ele/os/talkback/ele_talkback.mk)
