# CLAUDE.md

Guidance for Claude Code (and other agents) working in this repository.

## What this is

Bloaty McBloatface: a size profiler for binaries. It parses ELF, Mach-O,
PE/COFF, and WebAssembly (plus DWARF debug info and JS source maps) with its
own vendored-format-header parsers, so **any host OS can analyze any format**
— analyzing a Mach-O binary does not need a Mac, analyzing an EXE does not
need Windows. CI enforces exactly that (see below).

## Build

CMake >= 3.16 + a C++20 compiler. All dependencies are git submodules
(system versions are preferred on UNIX when available via pkg-config).

```sh
git submodule update --init --recursive   # configure hard-errors if missing
cmake -B build -G Ninja
cmake --build build --parallel
```

The binary lands at `build/bloaty`. CMake 4.x works out of the box: the
top-level CMakeLists sets `CMAKE_POLICY_VERSION_MINIMUM=3.5` for vendored
subprojects (zlib) that declare pre-3.5 minimums.

## Tests

Two suites:

### ctest (googletest; no external tools needed)

```sh
ctest --test-dir build --output-on-failure
```

Includes `bloaty_ingestion_test`, which runs bloaty against checked-in
fixtures of every supported format (ELF, PE, Mach-O thin + universal, WASM,
DWARF5) — this is the cross-platform ingestion guarantee, and it must pass on
Linux, macOS, and Windows. Fixtures live in `tests/testdata/`; the
Mach-O/WASM ones are in `tests/testdata/ingestion/` with their yaml2obj
sources and a README describing regeneration.

### lit (deep per-format coverage; needs LLVM tools)

Requires `FileCheck`, `yaml2obj`, and `lit` at configure time. On
Ubuntu/Debian:

```sh
apt-get install llvm-18 llvm-18-tools
```

Gotcha: **FileCheck is in `llvm-18-tools`, not `llvm-18`** (which has
yaml2obj). `llvm-18-tools` also ships a runnable lit at
`/usr/lib/llvm-18/build/utils/lit/lit.py`, so no pip install is needed.
Configure with the tools pinned, then run the suite:

```sh
cmake -B build -G Ninja \
  -DFILECHECK_EXECUTABLE=/usr/lib/llvm-18/bin/FileCheck \
  -DYAML2OBJ_EXECUTABLE=/usr/lib/llvm-18/bin/yaml2obj \
  -DLIT_EXECUTABLE=/usr/lib/llvm-18/build/utils/lit/lit.py
cmake --build build --target check-bloaty
```

If any of the three tools is not found at configure time, the `check-bloaty`
target silently does not exist — a passing build does NOT mean lit ran.

## CI

`.github/workflows/ci.yml`, trigger `on: push:`. Four jobs: linux-gcc,
linux-clang, macos (AppleClang, arm64), windows (MSVC x64, Ninja, Release).
All four run ctest (including the ingestion test — the all-formats-on-all-OSes
proof); Linux and macOS also run the lit suite. Windows is ctest-only (no
cheap prebuilt FileCheck/yaml2obj there). Only GitHub-owned actions are used;
ccache + actions/cache make warm builds fast. The built binary is uploaded as
an artifact per OS.

## Layout

```
src/            bloaty core; per-format parsers (elf.cc, macho.cc, pe.cc,
                webassembly.cc, dwarf.cc + dwarf/) are compiled on ALL
                platforms — never gate a parser on the host OS
tests/          googletest suites (*.cc), lit suites (elf/ dwarf/ macho/
                pe/ wasm/ *.test), fixtures under tests/testdata/
third_party/    submodules (protobuf, capstone, re2, abseil, zlib, zstd,
                googletest, demumble) + vendored format headers
                (freebsd_elf/, darwin_xnu_macho/, lief_pe/)
```

## Conventions

- Format parsers read bytes via the vendored structs in `third_party/`
  (freebsd_elf, darwin_xnu_macho, lief_pe); do not include system format
  headers.
- Platform-specific code is guarded (`_WIN32`/`_MSC_VER`); the only such
  site in src/ is the mmap-vs-Win32-file-mapping input path in bloaty.cc.
- Test fixtures are checked-in binaries (no LFS). Generated fixtures keep
  their yaml2obj YAML source next to the binary; the YAML is the source of
  truth.
