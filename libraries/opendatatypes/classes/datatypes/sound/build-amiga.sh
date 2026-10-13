#!/bin/sh
# Opensound with the os32-gcc16 stove: sound.datatype (47.100), opensound.datatype (the same code
# under its own name, side by side) and tests/SoundDTClassABITest. See
# ../picture/build-class.sh.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$HERE/../picture/build-class.sh" "$HERE" sound sound.datatype SoundDTClassABITest
