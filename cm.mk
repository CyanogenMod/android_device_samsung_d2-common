$(call inherit-product, device/samsung/d2lte/full_d2lte.mk)

# Enhanced NFC
$(call inherit-product, vendor/cm/config/nfc_enhanced.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME=d2uc \
    TARGET_DEVICE=d2lte \
    BUILD_FINGERPRINT="xxxxxxxxxx" \
    PRIVATE_BUILD_DESC="xxxxxxxxxx"

PRODUCT_NAME := cm_d2lte
PRODUCT_DEVICE := d2lte

