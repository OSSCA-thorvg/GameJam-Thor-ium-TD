#pragma once
#include "engine/audio/Audio.h"
#include "game/presentation/Shell.h"
#include <string>

namespace thorium {
/** Stored progression and settings; absent fields share the live application's defaults. */
struct SaveData {
    bool tutorialCompleted = false;
    int highestUnlockedStage = 1;
    bool reducedMotion = presentation::Preferences{}.reducedMotion;
    bool damageNumbers = presentation::Preferences{}.damageNumbers;
    bool cachedAnimations = presentation::Preferences{}.cachedAnimations;
    bool excavation = presentation::Preferences{}.excavation;
    float volume = engine::AudioSettings{}.volume;
    bool muted = engine::AudioSettings{}.muted;
};

/** Restore and save settings using platform storage with defaults for unavailable data. */
class Persistence {
  public:
    /** Load available saved fields; retain shared defaults for missing data or inaccessible storage. */
    static SaveData load();
    /** Persist the supplied settings and progress through the platform storage adapter. */
    static void save(const SaveData &data);
};
} // namespace thorium
