/* OpenDatatypes' datatypesclass under qemu-m68k: libraries/opendatatypes/
 * dtclass_core.c hosted on OpenIntuition's BOOPSI core (rootclass and
 * gadgetclass, libraries/openintuition), with a probe subclass
 * (probe.datatype) and a mock env that logs every source it opens, every
 * band it draws, every render and every notification.
 *
 * It runs a script python3 writes at test time (opendatatypes_class_ref.py),
 * one command a line, and prints a log; test_opendatatypes_class.sh
 * compares it with the log python's own model of the class expects:
 *   new KIND TOTV TOTH [nosel]   NewObject of probe.datatype; KIND is file,
 *                                iff, ram, mem, clip or failfile
 *   units VU HU                  OM_SET DTA_VertUnit, DTA_HorizUnit
 *   layout W H INITIAL           GA_Width/GA_Height, GM_LAYOUT, DoAsyncLayout
 *   set TV TH / update TV TH     OM_SET / OM_UPDATE of DTA_TopVert, DTA_TopHoriz
 *   down X Y / move X Y / up X Y / menu   the mouse, gadget-relative
 *   startsel                     StartDragSelect
 *   get                          GetDTAttrsA of the numbers and the domain
 *   write                        DTM_WRITE, DTWM_RAW (memory source)
 *   async 0|1                    layouts and printing on (mock) processes or not
 *   runproc                      the started processes run, in order
 *   print                        PrintDTObjectA
 *   dispose
 * Copyright (c) 2026 Dalsin Limited. MIT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "boopsi_core.h"
#include "classes_core.h"
#include "dtclass_env.h"

static long live;
static void *u_alloc(struct ou_env *e, ou_u32 n) { (void)e; live++; return calloc(1, n); }
static void u_free(struct ou_env *e, void *m) { (void)e; if (m) { live--; free(m); } }
static void u_lock(struct ou_env *e, void *l) { (void)e; (void)l; }
static void u_initlock(struct ou_env *e, void *l) { (void)e; memset(l, 0, OU_LOCKSPACE); }
static void u_reply(struct ou_env *e, void *m, void *name) { (void)e; (void)m; (void)name; }
static struct ou_env uenv = { u_alloc, u_free, u_lock, u_lock, u_initlock, u_reply };

/* ---- the BOOPSI host -------------------------------------------------------- */

static struct oi_boopsi boopsi;
static struct oi_classes oic;
static struct odt_cenv cenv;
static ou_u32 builtin_entry(void) { return 0; }
static ou_u32 dt_entry(void) { return 0; }

static ou_u32 call_hook(struct oi_env *env, struct oi_Hook *hook, void *obj, void *msg)
{
    if (hook->h_Entry == builtin_entry)
        return ((oi_dispatch_fn)(void (*)(void))hook->h_SubEntry)(env, (struct oi_IClass *)hook, obj, msg);
    if (hook->h_Entry == dt_entry)
        return ((odt_dispatch_fn)(void (*)(void))hook->h_SubEntry)(&cenv, (struct odt_IClass *)hook, obj, msg);
    printf("FAIL a hook with no entry\n");
    exit(1);
}
static struct oi_env oenv = { .u = &uenv, .call_hook = call_hook };

static struct oi_IClass *add_class(const char *name, const char *super, ou_u32 size, odt_dispatch_fn fn)
{
    struct oi_IClass *cl = oi_MakeClass(&oenv, &boopsi, name, super, 0, size, 0);
    if (!cl) { printf("FAIL MakeClass %s\n", name); exit(1); }
    cl->cl_Dispatcher.h_Entry = dt_entry;
    cl->cl_Dispatcher.h_SubEntry = (ou_u32 (*)(void))(void (*)(void))fn;
    oi_AddClass(&oenv, &boopsi, cl);
    return cl;
}

/* ---- names for tags ---------------------------------------------------------- */

static void put_tag(odt_u32 tag, odt_u32 v)
{
    const char *n = NULL;
    switch (tag) {
    case ODT_DTA_TopVert: n = "tv"; break;
    case ODT_DTA_VisibleVert: n = "vv"; break;
    case ODT_DTA_TotalVert: n = "tov"; break;
    case ODT_DTA_TopHoriz: n = "th"; break;
    case ODT_DTA_VisibleHoriz: n = "vh"; break;
    case ODT_DTA_TotalHoriz: n = "toh"; break;
    case ODT_DTA_Busy: n = "busy"; break;
    case ODT_DTA_Sync: n = "sync"; break;
    case ODT_DTA_PrinterStatus: n = "pstat"; break;
    case ODT_GA_ID: n = "id"; break;
    }
    if (n) printf(" %s=%ld", n, (long)v);
    else printf(" %lx=%ld", (unsigned long)tag, (long)v);
}

/* ---- the logger: ICA_TARGET of every object -------------------------------- */

static odt_u32 logger_dispatch(const struct odt_cenv *env, struct odt_IClass *cl, void *obj, void *msg)
{
    odt_u32 id = *(odt_u32 *)msg;
    if (id == ODT_OM_UPDATE) {
        struct odt_opUpdate *u = msg;
        const struct odt_TagItem *tp = u->opu_AttrList, *t;
        printf("notify%s", (u->opu_Flags & ODT_OPUF_INTERIM) ? " interim" : "");
        while ((t = odt_next_tag(&tp)) != NULL) put_tag(t->ti_Tag, t->ti_Data);
        printf("\n");
        return 0;
    }
    return env->do_super(env, cl, obj, msg);
}

/* ---- the probe subclass ------------------------------------------------------ */

#define PROBE_TotV  (ODT_TAG_USER + 0x7700001UL)
#define PROBE_TotH  (ODT_TAG_USER + 0x7700002UL)
#define PROBE_NoSel (ODT_TAG_USER + 0x7700003UL)
struct probe { odt_s32 totv, toth; odt_u32 nosel; };
static const odt_u32 nosel_methods[] = { ODT_OM_NEW, ODT_GM_LAYOUT, ODT_DTM_ASYNCLAYOUT, ~0UL };

static odt_u32 probe_dispatch(const struct odt_cenv *env, struct odt_IClass *cl, void *obj, void *msg)
{
    odt_u32 id = *(odt_u32 *)msg;
    struct probe *p = obj ? ODT_INST_DATA(cl, obj) : NULL;
    switch (id) {
    case ODT_OM_NEW: {
        void *o = (void *)env->do_super(env, cl, obj, msg);
        if (o) {
            struct odt_TagItem *tags = ((struct odt_opSet *)msg)->ops_AttrList;
            const struct odt_TagItem *tp = tags, *t;
            p = ODT_INST_DATA(cl, o);
            while ((t = odt_next_tag(&tp)) != NULL) {
                if (t->ti_Tag == PROBE_TotV) p->totv = (odt_s32)t->ti_Data;
                if (t->ti_Tag == PROBE_TotH) p->toth = (odt_s32)t->ti_Data;
                if (t->ti_Tag == PROBE_NoSel) p->nosel = t->ti_Data;
            }
        }
        return (odt_u32)o;
    }
    case ODT_OM_GET:
        if (((struct odt_opGet *)msg)->opg_AttrID == ODT_DTA_Methods && p->nosel) {
            *((struct odt_opGet *)msg)->opg_Storage = (odt_u32)nosel_methods;
            return 1;
        }
        break;
    case ODT_DTM_ASYNCLAYOUT: {
        struct odt_TagItem t[3];
        struct odt_opSet s;
        printf("asynclayout %lu\n", (unsigned long)((struct odt_gpLayout *)msg)->gpl_Initial);
        t[0].ti_Tag = ODT_DTA_TotalVert; t[0].ti_Data = (odt_u32)p->totv;
        t[1].ti_Tag = ODT_DTA_TotalHoriz; t[1].ti_Data = (odt_u32)p->toth;
        t[2].ti_Tag = ODT_TAG_DONE; t[2].ti_Data = 0;
        s.MethodID = ODT_OM_SET; s.ops_AttrList = t; s.ops_GInfo = NULL;
        env->do_method(env, obj, &s);
        return 1;
    }
    case ODT_GM_RENDER: {
        struct odt_DTSpecialInfo *si = ((struct odt_Gadget *)obj)->SpecialInfo;
        printf("render %ld %ld %ld %ld %ld\n", (long)((struct odt_gpRender *)msg)->gpr_Redraw, (long)si->si_TopVert,
               (long)si->si_TopHoriz, (long)si->si_OTopVert, (long)si->si_OTopHoriz);
        return 1;
    }
    case ODT_DTM_PRINT:
        printf("print\n");
        return ODT_PDERR_NOERR;
    case ODT_DTM_SELECT: {
        struct odt_Rectangle *r = &((struct odt_dtSelect *)msg)->dts_Select;
        printf("select %d %d %d %d\n", r->MinX, r->MinY, r->MaxX, r->MaxY);
        break;
    }
    }
    return env->do_super(env, cl, obj, msg);
}

/* ---- the env -------------------------------------------------------------------- */

static odt_u32 e_do_method(const struct odt_cenv *e, void *obj, void *msg) { (void)e; return oi_DoMethodA(&oenv, obj, msg); }
static odt_u32 e_do_super(const struct odt_cenv *e, struct odt_IClass *cl, void *obj, void *msg)
{ (void)e; return oi_DoSuperMethodA(&oenv, (struct oi_IClass *)cl, obj, msg); }
static odt_u32 e_coerce(const struct odt_cenv *e, struct odt_IClass *cl, void *obj, void *msg)
{ (void)e; return oi_CoerceMethodA(&oenv, (struct oi_IClass *)cl, obj, msg); }
static void *e_new_object(const struct odt_cenv *e, struct odt_IClass *cl, const char *id, struct odt_TagItem *tags)
{ (void)e; return oi_NewObjectA(&oenv, &boopsi, (struct oi_IClass *)cl, id, (struct ou_TagItem *)tags); }
static void e_dispose(const struct odt_cenv *e, void *obj) { (void)e; oi_DisposeObject(&oenv, obj); }
static void *e_alloc(const struct odt_cenv *e, odt_u32 n) { (void)e; live++; return calloc(1, n); }
static void e_free(const struct odt_cenv *e, void *p) { (void)e; if (p) { live--; free(p); } }

/* semaphores: a count each, which must balance, and who holds it
 * (opaque[1]: 1 the program, 2 + n started process n); one another holds
 * is waited for: that process runs to its end first */
static int sem_depth;
static int cur = -1;
static void run_one(int i);
static void e_sem_init(const struct odt_cenv *e, struct odt_SignalSemaphore *s) { (void)e; memset(s, 0, sizeof *s); }
static void e_sem_obtain(const struct odt_cenv *e, struct odt_SignalSemaphore *s)
{
    (void)e;
    if (s->opaque[0] && s->opaque[1] != cur + 2) {
        if (s->opaque[1] >= 2) run_one(s->opaque[1] - 2);
        if (s->opaque[0]) printf("FAIL a semaphore another holds\n");
    }
    s->opaque[0]++;
    s->opaque[1] = (odt_u8)(cur + 2);
    sem_depth++;
}
static void e_sem_release(const struct odt_cenv *e, struct odt_SignalSemaphore *s)
{
    (void)e;
    if (!s->opaque[0]) printf("FAIL release of a semaphore not held\n");
    if (!--s->opaque[0]) s->opaque[1] = 0;
    sem_depth--;
}

static odt_s32 ioerr;
static odt_s32 e_io_err(const struct odt_cenv *e) { (void)e; return ioerr; }
static void e_set_io_err(const struct odt_cenv *e, odt_s32 c) { (void)e; ioerr = c; }
static int fail_open;
static odt_u32 e_open(const struct odt_cenv *e, const char *name, odt_s32 mode)
{
    (void)e;
    printf("open %s %ld\n", name, (long)mode);
    if (fail_open) { ioerr = 205; return 0; }
    return 7;
}
static void e_close(const struct odt_cenv *e, odt_u32 fh) { (void)e; printf("close %lu\n", (unsigned long)fh); }
static odt_s32 e_write(const struct odt_cenv *e, odt_u32 fh, const void *buf, odt_s32 n)
{
    (void)e;
    printf("write %lu %ld", (unsigned long)fh, (long)n);
    if (n > 0) printf(" %02x..%02x", ((const unsigned char *)buf)[0], ((const unsigned char *)buf)[n - 1]);
    printf("\n");
    return n;
}
static odt_s32 e_seek(const struct odt_cenv *e, odt_u32 fh, odt_s32 off, odt_s32 mode)
{ (void)e; printf("seek %lu %ld %ld\n", (unsigned long)fh, (long)off, (long)mode); return 0; }
static odt_s32 e_read(const struct odt_cenv *e, odt_u32 fh, void *buf, odt_s32 n) { (void)e; (void)fh; (void)buf; (void)n; return 0; }
static char iff_obj[4];
static void *e_iff_dos(const struct odt_cenv *e, odt_u32 fh, int w) { (void)e; printf("iffdos %lu %d\n", (unsigned long)fh, w); return iff_obj; }
static void *e_iff_clip(const struct odt_cenv *e, odt_u32 unit, int w) { (void)e; printf("iffclip %lu %d\n", (unsigned long)unit, w); return iff_obj; }
static void *e_iff_mem(const struct odt_cenv *e, const void *at, odt_u32 n) { (void)e; (void)at; printf("iffmem %lu\n", (unsigned long)n); return iff_obj; }
static void e_iff_close(const struct odt_cenv *e, void *iff) { (void)e; printf("iffclose%s\n", iff == iff_obj ? "" : " ?"); }
static char the_rp[4];
static void *e_obtain_rp(const struct odt_cenv *e, struct odt_GadgetInfo *gi) { (void)e; (void)gi; return the_rp; }
static void e_release_rp(const struct odt_cenv *e, void *rp) { (void)e; (void)rp; }
static void e_box(const struct odt_cenv *e, void *rp, odt_s32 x0, odt_s32 y0, odt_s32 x1, odt_s32 y1)
{ (void)e; (void)rp; printf("band %ld %ld %ld %ld\n", (long)x0, (long)y0, (long)x1, (long)y1); }

/* processes: started ones wait until the script's runproc runs them in order */
struct pend { int printing, stopped, live; void *obj; struct odt_gpLayout gpl; struct odt_dtPrint pr; };
static struct pend pend[16];
static int npend, async_on;
static void *e_start(const struct odt_cenv *e, int printing, void *o, void *win, void *req, void *msg)
{
    struct pend *p;
    (void)e; (void)win; (void)req;
    if (!async_on || npend >= 16) return NULL;
    p = &pend[npend];
    memset(p, 0, sizeof *p);
    p->printing = printing;
    p->obj = o;
    p->live = 1;
    if (printing) p->pr = *(struct odt_dtPrint *)msg;
    else p->gpl = *(struct odt_gpLayout *)msg;
    printf("start %d %s\n", npend, printing ? "print" : "layout");
    if (printing) {                     /* its first step, before we answer */
        int was = cur;
        cur = npend;
        odt_print_hold(&cenv, o);
        cur = was;
    }
    return &pend[npend++];
}
static void e_signal_stop(const struct odt_cenv *e, void *proc)
{
    (void)e;
    printf("stop %d\n", (int)((struct pend *)proc - pend));
    ((struct pend *)proc)->stopped = 1;
}
static int e_check_stop(const struct odt_cenv *e) { (void)e; return cur >= 0 ? pend[cur].stopped : 0; }
static void run_one(int i)
{
    int was = cur;
    if (i < 0 || i >= npend || !pend[i].live) return;
    cur = i;
    if (pend[i].printing) odt_print_run(&cenv, pend[i].obj, &pend[i].pr);
    else odt_layout_run(&cenv, pend[i].obj, &pend[i].gpl);
    pend[i].live = 0;
    cur = was;
}
static void run_procs(void)
{
    int i;
    for (i = 0; i < npend; i++) run_one(i);
    npend = 0;
}

static void env_init(void)
{
    cenv.do_method = e_do_method;
    cenv.do_super = e_do_super;
    cenv.coerce = e_coerce;
    cenv.new_object = e_new_object;
    cenv.dispose_object = e_dispose;
    cenv.alloc = e_alloc;
    cenv.free = e_free;
    cenv.sem_init = e_sem_init;
    cenv.sem_obtain = e_sem_obtain;
    cenv.sem_obtain_shared = e_sem_obtain;
    cenv.sem_release = e_sem_release;
    cenv.io_err = e_io_err;
    cenv.set_io_err = e_set_io_err;
    cenv.open = e_open;
    cenv.close = e_close;
    cenv.write = e_write;
    cenv.seek = e_seek;
    cenv.read = e_read;
    cenv.iff_open_dos = e_iff_dos;
    cenv.iff_open_clip = e_iff_clip;
    cenv.iff_open_mem = e_iff_mem;
    cenv.iff_close = e_iff_close;
    cenv.obtain_rp = e_obtain_rp;
    cenv.release_rp = e_release_rp;
    cenv.complement_box = e_box;
    cenv.dt_string = NULL;
    cenv.start_process = e_start;
    cenv.signal_stop = e_signal_stop;
    cenv.check_stop = e_check_stop;
}

/* ---- the run ---------------------------------------------------------------------- */

static struct odt_DataTypeHeader hdr_bin = { "probe", "probe", "#?", NULL, ODT_GID_PICTURE, 0, 0, 0, ODT_DTF_BINARY, 0 };
static struct odt_DataTypeHeader hdr_iff = { "probe iff", "probe", "#?", NULL, ODT_GID_PICTURE, 0, 0, 0, ODT_DTF_IFF, 0 };
static struct odt_DataType dt_bin, dt_iff;
static unsigned char memdata[300];
static struct odt_GadgetInfo gi;
static void *obj, *logger;
static int active;

static void input(odt_u32 method, odt_u16 code, int x, int y)
{
    struct odt_InputEvent ie;
    struct odt_gpInput m;
    odt_s32 term = -1;
    odt_u32 r;
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = ODT_IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    m.MethodID = method;
    m.gpi_GInfo = &gi;
    m.gpi_IEvent = &ie;
    m.gpi_Termination = &term;
    m.gpi_Mouse.x = (odt_s16)x;
    m.gpi_Mouse.y = (odt_s16)y;
    m.gpi_TabletData = NULL;
    r = oi_DoMethodA(&oenv, obj, &m);
    printf("%s %lu\n", method == ODT_GM_GOACTIVE ? "active" : "input", (unsigned long)r);
    if (r != ODT_GMR_MEACTIVE) {
        struct odt_gpGoInactive g = { ODT_GM_GOINACTIVE, &gi, 0 };
        oi_DoMethodA(&oenv, obj, &g);
        printf("inactive\n");
        active = 0;
    } else {
        active = 1;
    }
}

static void set_tops(odt_u32 method, long tv, long th)
{
    struct odt_TagItem t[3] = { { ODT_DTA_TopVert, (odt_u32)tv }, { ODT_DTA_TopHoriz, (odt_u32)th }, { 0, 0 } };
    struct odt_opUpdate m = { method, t, &gi, 0 };
    oi_DoMethodA(&oenv, obj, &m);
}

int main(int argc, char **argv)
{
    char line[512], *w[16];
    FILE *f;
    long base;
    if (argc < 2 || !(f = fopen(argv[1], "r"))) { printf("usage: %s script\n", argv[0]); return 2; }
    env_init();
    if (!oi_boopsi_init(&oenv, &boopsi, builtin_entry) || !oi_classes_init(&oenv, &boopsi, &oic)) {
        printf("FAIL boopsi\n");
        return 1;
    }
    cenv.dtclass = (struct odt_IClass *)add_class("datatypesclass", "gadgetclass", odt_dtclass_inst_size(),
                                                  odt_dtclass_dispatch);
    add_class("probe.datatype", "datatypesclass", sizeof(struct probe), probe_dispatch);
    add_class("logger", "rootclass", 0, logger_dispatch);
    dt_bin.dtn_Header = &hdr_bin;
    dt_iff.dtn_Header = &hdr_iff;
    for (int i = 0; i < 300; i++) memdata[i] = (unsigned char)(i * 7 + 1);
    gi.gi_Domain.Left = 10; gi.gi_Domain.Top = 20; gi.gi_Domain.Width = 640; gi.gi_Domain.Height = 400;
    logger = oi_NewObjectA(&oenv, &boopsi, NULL, "logger", NULL);
    base = live;
    while (fgets(line, sizeof line, f)) {
        int n = 0;
        char *s = strtok(line, " \t\n");
        while (s && n < 16) { w[n++] = s; s = strtok(NULL, " \t\n"); }
        if (!n) continue;
        if (!strcmp(w[0], "new")) {
            const char *k = w[1];
            odt_u32 st = ODT_DTST_FILE;
            struct odt_DataType *dt = &dt_bin;
            odt_u32 name = (odt_u32)"data";
            struct odt_TagItem t[13];
            int i = 0;
            fail_open = !strcmp(k, "failfile");
            if (!strcmp(k, "iff")) dt = &dt_iff;
            else if (!strcmp(k, "ram")) { st = ODT_DTST_RAM; name = 0; }
            else if (!strcmp(k, "mem")) { st = ODT_DTST_MEMORY; name = 0; }
            else if (!strcmp(k, "clip") || !strcmp(k, "cliph")) { st = ODT_DTST_CLIPBOARD; name = 0; dt = &dt_iff; }
            /* cliph: the program's own IFFHandle as DTA_Handle */
            t[i].ti_Tag = !strcmp(k, "cliph") ? ODT_DTA_Handle : ODT_TAG_IGNORE; t[i++].ti_Data = (odt_u32)iff_obj;
            t[i].ti_Tag = ODT_DTA_Name; t[i++].ti_Data = name;
            t[i].ti_Tag = ODT_DTA_SourceType; t[i++].ti_Data = st;
            t[i].ti_Tag = ODT_DTA_DataType; t[i++].ti_Data = (odt_u32)dt;
            t[i].ti_Tag = ODT_DTA_SourceAddress; t[i++].ti_Data = (odt_u32)memdata;
            t[i].ti_Tag = ODT_DTA_SourceSize; t[i++].ti_Data = sizeof memdata;
            t[i].ti_Tag = ODT_GA_Left; t[i++].ti_Data = 5;
            t[i].ti_Tag = ODT_GA_Top; t[i++].ti_Data = 6;
            t[i].ti_Tag = ODT_GA_ID; t[i++].ti_Data = 3;
            t[i].ti_Tag = ODT_ICA_TARGET; t[i++].ti_Data = (odt_u32)logger;
            t[i].ti_Tag = PROBE_TotV; t[i++].ti_Data = (odt_u32)atol(w[2]);
            t[i].ti_Tag = PROBE_TotH; t[i++].ti_Data = (odt_u32)atol(w[3]);
            t[i].ti_Tag = n > 4 ? PROBE_NoSel : ODT_TAG_IGNORE; t[i++].ti_Data = 1;
            {
                struct odt_TagItem all[14];
                memcpy(all, t, sizeof(struct odt_TagItem) * (size_t)i);
                all[i].ti_Tag = ODT_TAG_DONE; all[i].ti_Data = 0;
                ioerr = 0;
                obj = oi_NewObjectA(&oenv, &boopsi, NULL, "probe.datatype", (struct ou_TagItem *)all);
            }
            if (!obj) { printf("new fail %ld\n", (long)ioerr); continue; }
            {
                odt_u32 h = 1, stt = 0;
                struct odt_TagItem g[3] = { { ODT_DTA_Handle, (odt_u32)&h }, { ODT_DTA_SourceType, (odt_u32)&stt }, { 0, 0 } };
                odt_u32 c = odt_get_dt_attrs(&cenv, obj, g);
                printf("new ok %lu h=%s st=%lu\n", (unsigned long)c,
                       h == 0 ? "0" : h == 7 ? "7" : (void *)h == iff_obj ? "iff" : "?", (unsigned long)stt);
            }
        } else if (!strcmp(w[0], "tool")) {
            /* tool PROGRAM PROJECT SIZE: ~ is a space, ' a quote, - no project */
            char buf[256], prog[128], proj[128];
            unsigned i;
            odt_u32 r;
            for (i = 0; w[1][i] && i < 127; i++) prog[i] = w[1][i] == '~' ? ' ' : w[1][i] == '\'' ? '"' : w[1][i];
            prog[i] = 0;
            for (i = 0; w[2][i] && i < 127; i++) proj[i] = w[2][i] == '~' ? ' ' : w[2][i] == '\'' ? '"' : w[2][i];
            proj[i] = 0;
            memset(buf, 'x', sizeof buf);
            r = odt_tool_command(buf, (odt_u32)atol(w[3]), prog, strcmp(w[2], "-") ? proj : NULL);
            printf("tool %lu [%s]\n", (unsigned long)r, r ? buf : "");
        } else if (!strcmp(w[0], "group")) {
            const char *b = odt_group_base((odt_u32)strtoul(w[1], NULL, 0));
            printf("group %s\n", b ? b : "-");
        } else if (!strcmp(w[0], "string")) {
            const char *t = odt_GetDTString((odt_u32)strtoul(w[1], NULL, 0));
            printf("string %s [%s]\n", w[1], t ? t : "(null)");
        } else if (!obj) {
            continue;
        } else if (!strcmp(w[0], "units")) {
            struct odt_TagItem t[3] = { { ODT_DTA_VertUnit, (odt_u32)atol(w[1]) }, { ODT_DTA_HorizUnit, (odt_u32)atol(w[2]) }, { 0, 0 } };
            struct odt_opSet m = { ODT_OM_SET, t, &gi };
            oi_DoMethodA(&oenv, obj, &m);
        } else if (!strcmp(w[0], "layout")) {
            struct odt_TagItem t[3] = { { ODT_GA_Width, (odt_u32)atol(w[1]) }, { ODT_GA_Height, (odt_u32)atol(w[2]) }, { 0, 0 } };
            struct odt_opSet m = { ODT_OM_SET, t, &gi };
            struct odt_gpLayout l = { ODT_GM_LAYOUT, &gi, (odt_u32)atol(w[3]) };
            oi_DoMethodA(&oenv, obj, &m);
            oi_DoMethodA(&oenv, obj, &l);
            odt_do_async_layout(&cenv, obj, &l);
        } else if (!strcmp(w[0], "set")) {
            set_tops(ODT_OM_SET, atol(w[1]), atol(w[2]));
        } else if (!strcmp(w[0], "update")) {
            set_tops(ODT_OM_UPDATE, atol(w[1]), atol(w[2]));
        } else if (!strcmp(w[0], "down")) {
            input(ODT_GM_GOACTIVE, ODT_SELECTDOWN, atoi(w[1]), atoi(w[2]));
        } else if (!strcmp(w[0], "move")) {
            if (active) input(ODT_GM_HANDLEINPUT, ODT_IECODE_NOBUTTON, atoi(w[1]), atoi(w[2]));
        } else if (!strcmp(w[0], "up")) {
            if (active) input(ODT_GM_HANDLEINPUT, ODT_SELECTUP, atoi(w[1]), atoi(w[2]));
        } else if (!strcmp(w[0], "menu")) {
            if (active) input(ODT_GM_HANDLEINPUT, ODT_MENUDOWN, 0, 0);
        } else if (!strcmp(w[0], "startsel")) {
            printf("startsel %lu\n", (unsigned long)odt_start_drag_select(&cenv, obj));
        } else if (!strcmp(w[0], "get")) {
            odt_u32 v[8] = { 0 }, dom = 0, nothing = 5;
            struct odt_TagItem t[] = {
                { ODT_DTA_TopVert, (odt_u32)&v[0] }, { ODT_DTA_VisibleVert, (odt_u32)&v[1] },
                { ODT_DTA_TotalVert, (odt_u32)&v[2] }, { ODT_DTA_TopHoriz, (odt_u32)&v[3] },
                { ODT_DTA_VisibleHoriz, (odt_u32)&v[4] }, { ODT_DTA_TotalHoriz, (odt_u32)&v[5] },
                { ODT_DTA_Busy, (odt_u32)&v[6] }, { ODT_DTA_Sync, (odt_u32)&v[7] },
                { ODT_DTA_Domain, (odt_u32)&dom }, { ODT_TAG_USER + 0x7700099UL, (odt_u32)&nothing }, { 0, 0 } };
            odt_u32 c = odt_get_dt_attrs(&cenv, obj, t);
            struct odt_IBox *b = (struct odt_IBox *)dom;
            printf("get %lu %ld %ld %ld %ld %ld %ld %lu %lu dom %d %d %d %d %lu\n", (unsigned long)c, (long)v[0], (long)v[1],
                   (long)v[2], (long)v[3], (long)v[4], (long)v[5], (unsigned long)v[6], (unsigned long)v[7],
                   b->Left, b->Top, b->Width, b->Height, (unsigned long)nothing);
        } else if (!strcmp(w[0], "write")) {
            struct odt_dtWrite m = { ODT_DTM_WRITE, NULL, 9, ODT_DTWM_RAW, NULL };
            printf("written %lu\n", (unsigned long)oi_DoMethodA(&oenv, obj, &m));
        } else if (!strcmp(w[0], "async")) {
            async_on = atoi(w[1]);
        } else if (!strcmp(w[0], "runproc")) {
            run_procs();
        } else if (!strcmp(w[0], "print")) {
            struct odt_dtPrint m = { ODT_DTM_PRINT, NULL, NULL, NULL };
            printf("print %lu\n", (unsigned long)odt_print_object(&cenv, obj, NULL, NULL, &m));
        } else if (!strcmp(w[0], "dispose")) {
            oi_DisposeObject(&oenv, obj);
            obj = NULL;
            active = 0;
            printf("dispose %ld %d\n", live - base, sem_depth);
        } else {
            printf("FAIL script word %s\n", w[0]);
            return 1;
        }
    }
    if (obj) { oi_DisposeObject(&oenv, obj); printf("dispose %ld %d\n", live - base, sem_depth); }
    return 0;
}
