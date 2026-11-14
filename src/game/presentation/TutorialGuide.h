#pragma once

namespace thorium::presentation {
/** Visible lesson identity; Scan/FirstDig together form the sixth of eight lessons. */
enum class Lesson {
    None,
    Briefing,
    Building,
    Unit,
    Combat,
    Fork,
    Scan,
    FirstDig,
    Crafting,
    Investment,
    Complete,
    Failed
};
/** Read-only presentation intent written by the app's tutorial director. */
struct TutorialGuide {
    Lesson lesson = Lesson::None;
    int mineCell = -1;
    /** Distinguish the isolated practice expedition from a regular run. */
    bool active() const {
        return lesson != Lesson::None;
    }
};
} // namespace thorium::presentation
