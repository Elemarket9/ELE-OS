# ele_os_cf_build.mk inherits ele_os_cf_x86_64.mk and keeps that product name.
# android-16.0.0_r1 has no trunk_staging release config. The tag's
# release_config_map.textproto aliases aosp_current to bp2a. lunch has
# not been run, so these combos are unchecked.
# The low-RAM products and the Treble GSI use the same release config.
PRODUCT_MAKEFILES := $(LOCAL_DIR)/ele_os_cf_build.mk
PRODUCT_MAKEFILES += \
    $(LOCAL_DIR)/ele_os_cf_x86_64_low_ram.mk \
    $(LOCAL_DIR)/ele_os_gsi_arm64.mk \
    $(LOCAL_DIR)/ele_os_gsi_low_ram_arm64.mk

COMMON_LUNCH_CHOICES := ele_os_cf_x86_64-aosp_current-userdebug
COMMON_LUNCH_CHOICES += ele_os_cf_x86_64_low_ram-aosp_current-userdebug
COMMON_LUNCH_CHOICES += ele_os_gsi_arm64-aosp_current-userdebug
COMMON_LUNCH_CHOICES += ele_os_gsi_low_ram_arm64-aosp_current-userdebug
