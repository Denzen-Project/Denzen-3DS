// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>

#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

TEST_CASE("KernelSystem::ConfigureNew3dsCpu", "[core][kernel]") {
    Core::Timing timing(1, 100, 0);
    Core::System system;
    Memory::MemorySystem memory{system};
    KernelSystem kernel(memory, timing, [] {}, MemoryMode::NewProd, 1);

    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);
    REQUIRE_FALSE(kernel.GetRunning804MHz());
    REQUIRE_FALSE(kernel.GetL2CacheEnabled());

    kernel.ConfigureNew3dsCpu(0x2);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0x2);
    REQUIRE_FALSE(kernel.GetRunning804MHz());
    REQUIRE(kernel.GetL2CacheEnabled());

    kernel.ConfigureNew3dsCpu(0x1);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0x1);
    REQUIRE(kernel.GetRunning804MHz());
    REQUIRE_FALSE(kernel.GetL2CacheEnabled());

    // The central kernel transition must drive timing, not just report the flags.
    const auto timer = timing.GetTimer(0);
    // Kernel initialization schedules the shared-page clock at tick zero. Match the production
    // dispatcher by processing that initial boundary before creating the first execution slice.
    timer->Advance();
    timer->SetNextSlice(3);
    timer->AddTicks(9);
    REQUIRE(timer->GetDowncount() == 0);
    timer->Advance();

    kernel.ConfigureNew3dsCpu(0x2);
    timer->SetNextSlice(3);
    timer->AddTicks(3);
    REQUIRE(timer->GetDowncount() == 0);

    kernel.ConfigureNew3dsCpu(0xFF);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0x3);
    REQUIRE(kernel.GetRunning804MHz());
    REQUIRE(kernel.GetL2CacheEnabled());

    kernel.ConfigureNew3dsCpu(0xFC);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);
    REQUIRE_FALSE(kernel.GetRunning804MHz());
    REQUIRE_FALSE(kernel.GetL2CacheEnabled());

    const bool was_new_3ds = Settings::values.is_new_3ds.GetValue();
    SCOPE_EXIT({ Settings::values.is_new_3ds.SetValue(was_new_3ds); });
    Settings::values.is_new_3ds.SetValue(true);

    constexpr u64 high_clock_applet = 0x0004003000009302;
    constexpr u64 base_clock_applet = 0x0004003020009402;
    const New3dsHwCapabilities high_clock_capabilities{true, true, {}};
    const New3dsHwCapabilities base_clock_capabilities{};

    // Nested foreground launches must restore the mode of the process they replaced.
    kernel.UpdateCPUAndMemoryState(high_clock_applet, {}, high_clock_capabilities);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0x3);
    kernel.UpdateCPUAndMemoryState(base_clock_applet, {}, base_clock_capabilities);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);
    kernel.RestoreCPUAndMemoryState(base_clock_applet);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0x3);
    kernel.RestoreCPUAndMemoryState(high_clock_applet);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);

    // Removing a suspended non-foreground process must bypass its stale restore point.
    kernel.UpdateCPUAndMemoryState(high_clock_applet, {}, high_clock_capabilities);
    kernel.UpdateCPUAndMemoryState(base_clock_applet, {}, base_clock_capabilities);
    kernel.RestoreCPUAndMemoryState(high_clock_applet);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);
    kernel.RestoreCPUAndMemoryState(base_clock_applet);
    REQUIRE(kernel.GetNew3dsCpuConfig() == 0);
}

} // namespace Kernel
