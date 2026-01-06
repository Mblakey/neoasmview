# NVIM Assembly Viewer

Inspired by my love of GodBolt, but wanting to see real assembly outputs from large C projects that I work on. This repo contains the tooling 
needed to view clean filtered assembly code from invididual function, using the CMake compile_commands.json as a guide for building 
the invididual ASM files. 

## RoadMap

### Underlying Logic

[x] - Prototype, show assembly from given functions parsed from compile_commands <br>
[x]  - test with library code, imported packages from CMake etc. <br>
[]  - client-server model, have my execs be able to run and accept multiple requests <br>
[]  - create hash tables and store function assembly for quick access <br>
[]  - have assembly regenerated on last edit change (see Make for logic) <br>
[]  - create tooling for neovim plugin, use Treesitter as dependency for function hooking <br>
[]  - live event hooking, assembly can change as the user types to show changes as they happen <br>
[]  - lua build system to work with Lazy <br>
[]  - full testing and release <br>


## Standalone Tooling 

The tooling for this project can be used outside of neovim for debugging/testing. Plus the server implementation should be quickly portable to other
IDEs. 

```
asm-filter <project dir> <file> <label>
```

Project directory is the `dir` where `compile_commands.json` is located. <br>
File is which source file the function is located in, and label is the function name. 

For example on the test suite:  <br>

```
./build/asm-filter ./build/tests/simple_exec ./tests/simple_exec/foo.c foo
```

