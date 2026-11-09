#pragma once
#include "Scenario.h"
#include "app/Application.h"

namespace thorium {
/** Compile-time fixture access only; production app callers cannot replace or advance its run. */
struct ApplicationTestAccess {
    /** Populate an offline benchmark using the existing privileged domain fixture. */
    /** Inspect rendered presentation intent without opening a production mutation API. */
    static const presentation::View &view(const Application &app) {
        return app.view;
    }
    static game::Run &run(Application &app) {
        return app.run;
    }
};
} // namespace thorium
