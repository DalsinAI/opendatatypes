/* AddDataTypes: OpenDatatypes' own, for datatypes.library 47.100 and up
 * (OpenDatatypes).
 * Copyright (c) 2026 Dalsin Limited. MIT.
 *
 *   AddDataTypes [FILES/M] [QUIET/S] [REFRESH/S] [LIST/S]
 *
 * FILES: descriptor files to add (patterns allowed); a type of the same
 * name is replaced. REFRESH reads DEVS:DataTypes again. LIST prints every
 * type the library knows. Written from the command's documented template
 * and behaviour (the AmigaDOS manual), not from OS 3.2's command. It talks
 * to the library through OpenDatatypes' private call at -252 (registers
 * a0 files, d0 flags, a1 hook), so it needs OpenDatatypes: with any other
 * datatypes.library it says so and fails.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <dos/rdargs.h>
#include <datatypes/datatypes.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>

#define ODT_ADT_REFRESH 1
#define ODT_ADT_LIST    2
#define MAXFILES 256
#define PATHLEN 512

struct Library *DataTypesBase;

static const char version[] __attribute__((used)) = "\0$VER: AddDataTypes 47.100 (13.10.2026) OpenDatatypes";
static const char template[] = "FILES/M,QUIET/S,REFRESH/S,LIST/S";

/* OpenDatatypes' private call: ULONG AddDataTypesA(STRPTR *files, ULONG flags, struct Hook *hook) */
static ULONG add_types(STRPTR *files, ULONG flags, struct Hook *hook)
{
    register ULONG d0 __asm("d0") = flags;
    register STRPTR *a0 __asm("a0") = files;
    register struct Hook *a1 __asm("a1") = hook;
    register struct Library *d1 __asm("d1") = DataTypesBase;
    /* A6 saved by hand: it may be the frame pointer */
    __asm volatile ("move.l %%a6,-(%%sp)\n\tmove.l %3,%%a6\n\tjsr -252(%%a6)\n\tmove.l (%%sp)+,%%a6"
                    : "+r"(d0), "+r"(a0), "+r"(a1), "+r"(d1) : : "cc", "memory");
    return d0;
}

static int contains(const char *s, const char *what)
{
    int i, j;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        for (j = 0; what[j] && s[i + j] == what[j]; j++) ;
        if (!what[j]) return 1;
    }
    return 0;
}

/* LIST: one line a type, its name, its class and its group */
#define REG(r, decl) register decl __asm(#r)
static ULONG list_one(REG(a0, struct Hook *hook), REG(a2, struct DataType *dt), REG(a1, APTR msg))
{
    struct DataTypeHeader *h = dt->dtn_Header;
    CONST_STRPTR group = GetDTString(h->dth_GroupID);
    ULONG args[3];
    args[0] = (ULONG)h->dth_Name;
    args[1] = (ULONG)h->dth_BaseName;
    args[2] = (ULONG)(group ? group : (CONST_STRPTR)"");
    VPrintf((CONST_STRPTR)"%-28s %-14s %s\n", (APTR)args);
    return 0;
}

int main(void)
{
    LONG opts[4] = { 0, 0, 0, 0 };
    struct RDArgs *rda;
    STRPTR *files = NULL, *names;
    struct AnchorPath *ap = NULL;
    struct Hook hook;
    ULONG n = 0, added, flags = 0;
    int rc = RETURN_FAIL;

    if (!(rda = ReadArgs((CONST_STRPTR)template, opts, NULL))) {
        PrintFault(IoErr(), (CONST_STRPTR)"AddDataTypes");
        return RETURN_FAIL;
    }
    DataTypesBase = OpenLibrary((CONST_STRPTR)"datatypes.library", 47);
    /* the private vector add_types calls is 47.100's */
    if (!DataTypesBase || !contains((const char *)DataTypesBase->lib_IdString, "OpenDatatypes") ||
        (DataTypesBase->lib_Version == 47 && DataTypesBase->lib_Revision < 100)) {
        PutStr((CONST_STRPTR)"AddDataTypes: this one needs OpenDatatypes' datatypes.library 47.100 or newer\n");
        goto out;
    }
    if (opts[2]) flags |= ODT_ADT_REFRESH;
    if (opts[3]) flags |= ODT_ADT_LIST;
    names = (STRPTR *)opts[0];
    if (names) {
        files = AllocVec((MAXFILES + 1) * sizeof(STRPTR), MEMF_ANY | MEMF_CLEAR);
        ap = AllocVec(sizeof(struct AnchorPath) + PATHLEN, MEMF_ANY | MEMF_CLEAR);
        if (!files || !ap) { PrintFault(ERROR_NO_FREE_STORE, (CONST_STRPTR)"AddDataTypes"); goto out; }
        ap->ap_Strlen = PATHLEN;
        for (; *names && n < MAXFILES; names++) {
            LONG err = MatchFirst(*names, ap);
            for (; !err && n < MAXFILES; err = MatchNext(ap)) {
                ULONG l = 0;
                if (ap->ap_Info.fib_DirEntryType > 0) continue;
                while (ap->ap_Buf[l]) l++;
                if (!(files[n] = AllocVec(l + 1, MEMF_ANY))) {
                    err = ERROR_NO_FREE_STORE;
                    break;
                }
                CopyMem(ap->ap_Buf, files[n], l + 1);
                n++;
            }
            MatchEnd(ap);
            if (err && err != ERROR_NO_MORE_ENTRIES) {
                PrintFault(err, *names);
                goto out;
            }
        }
    }
    hook.h_Entry = (HOOKFUNC)list_one;
    hook.h_SubEntry = NULL;
    hook.h_Data = NULL;
    added = add_types(files, flags, &hook);
    if (!opts[1] && n && added < n) {
        ULONG a[2];
        a[0] = n - added;
        a[1] = n;
        VPrintf((CONST_STRPTR)"AddDataTypes: %lu of %lu files were not descriptors\n", (APTR)a);
    }
    rc = n && added < n ? RETURN_WARN : RETURN_OK;
out:
    if (files) {
        ULONG i;
        for (i = 0; i < n; i++) FreeVec(files[i]);
        FreeVec(files);
    }
    FreeVec(ap);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    FreeArgs(rda);
    return rc;
}
