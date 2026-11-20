# Copyright 2026 Sweet Meet
# SPDX-License-Identifier: Apache-2.0
#
# Treble GSI lunch target ele_os_gsi_arm64.
# The inherit list follows AOSP build/make/target/product/aosp_arm64.mk at
# android-16.0.0_r1. Upstream enables gsi_release.mk only when the product
# name is aosp_arm64, so this file inherits that makefile itself.
# This product has not been built or booted in this repository.

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
$(call inherit-product, device/ele/os/network/ele_privacy.mk)

PRODUCT_NAME := ele_os_gsi_arm64
PRODUCT_DEVICE := generic_arm64
PRODUCT_BRAND := ELE-OS
PRODUCT_MODEL := ELE-OS
