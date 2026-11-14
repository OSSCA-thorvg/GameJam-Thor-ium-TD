#pragma once
#include <string>
#include <utility>
#include <vector>

namespace thorium::game {
/** Named values for a message's `{name}` placeholders. A value written as `@key` is itself a string-table key,
 *  and `#cost/iron/copper/crystal/gold` is a resource price; the presentation layer resolves both. */
using MessageArgs = std::vector<std::pair<std::string, std::string>>;
/** Text a rule reports, as a string-table key with values, so the rules never hold display language. */
struct Message {
    std::string key;
    MessageArgs args;
    Message() = default;
    explicit Message(std::string text, MessageArgs values = {}) : key(std::move(text)), args(std::move(values)) {}
    bool empty() const {
        return key.empty();
    }
    bool operator==(const Message &other) const {
        return key == other.key && args == other.args;
    }
    bool operator!=(const Message &other) const {
        return !(*this == other);
    }
};
/** A message value that names another string-table key. */
inline std::string ref(const std::string &key) {
    return "@" + key;
}
} // namespace thorium::game
