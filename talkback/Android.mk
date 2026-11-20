# SPDX-License-Identifier: Apache-2.0
# Prebuilt of an APK built from the pinned google/talkback revision.
# The APK is not in git. This makefile defines the module only when the
# file is present. ele_talkback.mk still errors if an ELE-OS product is
# configured without it, so the image cannot omit the reader.

LOCAL_PATH := $(call my-dir)

ifeq ($(wildcard $(LOCAL_PATH)/EleTalkBack.apk),)
else
include $(CLEAR_VARS)
LOCAL_MODULE := EleTalkBack
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := EleTalkBack.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_PRODUCT_MODULE := true
LOCAL_DEX_PREOPT := false
include $(BUILD_PREBUILT)
endif
