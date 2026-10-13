# OpenDatatypes: an open datatypes.library

datatypes.library 47.100 for AmigaOS 3.2, API and ABI compatible, clean-room
(written from the autodocs, the NDK 3.2 headers' layouts and OS 3.2's
answers measured by `tests/DatatypesABITest`, never from Kickstart,
Hyperion or AROS code). It runs on a 68000. One of the open libraries; the
plan is [`../OPEN_LIBRARIES.md`](../OPEN_LIBRARIES.md).

## Steps

| Step | What | State |
| --- | --- | --- |
| D1 | The descriptor database (DEVS:DataTypes), recognition (ObtainDataTypeA by file and memory; ReleaseDataType), recognition code (DTCD), the V45 method tables, GetDTString, GetDTTriggerMethodDataFlags | this step: written, waiting on OS 3.2's probe |
| D2 | datatypesclass and the object calls: NewDTObjectA, DisposeDTObject, Set/GetDTAttrsA, Add/RemoveDTObject, RefreshDTObjectA, DoDTMethodA, DoAsyncLayout, Obtain/ReleaseDTDrawInfo, DrawDTObjectA (the private -120 to -132 slots), StartDragSelect, SaveDTObjectA, PrintDTObjectA; DTST_CLIPBOARD; the datatypes.library replacement | next |
| D3 | Launching a type's tools, sys/datatypes.catalog, our own AddDataTypes | after D2 |
| D4 | The class libraries (picture, sound, text, animation, amigaguide) | after D3 |

Until D2 it builds only as `opendatatypes.library`, the side-by-side twin
the ABI test opens next to OS 3.2's; its object calls answer as a failed
call does.

| File | What it is |
| --- | --- |
| `dt_core.c`, `.h` | Descriptor files, the lists recognition walks, recognition, the method tables, tools and strings, in plain C with no Amiga headers. |
| `opendatatypes_lib.c` | The library: RomTag, the jump table in datatypes.library's LVO order, each call in its documented registers; reads DEVS:DataTypes the first time it is asked, loads recognition code from memory with InternalLoadSeg. |
| `datatypes.lvo` | The jump table, LVO by LVO, as NDK 3.2's fd has it: -120 to -132 and -144 to -252 private, the V45 calls from -258. `../common/check_fd.py` compares it with the NDK's `datatypes_lib.fd`. |
| `ndk_check.c` | Every constant and layout `dt_core.h` carries, checked against the NDK 3.2 headers when it compiles. |
| `build-amiga.sh` | With the os32-gcc16 stove: the LVO and NDK checks, then `opendatatypes.library` and `DatatypesABITest`. |
| `tests/DatatypesABITest.c` | Runs the D1 calls through OS 3.2's datatypes.library and `opendatatypes.library` and prints each line that differs; PROBE prints OS 3.2's answers. DESC adds descriptors made to ask one question each (lab Amigas only), SCAN=dir recognises a directory's files, LIST dumps OS 3.2's own list. |

## What the autodocs leave open

`dt_core.c` marks each answer `measured` once OS 3.2's probe has given it,
or `guess` until then. Measured so far (labs at 1cba45a and 1ea835e):

- Recognition looks at the first 1084 bytes, from a file or from memory.
- Text is every byte but the C1 controls (0x80-0x9f) and the C0 controls
  other than BEL to CR, DC1, DC3 and ESC.
  Text tries only the ASCII types, binary only the binary types, IFF (FORM,
  CAT or LIST: FORM from 6 bytes on) only the IFF types; then the
  library's own type. No data gives no type and leaves IoErr alone; types
  0 and 99 give IoErr 236.
- Order: the longer mask first, then the lower priority (an unsigned word:
  0 beats 5, 1 beats 0xffff), then the newest.
- A mask without DTF_CASE ignores case; a negative mask word is any byte.
  A type without a mask needs a pattern other than "#?". An IFF type's ID
  isn't compared to the FORM's.
- Memory has no name: "#?" fits it, "#?.x" doesn't. A pattern without
  wildcards fits every name but its own (Stricmp), so such a type takes
  nearly all binary data. We copy this because OS 3.2 does it. Patterns see
  the file's name, not its path.
- A DTHD offset of 0 points at the header itself. The tools are not put in
  dtn_ToolList, even from one DTTL chunk. A DTCD that isn't a load file is
  ignored.
- Recognition code gets the context in A0 with every base, IFFParseBase
  included. We also push it on the stack for code with stack arguments.
  For data in memory it still gets a lock, an empty FIB and a file handle;
  ours answer DOS packets from a PA_SOFTINT port (read, seek, examine).

Open: whether the mask outranks the priority in every case.

## Tests

- `sh tests/m68k/test_opendatatypes.sh` (from the repository root): the
  core at -O0 and -O2 for the 68000 and 68040 under qemu-m68k.
- `DatatypesABITest PROBE DESC LIST`: OS 3.2's answers, on a lab Amiga.
- `DatatypesABITest DESC LIB=dir [SCAN=dir]`: ours against OS 3.2's.
