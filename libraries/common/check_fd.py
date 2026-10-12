#!/usr/bin/env python3
"""Compare an open library's .lvo table with the NDK's _lib.fd: same call at every LVO,
same registers. Private slots in the fd may be private or named here only
if the fd has them private. Exit 1 on any difference, listing each."""
import re, sys

def read_fd(path):
    bias, public, out = 30, True, {}
    for line in open(path, encoding="latin-1"):
        line = line.strip()
        if line.startswith("##bias"): bias = int(line.split()[1]); continue
        if line.startswith("##private"): public = False; continue
        if line.startswith("##public"): public = True; continue
        if line.startswith("##end"): break
        if not line or line.startswith("*") or line.startswith("##"): continue
        m = re.match(r"(\w+)\(([^)]*)\)\(([^)]*)\)", line)
        if not m: continue
        regs = ",".join(r.strip().lower() for r in re.split(r"[,/]", m.group(3)) if r.strip())
        out[-bias] = (m.group(1) if public else "private", regs)
        bias += 6
    return out

def read_lvo(path):
    out = {}
    for line in open(path):
        line = line.split("#")[0].split()
        if not line: continue
        out[int(line[0])] = (line[1], line[2] if len(line) > 2 else "")
    return out

fd, ours = read_fd(sys.argv[1]), read_lvo(sys.argv[2])
bad = 0
for lvo in sorted(set(fd) | set(ours), reverse=True):
    f, o = fd.get(lvo), ours.get(lvo)
    if f is None and o[0] == "private":
        pass                                  # a slot the NDK leaves out is private
    elif f is None:
        print("%d: ours has %s, the NDK has nothing" % (lvo, o[0])); bad = 1
    elif o is None:
        print("%d: the NDK has %s(%s), ours has nothing" % (lvo, f[0], f[1])); bad = 1
    elif f[0] == "private" and o[0] == "private":
        pass
    elif f != o:
        print("%d: the NDK has %s(%s), ours %s(%s)" % (lvo, f[0], f[1], o[0], o[1])); bad = 1
print("%s %s the NDK's %s" % (sys.argv[2], "differs from" if bad else "matches", sys.argv[1]))
sys.exit(bad)
