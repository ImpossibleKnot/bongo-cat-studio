# SPDX-License-Identifier: GPL-3.0-only
"""Encode the six-second real-renderer captures; requires Pillow."""
from pathlib import Path
from PIL import Image, ImageOps, ImageDraw
root=Path(__file__).resolve().parents[1]
stills=[]
for name in ('studio','maximum-cat','cat-cafe'):
    raw=(root/'build/media'/f'{name}.rgb').read_bytes()
    frames=[Image.frombytes('RGB',(240,240),raw[i:i+172800]) for i in range(0,len(raw),172800)]
    # One palette per animation, sampled over time, avoids colour flicker.
    atlas=Image.new('RGB',(240*8,240))
    for i in range(8):atlas.paste(frames[i*12],(i*240,0))
    pal=atlas.quantize(colors=192,method=Image.Quantize.MEDIANCUT)
    out=[f.quantize(palette=pal,dither=Image.Dither.NONE).resize((480,480),Image.Resampling.NEAREST) for f in frames]
    out[0].save(root/'docs/media'/f'{name}.gif',save_all=True,append_images=out[1:],duration=[60,60,60,70]*24,loop=0,optimize=False,disposal=1)
    stills.append(frames[24].resize((480,480),Image.Resampling.NEAREST))
    print(name,len(frames),(root/'docs/media'/f'{name}.gif').stat().st_size)
contact=Image.new('RGB',(1440,480))
for i,im in enumerate(stills):contact.paste(im,(i*480,0))
contact.save(root/'build/media-review.png')
