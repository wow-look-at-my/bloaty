
# Bloaty Tests

Bloaty has two sets of tests:

- C++ tests in `tests/*.cc`
- `lit` tests in `tests/**/*.test`

We are in the process of migrating existing tests to `lit` where possible (see: https://github.com/google/bloaty/issues/221).

## `lit` tests

These tests use the `lit` and `yaml2obj` tools from LLVM:
- `yaml2obj` allows us to generate very specific ELF/Mach-O/PE files from a
  text-based YAML format.
- `lit` lets us intermix this YAML with commands to run Bloaty and make
  assertions about its output.

This is ideal for testing Bloaty's parsers, because
`yaml2obj` is a precise and readable way of constructing
input payloads.

To run these tests via CMake, a few additional parameters
must be specified currently:
- `-DLIT_EXECUTABLE=<PATH>`: specifies where to find the lit tool
- `-DFILECHECK_EXECUTABLE=<PATH>`: specifies where to find the FileCheck tool
- `-DYAML2OBJ_EXECUTABLE=<PATH>`: specifies where to find the yaml2obj tool

Any reasonably recent LLVM release provides suitable tools (LLVM 18 is known
to work). On Ubuntu/Debian, `apt-get install llvm-18 llvm-18-tools` provides
all three — note that FileCheck is in `llvm-18-tools`, not `llvm-18`, and
`llvm-18-tools` also ships a runnable lit, so no pip install is required:

```sh
cmake -B build -G Ninja -S . \
    -DLIT_EXECUTABLE=/usr/lib/llvm-18/build/utils/lit/lit.py \
    -DFILECHECK_EXECUTABLE=/usr/lib/llvm-18/bin/FileCheck \
    -DYAML2OBJ_EXECUTABLE=/usr/lib/llvm-18/bin/yaml2obj
cmake --build build
cmake --build build --target check-bloaty
```

On macOS, `brew install llvm` provides `FileCheck` and `yaml2obj` under
`$(brew --prefix llvm)/bin`, and lit can be installed with
`pip install --user lit` or `pipx install lit`.

`LIT_EXECUTABLE` accepts either form of lit: LLVM's `lit.py` source script
(CMake runs it through the Python interpreter it finds), or an installed
`lit` entry point such as the pip/pipx ones above (CMake executes it
directly, so the entry point's own shebang — and therefore its venv, in the
pipx case — is respected).

Note: if any of the three tools is missing at configure time, the
`check-bloaty` target is silently not created — "the build passed" does not
imply the lit tests ran.


## C++ Tests

The C++ tests are conventional C++ unit tests that use https://github.com/google/googletest.

Going forward, C++ should only be used for tests that do not
parse binary input files.  For example, C++ is good for
testing Bloaty's data structures and aggregation/reporting
logic.

One deliberate exception: `bloaty_ingestion_test` parses checked-in fixtures
of every supported format (see `tests/testdata/ingestion/README.md`). It
exists precisely because it needs no LLVM tools at test time, so it can prove
on every CI platform — including hosts without FileCheck/yaml2obj — that any
OS can ingest any format.

To run the C++ tests (Git only, these are not included in the release tarball), type:

```
$ cmake --build build --config Debug --target test
```
