#pragma once
#include <string>
#include <vector>

namespace thorium::presentation {
/** App screens are independent of the run's battle/preparation phase. */
enum class Screen { Home, Game, Settings, Guide, Credits };
/** Presentation preferences survive starting another run in the same app session. */
struct Preferences {
    bool reducedMotion = false;
    bool damageNumbers = true;
    bool cachedAnimations = false;
    /** New expeditions excavate a rock wall; otherwise they dig tile by tile with nest clues. */
    bool excavation = true;
    /** Code of the interface language, a file name under `content/i18n`. */
    std::string language = "ko";
};
/** Own bounded in-app navigation without serializing or mutating the game run. */
class Navigation {
  public:
    /** Read the currently visible app screen. */
    Screen current() const {
        return current_;
    }
    /** Read whether an in-memory expedition can be continued from the title. */
    bool hasRun() const {
        return hasRun_;
    }
    /** Decide whether fixed game simulation may run under this screen. */
    bool gameplay() const {
        return current_ == Screen::Game;
    }
    /** Open a utility page and remember its actual caller for Back. */
    void open(Screen target) {
        if (target == current_ || target == Screen::Home || target == Screen::Game)
            return;
        if (history_.size() >= 8)
            history_.erase(history_.begin());
        history_.push_back(current_);
        current_ = target;
    }
    /** Return one level; the title is the fallback for a detached utility page. */
    void back() {
        current_ = history_.empty() ? Screen::Home : history_.back();
        if (!history_.empty())
            history_.pop_back();
    }
    /** Show a new or preserved expedition, clearing unrelated utility-page history. */
    void enterGame() {
        current_ = Screen::Game;
        hasRun_ = true;
        history_.clear();
    }
    /** Offer an expedition restored from storage through Continue while the title stays open. */
    void resume() {
        hasRun_ = true;
    }
    /** Return to title and preserve a living expedition only when requested. */
    void home(bool preserveRun = true) {
        current_ = Screen::Home;
        hasRun_ = hasRun_ && preserveRun;
        history_.clear();
    }

  private:
    Screen current_ = Screen::Home;
    bool hasRun_ = false;
    std::vector<Screen> history_;
};
} // namespace thorium::presentation
