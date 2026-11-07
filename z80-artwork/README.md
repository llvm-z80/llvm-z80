# LLVM-Z80 Artwork

Artwork for the LLVM-Z80 project, drawn by zlfn in
[Aseprite](https://www.aseprite.org/).

| File                      | Description                                           |
| ------------------------- | ----------------------------------------------------- |
| `LLVM-Z80.aseprite`       | Source file                                           |
| `LLVM-Z80.png`            | 256×128, original size                                |
| `LLVM-Z80@10x.png`        | 2560×1280, used in the top-level README               |
| `LLVM-Z80-light.aseprite` | Source file for light backgrounds                     |
| `LLVM-Z80-light.png`      | 256×128, original size                                |
| `LLVM-Z80-light@10x.png`  | 2560×1280, used in the top-level README in light mode |

To export the PNGs again:

```
aseprite -b LLVM-Z80.aseprite --save-as LLVM-Z80.png
aseprite -b LLVM-Z80.aseprite --scale 10 --save-as LLVM-Z80@10x.png
aseprite -b LLVM-Z80-light.aseprite --save-as LLVM-Z80-light.png
aseprite -b LLVM-Z80-light.aseprite --scale 10 --save-as LLVM-Z80-light@10x.png
```

## License

The artwork is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
([LICENSE-CC-BY-4.0](LICENSE-CC-BY-4.0)). Credit it as "LLVM-Z80 logo by zlfn".

The dragon is a pixel art version of the LLVM wyvern logo
(`img/LLVMWyvernBig.png` in the [llvm-www](https://github.com/llvm/llvm-www)
repository). The license of the wyvern logo is unclear. The LLVM website does
not state one, but the llvm-www repository is distributed under the Apache
License v2.0 with LLVM Exceptions, so use of the dragon is expected to be
permitted under that license. This is not guaranteed. When you redistribute the
artwork, include a copy of that license ([LICENSE-LLVM](LICENSE-LLVM)).

Neither license grants permission to use trademarks. For the LLVM name, see the
notice in the top-level [README](../README.md#notice).

## Third-party logos

The `third-party/` directory holds unmodified copies of other projects' logos
used in this repository, such as in the "Thanks to" section of the top-level
README. They are not covered by the licenses above and belong to their
respective owners.

| File                                    | Source                                                                                                                           |
| --------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `third-party/fossforall-black-text.svg` | `gradient/logo-black-text.svg` in [foss-for-all/logo-artworks](https://github.com/foss-for-all/logo-artworks)                    |
| `third-party/fossforall-white-text.svg` | `gradient/logo-white-text.svg` in [foss-for-all/logo-artworks](https://github.com/foss-for-all/logo-artworks)                    |
| `third-party/compiler-explorer.svg`     | `views/resources/site-logo.svg` in [compiler-explorer/compiler-explorer](https://github.com/compiler-explorer/compiler-explorer) |
