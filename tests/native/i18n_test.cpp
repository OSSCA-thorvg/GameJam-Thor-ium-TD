#include "engine/text/Strings.h"
#include "game/presentation/Text.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace thorium::engine;
using namespace thorium::presentation;
int checks = 0;
/** Keep release-build assertions active and report a concrete broken text contract. */
void require(bool condition, const std::string &message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Parse flat string objects, decode escapes and reject anything else with a position. */
void parsing() {
    StringTable table;
    std::string error;
    require(table.parse("{}", error) && table.size() == 0, "empty object parses");
    require(table.parse(" {\n \"a\" : \"one\", \"b\":\"two\" }\n", error) && table.size() == 2,
            "whitespace around tokens is accepted");
    require(table.find("a") && *table.find("a") == "one" && !table.find("c"),
            "keys resolve and misses are null");
    require(table.parse(R"({"e":"q\" b\\ s\/ n\n t\t"})", error) && *table.find("e") == "q\" b\\ s/ n\n t\t",
            "simple escapes decode");
    require(table.parse(R"({"u":"\u00e9\uD55C\uD83D\uDE00"})", error) &&
                *table.find("u") == "\xC3\xA9\xED\x95\x9C\xF0\x9F\x98\x80",
            "unicode escapes and surrogate pairs decode to UTF-8");
    require(table.parse("{\"k\":\"한글\"}", error) && *table.find("k") == "한글", "raw UTF-8 passes through");
    for (const char *bad : {"", "[]", "{\"a\":1}", "{\"a\":\"x\",}", "{\"a\":\"x\"} trailing", "{\"a\":\"x\"",
                            "{\"a\":\"x\",\"a\":\"y\"}", "{\"a\":\"\\q\"}", "{\"a\":\"\\uD83D\"}",
                            "{\"a\":\"line\nbreak\"}", "{\"a\" \"x\"}"}) {
        require(!table.parse(bad, error) && table.size() == 0 && error.find("at byte") != std::string::npos,
                std::string("malformed input rejects with a position: ") + bad);
    }
    require(!table.load("/nonexistent/strings.json", error) && error.find("cannot open") == 0,
            "missing file reports its path");
}
/** Fill named placeholders and leave unknown or unclosed braces as written. */
void formatting() {
    require(formatText("{a} and {b}", {{"a", "1"}, {"b", "2"}}) == "1 and 2", "placeholders fill");
    require(formatText("{a}{a}", {{"a", "x"}}) == "xx", "repeated placeholders fill each time");
    require(formatText("{missing} {", {{"a", "x"}}) == "{missing} {", "unknown and unclosed braces stay");
    require(formatText("{a}", {{"a", "{b}"}, {"b", "no"}}) == "{b}", "values are not expanded again");
    const auto names = placeholders("{b} {a} {b}");
    require(names.size() == 2 && names[0] == "a" && names[1] == "b",
            "placeholder names are sorted and unique");
}
/** Every language carries the source language's keys with the same placeholders, and lookups fall back. */
void tables(const std::string &directory) {
    StringTable source;
    std::string error;
    require(source.load(directory + "/" + languages().front().code + ".json", error), error);
    require(source.size() > 0, "source table has strings");
    for (const LanguageInfo &info : languages()) {
        StringTable table;
        require(table.load(directory + "/" + info.code + ".json", error), error);
        for (const std::string &key : source.keys()) {
            const std::string *text = table.find(key);
            require(text != nullptr, std::string(info.code) + " has " + key);
            require(!text->empty(), std::string(info.code) + " " + key + " is not empty");
            require(placeholders(*text) == placeholders(*source.find(key)),
                    std::string(info.code) + " " + key + " uses the source placeholders");
        }
        for (const std::string &key : table.keys())
            require(source.find(key) != nullptr,
                    std::string(info.code) + " key " + key + " exists in the source");
    }
    require(setLanguage("en", error, directory) && language() == "en", "English selects");
    require(tr("menu.settings") == "Settings", "selected language resolves");
    require(tr("no.such.key") == "no.such.key", "missing keys show the key");
    require(tr("home.continue", {{"id", "3"}}) == "Continue Zone 3", "formatted lookup fills placeholders");
    require(nextLanguage() == "ko", "language toggle wraps around");
    require(!setLanguage("xx", error, directory) && language() == "ko" && tr("menu.settings") == "설정",
            "unknown language keeps the source language");
    require(setLanguage("ko", error, directory) && stageName(1) == *source.find("stage.1.name"),
            "stage names come from the table");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: thorium-i18n-tests <content/i18n directory>\n";
        return 2;
    }
    parsing();
    formatting();
    tables(argv[1]);
    std::cout << "i18n checks passed: " << checks << '\n';
    return 0;
}
