# DBPF 1.0 reader/writer with QFS (RefPack) decompression. Writes entries uncompressed (no DIR record).
import struct
DIR=0xE86B1EEF
def qfs_decompress(b):
    p=0
    flags=b[p]|(b[p+1]<<8); p+=2  # 0x10FB (or with 0x01 flag: extra size)
    if b[0]&0x01: p+=3
    size=(b[p]<<16)|(b[p+1]<<8)|b[p+2]; p+=3
    out=bytearray()
    while p<len(b):
        c0=b[p]
        if c0<0x80:
            c1=b[p+1]; p+=2
            n=c0&3; out+=b[p:p+n]; p+=n
            ln=((c0&0x1C)>>2)+3; off=((c0&0x60)<<3)+c1+1
        elif c0<0xC0:
            c1,c2=b[p+1],b[p+2]; p+=3
            n=(c1>>6)&3; out+=b[p:p+n]; p+=n
            ln=(c0&0x3F)+4; off=((c1&0x3F)<<8)+c2+1
        elif c0<0xE0:
            c1,c2,c3=b[p+1],b[p+2],b[p+3]; p+=4
            n=c0&3; out+=b[p:p+n]; p+=n
            ln=((c0&0x0C)<<6)+c3+5; off=((c0&0x10)<<12)+(c1<<8)+c2+1
        elif c0<0xFC:
            p+=1; n=((c0&0x1F)<<2)+4; out+=b[p:p+n]; p+=n; continue
        else:
            p+=1; n=c0&3; out+=b[p:p+n]; p+=n; break
        for _ in range(ln): out.append(out[-off])
    assert len(out)==size,(len(out),size)
    return bytes(out)
def read(path):
    d=open(path,'rb').read()
    assert d[:4]==b'DBPF'
    idxver=struct.unpack_from('<I',d,0x3C)[0]
    count,off,size=struct.unpack_from('<III',d,0x24)
    rec=20 if idxver in (0,7) else 24
    ents=[]
    for i in range(count):
        t,g,ins,o,s=struct.unpack_from('<IIIII',d,off+i*rec)
        ents.append(dict(t=t,g=g,i=ins,raw=d[o:o+s]))
    comp=set()
    for e in ents:
        if e['t']==DIR:
            r=e['raw']
            for k in range(0,len(r),16):
                t,g,i,u=struct.unpack_from('<IIII',r,k); comp.add((t,g,i))
    out=[]
    for e in ents:
        if e['t']==DIR: continue
        data=e['raw']
        if (e['t'],e['g'],e['i']) in comp: data=qfs_decompress(data[4:])  # 4-byte compressed size prefix
        out.append(dict(t=e['t'],g=e['g'],i=e['i'],data=data))
    return d,out
def write(path, entries, header_src=None):
    hdr=bytearray(0x60); hdr[:4]=b'DBPF'
    struct.pack_into('<II',hdr,4,1,0)
    if header_src: hdr[0x18:0x20]=header_src[0x18:0x20]  # dates
    struct.pack_into('<I',hdr,0x20,7) # index major version
    struct.pack_into('<I',hdr,0x3C,0) # index minor version
    out=bytearray(hdr); idx=[]
    for e in entries:
        idx.append((e['t'],e['g'],e['i'],len(out),len(e['data']))); out+=e['data']
    ioff=len(out)
    for t,g,i,o,s in idx: out+=struct.pack('<IIIII',t,g,i,o,s)
    struct.pack_into('<III',out,0x24,len(idx),ioff,20*len(idx))
    open(path,'wb').write(out)
