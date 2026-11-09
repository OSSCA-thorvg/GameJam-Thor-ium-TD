#pragma once
#include "Catalog.h"

namespace thorium::game {
/** Supported authored bounds for the fixed-grid prototype, independently of UI controls. */
struct CatalogLimits {
    static constexpr int MaxWaves = 100;
    static constexpr int MaxWaveEnemies = 10000;
    static constexpr int MaxAmount = 1000000;
    static constexpr int MaxDamage = 10000;
    static constexpr int MaxSlots = 180;
    static constexpr int MaxNestEnemies = 100;
};
/** Point to an authored field rather than losing its location in a generic load failure. */
struct CatalogIssue {
    std::string path;
    std::string message;
};
/** Collect independent authoring errors without constructing a partially valid run. */
struct CatalogValidation {
    std::vector<CatalogIssue> issues;
    /** Decide whether the entire candidate can be committed as one immutable catalog. */
    bool ok() const {
        return issues.empty();
    }
    /** Format actionable diagnostics for native startup and future import screens. */
    std::string summary() const;
};
/** Validate all numeric, geometry and definition-reference invariants before Run creation. */
CatalogValidation validateCatalog(const Catalog &catalog);
} // namespace thorium::game
