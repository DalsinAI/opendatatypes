/* OpenDatatypes: an open datatypes.library for AmigaOS 3.2, API and ABI
 * compatible.
 * Copyright (c) 2026 Dalsin Limited. MIT.
 *
 * Clean-room: written from datatypes.library's autodocs, the NDK 3.2
 * headers' published layouts and OS 3.2's answers measured by
 * tests/DatatypesABITest, never from Kickstart, Hyperion or AROS code or
 * bytes. The work is done in dt_core.c, which is plain C and is tested
 * under qemu-m68k (tests/m68k/test_opendatatypes.sh); this file is the
 * library around it: the RomTag, the vector table in datatypes.library's
 * LVO order (datatypes.lvo, which build-amiga.sh checks against the NDK's
 * datatypes_lib.fd), each call taking its arguments in the documented
 * registers, the descriptor files in DEVS:DataTypes and their recognition
 * code.
 *
 * Step D1 (this file today): the descriptor database, ObtainDataTypeA and
 * ReleaseDataType, the V45 method tables and strings. The object calls
 * (NewDTObjectA and the rest) and datatypesclass come with step D2, so
 * until then this builds only as opendatatypes.library, the side-by-side
 * twin tests/DatatypesABITest opens next to OS 3.2's own, and its object
 * calls answer as a failed call does.
 */
#include <stddef.h>
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/doshunks.h>
#include <dos/dostags.h>
#include <libraries/iffparse.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/iffparse.h>

#include "dt_core.h"

/* V44/V45 names, in case the NDK leaves them out */
#ifndef DTST_MEMORY
#define DTST_MEMORY 5
#endif
#ifndef DTA_SourceAddress
#define DTA_SourceAddress (DTA_Dummy + 39)
#endif
#ifndef DTA_SourceSize
#define DTA_SourceSize (DTA_Dummy + 40)
#endif
#ifndef DTERROR_UNKNOWN_DATATYPE
#define DTERROR_UNKNOWN_DATATYPE 2000
#endif

#define REG(r, decl) register decl __asm(#r)

#ifndef LIBNAME
#define LIBNAME "opendatatypes.library"
#endif
#define LIB_VERSION  47
#define LIB_REVISION 100                /* 47.100 and up: OpenDatatypes */

/* How much of the data recognition looks at (measured on OS 3.2: the hook's
 * BufferLength is the size up to 1084, from a file or from memory) */
#define PEEK 1084
/* The biggest descriptor file read */
#define MAX_DESCRIPTOR 65536

struct ODBase {
    struct Library lib;
    BPTR seglist;
    struct SignalSemaphore lock;        /* the database and every entry's uses */
    struct odt_db db;
    UBYTE loaded;                       /* DEVS:DataTypes read */
};

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;         /* as NDK 3.2's proto/utility.h has it */
struct Library *IFFParseBase;

/* Run as a program, the library does nothing. This must stay the first code
 * in the file (-fno-toplevel-reorder). */
int start(void) { return -1; }

static const char lib_name[] = LIBNAME;
static const char lib_id[] =
    "datatypes.library 47.100 (13.10.2026) OpenDatatypes, Dalsin Limited\r\n";
static const char lib_ver[] __attribute__((used)) =
    "\0$VER: datatypes.library 47.100 (13.10.2026) OpenDatatypes, Dalsin Limited";
static const char dos_name[] = "dos.library";
static const char utility_name[] = "utility.library";
static const char iffparse_name[] = "iffparse.library";
static const char devs_datatypes[] = "DEVS:DataTypes";

/* ---- what the core asks of the system --------------------------------------- */

static ULONG strlen_(const char *s)
{
    ULONG n = 0;
    while (s[n]) n++;
    return n;
}

static void memset_(void *p, ULONG n)
{
    UBYTE *b = p;
    while (n--) *b++ = 0;
}

static void *env_alloc(void *ctx, odt_u32 n) { return AllocVec(n, MEMF_ANY | MEMF_CLEAR); }
static void env_free(void *ctx, void *p) { FreeVec(p); }

static int env_match(void *ctx, const char *pattern, const char *name)
{
    ULONG n = 2 * strlen_(pattern) + 2;
    STRPTR tok;
    LONG wild;
    int r;
    if (!(tok = AllocVec(n, MEMF_ANY))) return 0;
    wild = ParsePatternNoCase((CONST_STRPTR)pattern, tok, n);
    if (wild < 0) r = 0;
    else if (wild == 0) r = -1;             /* no wildcards: the core compares */
    else r = MatchPatternNoCase(tok, (STRPTR)name) != 0;
    FreeVec(tok);
    return r;
}

/* Recognition code: a DTCD chunk is a load file whose first code is
 * BOOL hook(struct DTHookContext *) taking the context in A0 (and, for
 * code written with stack arguments, on the stack as well). It is loaded
 * from memory with InternalLoadSeg the first time it is needed. */
struct memfile { const UBYTE *at; ULONG left; };

static LONG ls_read(REG(d1, struct memfile *f), REG(d2, UBYTE *buf), REG(d3, LONG len))
{
    LONG n = len < 0 ? 0 : ((ULONG)len > f->left ? (LONG)f->left : len);
    CopyMem((APTR)f->at, buf, n);
    f->at += n;
    f->left -= n;
    return n;
}

static APTR ls_alloc(REG(d0, ULONG size), REG(d1, ULONG flags), REG(a6, struct ExecBase *sys))
{
    return AllocMem(size, flags);
}

static void ls_free(REG(a1, APTR mem), REG(d0, ULONG size), REG(a6, struct ExecBase *sys))
{
    FreeMem(mem, size);
}

static const APTR ls_funcs[] = { (APTR)ls_read, (APTR)ls_alloc, (APTR)ls_free };

static ULONG call_hook(APTR fn, struct DTHookContext *dthc)
{
    register ULONG r __asm("d0");
    register APTR a0 __asm("a0") = dthc;
    register APTR a1 __asm("a1") = fn;
    __asm volatile ("move.l %1,-(%%sp)\n\tjsr (%2)\n\taddq.l #4,%%sp"
                    : "=d"(r), "+a"(a0), "+a"(a1) : : "d1", "cc", "memory");
    return r;
}

static int env_hook(void *ctx, struct odt_entry *e, struct odt_DTHookContext *dthc)
{
    if (!e->seg) {
        struct memfile f;
        LONG stack = 0;
        f.at = e->code;
        f.left = e->code_len;
        e->seg = (void *)InternalLoadSeg((BPTR)&f, 0, (LONG *)ls_funcs, &stack);
        /* measured: code that isn't a load file is no code; the mask decides */
        if (!e->seg) { e->code = NULL; return 1; }
    }
    if (dthc->dthc_FileHandle) Seek((BPTR)dthc->dthc_FileHandle, 0, OFFSET_BEGINNING);
    /* BOOL: the low word */
    return (call_hook(BADDR((BPTR)e->seg) + 4, (struct DTHookContext *)dthc) & 0xffff) != 0;
}

static const struct odt_env env = { NULL, env_alloc, env_free, env_match, env_hook };

static void free_entry(struct odt_entry *e)
{
    if (e->seg) InternalUnLoadSeg((BPTR)e->seg, (VOID (*)())ls_free);
    odt_free_entry(&env, e);
}

/* ---- data in memory as a file ------------------------------------------------- */

/* Recognition code for data in memory gets a lock, a FIB and a file handle
 * all the same, as OS 3.2's does (measured: their port's SigTask is an
 * object named "datatypes.library", the FIB is empty). Ours is a
 * PA_SOFTINT port: each packet DOS sends it is answered in the software
 * interrupt, before the sender's WaitPort returns. It reads, seeks and
 * examines the memory; anything else fails with ERROR_ACTION_NOT_KNOWN. */
struct memstream { const UBYTE *at; LONG size, pos; BPTR lock; };

static struct MsgPort mem_port;
static struct Interrupt mem_int;
static const char mem_name[] = "datatypes.library";

static struct memstream *stream_of_lock(LONG lock)
{
    return (struct memstream *)((struct FileLock *)BADDR((BPTR)lock))->fl_Key;
}

static void mem_examine(struct memstream *ms, struct FileInfoBlock *fib)
{
    memset_(fib, sizeof *fib);
    fib->fib_DirEntryType = ST_FILE;
    fib->fib_EntryType = ST_FILE;
    fib->fib_Size = ms->size;
    fib->fib_NumBlocks = (ms->size + 511) / 512;
    /* the names are BSTRs here: empty */
}

static void mem_packet(struct DosPacket *dp)
{
    struct memstream *ms;
    LONG r1 = DOSFALSE, r2 = 0, n;
    switch (dp->dp_Type) {
    case ACTION_READ:
        ms = (struct memstream *)dp->dp_Arg1;
        n = dp->dp_Arg3;
        if (n < 0) n = 0;
        if (n > ms->size - ms->pos) n = ms->size - ms->pos;
        CopyMem((APTR)(ms->at + ms->pos), (APTR)dp->dp_Arg2, n);
        ms->pos += n;
        r1 = n;
        break;
    case ACTION_SEEK: {
        LONG to = dp->dp_Arg2;
        ms = (struct memstream *)dp->dp_Arg1;
        if (dp->dp_Arg3 == OFFSET_CURRENT) to += ms->pos;
        else if (dp->dp_Arg3 == OFFSET_END) to += ms->size;
        if (to < 0 || to > ms->size) { r1 = -1; r2 = ERROR_SEEK_ERROR; break; }
        r1 = ms->pos;
        ms->pos = to;
        break;
    }
    case ACTION_EXAMINE_OBJECT:
        mem_examine(stream_of_lock(dp->dp_Arg1), (struct FileInfoBlock *)BADDR((BPTR)dp->dp_Arg2));
        r1 = DOSTRUE;
        break;
    case ACTION_EXAMINE_FH:
        mem_examine((struct memstream *)dp->dp_Arg1, (struct FileInfoBlock *)BADDR((BPTR)dp->dp_Arg2));
        r1 = DOSTRUE;
        break;
    case ACTION_COPY_DIR:                   /* DupLock: the one lock serves */
        r1 = dp->dp_Arg1;
        break;
    case ACTION_COPY_DIR_FH:
        r1 = (LONG)((struct memstream *)dp->dp_Arg1)->lock;
        break;
    case ACTION_FREE_LOCK:
    case ACTION_END:                        /* the library frees them */
        r1 = DOSTRUE;
        break;
    case ACTION_PARENT:
    case ACTION_PARENT_FH:
    case ACTION_IS_FILESYSTEM:
        break;
    default:
        r2 = ERROR_ACTION_NOT_KNOWN;
        break;
    }
    dp->dp_Res1 = r1;
    dp->dp_Res2 = r2;
}

static void mem_softint(REG(a1, struct MsgPort *port))
{
    struct Message *m;
    while ((m = GetMsg(port))) {
        struct DosPacket *dp = (struct DosPacket *)m->mn_Node.ln_Name;
        struct MsgPort *reply = dp->dp_Port;
        mem_packet(dp);
        dp->dp_Port = port;
        PutMsg(reply, dp->dp_Link);
    }
}

static void mem_port_init(void)
{
    if (mem_int.is_Code) return;
    mem_int.is_Node.ln_Type = NT_INTERRUPT;
    mem_int.is_Node.ln_Name = (char *)mem_name;
    mem_int.is_Data = &mem_port;
    mem_int.is_Code = (VOID (*)())(void (*)(void))mem_softint;
    mem_port.mp_Node.ln_Type = NT_MSGPORT;
    mem_port.mp_Node.ln_Name = (char *)mem_name;
    mem_port.mp_Flags = PA_SOFTINT;
    mem_port.mp_SigTask = &mem_int;
    mem_port.mp_MsgList.lh_Head = (struct Node *)&mem_port.mp_MsgList.lh_Tail;
    mem_port.mp_MsgList.lh_Tail = NULL;
    mem_port.mp_MsgList.lh_TailPred = (struct Node *)&mem_port.mp_MsgList.lh_Head;
    mem_port.mp_MsgList.lh_Type = NT_MESSAGE;
}

/* ---- the database --------------------------------------------------------------- */

static void add_entry(struct ODBase *base, struct odt_entry *e)
{
    struct odt_entry *old;
    if (!e) return;
    old = odt_db_add(&base->db, e);
    /* an old one still in use stays allocated (it is out of the lists) */
    if (old && !old->uses) free_entry(old);
}

/* Each file in DEVS:DataTypes, in the directory's order, as AddDataTypes
 * reads them (guess: until the probe measures OS 3.2's order). */
static void load_dir(struct ODBase *base)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    BPTR dir = Lock((CONST_STRPTR)devs_datatypes, ACCESS_READ), old;
    UBYTE *buf = AllocVec(MAX_DESCRIPTOR, MEMF_ANY);

    if (fib && dir && buf && Examine(dir, fib) && fib->fib_DirEntryType > 0) {
        old = CurrentDir(dir);
        while (ExNext(dir, fib)) {
            BPTR fh;
            LONG n;
            ULONG l = strlen_((char *)fib->fib_FileName);
            struct odt_entry *e;
            if (fib->fib_DirEntryType > 0) continue;
            if (l > 5 && !Stricmp((CONST_STRPTR)fib->fib_FileName + l - 5, (CONST_STRPTR)".info"))
                continue;
            if (!(fh = Open((CONST_STRPTR)fib->fib_FileName, MODE_OLDFILE))) continue;
            n = Read(fh, buf, MAX_DESCRIPTOR);
            Close(fh);
            if (n > 0 && odt_parse(&env, buf, (ULONG)n, &e) == ODT_OK) add_entry(base, e);
        }
        CurrentDir(old);
    }
    FreeVec(buf);
    if (dir) UnLock(dir);
    if (fib) FreeDosObject(DOS_FIB, fib);
}

static void load(struct ODBase *base)
{
    if (base->loaded) return;
    base->loaded = 1;
    if (!IFFParseBase) IFFParseBase = OpenLibrary((CONST_STRPTR)iffparse_name, 39);
    mem_port_init();
    add_entry(base, odt_builtin(&env, "binary", "binary", GID_SYSTEM, MAKE_ID('b','i','n','a'), DTF_BINARY, 0));
    add_entry(base, odt_builtin(&env, "ascii", "ascii", GID_TEXT, MAKE_ID('a','s','c','i'), DTF_ASCII, 0));
    add_entry(base, odt_builtin(&env, "iff", "iff", GID_SYSTEM, MAKE_ID('i','f','f',0), DTF_IFF, 0));
    add_entry(base, odt_builtin(&env, "directory", "directory", GID_SYSTEM, MAKE_ID('d','i','r','e'), DTF_MISC, 0));
    load_dir(base);
}

/* ---- Library housekeeping -------------------------------------------- */

static struct Library *lib_init(REG(d0, struct ODBase *base), REG(a0, BPTR seglist),
                                REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    base->seglist = seglist;
    base->lib.lib_Node.ln_Type = NT_LIBRARY;
    base->lib.lib_Node.ln_Name = (char *)lib_name;
    base->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->lib.lib_Version = LIB_VERSION;
    base->lib.lib_Revision = LIB_REVISION;
    base->lib.lib_IdString = (APTR)lib_id;
    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)dos_name, 39);
    UtilityBase = OpenLibrary((CONST_STRPTR)utility_name, 39);
    if (!DOSBase || !UtilityBase) {
        if (DOSBase) CloseLibrary((struct Library *)DOSBase);
        if (UtilityBase) CloseLibrary(UtilityBase);
        FreeMem((UBYTE *)base - base->lib.lib_NegSize, base->lib.lib_NegSize + base->lib.lib_PosSize);
        return NULL;
    }
    InitSemaphore(&base->lock);
    odt_db_init(&base->db);
    return &base->lib;
}

static struct Library *lib_open(REG(a6, struct ODBase *base))
{
    base->lib.lib_OpenCnt++;
    base->lib.lib_Flags &= ~LIBF_DELEXP;
    return &base->lib;
}

static BPTR lib_expunge(REG(a6, struct ODBase *base));

static BPTR lib_close(REG(a6, struct ODBase *base))
{
    if (base->lib.lib_OpenCnt)
        base->lib.lib_OpenCnt--;
    if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP))
        return lib_expunge(base);
    return 0;
}

static BPTR lib_expunge(REG(a6, struct ODBase *base))
{
    BPTR seglist;
    struct odt_Node *n;

    if (base->lib.lib_OpenCnt || !base->seglist) {
        base->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    /* a DataType somebody still holds keeps the library */
    for (n = base->db.all.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct odt_entry *e = (struct odt_entry *)((UBYTE *)n - offsetof(struct odt_DataType, dtn_Node2));
        if (e->uses) {
            base->lib.lib_Flags |= LIBF_DELEXP;
            return 0;
        }
    }
    while ((n = base->db.all.lh_Head)->ln_Succ) {
        struct odt_entry *e = (struct odt_entry *)((UBYTE *)n - offsetof(struct odt_DataType, dtn_Node2));
        odt_db_remove(e);
        free_entry(e);
    }
    seglist = base->seglist;
    Remove(&base->lib.lib_Node);
    if (IFFParseBase) CloseLibrary(IFFParseBase);
    CloseLibrary(UtilityBase);
    CloseLibrary((struct Library *)DOSBase);
    FreeMem((UBYTE *)base - base->lib.lib_NegSize,
            base->lib.lib_NegSize + base->lib.lib_PosSize);
    return seglist;
}

static ULONG lib_null(void)
{
    return 0;
}

/* ---- ObtainDataTypeA and friends ------------------------------------------------ */

/* Recognition with the context recognition code gets (parsed as IFF when
 * the data is IFF) */
static struct odt_entry *recognize(struct ODBase *base, const UBYTE *buf, LONG n, const char *name,
                                   BPTR lock, struct FileInfoBlock *fib, BPTR fh)
{
    struct DTHookContext dthc;
    struct IFFHandle *iff = NULL;
    struct odt_entry *e;
    odt_u32 form;

    memset_(&dthc, sizeof dthc);
    dthc.dthc_SysBase = (struct Library *)SysBase;
    dthc.dthc_DOSBase = (struct Library *)DOSBase;
    dthc.dthc_UtilityBase = UtilityBase;
    dthc.dthc_IFFParseBase = IFFParseBase;  /* measured: always there */
    dthc.dthc_Lock = lock;
    dthc.dthc_FIB = fib;
    dthc.dthc_FileHandle = fh;
    dthc.dthc_Buffer = (STRPTR)buf;
    dthc.dthc_BufferLength = n;
    if (n && fh && odt_classify(buf, n, &form) == DTF_IFF && IFFParseBase && (iff = AllocIFF())) {
        Seek(fh, 0, OFFSET_BEGINNING);
        iff->iff_Stream = fh;
        InitIFFasDOS(iff);
        if (OpenIFF(iff, IFFF_READ)) { FreeIFF(iff); iff = NULL; }
        dthc.dthc_IFF = iff;
    }
    e = odt_recognize(&env, &base->db, buf, n, name, (struct odt_DTHookContext *)&dthc);
    if (iff) { CloseIFF(iff); FreeIFF(iff); }
    return e;
}

/* Recognition for memory: the bytes, and a lock, empty FIB and file
 * handle on them through mem_port */
static struct odt_entry *recognize_memory(struct ODBase *base, const UBYTE *at, ULONG size)
{
    struct memstream ms;
    struct FileLock *fl = AllocVec(sizeof *fl, MEMF_PUBLIC | MEMF_CLEAR);
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    struct FileHandle *fh = AllocDosObject(DOS_FILEHANDLE, NULL);
    struct odt_entry *e = NULL;

    if (fl && fib && fh) {
        ms.at = at;
        ms.size = (LONG)size;
        ms.pos = 0;
        ms.lock = MKBADDR(fl);
        fl->fl_Key = (LONG)&ms;
        fl->fl_Access = SHARED_LOCK;
        fl->fl_Task = &mem_port;
        /* fh_Type and fh_Arg1, by their place in the 68k layout (the
         * NDKs name the second one differently) */
        ((LONG *)fh)[2] = (LONG)&mem_port;
        ((LONG *)fh)[9] = (LONG)&ms;
        e = recognize(base, at, size < PEEK ? size : PEEK, NULL, ms.lock, fib, MKBADDR(fh));
    }
    if (fh) FreeDosObject(DOS_FILEHANDLE, fh);
    if (fib) FreeDosObject(DOS_FIB, fib);
    FreeVec(fl);
    return e;
}

/* Recognition for a file: its first bytes, its name, and the context its
 * recognition code gets (the file open, and parsed as IFF when it is). */
static struct odt_entry *recognize_file(struct ODBase *base, BPTR lock)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    struct odt_entry *e = NULL;
    UBYTE *buf = AllocVec(PEEK, MEMF_ANY);
    BPTR dup, fh = 0;
    LONG n;

    if (!fib || !buf) goto out;
    if (!Examine(lock, fib)) goto out;
    if (fib->fib_DirEntryType > 0) {
        e = odt_db_find_name(&base->db, "directory");
        goto out;
    }
    if (!(dup = DupLock(lock))) goto out;
    if (!(fh = OpenFromLock(dup))) { UnLock(dup); goto out; }
    n = Read(fh, buf, PEEK);
    if (n < 0) goto out;
    e = recognize(base, buf, n, (const char *)fib->fib_FileName, lock, fib, fh);
out:
    if (fh) Close(fh);
    if (fib) FreeDosObject(DOS_FIB, fib);
    FreeVec(buf);
    return e;
}

static struct DataType *L_ObtainDataTypeA(REG(d0, ULONG type), REG(a0, APTR handle),
                                          REG(a1, struct TagItem *attrs),
                                          REG(a6, struct ODBase *base))
{
    struct odt_entry *e = NULL;

    ObtainSemaphore(&base->lock);
    load(base);
    switch (type) {
    case DTST_RAM:
        /* OS 3.2: not done, IoErr ERROR_NOT_IMPLEMENTED */
        ReleaseSemaphore(&base->lock);
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        return NULL;
    case DTST_FILE:
        if (handle) e = recognize_file(base, (BPTR)handle);
        break;
    case DTST_MEMORY: {
        UBYTE *at = (UBYTE *)GetTagData(DTA_SourceAddress, 0, attrs);
        ULONG size = GetTagData(DTA_SourceSize, 0, attrs);
        if (at && size) e = recognize_memory(base, at, size);
        break;
    }
    default:
        /* measured: OS 3.2 answers types 0 and 99 so. DTST_CLIPBOARD comes
         * with step D2 */
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        break;
    }
    if (e) e->uses++;
    ReleaseSemaphore(&base->lock);
    /* measured: no data (or no address or size) leaves IoErr as it was */
    return e ? (struct DataType *)&e->dt : NULL;
}

static void L_ReleaseDataType(REG(a0, struct DataType *dt), REG(a6, struct ODBase *base))
{
    struct odt_entry *e = (struct odt_entry *)dt;
    if (!e) return;
    ObtainSemaphore(&base->lock);
    if (e->uses && !--e->uses && !e->dt.dtn_Node2.ln_Succ) free_entry(e);   /* replaced meanwhile */
    ReleaseSemaphore(&base->lock);
}


/* ---- tools, method tables, strings ------------------------------------------------ */


static ULONG *L_FindMethod(REG(a0, ULONG *methods), REG(a1, ULONG id))
{
    return (ULONG *)odt_FindMethod((odt_u32 *)methods, id);
}

static struct DTMethod *L_FindTriggerMethod(REG(a0, struct DTMethod *methods),
                                            REG(a1, STRPTR command), REG(d0, ULONG method))
{
    return (struct DTMethod *)odt_FindTriggerMethod((struct odt_DTMethod *)methods,
                                                    (const char *)command, method);
}

static ULONG *L_CopyDTMethods(REG(a0, ULONG *methods), REG(a1, ULONG *include),
                              REG(a2, ULONG *exclude))
{
    return (ULONG *)odt_CopyDTMethods(&env, (const odt_u32 *)methods, (const odt_u32 *)include,
                                      (const odt_u32 *)exclude);
}

static struct DTMethod *L_CopyDTTriggerMethods(REG(a0, struct DTMethod *methods),
                                               REG(a1, struct DTMethod *include),
                                               REG(a2, struct DTMethod *exclude))
{
    return (struct DTMethod *)odt_CopyDTTriggerMethods(&env, (const struct odt_DTMethod *)methods,
                                                       (const struct odt_DTMethod *)include,
                                                       (const struct odt_DTMethod *)exclude);
}

static void L_FreeDTMethods(REG(a0, APTR methods))
{
    FreeVec(methods);
}

static ULONG L_GetDTTriggerMethodDataFlags(REG(d0, ULONG method))
{
    return odt_GetDTTriggerMethodDataFlags(method);
}

static CONST_STRPTR L_GetDTString(REG(d0, ULONG id))
{
    return (CONST_STRPTR)odt_GetDTString(id);
}

/* ---- step D2: objects and datatypesclass ------------------------------------------ */

/* Until then each answers as the call does when it fails: no object, no
 * attributes, nothing done. */
static ULONG L_none(void)
{
    return 0;
}

/* datatypes.library's LVO order as NDK 3.2's fd has it; datatypes.lvo says
 * the same, line for line. The private slots (-120 to -132, -144 to -252)
 * answer 0. */
static const APTR lib_vectors[] = {
    (APTR)lib_open, (APTR)lib_close, (APTR)lib_expunge, (APTR)lib_null,
    (APTR)lib_null,                      /* -30 private */
    (APTR)L_ObtainDataTypeA,             /* -36 */
    (APTR)L_ReleaseDataType,             /* -42 */
    (APTR)L_none,                        /* -48 NewDTObjectA */
    (APTR)L_none,                        /* -54 DisposeDTObject */
    (APTR)L_none,                        /* -60 SetDTAttrsA */
    (APTR)L_none,                        /* -66 GetDTAttrsA */
    (APTR)L_none,                        /* -72 AddDTObject */
    (APTR)L_none,                        /* -78 RefreshDTObjectA */
    (APTR)L_none,                        /* -84 DoAsyncLayout */
    (APTR)L_none,                        /* -90 DoDTMethodA */
    (APTR)L_none,                        /* -96 RemoveDTObject */
    (APTR)L_none,                        /* -102 GetDTMethods */
    (APTR)L_none,                        /* -108 GetDTTriggerMethods */
    (APTR)L_none,                        /* -114 PrintDTObjectA */
    (APTR)L_none,                        /* -120 private */
    (APTR)L_none,                        /* -126 private */
    (APTR)L_none,                        /* -132 private */
    (APTR)L_GetDTString,                 /* -138 */
    (APTR)L_none,                        /* -144 private */
    (APTR)L_none,                        /* -150 private */
    (APTR)L_none,                        /* -156 private */
    (APTR)L_none,                        /* -162 private */
    (APTR)L_none,                        /* -168 private */
    (APTR)L_none,                        /* -174 private */
    (APTR)L_none,                        /* -180 private */
    (APTR)L_none,                        /* -186 private */
    (APTR)L_none,                        /* -192 private */
    (APTR)L_none,                        /* -198 private */
    (APTR)L_none,                        /* -204 private */
    (APTR)L_none,                        /* -210 private */
    (APTR)L_none,                        /* -216 private */
    (APTR)L_none,                        /* -222 private */
    (APTR)L_none,                        /* -228 private */
    (APTR)L_none,                        /* -234 private */
    (APTR)L_none,                        /* -240 private */
    (APTR)L_none,                        /* -246 private */
    (APTR)L_none,                        /* -252 private */
    (APTR)L_FindMethod,                  /* -258 */
    (APTR)L_FindTriggerMethod,           /* -264 */
    (APTR)L_CopyDTMethods,               /* -270 */
    (APTR)L_CopyDTTriggerMethods,        /* -276 */
    (APTR)L_FreeDTMethods,               /* -282 */
    (APTR)L_GetDTTriggerMethodDataFlags, /* -288 */
    (APTR)L_none,                        /* -294 SaveDTObjectA */
    (APTR)L_none,                        /* -300 StartDragSelect */
    (APTR)-1
};

static const struct {
    ULONG size;
    const APTR *vectors;
    APTR data;
    APTR init;
} lib_inittable = {
    sizeof(struct ODBase), lib_vectors, NULL, (APTR)lib_init
};

const struct Resident lib_romtag = {
    RTC_MATCHWORD, (struct Resident *)&lib_romtag, (APTR)(&lib_romtag + 1),
    RTF_AUTOINIT, LIB_VERSION, NT_LIBRARY, 0,
    (char *)lib_name, (char *)lib_id, (APTR)&lib_inittable
};
