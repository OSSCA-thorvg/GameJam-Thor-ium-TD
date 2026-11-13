#include "BenchScene.h"
#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace thorium {
namespace {
/** Read one `key=value` pair of an `&`-separated query, without URL decoding. */
std::optional<std::string> queryValue(const std::string &query, const std::string &key) {
    std::size_t start = query.empty() || query.front() != '?' ? 0 : 1;
    while (start < query.size()) {
        const std::size_t end = std::min(query.find('&', start), query.size());
        const std::string pair = query.substr(start, end - start);
        if (pair.compare(0, key.size() + 1, key + "=") == 0)
            return pair.substr(key.size() + 1);
        start = end + 1;
    }
    return std::nullopt;
}
} // namespace
std::optional<BenchRequest> parseBenchQuery(const std::string &query) {
    const std::optional<std::string> scene = queryValue(query, "bench");
    if (!scene)
        return std::nullopt;
    BenchRequest request;
    int stage = 0, wave = 0;
    char tail = 0;
    if (std::sscanf(scene->c_str(), "s%dw%d%c", &stage, &wave, &tail) != 2 || stage < 1 ||
        stage > game::TotalStages || wave < 1 ||
        wave > static_cast<int>(game::stageCatalog(stage).waves.size()))
        return std::nullopt;
    request.stage = stage;
    request.wave = wave;
    if (const std::optional<std::string> crowd = queryValue(query, "crowd")) {
        const float value = std::strtof(crowd->c_str(), nullptr);
        if (!std::isfinite(value) || value < .25F || value > 4)
            return std::nullopt;
        request.crowd = value;
    }
    if (const std::optional<std::string> backend = queryValue(query, "anim")) {
        if (*backend != "vector" && *backend != "cache")
            return std::nullopt;
        request.cachedAnimations = *backend == "cache";
    }
    return request;
}
BenchScene prepareBenchScene(const BenchRequest &request) {
    game::Catalog content = game::stageCatalog(request.stage);
    if (request.crowd != 1) {
        for (std::size_t wave = static_cast<std::size_t>(request.wave - 1); wave < content.waves.size();
             ++wave)
            for (game::SpawnGroup &group : content.waves[wave])
                if (group.count > 0)
                    group.count = std::max(1, static_cast<int>(std::lround(group.count * request.crowd)));
        if (request.crowd > 1)
            content.balance.baseHp = std::max(content.balance.baseHp, 999);
    }
    game::RunCreation created = game::Run::create(std::move(content));
    if (!created.run)
        return {nullptr, created.validation.summary()};
    game::Run &run = *created.run;
    // The same 0.1 s cadence as the campaign playthrough test: a card decision every 0.5 s of battle.
    for (int step = 0; step < 40000; ++step) {
        const game::RunState &state = run.state();
        if (state.phase == game::Phase::Won || state.phase == game::Phase::Lost)
            return {nullptr, "Autoplayer ended the stage before the bench wave"};
        if (state.phase == game::Phase::Ready || state.phase == game::Phase::Prep) {
            if (state.phase == game::Phase::Prep)
                game::autoplay::prepare(run);
            if (state.wave + 1 == request.wave)
                return {std::move(created.run), {}};
            if (!run.startWave().ok)
                return {nullptr, "Autoplayer could not start a wave"};
        }
        if (step % 5 == 0)
            game::autoplay::playCard(run);
        run.advance(.1F);
    }
    return {nullptr, "Autoplayer did not reach the bench wave"};
}
} // namespace thorium
