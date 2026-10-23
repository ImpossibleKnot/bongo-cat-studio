# Source and release provenance

Tiny Studio 0.4.0-rc1 is an independent modification of Felucca 1.6.1:

- Felucca: https://github.com/hugelton/Felucca, commit `a53ecf428c29c780108fd70a31b71a32a6b5c313`.
- Felucca-Engines: https://github.com/hugelton/Felucca-Engines, commit `c46e0086f69bdc26df1cd144dca9e683cc4531dd`.
- Firmware package SHA-256: `6796ac71b56b2bffbd35a18ee6df5a0e8010ee9a16714c9c64a716b10c6c3a39`.
- Complete release source SHA-256: `3b5ee6a28e62d3e8ef2de5b35454f1e43b0d3e240e2bc27e71de579817d46621`.

The Git repository begins from that complete release source snapshot. Engine files are vendored at the commit above, with their original MIT notices, so ordinary clones and GitHub source ZIPs contain them. The original release tarball retains its upstream `.gitmodules` declaration and also contains the actual engine files.

Publication adds the README, rendered GIFs, capture/encoding tools, installation tools, verification reports and contributor documentation. These additions do not change the released firmware or the archived corresponding source. The Git version of the build/licensing guide explains vendoring; the original upstream build guide, README and contribution guide are preserved in `docs/UPSTREAM-*` as historical references.

The overlay, project metadata and firmware hooks are GPL-3.0-only. All upstream notices and third-party licences are retained. See [credits](../CREDITS.md), [licensing](../LICENSING.md) and [licence texts](../LICENSES/).

The distributed package uses the existing Felucca loader, custom identity `FM-1_980`, and the three pinned JieLi SDK files described in the licensing guide. The SDK checkout/toolchain are external build prerequisites, not vendored here. No official M-VAVE firmware is distributed.
