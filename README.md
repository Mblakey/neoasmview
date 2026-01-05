# NVIM Assembly Viewer

Inspired by my love of GodBolt, but wanting to see real assembly outputs from large C projects that I work on. This repo contains the tooling 
needed to view clean filtered assembly code from invididual function, using the CMake compile_commands.json as a guide for building 
the invididual ASM files. 

## RoadMap

### Underlying Logic

[x] - Prototype, show assembly from given functions parsed from compile_commands <br>
[]  - test with library code, imported packages from CMake etc. <br>
[]  - client-server model, have my execs be able to run and accept multiple requests <br>
[]  - create hash tables and store function assembly for quick access <br>
[]  - have assembly regenerated on last edit change (see Make for logic) <br>
[]  - create tooling for neovim plugin, use Treesitter as dependency for function hooking <br>
[]  - live event hooking, assembly can change as the user types to show changes as they happen <br>
[]  - lua build system to work with Lazy <br>
[]  - full testing and release <br>
