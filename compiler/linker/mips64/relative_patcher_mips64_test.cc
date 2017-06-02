/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "linker/relative_patcher_test.h"
#include "linker/mips64/relative_patcher_mips64.h"

namespace art {
namespace linker {

class Mips64RelativePatcherTest : public RelativePatcherTest {
 public:
  Mips64RelativePatcherTest() : RelativePatcherTest(kMips64, "default") {}

 protected:
  static const uint8_t kUnpatchedPcRelativeRawCode[];
  static const uint8_t kUnpatchedPcRelativeCallRawCode[];
  static const uint32_t kLiteralOffsetHigh;
  static const uint32_t kLiteralOffsetLow1;
  static const uint32_t kLiteralOffsetLow2;
  static const uint32_t kAnchorOffset;
  static const ArrayRef<const uint8_t> kUnpatchedPcRelativeCode;

  uint32_t GetMethodOffset(uint32_t method_idx) {
    auto result = method_offset_map_.FindMethodOffset(MethodRef(method_idx));
    CHECK(result.first);
    return result.second;
  }

  void CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches, uint32_t target_offset);
  void TestDexCacheReference(uint32_t dex_cache_arrays_begin, uint32_t element_offset);
  void TestStringReference(uint32_t string_offset);
};

const uint8_t Mips64RelativePatcherTest::kUnpatchedPcRelativeRawCode[] = {
    0x34, 0x12, 0x5E, 0xEE,  // auipc  s2, high(diff); placeholder = 0x1234
    0x78, 0x56, 0x52, 0x66,  // daddiu s2, s2, low(diff); placeholder = 0x5678
    0x78, 0x56, 0x52, 0x9E,  // lwu    s2, (low(diff))(s2) ; placeholder = 0x5678
};
const uint32_t Mips64RelativePatcherTest::kLiteralOffsetHigh = 0;  // At auipc.
const uint32_t Mips64RelativePatcherTest::kLiteralOffsetLow1 = 4;  // At daddiu.
const uint32_t Mips64RelativePatcherTest::kLiteralOffsetLow2 = 8;  // At lwu.
const uint32_t Mips64RelativePatcherTest::kAnchorOffset = 0;  // At auipc (where PC+0 points).
const ArrayRef<const uint8_t> Mips64RelativePatcherTest::kUnpatchedPcRelativeCode(
    kUnpatchedPcRelativeRawCode);

void Mips64RelativePatcherTest::CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches,
                                                     uint32_t target_offset) {
  AddCompiledMethod(MethodRef(1u), kUnpatchedPcRelativeCode, ArrayRef<const LinkerPatch>(patches));
  Link();

  auto result = method_offset_map_.FindMethodOffset(MethodRef(1u));
  ASSERT_TRUE(result.first);

  uint32_t diff = target_offset - (result.second + kAnchorOffset);
  diff += (diff & 0x8000) << 1;  // Account for sign extension in daddiu/lwu.

  const uint8_t expected_code[] = {
      static_cast<uint8_t>(diff >> 16), static_cast<uint8_t>(diff >> 24), 0x5E, 0xEE,
      static_cast<uint8_t>(diff), static_cast<uint8_t>(diff >> 8), 0x52, 0x66,
      static_cast<uint8_t>(diff), static_cast<uint8_t>(diff >> 8), 0x52, 0x9E,
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

void Mips64RelativePatcherTest::TestDexCacheReference(uint32_t dex_cache_arrays_begin,
                                                      uint32_t element_offset) {
  dex_cache_arrays_begin_ = dex_cache_arrays_begin;
  LinkerPatch patches[] = {
      LinkerPatch::DexCacheArrayPatch(kLiteralOffsetHigh, nullptr, kAnchorOffset, element_offset),
      LinkerPatch::DexCacheArrayPatch(kLiteralOffsetLow1, nullptr, kAnchorOffset, element_offset),
      LinkerPatch::DexCacheArrayPatch(kLiteralOffsetLow2, nullptr, kAnchorOffset, element_offset)
  };
  CheckPcRelativePatch(ArrayRef<const LinkerPatch>(patches),
                       dex_cache_arrays_begin_ + element_offset);
}

TEST_F(Mips64RelativePatcherTest, DexCacheReference) {
  TestDexCacheReference(/* dex_cache_arrays_begin */ 0x12345678, /* element_offset */ 0x1234);
}

}  // namespace linker
}  // namespace art
