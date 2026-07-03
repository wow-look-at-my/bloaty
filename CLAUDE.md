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
linux-clang, macos (AppleClang, universal arm64+x86_64), windows (MSVC x64,
Ninja, Release). All four run ctest (including the ingestion test — the
all-formats-on-all-OSes proof); Linux and macOS also run the lit suite.
Windows is ctest-only (no cheap prebuilt FileCheck/yaml2obj there). Only
GitHub-owned actions are used; ccache + actions/cache make warm builds fast.

Artifact binaries (one per OS) are "runs anywhere" static, with per-job
enforcement steps: linux-gcc ships a fully static Release ELF (`-static`,
all deps vendored; verified via ldd/readelf); windows ships a static-CRT exe
(verified via dumpbin — no vcruntime/msvcp/api-ms-win-crt-* deps); macos
ships `bloaty-macos-universal` (arm64+x86_64, deployment target 11.0, deps
vendored **including the C++ runtime**: a pinned-LLVM source-built universal
libc++.a/libc++abi.a replaces libc++.1.dylib, cached via actions/cache).
The macOS binary is otool-verified per arch to depend on exactly two OS
dylibs: `/usr/lib/libSystem.B.dylib` (XNU's only stable syscall ABI — Apple
ships no static libSystem, and raw-syscall binaries break across macOS
updates) and CoreFoundation (required by abseil cctz `local_time_zone()`,
unconditional under `__APPLE__`; unremovable without patching the abseil
submodule). linux-clang stays a dynamic RelWithDebInfo sanity build with no
artifact.

## Cosmopolitan APE build (one file, every OS)

`cosmocc` (GCC 14 + Cosmopolitan Libc, from https://cosmo.zip/pub/cosmocc/)
builds bloaty as a single fat (x86_64+aarch64) Actually Portable Executable
that runs natively on Linux, macOS incl. Apple Silicon, Windows, and BSDs
with zero dylib/DLL dependencies. The exact configure line is in README.md
("One-file build" section) and in `.github/workflows/ape-probe.yml`
(manual-dispatch CI: ubuntu builds the APE, macOS-arm64 + Windows runners
ingestion-test the same file).

Why each knob (no submodule sources are patched — cache/flag level only):

- `-D__HAIKU__` (in CMAKE_C(XX)_FLAGS **and** env `CXXFLAGS`, because the
  top-level CMakeLists overwrites CMAKE_CXX_FLAGS after the third_party
  subdirs are added and appends env CXXFLAGS after that): cosmocc defines no
  OS macro abseil recognizes, so `ABSL_HAVE_MMAP` stays off and
  low_level_alloc.cc/per_thread_sem.cc compile EMPTY while mutex.cc still
  references them (undefined refs at link). `__HAIKU__`'s entire footprint in
  the vendored trees is ABSL_HAVE_MMAP=1 + absl elf_mem_image off +
  GTEST_OS_HAIKU. Do NOT use `__ros__` instead: it also flips
  ABSL_HAVE_SEMAPHORE_H and cosmo's sem_t is 256 bytes, overflowing absl's
  256-byte WaiterState (static assert).
- `-DC_FLAG_WA_NOEXECSTACK=OFF`: pre-seeds a zstd cache check so zstd drops
  `huf_decompress_amd64.S` (fat cosmocc refuses `.S` inputs entirely) and
  defines ZSTD_DISABLE_ASM.
- `-DCAPSTONE_SH_SUPPORT=OFF`: capstone's SH module declares
  `enum direction {read, write}` which collides with cosmo's amalgamated
  stdlib.h (declares POSIX read/write). Bloaty never maps to CS_ARCH_SH.
- `-DBLOATY_ENABLE_BUILDID=OFF`: `-Wl,--build-id` inserts
  .note.gnu.build-id ahead of cosmo's `.head` section and breaks the APE
  image layout ("PT_LOAD segments must be ordered by p_vaddr").

Gotchas: `chmod +x $COSMO/bin/cosmoranlib` after unzipping (ships 0644) and
keep `$COSMO/bin` on PATH during builds (cosmoranlib execs its real binary
via PATH). cosmoar rejects `@rsp` response files and paths with spaces.
Cosmo cannot build shared libs, so zlib's `example`/`example64` ctest
entries can't build (`-shared flag not supported`) — all 8 bloaty tests
pass; vendored protoc links as an APE and runs fine on the Linux build host
(no native protoc needed). ctest/bash can spawn APEs via the execvp ENOEXEC
shell fallback; raw execve spawners (python subprocess) need binfmt_misc or
an `assimilate`d copy.

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
