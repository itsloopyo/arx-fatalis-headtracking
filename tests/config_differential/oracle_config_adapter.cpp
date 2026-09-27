// Compiled into the config oracle library only, with `cameraunlock` and `ArxHeadTracking` renamed,
// so "config.h" here is the dev build's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

#include <stdexcept>

namespace arx_oracle_view {

OracleConfig RunOracle(const std::string& path) {
    ArxHeadTracking::Config c;
    if (!c.LoadOrCreate(path.c_str())) throw std::runtime_error("the oracle refused the path " + path);
    OracleConfig o{};
    o.udp_port = c.udp_port;
    o.enabled_on_startup = c.enabled_on_startup;
    o.sens_yaw = c.sens_yaw;
    o.sens_pitch = c.sens_pitch;
    o.sens_roll = c.sens_roll;
    o.invert_yaw = c.invert_yaw;
    o.invert_pitch = c.invert_pitch;
    o.invert_roll = c.invert_roll;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.position_enabled = c.position_enabled;
    o.pos_sens_x = c.pos_sens_x;
    o.pos_sens_y = c.pos_sens_y;
    o.pos_sens_z = c.pos_sens_z;
    o.pos_limit_x = c.pos_limit_x;
    o.pos_limit_y = c.pos_limit_y;
    o.pos_limit_z = c.pos_limit_z;
    o.pos_limit_z_back = c.pos_limit_z_back;
    o.collision_enabled = c.collision_enabled;
    o.collision_radius = c.collision_radius;
    o.collision_release_smoothing = c.collision_release_smoothing;
    o.field_of_view = c.field_of_view;
    o.move_crosshair = c.move_crosshair;
    o.vk_toggle = c.vk_toggle;
    o.vk_cycle_mode = c.vk_cycle_mode;
    o.diagnostics = c.diagnostics;
    return o;
}

}  // namespace arx_oracle_view
