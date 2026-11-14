#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thorium::engine {
/** One language's text: a flat JSON object whose keys name strings and whose values are the strings. */
class StringTable {
  public:
    /** Replace the table with the entries of one flat JSON object of string values; on failure the table is
     *  left empty and `error` says where parsing stopped. */
    bool parse(std::string_view json, std::string &error);
    /** Read and parse a file. */
    bool load(const std::string &path, std::string &error);
    /** Read a string by key, or nullptr when the table has none. */
    const std::string *find(std::string_view key) const;
    /** Read every key, sorted, for tooling and tests. */
    std::vector<std::string> keys() const;
    std::size_t size() const {
        return entries_.size();
    }

  private:
    std::unordered_map<std::string, std::string> entries_;
};
/** Named values for `{name}` placeholders. */
using TextArgs = std::vector<std::pair<std::string, std::string>>;
/** Replace each `{name}` with its value; unknown placeholders stay as written. */
std::string formatText(std::string_view text, const TextArgs &args);
/** Read the placeholder names a string uses, sorted and without repeats. */
std::vector<std::string> placeholders(std::string_view text);
} // namespace thorium::engine
