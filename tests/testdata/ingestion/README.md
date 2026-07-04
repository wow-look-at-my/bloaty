# Cross-format ingestion fixtures

These fixtures back `bloaty_ingestion_test` (see `tests/bloaty_ingestion_test.cc`),
which proves that **every** file format bloaty supports can be ingested on
**every** host OS: the test runs under plain ctest on Linux, macOS, and Windows
with no external tools required.

The `.yaml` files are the source of truth; the binaries are checked in so the
test is hermetic and never needs `yaml2obj` at test time.

## Checked-in binaries (generated from the YAML in this directory)

| File | Format | Generated with |
|------|--------|----------------|
| `macho-thin-arm64.bin` | Mach-O 64-bit arm64 executable (thin) | `yaml2obj macho-thin-arm64.yaml -o macho-thin-arm64.bin` |
| `macho-universal.bin` | Mach-O universal (fat) binary, x86_64 + arm64 slices | `yaml2obj macho-universal.yaml -o macho-universal.bin` |
| `wasm-module.wasm` | WebAssembly object with Code/Data/linking/producers sections | `yaml2obj wasm-module.yaml -o wasm-module.wasm` |

All three were generated with LLVM 18 `yaml2obj` (18.1.3, Ubuntu
`llvm-18` package). Any reasonably recent `yaml2obj` should produce equivalent
output; regenerate only when the YAML changes.

The Mach-O YAML is derived from `tests/macho/archs.test` (the lit test for the
`archs` data source); the WASM YAML is derived from `tests/wasm/sections.test`.

## Fixtures reused from sibling directories

To avoid duplicating binaries already in the repository, the ingestion test
references these existing fixtures by relative path:

| Fixture | Format |
|---------|--------|
| `../linux-x86_64/05-binary.bin` | ELF x86-64 executable |
| `../linux-x86_64/04-simple.so` | ELF x86-64 shared object |
| `../PE/x64/msvc-16.0-foo-bar-main-cv.bin` | PE/COFF x64 executable |
| `../PE/x64/msvc-16.0-foo-bar.dll` | PE/COFF x64 DLL |
| `../dwarf5/dwarf5_simple_exe` | ELF executable with DWARF 5 debug info |

See `../make_test_files.sh` (ELF), `../make_all_msvc_test_files.bat` (PE), and
`../dwarf5/README.md` (DWARF 5) for how those were built.
