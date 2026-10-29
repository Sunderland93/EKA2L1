/*
 * Regression test for 12l1r decoder pattern/handler mismatches.
 * This test can also be built without the emulator's external dependencies:
 * c++ -std=c++17 -O2 -I src/emu/cpu/include -I src/emu/common/include \
 *     src/emu/cpu/src/12l1r/tests/decoder_patterns.cpp -o decoder_patterns
 */

#include <cpu/12l1r/decoder/decoder_detail.h>
#include <cpu/12l1r/decoder/matcher.h>

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {
    struct pattern_entry {
        std::string_view handler;
        std::string_view bits;
    };

    constexpr pattern_entry arm_patterns[] = {
#define INST(fn, name, bits) { #fn, bits },
#include <cpu/12l1r/encoding/arm.inc>
#undef INST
    };
    constexpr pattern_entry thumb_patterns[] = {
#define INST(fn, name, bits) { #fn, bits }
#include <cpu/12l1r/encoding/thumb16.inc>
#undef INST
    };

    template <std::size_t N>
    constexpr std::string_view find_pattern(const pattern_entry (&patterns)[N], std::string_view name) {
        for (const auto &pattern : patterns) {
            if (pattern.handler == name) {
                return pattern.bits;
            }
        }
        return {};
    }

    struct visitor {
        using instruction_return_type = bool;
        unsigned calls = 0;
        bool breakpoint() { ++calls; return true; }
        bool two_fields(unsigned, unsigned) { return true; }
    };

    using zero_args = decltype(&visitor::breakpoint);
    using two_args = decltype(&visitor::two_fields);
    using eka2l1::arm::r12l1::decoder::valid_bitstring;
    constexpr auto arm_bkpt = find_pattern(arm_patterns, "arm_BKPT");
    constexpr auto thumb_bkpt = find_pattern(thumb_patterns, "thumb16_BKPT");

    static_assert(valid_bitstring<zero_args, std::uint32_t>(arm_bkpt));
    static_assert(valid_bitstring<zero_args, std::uint16_t>(thumb_bkpt));
    static_assert(!valid_bitstring<zero_args, std::uint32_t>("cccc00010010vvvvvvvvvvvv0111vvvv"));
    static_assert(!valid_bitstring<zero_args, std::uint16_t>("10111110xxxxxxxx"));
    static_assert(!valid_bitstring<zero_args, std::uint16_t>("10111110-------"));
    static_assert(valid_bitstring<two_args, std::uint16_t>("aaaa0000aaaa----"));
    static_assert(!valid_bitstring<two_args, std::uint16_t>("aaaaaaaa0000----"));
}

int main() {
    namespace decoder = eka2l1::arm::r12l1::decoder;
    using arm_matcher = decoder::matcher<visitor, std::uint32_t>;
    using thumb_matcher = decoder::matcher<visitor, std::uint16_t>;

    const auto arm = decoder::detail<arm_matcher>::get_matcher(&visitor::breakpoint, "BKPT", arm_bkpt.data());
    const auto thumb = decoder::detail<thumb_matcher>::get_matcher(&visitor::breakpoint, "BKPT", thumb_bkpt.data());

    // Verify that ignoring the arguments does not change the original match mask.
    if (arm.get_mask() != 0x0ff000f0 || arm.get_expected() != 0x01200070 ||
        thumb.get_mask() != 0xff00 || thumb.get_expected() != 0xbe00) {
        std::fputs("BKPT masks changed\n", stderr);
        return 1;
    }

    for (std::uint32_t condition = 0; condition < 16; ++condition) {
        for (std::uint32_t immediate = 0; immediate < 65536; ++immediate) {
            const std::uint32_t opcode = (condition << 28) | 0x01200070 |
                ((immediate & 0xfff0) << 4) | (immediate & 0xf);
            if (!arm.matches(opcode) || arm.matches(opcode ^ 0x10)) {
                std::fputs("ARM BKPT match failed\n", stderr);
                return 1;
            }
        }
    }
    for (std::uint32_t opcode = 0; opcode < 65536; ++opcode) {
        if (thumb.matches(static_cast<std::uint16_t>(opcode)) != ((opcode & 0xff00) == 0xbe00)) {
            std::fputs("Thumb BKPT match failed\n", stderr);
            return 1;
        }
    }

    visitor v;
    if (!arm.call(v, 0xe1200070) || !thumb.call(v, 0xbe00) || v.calls != 2) {
        std::fputs("BKPT handler invocation failed\n", stderr);
        return 1;
    }
    std::puts("12l1r decoder pattern tests passed");
}
