// Compiled into the hotkey oracle library only, with `cameraunlock` and `ArxHeadTracking` renamed,
// so "hotkeys.h" here is the dev build's Hotkeys and the poller under it is oracle_fake's.
#include "hotkeys.h"
#include "oracle_adapter.h"

#include <stdexcept>

namespace arx_oracle_view {

FireTable OracleFires(int vk_toggle, int vk_cycle_mode) {
    namespace input = cameraunlock::input;
    ArxHeadTracking::Config cfg;
    cfg.vk_toggle = vk_toggle;
    cfg.vk_cycle_mode = vk_cycle_mode;
    std::array<int, 2> fired{};
    input::FakeRegistrations().clear();
    ArxHeadTracking::Hotkeys hotkeys;
    if (!hotkeys.Start(cfg, [&fired] { ++fired[0]; }, [&fired] { ++fired[1]; })) {
        throw std::runtime_error("the oracle's Hotkeys::Start failed");
    }
    const std::vector<input::FakeRegistration> registered = input::FakeRegistrations();
    hotkeys.Stop();

    // The dev build's poller: a callback runs when its key goes down.
    FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            input::FakeHeld() = held;
            for (const input::FakeRegistration& r : registered) {
                if (r.vk == vk && r.callback) r.callback();
            }
            table.push_back(fired);
        }
    }
    input::FakeHeld() = 0;
    return table;
}

}  // namespace arx_oracle_view
