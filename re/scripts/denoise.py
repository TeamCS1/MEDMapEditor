import re, sys
# usage: denoise.py <file.c> <hexaddr>   -> prints function with Qt refcount/EH noise removed
src = open(sys.argv[1]).read()
a = sys.argv[2].lower()
m = re.search(r"^// ==== %s .*?(?=^// ====|\Z)" % a, src, re.S | re.M)
lines = m.group(0).split("\n")
out = []; i = 0
while i < len(lines):
    l = lines[i]; s = l.strip()
    if s in ("LOCK();", "UNLOCK();") or re.match(r"local_c(\._\d_\d)?\s*=", s) or re.match(r"(ExceptionList|puStack_\w+|local_14|local_c)\b.*=", s) and "ExceptionList" in s:
        i += 1; continue
    # drop refcount-release blocks: "iVarN = *p; *p = iVarN + -1; if (iVarN + -1 == 0) { FUN_006302a0(...); }"
    if re.match(r"if \(iVar\d+ \+ -1 == 0\) \{", s):
        j = i + 1; body = []
        while j < len(lines) and lines[j].strip() != "}": body.append(lines[j].strip()); j += 1
        if all(re.match(r"(FUN_006302a0|free)\(.*\);|iVar\d+ = FUN_006302a0\(.*\);", b) for b in body):
            i = j + 1; continue
    if re.match(r"iVar\d+ = \*\w+;$", s) or re.match(r"\*\w+ = iVar\d+ \+ -1;$", s):
        i += 1; continue
    out.append(l); i += 1
print("\n".join(x for x in out if x.strip() != "" or True))
