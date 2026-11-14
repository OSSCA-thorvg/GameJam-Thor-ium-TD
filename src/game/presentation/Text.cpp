#include "Text.h"
#include "game/content/ContentKeys.h"
#include <algorithm>
#include <array>
#include <set>
#include <sstream>

namespace thorium::presentation {
namespace {
/** Loaded tables: the source language and, when another is selected, that language. */
struct Tables {
    engine::StringTable source;
    engine::StringTable selected;
    std::string code;
    bool loaded = false;
    std::set<std::string> missing;
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
    tables().missing.emplace(key);
    return std::string(key);
}
std::vector<std::string> missingKeys() {
    const std::set<std::string> &missing = tables().missing;
    return {missing.begin(), missing.end()};
}
std::string tr(std::string_view key, const engine::TextArgs &args) {
    return engine::formatText(tr(key), args);
}
std::string costText(const game::ResourceCost &cost, bool compact) {
    const std::array<int, 4> amounts{{cost.iron, cost.copper, cost.crystal, cost.gold}};
    const std::array<game::Ore, 4> ores{
        {game::Ore::Iron, game::Ore::Copper, game::Ore::Crystal, game::Ore::Gold}};
    std::string result;
    for (std::size_t index = 0; index < amounts.size(); ++index)
        if (amounts[index] > 0) {
            if (!compact && !result.empty())
                result += " ";
            result += oreName(ores[index], compact) + std::to_string(amounts[index]);
        }
    return result.empty() ? tr("cost.free") : result;
}
namespace {
/** Resolve one message value: `@key` and `@key?name=value&name=value` are looked up (inner values resolve the
 *  same way), `#cost/...` becomes a price and anything else is used as written. */
std::string resolveValue(const std::string &value) {
    if (value.rfind("#cost/", 0) == 0) {
        game::ResourceCost cost;
        std::istringstream in(value.substr(6));
        char slash = 0;
        in >> cost.iron >> slash >> cost.copper >> slash >> cost.crystal >> slash >> cost.gold;
        return costText(cost);
    }
    if (value.size() < 2 || value[0] != '@')
        return value;
    const std::size_t query = value.find('?');
    if (query == std::string::npos)
        return tr(std::string_view(value).substr(1));
    engine::TextArgs args;
    std::size_t at = query + 1;
    while (at <= value.size()) {
        const std::size_t next = std::min(value.find('&', at), value.size());
        const std::string pair = value.substr(at, next - at);
        const std::size_t equals = pair.find('=');
        if (equals != std::string::npos)
            args.emplace_back(pair.substr(0, equals), resolveValue(pair.substr(equals + 1)));
        at = next + 1;
    }
    return tr(std::string_view(value).substr(1, query - 1), args);
}
} // namespace
std::string tr(const game::Message &message) {
    if (message.empty())
        return {};
    engine::TextArgs args;
    args.reserve(message.args.size());
    for (const auto &[name, value] : message.args)
        args.emplace_back(name, resolveValue(value));
    return tr(message.key, args);
}
std::string cardName(game::CardKind kind) {
    return tr("card." + std::string(game::cardKey(kind)) + ".name");
}
std::string enemyName(const game::EnemyDefinition &enemy) {
    return tr("enemy." + (enemy.key.empty() ? enemy.animation : enemy.key) + ".name");
}
std::string eliteName(game::EliteAffix affix) {
    return tr("elite." + std::string(game::eliteKey(affix)) + ".name");
}
std::string oreName(game::Ore ore, bool compact) {
    return tr("ore." + std::string(game::oreKey(ore)) + (compact ? ".short" : ".name"));
}
std::string mineLayerName(const game::MineLayerDefinition &layer) {
    return tr("minelayer." + layer.name + ".name");
}
std::string relicName(game::Relic relic) {
    return tr("relic." + std::string(game::relicKey(relic)) + ".name");
}
std::string stageName(int id) {
    return tr("stage." + std::to_string(id) + ".name");
}
std::string stageGimmick(int id) {
    return tr("stage." + std::to_string(id) + ".gimmick");
}
} // namespace thorium::presentation
