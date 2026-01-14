# NVIM Assembly Viewer

**VERSION 0.6**

Inspired by my love of GodBolt, but wanting to see real assembly outputs from large C projects that I work on. This repo contains the tooling 
needed to view clean filtered assembly code from invididual functions while editing in some sort of IDE (nvim). This uses the CMake compile_commands.json 
as a guide for building the invididual ASM files. <br>

There is also tooling avalible for Rust. The project still relies on the compile_commands.json file, but a tool called `bear-cargo` is avaliable in this project
to capture the `rustc` compiler invocations, similar to the `bear` tool for Makefiles. This is as close to C|C++ assembly per file (not function) that I could get with Rust. 

## RoadMap

### Underlying Logic

[x] - Prototype, show assembly from given functions parsed from compile_commands <br>
[x] - test with library code, imported packages from CMake etc. <br>
[x] - client-server model <br>
[x] - create hash tables and store function assembly for quick access <br>
[x]  - have assembly regenerated based on last edit change (see Make for logic) <br>
[x] - live event hooking, assembly can change as the user types to show changes as they happen <br>
[]  - lua build system to work with Lazy <br>
[]  - full testing and release <br>


## Standalone Server 

The server that can accept requests similar to the tool above is `asm-server`. This creates a pid specific socket (see logs)
where <file> <label> arguements can be sent and evaulated live.

```
asm-server [project dir]
```

requests can then be sent using a common tools such as `nc`. <br>
e.g `nc -U vimasm_<pid>.sock`. 

Further documentation on this project will be gathered as it matures. Version 1.0 release will just require some testing and coding in live environments to get it perfect.
