#!/sbin/sh
# Copyright (C) 2012 The Android Open Source Project
# Copyright (C) 2013-2014 The CyanogenMod Project
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
#
# Allows customization for various d2lte variants.
#

CMDLINE=`cat /proc/cmdline`

if [[ "$CMDLINE" != "${CMDLINE/SGH-I747}" ]]; #d2att
    then
       echo "Found d2att. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2uc/d2att:4.1.2/JZO54K/I747UCDMG2:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2uc-user 4.1.2 JZO54K I747UCDMG2 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SGH-I747|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2att|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SCH-R530C}" ]]; #d2cri
    then
       echo "Found d2cri. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2cri/d2cri:4.1.2/JZO54K/R530CVVBMD6:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2cri-user 4.1.2 JZO54K R530CVVBMD6 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SCH-R530C|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2cri|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SCH-R530M}" ]]; #d2mtr
    then
       echo "Found d2mtr. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2mtr/d2mtr:4.1.2/JZO54K/R530MVQAMF2:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2mtr-user 4.1.2 JZO54K R530MVQAMF2 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SCH-R530M|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2mtr|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SPH-L710}" ]]; #d2spr
    then
       echo "Found d2spr. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2spr/d2spr:4.1.2/JZO54K/L710VPBMD4:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2spr-user 4.1.2 JZO54K L710VPBMD4 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SPH-L710|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2spr|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SGH-T999}" ]]; #d2tmo
    then
       echo "Found d2tmo. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2uc/d2att:4.1.2/JZO54K/T999UVDMD5:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2uc-user 4.1.2 JZO54K T999UVDMD5 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SGH-T999|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2tmo|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SCH-R530U}" ]]; #d2usc
    then
       echo "Found d2usc. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=samsung/d2usc/d2usc:4.1.1/JRO03L/R530UVXALK5:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2usc-user 4.1.1 JRO03L R530UVXALK5 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SCH-R530U|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2usc|g' /system/build.prop

elif [[ "$CMDLINE" != "${CMDLINE/SCH-I535}" ]]; #d2vzw
    then
       echo "Found d2vzw. Setting props."
       sed -i 's|ro.build.fingerprint=xxxxxxxxxx|ro.build.fingerprint=Verizon/d2vzw/d2vzw:4.3/JSS15J/I535VRUCML1:user/release-keys|g' /system/build.prop
       sed -i 's|ro.build.description=xxxxxxxxxx|ro.build.description=d2vzw-user 4.3 JSS15J I535VRUCML1 release-keys|g' /system/build.prop
       sed -i 's|ro.product.model=d2lte|ro.product.model=SCH-I535|g' /system/build.prop
       sed -i 's|ro.product.device=d2lte|ro.product.device=d2vzw|g' /system/build.prop
       echo "ro.telephony.default_cdma_sub=0" >> /system/build.prop
fi
