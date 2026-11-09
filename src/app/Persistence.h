#pragma once
#include "engine/audio/Audio.h"
#include "game/presentation/Shell.h"
#include <string>

namespace thorium {
struct SaveData {
    bool tutorialCompleted = false;
    int highestUnlockedStage = 1;
    bool reducedMotion = false;
    bool damageNumbers = true;
    bool cachedAnimations = false;
    float volume = 1.0F;
    bool muted = false;
};

class Persistence {
  public:
    static SaveData load();
    static void save(const SaveData &data);
};
} // namespace thorium
