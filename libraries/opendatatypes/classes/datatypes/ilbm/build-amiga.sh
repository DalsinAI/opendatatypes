#!/bin/sh
# Openilbm with the os32-gcc16 stove: ilbm.datatype (47.100), openilbm.datatype (the same code
# under its own name, side by side) and tests/ILBMDTClassABITest. See
# ../picture/build-class.sh.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$HERE/../picture/build-class.sh" "$HERE" ilbm ilbm.datatype ILBMDTClassABITest
