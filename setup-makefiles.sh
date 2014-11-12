#!/bin/sh

set -e

export DEVICE=d2-common
export VENDOR=samsung
./../msm8960-common/setup-makefiles.sh $@
