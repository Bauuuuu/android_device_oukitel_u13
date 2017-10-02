# Copyright (C) 2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

$(call inherit-product, device/oukitel/u13/full_u13.mk)

$(call inherit-product, vendor/oukitel/u13/u13-vendor.mk)

# Inherit some common LineageOS stuff.

$(call inherit-product, vendor/cm/config/common_full_phone.mk)

PRODUCT_NAME := lineage_u13
BOARD_VENDOR := oukitel
PRODUCT_DEVICE := u13

PRODUCT_GMS_CLIENTID_BASE := android-oukitel

TARGET_VENDOR_PRODUCT_NAME := u13
TARGET_VENDOR_DEVICE_NAME := u13
PRODUCT_BUILD_PROP_OVERRIDES += TARGET_DEVICE=U13 PRODUCT_NAME=U13
