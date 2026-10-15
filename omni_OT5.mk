#
# TWRP product file for TCL OT5 (T508N)
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from OT5 device
$(call inherit-product, device/fih/OT5/device.mk)

PRODUCT_DEVICE := OT5
PRODUCT_NAME := omni_OT5
PRODUCT_BRAND := TCL
PRODUCT_MODEL := T508N
PRODUCT_MANUFACTURER := fih

PRODUCT_GMS_CLIENTID_BASE := android-fih

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="OT5_CM_FIH-user 13 TP1A.220624.014 508NCM5N release-keys"

BUILD_FINGERPRINT := TCL/OT5_CM_FIH/OT5:13/TP1A.220624.014/508NCM5N:user/release-keys