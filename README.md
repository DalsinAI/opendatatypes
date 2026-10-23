# opendatatypes

The AmigaChrome Team's datatypes for AmigaOS 3.x on 68k, in one place. Every
decoder is a datatype, so any datatypes program (MultiView, OpenBench,
OpenBrowser) opens the formats they read.

| Datatype | Folder | What it reads |
| --- | --- | --- |
| `openpicture.datatype` | `Datatypes/openpicture` | picture class (our own) |
| `openpng.datatype` | `Datatypes/png` | PNG (our own decoder) |
| `opentga.datatype` | `Datatypes/tga` | TGA (our own decoder) |
| `webp.datatype` | `Datatypes/webp` (libwebp 1.6.0) | WebP pictures |
| `webm.datatype` | `Datatypes/webm` (libvpx 1.17.0) | WebM films |
| `opensound.datatype` | `Datatypes/opensound` | sound class (our own) |
| `openmodule.datatype` | `Datatypes/openmodule` (libxmp 4.7.3) | music modules: MOD, MED, Oktalyzer, DigiBooster, XM, S3M, IT |
| `opendoc.datatype` | `Datatypes/opendoc` | documents |
| `cdxl.datatype` | `Datatypes/cdxl` | CDXL films |
| `openvideo.datatype` | `Datatypes/openvideo` | video class |

They came here from DalsinAI/openamigaimage on 23 October 2026, with their
history, unchanged: the same files at the same paths, so they build to the
same bytes. Building, the descriptors and the test results are in
[Datatypes/README.md](Datatypes/README.md).

## Building

With the os32-gcc16 stove (68040, soft float through
`build/m68k-soft-m68040.specs`):

```
./Datatypes/build.sh
```

The datatypes land in `Datatypes/out/Classes/DataTypes` and their
descriptors in `Datatypes/out/Devs/DataTypes` (set `PREFIX` to change that).
`SOURCES` lists the upstream tarballs (libwebp, libvpx, libxmp) with their
SHA-256; `tools/fetch-tarball.sh` fetches them. `achelpers/` is AC090's
native helpers for `Datatypes/common/dtlib.c` (`AC_HELPERS`, see
`Datatypes/build.sh`), built from amigachrome-guest at the commit in
`AMIGACHROME_GUEST_PINNED_COMMIT`.

The ROM's datatypes (openpicture, openpng, opentga, opensound, opendoc in
ROM:Classes/DataTypes) are built from a checkout of this repository by
amigachrome-guest's `tools/openkickrom/trial-1m/build.sh` (`DT=`).

## Tests

- `sh Datatypes/tests/test-tga.sh` and `sh Datatypes/tests/test-cdxl.sh`:
  the decoders on the host under AddressSanitizer and UBSan (CI runs both).
- `sh tests/achelpers/test_achelpers.sh`: the AC090 helpers and dtlib.c's
  memory functions under qemu-m68k.
- `Datatypes/tests/dtpic.c`, `dtsound.c`, `dtanim.c`: the Amiga-side checks.
- `python3 -I tests/pack/test_pack.py`: the pack, built from stand-ins and its
  Installer scripts run on a stand-in AmigaOS 3.2 disk (CI runs it).

## The pack: OpenDatatypes, with an Installer

```
python3 tools/build_pack.py --version 0.1 --lha
```

builds everything here (the datatypes, and datatypes.library with its
system classes once `libraries/opendatatypes/` is in) and lays it out as
`build/pack/OpenDatatypes`, a drawer to run the Installer from, and
`OpenDatatypes-0.1.lha` (jlha or lha on PATH). `--no-build` takes what the
builds already made.

- `Install-OpenDatatypes`, a Commodore Installer script, needs AmigaOS 3.2
  and a 68040 or 68060 and nothing else. Every datatype is a choice, and
  datatypes.library with its eight system classes is one more, ticked; a
  Novice user gets them all. AddDataTypes REFRESH runs at the end.
- Nothing is deleted: what a file replaces is kept in
  `SYS:Storage/OpenDatatypes/Superseded`, and each file the pack put in has
  a mark in `SYS:Storage/OpenDatatypes/Installed`.
- `Uninstall-OpenDatatypes`, left in `SYS:Storage/OpenDatatypes`, takes out
  only what the pack put in and puts back what it replaced.

Both scripts are written by `tools/build_pack.py` from what the builds
made: a descriptor goes with the class its DTHD names, so a new datatype
needs no change there (`LABELS` gives it a friendlier name in the
Installer). `build/pack/OpenDatatypes.json` lists every file, where it
goes and its SHA-256; OpenUp's OpenImage part takes its files from it.

## Licence

Our code is MIT (see LICENSE). libwebp and libvpx keep their BSD licences
with the WebM patent grant, libxmp its MIT licence: their notices are in
`upstream/`. Who wrote what is in CONTRIBUTORS.md.

Designed, tested and delivered by The AmigaChrome Team.
Copyright (c) 2026 AmigaChrome Team
