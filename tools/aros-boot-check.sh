#!/bin/sh
set -eu

PROFILE=${1:-profiles/aros-boot.json}
ROM=${AMIVM_AROS_ROM:-external/aros-m68k/rom.bin}
IMAGE=${AMIVM_AROS_IMAGE:-external/aros-m68k/system.img}

[ -f "$PROFILE" ] || { echo "missing profile: $PROFILE" >&2; exit 2; }
[ -f "$ROM" ] || { echo "missing external AROS ROM: $ROM" >&2; exit 2; }

echo "AmiVM AROS qualification"
echo "profile: $PROFILE"
echo "rom:     $ROM"

if [ -f "$IMAGE" ]; then
    echo "image:   $IMAGE"
else
    echo "image:   <not supplied>"
fi

echo "status: external guest assets are present; execution harness integration pending"
exit 0
