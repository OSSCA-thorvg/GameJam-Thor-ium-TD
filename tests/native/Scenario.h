#pragma once
#include "game/rules/Run.h"
#include <cstdlib>
#include <iostream>
#include <utility>

namespace thorium::game {
/** Test-only privileged setup; production callers receive only const RunState. */
struct RunTestAccess {
    /** Set up a deterministic scenario without adding a mutable runtime API. */
    /** Enter the real preparation transition when a fixture needs live card offers. */
    static void preparation(Run &run, int wave) {
        run.state_.wave = wave;
        run.finishWave();
    }
    static RunState &state(Run &run) {
        return run.state_;
    }
};
/** Access fixture state in headless tests; this helper is never linked into the app. */
inline RunState &scenario(Run &run) {
    return RunTestAccess::state(run);
}
/** Construct valid custom fixtures through the same fallible boundary as production startup. */
inline Run makeRun(Catalog catalog) {
    RunCreation candidate = Run::create(std::move(catalog));
    if (!candidate.run) {
        std::cerr << "Invalid fixture catalog: " << candidate.validation.summary() << '\n';
        std::exit(1);
    }
    return std::move(*candidate.run);
}
/** Seed offers and preparation counters through the production transition, not independent booleans. */
inline void enterPreparation(Run &run, int wave = 1) {
    RunTestAccess::preparation(run, wave);
}
} // namespace thorium::game
