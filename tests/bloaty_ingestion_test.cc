// Copyright 2026 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Cross-format ingestion test.
//
// Bloaty's file-format support is host-independent by design: the format
// dispatch tries ELF, Mach-O, WebAssembly, and PE unconditionally on every
// platform, and the parsers use vendored format headers rather than system
// ones.  This test pins that property down: it ingests a fixture of every
// supported format family and asserts that real parsing happened (expected
// segment/section/arch/compileunit names show up), not just magic-number
// sniffing.  It runs under plain ctest with no external tool dependencies,
// so it exercises the guarantee on Linux, macOS, and Windows alike.
//
// Runs with WORKING_DIRECTORY == tests/testdata/ingestion (see
// tests/testdata/ingestion/README.md for fixture provenance).

#include "test.h"

class IngestionTest : public BloatyTest {
 protected:
  void CollectNames(const bloaty::RollupRow& row,
                    std::vector<std::string>* names) {
    for (const auto& child : row.sorted_children) {
      names->push_back(child.name);
      CollectNames(child, names);
    }
  }

  // Returns all row names in the output hierarchy (all levels).
  std::vector<std::string> AllRowNames() {
    std::vector<std::string> names;
    CollectNames(*top_row_, &names);
    return names;
  }

  void ExpectRowsNamed(const std::vector<std::string>& expected) {
    std::vector<std::string> names = AllRowNames();
    for (const auto& want : expected) {
      EXPECT_THAT(names, testing::Contains(want));
    }
  }

  void ExpectRowContaining(const std::string& substring) {
    std::vector<std::string> names = AllRowNames();
    EXPECT_THAT(names,
                testing::Contains(testing::HasSubstr(substring)));
  }

  // The pretty-printed (human readable) report for the last RunBloaty call.
  std::string PrettyOutput() {
    bloaty::OutputOptions output_options;
    output_options.output_format = bloaty::OutputFormat::kPrettyPrint;
    std::ostringstream stream;
    output_->Print(output_options, &stream);
    return stream.str();
  }
};

TEST_F(IngestionTest, ElfExecutable) {
  RunBloaty({"bloaty", "-d", "segments,sections", "-n", "0",
             "../linux-x86_64/05-binary.bin"});
  ExpectRowsNamed({".text", ".data"});
  ExpectRowContaining("LOAD");  // Segment names look like "LOAD #2 [RX]".
}

TEST_F(IngestionTest, ElfSharedObject) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0",
             "../linux-x86_64/04-simple.so"});
  ExpectRowsNamed({".text", ".dynsym", ".dynamic"});
}

TEST_F(IngestionTest, PEExecutable) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0",
             "../PE/x64/msvc-16.0-foo-bar-main-cv.bin"});
  ExpectRowsNamed({".text", ".rdata", "[PE Headers]"});
}

TEST_F(IngestionTest, PEDll) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0",
             "../PE/x64/msvc-16.0-foo-bar.dll"});
  ExpectRowsNamed({".text", ".rdata", "[PE Headers]"});
}

TEST_F(IngestionTest, MachOThinSegments) {
  RunBloaty({"bloaty", "-d", "segments", "-n", "0",
             "macho-thin-arm64.bin"});
  ExpectRowsNamed({"__TEXT", "__LINKEDIT"});
}

TEST_F(IngestionTest, MachOThinSections) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0",
             "macho-thin-arm64.bin"});
  ExpectRowsNamed({"__TEXT,__text"});
}

// A universal (fat) binary must report all of its architecture slices; this
// exercises the fat header parsing path, not just the per-slice parser.
TEST_F(IngestionTest, MachOUniversalArchs) {
  RunBloaty({"bloaty", "-d", "archs", "--domain=file", "-n", "0",
             "macho-universal.bin"});
  ExpectRowsNamed({"arm64", "x86_64"});
}

TEST_F(IngestionTest, MachOUniversalSegmentsWithinArch) {
  RunBloaty({"bloaty", "-d", "archs,segments", "--domain=file", "-n", "0",
             "macho-universal.bin"});
  ExpectRowsNamed({"arm64", "x86_64", "__TEXT"});
}

TEST_F(IngestionTest, WasmSections) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0", "wasm-module.wasm"});
  ExpectRowsNamed({"Code", "Data", "linking", "producers"});
}

// A WASM module has no VM data at all, so every VM percentage is 0/0; the
// report must print a sane value (0.0%), not "NAN%".
TEST_F(IngestionTest, WasmZeroVmPercentIsSane) {
  RunBloaty({"bloaty", "-d", "sections", "-n", "0", "wasm-module.wasm"});
  std::string pretty = PrettyOutput();
  EXPECT_THAT(pretty, testing::Not(testing::HasSubstr("NAN")));
  EXPECT_THAT(pretty, testing::Not(testing::HasSubstr("nan")));
  EXPECT_THAT(pretty, testing::HasSubstr("0.0%"));
}

TEST_F(IngestionTest, WasmSymbols) {
  RunBloaty({"bloaty", "-d", "symbols", "--domain=file", "-n", "0",
             "wasm-module.wasm"});
  ExpectRowContaining("func[");  // Per-function attribution happened.
}

// DWARF parsing is format-independent (used for ELF, Mach-O, and WASM
// inputs); this proves the deep DWARF 5 path, including the line table.
TEST_F(IngestionTest, ElfDwarf5CompileUnits) {
  RunBloaty({"bloaty", "-d", "compileunits", "-n", "0",
             "../dwarf5/dwarf5_simple_exe"});
  ExpectRowContaining("dwarf5_simple.c");
}
