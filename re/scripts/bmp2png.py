import sys,struct,zlib
src,dst=sys.argv[1],sys.argv[2]
d=open(src,"rb").read()
off=struct.unpack_from("<I",d,10)[0]; w,h=struct.unpack_from("<ii",d,18); rb=(w*3+3)&~3
rows=[]
for y in range(h-1,-1,-1):
    r=d[off+y*rb:off+y*rb+w*3]; out=bytearray()
    for x in range(w): out+=bytes((r[x*3+2],r[x*3+1],r[x*3]))
    rows.append(b"\0"+bytes(out))
def ch(t,b): c=struct.pack(">I",len(b))+t+b; return c+struct.pack(">I",zlib.crc32(t+b)&0xffffffff)
open(dst,"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(b"".join(rows),6))+ch(b"IEND",b""))
