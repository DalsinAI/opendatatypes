#!/bin/sh
# OpenDatatypes with the os32-gcc16 stove: checks datatypes.lvo against the
# NDK 3.2 datatypes_lib.fd and every constant and layout dt_core.h and
# dtclass_env.h carry against the NDK 3.2 headers (ndk_check.c, compiled
# only), then builds
#   datatypes.library      the replacement (its class is "datatypesclass")
#   opendatatypes.library  the same under its own name, for the ABI test
#                          (its class is "opendatatypesclass")
#   AddDataTypes           OpenDatatypes' AddDataTypes command
#   DatatypesABITest       runs the same calls through OS 3.2's library and
#                          ours and lists every difference
# and sys/datatypes.catalog's descriptor stays in catalogs/datatypes.cd.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
P="$STOVE/prefix"
CC=${CC:-"$P/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build-amiga"}
mkdir -p "$OUT/libs" "$OUT/tests" "$OUT/c"

FD=${FD:-$(find "$P" -name datatypes_lib.fd 2>/dev/null | head -1)}
[ -n "$FD" ] || { echo "no datatypes_lib.fd under $P: set FD=" >&2; exit 1; }
python3 "$HERE/../common/check_fd.py" "$FD" "$HERE/datatypes.lvo"

"$CC" -m68000 -O2 -std=gnu99 -fno-builtin -Wall -Wextra -Werror -I"$HERE" -c "$HERE/ndk_check.c" \
  -o "$OUT/ndk_check.o"
echo "ndk_check: every constant and layout matches NDK 3.2"

LIBFLAGS="-m68000 -O2 -std=gnu99 -fno-delete-null-pointer-checks -fomit-frame-pointer -fno-toplevel-reorder
  -fno-lto -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -Werror
  -Wno-unused-parameter -nostartfiles -nostdlib"
for lib in datatypes opendatatypes; do
    cls=datatypesclass
    [ "$lib" = datatypes ] || cls=opendatatypesclass
    # shellcheck disable=SC2086
    "$CC" $LIBFLAGS -DLIBNAME="\"$lib.library\"" -DDTCLASSNAME="\"$cls\"" -I"$HERE" -o "$OUT/libs/$lib.library" \
      "$HERE/opendatatypes_lib.c" "$HERE/dt_core.c" "$HERE/dtclass_core.c" -lgcc
    echo "$OUT/libs/$lib.library ($(wc -c < "$OUT/libs/$lib.library") bytes)"
    und=$("$P/bin/m68k-amigaos-nm" -u "$OUT/libs/$lib.library" || true)
    [ -z "$und" ] || { echo "undefined in $lib.library: $und" >&2; exit 1; }
done

"$CC" -m68000 -O2 -std=gnu99 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -Wno-unused-parameter \
  -noixemul -o "$OUT/c/AddDataTypes" "$HERE/tools/AddDataTypes.c"
echo "$OUT/c/AddDataTypes ($(wc -c < "$OUT/c/AddDataTypes") bytes)"

"$CC" -m68000 -O2 -std=gnu99 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -Wno-pointer-sign -Wno-format \
  -noixemul -o "$OUT/tests/DatatypesABITest" "$HERE/tests/DatatypesABITest.c"
echo "$OUT/tests/DatatypesABITest ($(wc -c < "$OUT/tests/DatatypesABITest") bytes)"
sha256sum "$OUT"/libs/* "$OUT/c/AddDataTypes" "$OUT/tests/DatatypesABITest"
