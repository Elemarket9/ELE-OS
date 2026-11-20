# ELE-OS Cuttlefish build contract.
#
# PROTOTYPE. lunch has not been run against this file. No image has been
# built or booted from it. The AOSP tag is recorded in manifest/aosp-tag.
#
# AndroidProducts.mk registers this file. Product name, device, brand,
# model, and PRODUCT_PACKAGES stay in ele_os_cf_x86_64.mk, which inherits
# the upstream AOSP Cuttlefish phone (aosp_cf.mk, not a GMS product).
# This overlay does not add a phone device tree. The Treble GSI products
# are separate makefiles. This file is the default Cuttlefish product.
# The low-RAM class switch lives only in products/low_ram.mk.
#
# Signing uses AOSP's published test key. That key is public and is not
# an ELE-OS release key. Release private keys are kept offline and are
# not referenced here. Leaving PRODUCT_DEFAULT_DEV_CERTIFICATE unset
# selects the same path (build/make/core/config.mk on the pinned tag);
# it is set so a later edit has to change this line to use another key.

$(call inherit-product, $(LOCAL_DIR)/ele_os_cf_x86_64.mk)

# The virtual product is not a handset from a phone maker.
PRODUCT_MANUFACTURER := ELE-OS

PRODUCT_DEFAULT_DEV_CERTIFICATE := build/make/target/product/security/testkey

PRODUCT_SYSTEM_PROPERTIES += \
    ro.eleos.keys=test \
    ro.eleos.prototype=1

$(call inherit-product, $(LOCAL_DIR)/products/common.mk)
