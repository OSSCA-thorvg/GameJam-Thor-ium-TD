#pragma once
#include "engine/text/Strings.h"
#include "game/content/Catalog.h"
#include "game/model/Message.h"
#include <array>
#include <string>
#include <string_view>

namespace thorium::presentation {
/** A supported language: its file code under `content/i18n` and the name shown in its own language. */
struct LanguageInfo {
    const char *code;
    const char *name;
};
/** Supported languages; the first is the source language every other one falls back to. */
const std::array<LanguageInfo, 2> &languages();
/** Select a language by code, loading `<directory>/<code>.json` over the source language's table. An unknown
 *  code or unreadable file keeps the source language and reports why. */
bool setLanguage(std::string_view code, std::string &error,
                 const std::string &directory = std::string(THORIUM_CONTENT_DIR) + "/i18n");
/** Read the code of the selected language. */
const std::string &language();
/** Read the code of the language after the selected one, wrapping around, for a settings toggle. */
std::string nextLanguage();
/** Text for a key in the selected language, else the source language, else the key itself so a missing
 *  string shows on screen. The source table loads on first use. */
std::string tr(std::string_view key);
/** The same, with `{name}` placeholders filled. */
std::string tr(std::string_view key, const engine::TextArgs &args);
/** Keys looked up since start that no table defines, sorted, for diagnostics and tests. */
std::vector<std::string> missingKeys();
/** A rule message: `@key` values are looked up and `#cost/...` values become a price before the
 *  placeholders are filled. An empty message is an empty string. */
std::string tr(const game::Message &message);
/** A card kind's name, `card.<id>.name`. */
std::string cardName(game::CardKind kind);
/** An enemy definition's name, `enemy.<key>.name`. */
std::string enemyName(const game::EnemyDefinition &enemy);
/** An elite modifier's name, `elite.<id>.name`. */
std::string eliteName(game::EliteAffix affix);
/** An ore's name, `ore.<id>.name`, or its short form `ore.<id>.short` for tight labels. */
std::string oreName(game::Ore ore, bool compact = false);
/** A mine layer's name, `minelayer.<id>.name`. */
std::string mineLayerName(const game::MineLayerDefinition &layer);
/** A relic's name, `relic.<id>.name`. */
std::string relicName(game::Relic relic);
/** A resource price as ore names with amounts, or `cost.free` when nothing is charged; compact uses short ore
 *  names without spaces. */
std::string costText(const game::ResourceCost &cost, bool compact = false);
/** A campaign stage's name, read from `stage.<id>.name`. */
std::string stageName(int id);
/** A campaign stage's one-line gimmick summary, read from `stage.<id>.gimmick`. */
std::string stageGimmick(int id);
} // namespace thorium::presentation
