#include "Text.h"
#include <algorithm>

namespace thorium::presentation {
namespace {
/** Loaded tables: the source language and, when another is selected, that language. */
struct Tables {
    engine::StringTable source;
    engine::StringTable selected;
    std::string code;
    bool loaded = false;
};
Tables &tables() {
    static Tables state;
    return state;
}
/** Load the source table once, from the default content directory, when nothing selected a language yet. */
void ensureSource() {
    Tables &state = tables();
    if (state.loaded)
        return;
    state.loaded = true;
    state.code = languages().front().code;
    std::string error;
    state.source.load(std::string(THORIUM_CONTENT_DIR) + "/i18n/" + state.code + ".json", error);
}
} // namespace

const std::array<LanguageInfo, 2> &languages() {
    static const std::array<LanguageInfo, 2> list{{{"ko", "한국어"}, {"en", "English"}}};
    return list;
}
bool setLanguage(std::string_view code, std::string &error, const std::string &directory) {
    Tables &state = tables();
    state.loaded = true;
    error.clear();
    const std::string source = languages().front().code;
    if (!state.source.load(directory + "/" + source + ".json", error))
        return false;
    state.selected = {};
    state.code = source;
    const bool known = std::any_of(languages().begin(), languages().end(),
                                   [code](const LanguageInfo &info) { return code == info.code; });
    if (!known) {
        error = "unknown language " + std::string(code);
        return false;
    }
    if (code == source)
        return true;
    if (!state.selected.load(directory + "/" + std::string(code) + ".json", error))
        return false;
    state.code = std::string(code);
    return true;
}
const std::string &language() {
    ensureSource();
    return tables().code;
}
std::string nextLanguage() {
    const std::string &current = language();
    const auto &list = languages();
    for (std::size_t index = 0; index < list.size(); ++index)
        if (current == list[index].code)
            return list[(index + 1) % list.size()].code;
    return list.front().code;
}
std::string tr(std::string_view key) {
    ensureSource();
    const Tables &state = tables();
    if (const std::string *text = state.selected.find(key))
        return *text;
    if (const std::string *text = state.source.find(key))
        return *text;
    return std::string(key);
}
std::string tr(std::string_view key, const engine::TextArgs &args) {
    return engine::formatText(tr(key), args);
}
std::string stageName(int id) {
    return tr("stage." + std::to_string(id) + ".name");
}
std::string stageGimmick(int id) {
    return tr("stage." + std::to_string(id) + ".gimmick");
}
} // namespace thorium::presentation
