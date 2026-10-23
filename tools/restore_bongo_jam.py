#!/usr/bin/env python3
"""Restore only the live jam from a validated Bongo archive to a supported Bongo build.
Uses Felucca's bounded, CRC-checked BK_PUT protocol; never raw flash addresses.
Requires the 0.2 build with the runtime metadata-restore hook. Other projects,
user presets, samples and settings are untouched. The firmware saves the jam.
"""
import base64
import json
from pathlib import Path
import sys
import time
import zlib
import mido
from capture_felucca_backup import ota, info

archive=json.loads(Path(sys.argv[1]).read_text())
assert archive['format']=='felucca-backup' and archive['version']==1
assert archive['firmware'].startswith('BONGO CAT ')
for item in archive['objects']:
    data=base64.b64decode(item['data'],validate=True)
    assert len(data)==item['size'] and zlib.crc32(data)==item['crc'], item['id']
item=next(x for x in archive['objects'] if x['id']==0)
raw=base64.b64decode(item['data']);assert len(raw)==3840

def u32(n):return [(n>>(7*k))&127 for k in range(5)]
def pack(data):
    out=[]
    for offset in range(0,len(data),7):
        part=data[offset:offset+7]
        out.append(sum((b>>7)<<k for k,b in enumerate(part)))
        out.extend(b&127 for b in part)
    return out

dev=ota.Updater(ota.MidoBackend(),port='Felucca').find()
assert dev and dev.id.text=='FM-1_980'
try:
    state=info(dev.link);assert state['version'] in ('BONGO CAT 0.2.0', 'BONGO CAT 0.2.1', 'BONGO CAT 0.3.0', 'BONGO CAT 0.4.0-rc1'),state
    def put(args):
        dev.link.drain();assert dev.link.send(bytes([240,125,70,76,67,*args,247]))
        end=time.monotonic()+4
        while time.monotonic()<end:
            p=dev.link.read(.2)
            if p and p[:5]==bytes([240,125,70,76,67]):
                assert p[5:-1]==bytes([args[0],0,0]),list(p)
                return
        raise TimeoutError('No backup PUT acknowledgement')
    dev.link.out.send(mido.Message('stop'))
    for ch in range(4):dev.link.out.send(mido.Message('control_change',channel=ch,control=123,value=0))
    put([0,0,*u32(len(raw)),*u32(zlib.crc32(raw))])
    try:
        for offset in range(0,len(raw),256):put([1,0,*u32(offset),*pack(raw[offset:offset+256])])
        put([2,0])
    except Exception:
        try:put([3,0])
        except Exception:pass
        raise
    print(json.dumps({'firmware':state['version'],'restored_object':0,'bytes':len(raw),'source':sys.argv[1],'source_crc':item['crc'],'result':'CRC-checked runtime restore accepted; firmware queues save'},indent=2))
finally:
    dev.link.close()
