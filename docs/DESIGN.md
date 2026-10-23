# opendatatypes: the rules every datatype keeps

Design 0.1, 23 October 2026. The Team's decisions of that morning:

1. **One home.** Every datatype the Team makes, and datatypes.library with
   its system classes, lives in this repository.
2. **Every datatype writes**, where the licences allow: "all datatypes
   should have a write option if licensing allows".
3. **The best place first**: datatypes "should use our OpenUp capabilities
   to look for cores, GPU etc before falling back to a single core m68k".

Sections 1 and 2 say what rules 2 and 3 mean in code. Section 3 is where
each datatype stands and what it still needs.

## 1. Writing

`DTM_WRITE` has two modes, and a datatype answers both:

- **`DTWM_IFF`** writes the group's IFF form: ILBM for a picture, 8SVX for
  a sound, ANIM for an animation, FTXT for text. The system classes do it
  (`libraries/opendatatypes/classes/datatypes/`), so a subclass that leaves
  `DTM_WRITE` to its superclass already has it: what it decoded goes out as
  ILBM or 8SVX. That is what AmigaOS's own datatypes do, and clipboard
  copies (`DTM_COPY`) go the same way.
- **`DTWM_RAW`** writes the datatype's **own** format: a WebP for
  webp.datatype, a PNG for openpng.datatype. This is the new part. It is
  what lets a program save in a format through its datatype, with no
  library of its own (MultiView's Save As, OpenBrowser's image save, a
  converter in three calls: `NewDTObject` from one file, `SetDTAttrs` for
  the target datatype's options, `DoMethod(DTM_WRITE, DTWM_RAW)` to the
  other).

What raw mode writes, in order of preference:

1. The object as it is now, encoded in the datatype's format (after
   `PDTM_WRITEPIXELARRAY`, a scale, or new sound data). This is the real
   write option.
2. Where the object can't be encoded but came from a file of that format,
   the file as it came (openmodule.datatype does this today). That keeps
   Save working while 1 is written, and stays as the answer for formats
   that can't be made from what the object holds (section 2).

Options go through attributes the datatype documents (quality, lossless,
compression level, bit rate), with defaults that need no attribute at all.
Each datatype's README lists them.

A format whose encoder the Team may not ship (section 2's "no" rows) answers
`DTWM_RAW` with the original file if it has one, else `ERROR_NOT_IMPLEMENTED`,
and still writes IFF.

## 2. The ladder: where the work happens

Every datatype looks, when an object opens, for the places OpenUp offers,
and uses the first that is there for each kind of work. Only when none is
does it fall back to this CPU alone.

| Rung | What | Through | Good for |
| --- | --- | --- | --- |
| 1 | **A cores board core**: AmigaChrome's cores board ($DA15 product 7, up to eight translated 68040 cores) | `openmulticore.library` jobs (`OMC_Submit`, `OMC_Wait`), DalsinAI/openamigamulticore | 68k compute: decoding and encoding (libwebp, libvpx, our PNG and TGA code), mixing, conversion, scaling. A job calls no OS, touches no Chip RAM and writes only its granted buffers |
| 2 | **The GPU** | `opengpu.library` (the one library: OpenGPU, OpenGfx and SDL 2) | Pixel work: scaling, colour conversion, dithering to 256 colours or HAM8, presenting video frames. Video decode where a driver offers it, through `openmedia.library` once it exists |
| 3 | **A services card or a paired Cradle** | `openservice.device`: `media.decode/1`, `doc.render/1`, and the encode calls section 3 adds, DalsinAI/openamigaservice | Formats with no 68k code (AVIF, HEIC, office documents, most video), and heavy work a core can't take |
| 4 | **This CPU**, a single 68k | plain calls | Everything that has 68k code, when nothing above is there |

The rules:

- **Chosen per object, per kind of work**, when the object opens: decoding
  may go to a core while dithering goes to the GPU. A rung that fails (a
  job faults, the GPU refuses a surface, the service drops) falls to the
  next, as openmodule.datatype does when a core fails it.
- **The same answer on every rung.** A rung is faster, never different:
  each kind of work is tested against this CPU's result.
- **Rung 4 is always there for formats with 68k code.** Formats with none
  (rung 3 only) fail to open without a service, as they do today, and say
  why.
- **One piece of code chooses**, `Datatypes/common/`, shared by every
  datatype, so each one asks the same questions in the same order.
  openmodule.datatype's choice (`openmodule/DESIGN.md`, section 7) is the
  first user and the model.
- **Forcing a rung** for tests: `ENV:OpenImage/Rung` set to `cores`, `gpu`,
  `service` or `cpu`, per datatype as `ENV:OpenImage/<Datatype>Rung`
  (openmodule's `ModulePlayer` stays as a synonym). A forced rung that is
  not there falls to the next.
- **Saying where**: `OIA_DecodedBy`, `OIA_Stats` and `OIA_Rung`
  (`Datatypes/include/datatypes/openimage.h`) on every datatype, not only
  openmodule, so OpenPlay and MultiView's info can show it. `OIA_Rung`'s
  numbers get a GPU value (4) beside today's three.
- **Writing uses the same ladder.** Encoding is compute like decoding:
  WebP and VP9 encodes go to a core first, then the service, then this CPU.

`openmulticore.library` and `opengpu.library` are not in OpenUp yet, so
rungs 1 and 2 are only there where they are installed by hand. OpenUp
should carry both before the datatypes rely on them.

## 3. Where each datatype stands

"Licence" is about shipping an encoder: the Team's code, or an encoder whose
licence and patents let the Team ship it. It is the Team's reading, not
legal advice; the "no" rows need the Team's word before anyone writes them.

### The Team's datatypes (`Datatypes/`)

| Datatype | Raw write today | Raw write to add | Licence | Ladder today | Ladder to add |
| --- | --- | --- | --- | --- | --- |
| webp | none | WebP, lossy and lossless, with alpha | yes: libwebp's encoder is in the same BSD tarball | this CPU | encode and decode as core jobs; the service |
| webm | none | WebM, VP8 or VP9, with Opus or Vorbis sound | yes: libvpx (BSD, WebM patent grant); Opus and Vorbis are BSD and royalty-free | this CPU (sound by the service) | frames as core jobs; dithering on the GPU |
| openpng | none (PNG out in #2) | PNG, every colour type | yes: our own code | this CPU | core jobs |
| opentga | none | TGA 2.0, uncompressed and RLE | yes: our own code, Truevision's public spec | this CPU | core jobs |
| cdxl | none | CDXL, the film as it is | yes: our own code | this CPU | GPU for chunky films' dithering |
| openmodule | the module as it came | (IFF: the song rendered to 8SVX) | yes for 8SVX (libxmp, MIT); a module can't be made from sound | cores, service, this CPU | the GPU has nothing to do here |
| openpicture | none | AVIF, JPEG XL, QOI, OpenEXR, Radiance HDR, JPEG 2000, DDS through the service | yes: libavif, libjxl, OpenJPEG and OpenEXR are BSD; AV1 is royalty-free. **No**: HEIC (HEVC patents), camera RAW and fonts (nothing to write); PSD and XCF later | the service | scaling on the GPU |
| opensound | none | FLAC, Ogg Vorbis, Opus, ALAC, MP3 through the service | yes: FLAC, Vorbis, Opus (BSD), ALAC (Apache 2.0), MP3 (LAME runs in the service; the patents ended in 2017). **No**: AAC and WMA (encoder licences and patents); MIDI, SID and modules can't be made from sound | the service | decode as core jobs where there is 68k code (FLAC first) |
| opendoc | none | the document as it came; converted by the service to ODF, PDF or DOCX | yes: LibreOffice (MPL 2.0) runs as its own program on the Cradle | the service | page scaling on the GPU |
| openvideo | none | WebM (VP9 or AV1 with Opus), MKV with the same, MPEG-1 | yes for those (royalty-free; MPEG-1's patents have ended). **No**: MP4/H.264, HEVC, WMV (encoder patents) | the service | HAM8 and 256-colour conversion on the GPU, then cores |
| stream, gzip, brotli | write in #1 | done there | yes: zlib licence and MIT | this CPU | core jobs for compression |

### datatypes.library's system classes (`libraries/opendatatypes/`, arriving in #3)

| Class | IFF write today | To add |
| --- | --- | --- |
| picture | ILBM | the ladder for scaling and remapping (`PDTM_SCALE`, the pen remap) |
| sound | 8SVX | none |
| text, ascii | FTXT, and raw text | none |
| animation, anim | **none** | ANIM writing (ANIM-5 deltas), so every animation datatype gets IFF write |
| ilbm, 8svx | through picture and sound | raw write of their own forms |

## 4. Order of work

1. `Datatypes/common/`: the shared ladder code, lifted from
   openmodule.datatype, with `ENV:OpenImage/Rung` and `OIA_*` on every
   datatype. Openmodule moves onto it with no change in behaviour.
2. Raw write for the Team's own formats: opentga, openpng (with #2),
   cdxl, then webp.
3. ANIM write in the animation class, so every animation datatype writes
   IFF.
4. Core jobs for webp, openpng and opentga decoding and encoding; GPU
   dithering for webm, cdxl and openvideo.
5. The service's encode calls (`media.encode/1`, `doc.convert/1`, in
   openamigaservice), then raw write for openpicture, opensound, opendoc,
   openvideo and webm.

Each step: a lab copy of an OS 3.2 instance, the same answer on every rung
against this CPU's, and the timings in the datatype's README.
