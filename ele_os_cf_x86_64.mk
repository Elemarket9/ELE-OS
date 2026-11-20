$(call inherit-product, device/google/cuttlefish/vsoc_x86_64_only/phone/aosp_cf.mk)

PRODUCT_NAME := ele_os_cf_x86_64
PRODUCT_DEVICE := vsoc_x86_64_only
PRODUCT_BRAND := ELE-OS
PRODUCT_MODEL := ELE-OS Virtual Phone

PRODUCT_PACKAGES += ele-status
