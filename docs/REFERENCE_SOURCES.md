# Reference External Sources

This document records the external reference repositories and upstream sources used during the analysis, reverse-engineering, and recompilation of *Super Mario World 2: Yoshi's Island*.

## 1. Yoshi's Island SNES Disassembly
- **Repository**: [brunovalads/yoshisisland-disassembly](https://github.com/brunovalads/yoshisisland-disassembly)
- **Description**: Comprehensive assembly source disassembly of *Super Mario World 2: Yoshi's Island* (USA), containing complete symbol tables, memory map labels, bank definitions, and routine documentation.
- **Usage**: Used for verifying bank structures, symbol names (`yi.sym`), routine entry points, and SuperFX memory mappings. Not bundled in the core repository tree to maintain repository cleanliness and respect upstream boundaries.

## 2. SNESRecomp Toolchain & Runner
- **Repository**: [RetroPortingToolKit/snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp)
- **Description**: Recompilation toolchain and runtime framework for Super Nintendo Entertainment System binaries.
