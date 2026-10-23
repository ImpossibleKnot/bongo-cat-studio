#!/usr/bin/env python3
"""Build the custom studio with Felucca's checked builder and original loader."""
from pathlib import Path
import hashlib
import shutil
import build

build.PRODUCT = "FM-1_980"
build.VERSION = "BONGO 0.4.0-rc1"
build.main()
out = Path(__file__).resolve().parents[1] / "build"
dest = out / "bongo-cat-studio-0.4.0-rc1.fwsc"
shutil.copyfile(out / "felucca.fwsc", dest)
(out / "BONGO-SHA256SUMS").write_text(f"{hashlib.sha256(dest.read_bytes()).hexdigest()}  {dest.name}\n")
print(f"Bongo Cat's Tiny Studio: {dest}")
