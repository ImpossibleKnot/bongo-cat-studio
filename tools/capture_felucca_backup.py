#!/usr/bin/env python3
"""Read-only Felucca backup, matching its published editor JSON format.
Protocol reference: isod89/sloop-fm1 v2.8, sloop-editor-2.8.html.
Sends only editor INFO (1), BK_LIST (65) and BK_GET (66); no device writes.
"""
import base64
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('sloop_installer', ROOT/'tools/felucca-1.6.1/fm1_install.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)

class Reader:
    def __init__(self, data):
        self.data, self.i = data, 0
    def take(self, n):
        if self.i + n > len(self.data):
            raise ValueError('Truncated editor reply')
        data = self.data[self.i:self.i+n]
        self.i += n
        return data
    def byte(self):
        return self.take(1)[0]
    def u35(self):
        return sum(v << (7*k) for k,v in enumerate(self.take(5))) & 0xffffffff
    def string(self):
        n = self.data.index(0, self.i) - self.i
        result = self.take(n).decode('ascii')
        self.take(1)
        return result

def request(link, cmd, args=(), timeout=2):
    assert cmd in (1,65,66)
    for attempt in range(3):
        link.drain()
        if not link.send(bytes([240,125,70,76,cmd,*args,247])):
            raise RuntimeError('MIDI send failed')
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            pkt = link.read(max(0,end-time.monotonic()))
            if pkt and pkt[:5] == bytes([240,125,70,76,cmd]) and pkt[-1] == 247:
                return pkt[5:-1]
    raise TimeoutError(f'No reply to editor command {cmd}')

def unpack_editor(data):
    out = bytearray()
    for i in range(0,len(data),8):
        top = data[i]
        for j,v in enumerate(data[i+1:i+8]):
            out.append(v | (((top>>j)&1)<<7))
    return bytes(out)

def info(link):
    r=Reader(request(link,1))
    version=r.string()
    neng,pcount,gcount,nstep,pe0=r.take(5)
    engines=[r.string() for _ in range(neng)]
    tracks=r.byte() if r.i<len(r.data) else 0
    proto=r.byte() if r.i<len(r.data) else 0
    return dict(version=version, engines=engines, tracks=tracks, protocol=proto)

def capture(dest):
    dest=Path(dest)
    if dest.exists():
        raise FileExistsError(dest)
    backend=ota.MidoBackend()
    dev=ota.Updater(backend,port='Felucca').find()
    if dev is None or dev.id.loader:
        raise RuntimeError('Running FM-1 not found')
    try:
        state=info(dev.link)
        print(json.dumps(state),flush=True)
        if state['version'] not in ('FELUCCA v1.6.1', 'FELUCCA 1.6.1', 'v1.6.1', 'BONGO CAT 0.1.0', 'BONGO CAT 0.1.1', 'BONGO CAT 0.2.0', 'BONGO CAT 0.2.1', 'BONGO CAT 0.3.0', 'BONGO CAT 0.4.0-rc1'):
            raise RuntimeError('Expected Felucca 1.6.1 or Bongo 0.1.0; no backup requests sent')
        r=Reader(request(dev.link,65,timeout=5))
        protocol,rc,n=r.take(3)
        if protocol!=1: raise RuntimeError("Unknown archive protocol")
        if rc:
            raise RuntimeError(f'Backup LIST refused (status {rc}); stop playback and retry')
        items=[dict(id=r.byte(),size=r.u35(),crc=r.u35()) for _ in range(n)]
        if len({it['id'] for it in items})!=len(items) or not {0,1}.issubset({it['id'] for it in items}):
            raise RuntimeError('Backup object directory incomplete')
        if sum(it['size'] for it in items)>2*1024*1024:
            raise RuntimeError('Unexpected backup size')
        if r.i!=len(r.data) or [it['id'] for it in items] != [0,1,2,3,4,5,6,7,8,9,32,33,34]:
            raise RuntimeError('Unexpected object manifest')
        if any(it['size'] > (81920 if it['id']>=32 else 3840) for it in items):
            raise RuntimeError('Object too large')
        objects=[]
        for it in items:
            data=bytearray()
            for off in range(0,it['size'],256):
                count=min(256,it['size']-off)
                args=[it['id'],*[(off>>(7*k))&127 for k in range(5)],count&127,(count>>7)&127]
                q=Reader(request(dev.link,66,args))
                obj,rc=q.take(2)
                offset=q.u35()
                actual=q.byte()|(q.byte()<<7)
                raw=unpack_editor(q.data[q.i:])
                if (obj,rc,offset,actual,len(raw))!=(it['id'],0,off,count,count):
                    raise RuntimeError(f'Unexpected backup response: object {obj}, status {rc}, offset {offset}, count {actual}')
                data.extend(raw)
            if len(data)!=it['size'] or zlib.crc32(data)!=it['crc']:
                raise RuntimeError(f'Backup CRC failed for object {it["id"]}; no backup committed')
            objects.append(dict(it,data=base64.b64encode(data).decode('ascii')))
            print(f'Object {it["id"]}: {len(data)} bytes, CRC verified',flush=True)
        backup=dict(format='felucca-backup',version=1,firmware=state['version'],created=datetime.now(timezone.utc).isoformat().replace('+00:00','Z'),objects=objects)
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_text(json.dumps(backup,indent=2)+'\n')
        print(f'COMPLETE: {dest} ({sum(it["size"] for it in objects)} bytes in {len(objects)} objects)',flush=True)
    finally:
        dev.link.close()

if __name__=='__main__':
    capture(sys.argv[1])
