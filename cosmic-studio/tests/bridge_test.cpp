#include "hush_cosmic/engine_bridge.hpp"
#include "hush_cosmic/observatory.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace hush::cosmic;

    SpscQueue<EngineCommand, 4> queue;
    EngineCommand command;
    assert(!queue.try_pop(command));
    assert(queue.try_push({CommandType::SetMasterGain, 0, 0.75F, 1}));
    assert(queue.try_push({CommandType::SetTrackGain, 2, 0.5F, 2}));
    assert(queue.try_push({CommandType::SetTrackPan, 2, -0.25F, 3}));
    assert(!queue.try_push({CommandType::TransportPlay, 0, 1.0F, 4}));
    assert(queue.try_pop(command));
    assert(command.type == CommandType::SetMasterGain);
    assert(command.sequence == 1);
    assert(queue.try_push({CommandType::TransportPlay, 0, 1.0F, 4}));

    TelemetryChannel telemetry;
    TelemetrySnapshot snapshot;
    assert(!telemetry.try_read(snapshot));
    telemetry.publish({true, 0.8F, 0.4F, 0.2F, 12, 48000, 256, 4096});
    assert(telemetry.try_read(snapshot));
    assert(snapshot.valid);
    assert(std::abs(snapshot.output_peak - 0.8F) < 0.0001F);
    assert(snapshot.active_tracks == 12);
    assert(snapshot.sample_rate == 48000);
    assert(snapshot.rendered_frames == 4096);

    const auto point = orbital_point(0.0F, 0.5F, 20.0F);
    assert(std::abs(point.x - 20.0F) < 0.0001F);
    assert(std::abs(point.y) < 0.0001F);
    assert(std::abs(point.radius - 6.0F) < 0.0001F);

    return 0;
}
