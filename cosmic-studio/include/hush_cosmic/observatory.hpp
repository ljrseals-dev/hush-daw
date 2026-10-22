#pragma once

#include <algorithm>
#include <cmath>

namespace hush::cosmic {

struct OrbitalPoint {
    float x{0.0F};
    float y{0.0F};
    float radius{0.0F};
};

// Presentation-only mapping: call from the UI/render thread, not the audio
// callback. phase_radians is supplied by the caller's animation clock.
[[nodiscard]] inline OrbitalPoint orbital_point(float phase_radians,
                                                 float level,
                                                 float orbit_radius) noexcept {
    constexpr float tau = 6.2831853071795864769F;
    const float safe_phase = std::isfinite(phase_radians)
        ? std::fmod(phase_radians, tau) : 0.0F;
    const float safe_level = std::isfinite(level)
        ? std::clamp(level, 0.0F, 1.0F) : 0.0F;
    const float safe_orbit = std::isfinite(orbit_radius)
        ? std::max(0.0F, orbit_radius) : 0.0F;

    return {
        std::cos(safe_phase) * safe_orbit,
        std::sin(safe_phase) * safe_orbit,
        2.0F + 8.0F * safe_level
    };
}

} // namespace hush::cosmic
