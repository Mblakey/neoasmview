# NVIM Assembly Viewer

Inspired by my love of GodBolt, but wanting to see real assembly outputs from large C projects that I work on. This repo contains the tooling 
needed to view clean filtered assembly code from invididual function, using the CMake compile_commands.json as a guide for building 
the invididual ASM files. 

## RoadMap

### Underlying Logic

[x] - Prototype, show assembly from given functions parsed from compile_commands
[]  - test with library code, imported packages from CMake etc.
[]  - create hash tables and store function assembly for quick access 
[]  - have assembly regenerated on last edit change (see Make for logic) 
[]  - create tooling for neovim plugin, use Treesitter as dependency for function hooking
[]  - live event hooking, assembly can change as the user types to show changes as they happen
[]  - lua build system to work with Lazy
[]  - full testing and release
