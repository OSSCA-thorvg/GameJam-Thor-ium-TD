#pragma once
#include "game/rules/Run.h"
#include <memory>
#include <optional>
#include <string>

namespace thorium {
/** Measurement-only request for a late campaign battle reached by the reference autoplayer. */
struct BenchRequest {
    int stage = 5;
    /** One-based wave that starts when the scene opens. */
    int wave = 16;
    /** Multiplier of every spawn group count from the bench wave on; 1 keeps the authored schedule. */
    float crowd = 1;
    /** Animation backend for this session only; empty keeps the default preference. */
    std::optional<bool> cachedAnimations;
};
/** A run in preparation right before the requested wave, or the reason it could not be reached. */
struct BenchScene {
    std::unique_ptr<game::Run> run;
    std::string error;
};
/** Read `bench=s<stage>w<wave>`, `crowd=<x>` and `anim=vector|cache` from a URL query; ignore other keys. */
std::optional<BenchRequest> parseBenchQuery(const std::string &query);
/** Play the authored stage with the strong autoplayer through public Run commands up to the requested wave.
 * A crowd above 1 also raises base health so extra leaks do not end the measured battle. */
BenchScene prepareBenchScene(const BenchRequest &request);
} // namespace thorium
