// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <catch2/catch_test_macros.hpp>

#include "core/arm/dynarmic/arm_tick_counts.h"

namespace {

struct TimingCase {
    bool thumb;
    u32 instruction;
    u64 expected_ticks;
};

template <std::size_t N>
void CheckTimings(const std::array<TimingCase, N>& cases) {
    for (const auto& test : cases) {
        CAPTURE(test.thumb, test.instruction);
        CHECK(Core::TicksForInstruction(test.thumb, test.instruction) == test.expected_ticks);
    }
}

} // namespace

TEST_CASE("ARM11 tick matcher extracts encoded fields", "[core][arm][timing]") {
    // The immediate's low four bits deliberately differ from Rd. This catches the old
    // deposit-style extraction bug, which read source low bits instead of selected fields.
    constexpr std::array cases{
        TimingCase{false, 0xE280000FU, 1}, // ADD r0, r0, #15
        TimingCase{false, 0xE280F000U, 7}, // ADD pc, r0, #0
        TimingCase{false, 0xE0000291U, 2}, // MUL
        TimingCase{false, 0xE0100291U, 5}, // MULS
        TimingCase{true, 0x000046F7U, 4},  // MOV pc, lr (split destination field)
        TimingCase{true, 0x00004687U, 5},  // MOV pc, r0
        TimingCase{false, 0xEE200A00U, 1}, // VMUL.F32
        TimingCase{false, 0xEE200B00U, 2}, // VMUL.F64
        TimingCase{false, 0xEC400F00U, 2}, // MCRR must precede generic STC
        TimingCase{false, 0xEC500F00U, 2}, // MRRC must precede generic LDC
    };
    CheckTimings(cases);
}

TEST_CASE("ARM11 branch and return timing approximations", "[core][arm][timing]") {
    constexpr std::array cases{
        TimingCase{false, 0xEAFFFFFEU, 1}, // B
        TimingCase{false, 0xEBFFFFFEU, 1}, // BL
        TimingCase{false, 0xE12FFF1EU, 4}, // BX lr
        TimingCase{false, 0x112FFF1EU, 7}, // BXNE lr
        TimingCase{false, 0xE12FFF10U, 5}, // BX r0
        TimingCase{false, 0xE12FFF30U, 5}, // BLX r0
        TimingCase{false, 0xE1A0F00EU, 4}, // MOV pc, lr
        TimingCase{false, 0x11A0F00EU, 7}, // MOVNE pc, lr
        TimingCase{false, 0xE1B0F00EU, 7}, // MOVS pc, lr (exception-style return)
        TimingCase{true, 0x0000D100U, 1},  // BNE
        TimingCase{true, 0x0000E000U, 1},  // B
        TimingCase{true, 0x00004770U, 4},  // BX lr
        TimingCase{true, 0x00004700U, 5},  // BX r0
        TimingCase{true, 0x00004780U, 5},  // BLX r0
        TimingCase{true, 0xF000F800U, 2},  // BL immediate pair
        TimingCase{true, 0xF000E800U, 2},  // BLX immediate pair
    };
    CheckTimings(cases);
}

TEST_CASE("ARM11 load and multiple-transfer timing", "[core][arm][timing]") {
    constexpr std::array cases{
        TimingCase{false, 0xE59DF000U, 4}, // LDR pc, [sp]
        TimingCase{false, 0x159DF000U, 8}, // LDRNE pc, [sp]
        TimingCase{false, 0xE59FF000U, 8}, // LDR pc, [pc]
        TimingCase{false, 0xE79DF100U, 8}, // LDR pc, [sp, r0, LSL #2]
        TimingCase{false, 0xE71DF000U, 9}, // LDR pc, [sp, -r0]
        TimingCase{false, 0xE7910002U, 1}, // positive register offset
        TimingCase{false, 0xE7910102U, 1}, // positive register offset, LSL #2
        TimingCase{false, 0xE7910182U, 2}, // positive register offset, LSL #3
        TimingCase{false, 0xE7110002U, 2}, // negative register offset
        TimingCase{false, 0xE19100B2U, 1}, // LDRH positive register offset
        TimingCase{false, 0xE11100B2U, 2}, // LDRH negative register offset
        TimingCase{false, 0xE8B00003U, 1}, // LDM, two words
        TimingCase{false, 0xE8B0000FU, 2}, // LDM, four words
        TimingCase{false, 0xE8BD8000U, 4}, // POP {pc}
        TimingCase{false, 0xE8BD80F0U, 4}, // POP {r4-r7, pc}
        TimingCase{false, 0xE8BDFFFFU, 9}, // large POP remains transfer-bound
        TimingCase{false, 0xE8B08000U, 8}, // non-SP LDM to pc
        TimingCase{false, 0x18BD8000U, 9}, // conditional POP {pc}
        TimingCase{false, 0xE8A0000FU, 2}, // STM, four words
        TimingCase{true, 0x0000B510U, 1},  // PUSH {r4, lr}
        TimingCase{true, 0x0000B5F0U, 3},  // PUSH {r4-r7, lr}
        TimingCase{true, 0x0000BD00U, 4},  // POP {pc}
        TimingCase{true, 0x0000BD10U, 4},  // POP {r4, pc}
        TimingCase{true, 0x0000C81EU, 2},  // LDMIA, four words
        TimingCase{true, 0x0000C01EU, 2},  // STMIA, four words
    };
    CheckTimings(cases);
}

TEST_CASE("ARM11 VFP11 timing classes", "[core][arm][timing]") {
    constexpr std::array cases{
        TimingCase{false, 0xEE300A00U, 1},  // VADD.F32
        TimingCase{false, 0xEE300B00U, 1},  // VADD.F64
        TimingCase{false, 0xEE200A00U, 1},  // VMUL.F32
        TimingCase{false, 0xEE200B00U, 2},  // VMUL.F64
        TimingCase{false, 0xEE800A00U, 15}, // VDIV.F32
        TimingCase{false, 0xEE800B00U, 29}, // VDIV.F64
        TimingCase{false, 0xEEB10AC0U, 15}, // VSQRT.F32
        TimingCase{false, 0xEEB10BC0U, 29}, // VSQRT.F64
        TimingCase{false, 0xED2D0B04U, 2},  // VPUSH {d0-d1}, four words
        TimingCase{false, 0xECBD0B04U, 2},  // VPOP {d0-d1}, four words
        TimingCase{true, 0x00004348U, 5},   // Thumb MULS
        TimingCase{true, 0xF0000000U, 1},   // unmatched Thumb-32 default
    };
    CheckTimings(cases);
}
