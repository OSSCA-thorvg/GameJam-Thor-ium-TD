#include "Persistence.h"
#include "game/content/Catalog.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace thorium {
namespace {
std::string serialize(const SaveData &data) {
    std::ostringstream ss;
    ss << "{\"tutorialCompleted\":" << (data.tutorialCompleted ? "true" : "false")
       << ",\"highestUnlockedStage\":" << data.highestUnlockedStage
       << ",\"reducedMotion\":" << (data.reducedMotion ? "true" : "false")
       << ",\"damageNumbers\":" << (data.damageNumbers ? "true" : "false")
       << ",\"cachedAnimations\":" << (data.cachedAnimations ? "true" : "false")
       << ",\"excavation\":" << (data.excavation ? "true" : "false") << ",\"volume\":" << data.volume
       << ",\"music\":" << data.music << ",\"muted\":" << (data.muted ? "true" : "false") << "}";
    return ss.str();
}

SaveData deserialize(const std::string &json) {
    SaveData data;
    if (json.empty())
        return data;

    auto findBool = [&](const std::string &key, bool &out) {
        auto pos = json.find("\"" + key + "\":");
        if (pos != std::string::npos) {
            auto valPos = pos + key.length() + 3;
            if (json.substr(valPos, 4) == "true")
                out = true;
            else if (json.substr(valPos, 5) == "false")
                out = false;
        }
    };

    auto findInt = [&](const std::string &key, int &out) {
        auto pos = json.find("\"" + key + "\":");
        if (pos != std::string::npos) {
            auto valPos = pos + key.length() + 3;
            char *end = nullptr;
            long val = std::strtol(json.c_str() + valPos, &end, 10);
            if (end != json.c_str() + valPos)
                out = static_cast<int>(val);
        }
    };

    auto findFloat = [&](const std::string &key, float &out) {
        auto pos = json.find("\"" + key + "\":");
        if (pos != std::string::npos) {
            auto valPos = pos + key.length() + 3;
            char *end = nullptr;
            float val = std::strtof(json.c_str() + valPos, &end);
            if (end != json.c_str() + valPos)
                out = val;
        }
    };

    findBool("tutorialCompleted", data.tutorialCompleted);
    findInt("highestUnlockedStage", data.highestUnlockedStage);
    findBool("reducedMotion", data.reducedMotion);
    findBool("damageNumbers", data.damageNumbers);
    findBool("cachedAnimations", data.cachedAnimations);
    findBool("excavation", data.excavation);
    findFloat("volume", data.volume);
    findFloat("music", data.music);
    findBool("muted", data.muted);

    data.highestUnlockedStage = std::clamp(data.highestUnlockedStage, 1, game::TotalStages);
    data.volume = std::clamp(data.volume, 0.0F, 1.0F);
    data.music = std::clamp(data.music, 0.0F, 1.0F);
    return data;
}

#ifdef __EMSCRIPTEN__
/** Read one localStorage entry into an owned string; empty when missing or storage is unavailable. */
std::string readStorage(const char *key) {
    char *raw = reinterpret_cast<char *>(EM_ASM_INT(
        {
            try {
                var val = localStorage.getItem(UTF8ToString($0));
                if (!val)
                    return 0;
                var lengthBytes = lengthBytesUTF8(val) + 1;
                var stringOnWasmHeap = _malloc(lengthBytes);
                stringToUTF8(val, stringOnWasmHeap, lengthBytes);
                return stringOnWasmHeap;
            } catch (e) {
                return 0;
            }
        },
        key));
    if (!raw)
        return {};
    std::string value(raw);
    std::free(raw);
    return value;
}
/** Write or, for an empty value, remove one localStorage entry; storage failures are ignored. */
void writeStorage(const char *key, const std::string &value) {
    EM_ASM(
        {
            try {
                var key = UTF8ToString($0);
                var str = UTF8ToString($1);
                if (str.length)
                    localStorage.setItem(key, str);
                else
                    localStorage.removeItem(key);
            } catch (e) {
            }
        },
        key, value.c_str());
}
#else
/** Path of a save file in the home directory, or in the working directory without one. */
std::string homeFile(const char *name) {
    const char *home = std::getenv("HOME");
    return home && home[0] ? std::string(home) + "/" + name : std::string(name);
}
const char *saveFilePath() {
    static std::string path;
    if (path.empty()) {
        const char *home = std::getenv("HOME");
        if (home && home[0])
            path = std::string(home) + "/.thorium_td_save.json";
        else
            path = ".thorium_td_save.json";
    }
    return path.c_str();
}
#endif
} // namespace

std::string Persistence::loadRun() {
#ifdef __EMSCRIPTEN__
    return readStorage("thorium_td_run");
#else
    FILE *f = std::fopen(homeFile(".thorium_td_run.txt").c_str(), "r");
    if (!f)
        return {};
    std::string content;
    char buffer[4096];
    while (std::size_t bytes = std::fread(buffer, 1, sizeof(buffer), f))
        content.append(buffer, bytes);
    std::fclose(f);
    return content;
#endif
}
void Persistence::saveRun(const std::string &text) {
#ifdef __EMSCRIPTEN__
    writeStorage("thorium_td_run", text);
#else
    FILE *f = std::fopen(homeFile(".thorium_td_run.txt").c_str(), "w");
    if (!f)
        return;
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
#endif
}
void Persistence::clearRun() {
#ifdef __EMSCRIPTEN__
    writeStorage("thorium_td_run", {});
#else
    std::remove(homeFile(".thorium_td_run.txt").c_str());
#endif
}

SaveData Persistence::load() {
#ifdef __EMSCRIPTEN__
    char *raw = reinterpret_cast<char *>(EM_ASM_INT({
        try {
            var val = localStorage.getItem("thorium_td_save");
            if (!val)
                return 0;
            var lengthBytes = lengthBytesUTF8(val) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(val, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        } catch (e) {
            return 0;
        }
    }));
    if (!raw)
        return SaveData{};
    std::string json(raw);
    std::free(raw);
    return deserialize(json);
#else
    FILE *f = std::fopen(saveFilePath(), "r");
    if (!f)
        return SaveData{};
    std::string content;
    char buffer[512];
    while (std::size_t bytes = std::fread(buffer, 1, sizeof(buffer), f))
        content.append(buffer, bytes);
    std::fclose(f);
    return deserialize(content);
#endif
}

void Persistence::save(const SaveData &data) {
    const std::string json = serialize(data);
#ifdef __EMSCRIPTEN__
    EM_ASM(
        {
            try {
                var str = UTF8ToString($0);
                localStorage.setItem("thorium_td_save", str);
            } catch (e) {
            }
        },
        json.c_str());
#else
    FILE *f = std::fopen(saveFilePath(), "w");
    if (!f)
        return;
    std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);
#endif
}
} // namespace thorium
