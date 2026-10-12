# MEd revival: a GTA III map editor

MEd (MapEditor) v0.32 was a map editor for GTA III, Vice City and San Andreas by Tony Wroblewski (2005-2006). It was abandoned and its source was never released. This project rebuilds it from the original program (`MEdv032/`), decompiled with Ghidra, as modern C++, and adds what map makers need today.

**Status:** GTA III. Vice City and San Andreas are planned.

## What it does

- A 3D view of the whole map with MEd's camera and controls; regions (IPL files) can be shown or hidden.
- Select, move, rotate (typed values, fine steps, quarter turns), duplicate, copy and paste, delete (safely below the map, or permanently), undo and redo (20 steps).
- New objects from any IDE definition; new regions; moving objects between regions.
- The **Placements** tab (find any placed object by name or ID and fly to it), the **Item** tab (every object definition, how often it is placed, which game pool it uses) and **Bookmarks** (MEd's format).
- **Check Map** (problems the game itself rejects or chokes on) and **Pools** (how full the game's object pools are; limit adjuster aware).
- Save straight into the game folder (with backups) or **Save as Mod** for the separate GTA3 Mod Manager.

See [GUIDE.md](GUIDE.md) for how to use it, and [CLAUDE.md](CLAUDE.md) for everything that was recovered from MEd and from the game, how, and why.

## Building

Windows, MinGW-w64 g++ and CMake:

```
cmake -S . -B build/cmake -G "MinGW Makefiles"
cmake --build build/cmake
cd build/cmake && ctest
```

Run `build/cmake/med_viewer.exe` and choose your GTA III folder. **No game files are included**: you need your own copy of GTA III.

## What is not in this repository

GTA III's files and anything decompiled from the game's executable belong to Rockstar Games and are not published here. The Ghidra databases are left out because they are large and can be rebuilt (see CLAUDE.md, Tooling). The original MEd 0.32 release is included as permitted by its licence (`MEdv032/readme.txt`), with the Visual C++ 7.1 runtime DLLs (`msvcp71.dll`, `msvcr71.dll`) it shipped with, which MEd.exe needs to start.
