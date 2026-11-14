#pragma once
#include "engine/text/Strings.h"
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
/** A campaign stage's name, read from `stage.<id>.name`. */
std::string stageName(int id);
/** A campaign stage's one-line gimmick summary, read from `stage.<id>.gimmick`. */
std::string stageGimmick(int id);
} // namespace thorium::presentation
