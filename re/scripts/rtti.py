import struct, re, json, sys
d = open(r"C:\Users\Owner\Documents\MEDMapEditor\MEdv032\MEd.exe","rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
nsec = struct.unpack_from("<H", d, pe+6)[0]
opt = pe+24
base = struct.unpack_from("<I", d, opt+28)[0]
secs=[]
sp = opt+struct.unpack_from("<H", d, pe+20)[0]
for i in range(nsec):
    n,vs,va,rs,rp = struct.unpack_from("<8sIIII", d, sp+i*40)
    secs.append((n.rstrip(b"\0").decode(), base+va, max(vs,rs), rp, rs))
def va2off(va):
    for n,s,sz,rp,rs in secs:
        if s <= va < s+sz and va-s < rs: return rp+va-s
def off2va(o):
    for n,s,sz,rp,rs in secs:
        if rp <= o < rp+rs: return s+o-rp
text = [s for s in secs if s[0]==".text"][0]
tlo, thi = text[1], text[1]+text[2]
APP = "CBookMarks CColView CDFFView CFileTree CGLView CIDECTree CIDEChooser CIDEFinder CIDEModel CIDETree CIMG CIMGFinder CIMGTool CIMGTree CIMGTreeView CINSTChooser CIPLFinder CIPLTree CIPLViewer CLODChooser CLODItem CLODTree CMapList CMapView CStats CTexView CTree1 CTree2 CTree3 CTree4 CTreeItem Emitter IDEView Intro MainWindow ProgressDialog TextureView".split()
out = []
for c in APP:
    pat = (".?AV%s@@\0" % c).encode()
    for m in re.finditer(re.escape(pat), d):
        td = off2va(m.start()-8)
        if td is None: continue
        # COLs referencing td at COL+12
        tdb = struct.pack("<I", td)
        for cm in re.finditer(re.escape(tdb), d):
            col = off2va(cm.start()-12)
            if col is None: continue
            o = va2off(col)
            if struct.unpack_from("<I", d, o)[0] != 0: continue
            colb = struct.pack("<I", col)
            for vm in re.finditer(re.escape(colb), d):
                vt = off2va(vm.start())+4
                if vt is None: continue
                fns=[]; o=va2off(vt)
                while True:
                    p = struct.unpack_from("<I", d, o)[0]
                    if not (tlo <= p < thi): break
                    fns.append(p); o+=4
                offset = struct.unpack_from("<I", d, va2off(col)+4)[0]
                out.append(dict(cls=c, vtable=vt, col_offset=offset, funcs=fns))
                # code that stores vtable address (ctor/dtor candidates)
                refs=[]
                vb = struct.pack("<I", vt)
                for rm in re.finditer(re.escape(vb), d[text[3]:text[3]+text[4]]):
                    refs.append(off2va(text[3]+rm.start()))
                out[-1]["refs"]=refs
json.dump(out, open(r"C:\Users\Owner\Documents\MEDMapEditor\re\out\vtables.json","w"), indent=1)
for v in out: print("%-14s vt=%08x off=%d nvirt=%d refs=%d" % (v["cls"], v["vtable"], v["col_offset"], len(v["funcs"]), len(v["refs"])))
print(len(out),"vtables;", sum(len(v["funcs"]) for v in out),"virtual slots")
