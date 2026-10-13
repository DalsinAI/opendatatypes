#!/bin/sh
# OpenDatatypes with the os32-gcc16 stove: checks datatypes.lvo against the
# NDK 3.2 datatypes_lib.fd and every constant and layout dt_core.h carries
# against the NDK 3.2 headers (ndk_check.c, compiled only), then builds
#   opendatatypes.library  the library under its own name, for the ABI test
#                          (step D1: no datatypes.library replacement yet)
#   DatatypesABITest       runs the same calls through OS 3.2's library and
#                          ours and lists every difference
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
P="$STOVE/prefix"
CC=${CC:-"$P/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build-amiga"}
mkdir -p "$OUT/libs" "$OUT/tests"

FD=${FD:-$(find "$P" -name datatypes_lib.fd 2>/dev/null | head -1)}
[ -n "$FD" ] || { echo "no datatypes_lib.fd under $P: set FD=" >&2; exit 1; }
python3 "$HERE/../common/check_fd.py" "$FD" "$HERE/datatypes.lvo"

"$CC" -m68000 -O2 -std=gnu99 -fno-builtin -Wall -Wextra -Werror -I"$HERE" -c "$HERE/ndk_check.c" \
  -o "$OUT/ndk_check.o"
echo "ndk_check: every constant and layout matches NDK 3.2"

LIBFLAGS="-m68000 -O2 -std=gnu99 -fno-delete-null-pointer-checks -fomit-frame-pointer -fno-toplevel-reorder
  -fno-lto -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -Werror
  -Wno-unused-parameter -nostartfiles -nostdlib"
# shellcheck disable=SC2086
"$CC" $LIBFLAGS -DLIBNAME='"opendatatypes.library"' -I"$HERE" -o "$OUT/libs/opendatatypes.library" \
  "$HERE/opendatatypes_lib.c" "$HERE/dt_core.c" -lgcc
echo "$OUT/libs/opendatatypes.library ($(wc -c < "$OUT/libs/opendatatypes.library") bytes)"
und=$("$P/bin/m68k-amigaos-nm" -u "$OUT/libs/opendatatypes.library" || true)
[ -z "$und" ] || { echo "undefined in opendatatypes.library: $und" >&2; exit 1; }

"$CC" -m68000 -O2 -std=gnu99 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -Wno-pointer-sign -Wno-format \
  -noixemul -o "$OUT/tests/DatatypesABITest" "$HERE/tests/DatatypesABITest.c"
echo "$OUT/tests/DatatypesABITest ($(wc -c < "$OUT/tests/DatatypesABITest") bytes)"
sha256sum "$OUT"/libs/* "$OUT/tests/DatatypesABITest"
