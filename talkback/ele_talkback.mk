# SPDX-License-Identifier: Apache-2.0
# Screen reader required on every ELE-OS product, including low-RAM.
# Prototypes do not drop this package to save space.

ELE_TALKBACK_APK := device/ele/os/talkback/EleTalkBack.apk
ifeq ($(wildcard $(ELE_TALKBACK_APK)),)
$(error ELE-OS product $(TARGET_PRODUCT) is missing $(ELE_TALKBACK_APK). See device/ele/os/talkback/README.md. sync-pin.sh checks the pin and does not install an APK.)
endif

PRODUCT_PACKAGES += EleTalkBack EleTalkBackNotice
