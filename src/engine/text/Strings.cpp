#include "Strings.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace thorium::engine {
namespace {
/** Minimal reader for a flat JSON object of string values. */
class Reader {
  public:
    explicit Reader(std::string_view text) : text_(text) {}
    void space() {
        while (at_ < text_.size() &&
               (text_[at_] == ' ' || text_[at_] == '\n' || text_[at_] == '\r' || text_[at_] == '\t'))
            ++at_;
    }
    bool take(char expected) {
        space();
        if (at_ >= text_.size() || text_[at_] != expected)
            return false;
        ++at_;
        return true;
    }
    bool peek(char expected) {
        space();
        return at_ < text_.size() && text_[at_] == expected;
    }
    bool done() {
        space();
        return at_ >= text_.size();
    }
    std::size_t offset() const {
        return at_;
    }
    /** Read one quoted string, decoding JSON escapes into UTF-8. */
    bool string(std::string &out) {
        out.clear();
        if (!take('"'))
            return false;
        while (at_ < text_.size()) {
            const char c = text_[at_++];
            if (c == '"')
                return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return false;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (at_ >= text_.size())
                return false;
            const char escape = text_[at_++];
            switch (escape) {
            case '"':
            case '\\':
            case '/':
                out += escape;
                break;
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'u': {
                std::uint32_t code = 0;
                if (!hex(code))
                    return false;
                // A high surrogate must be followed by its low half.
                if (code >= 0xD800 && code <= 0xDBFF) {
                    std::uint32_t low = 0;
                    if (at_ + 1 >= text_.size() || text_[at_] != '\\' || text_[at_ + 1] != 'u')
                        return false;
                    at_ += 2;
                    if (!hex(low) || low < 0xDC00 || low > 0xDFFF)
                        return false;
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                } else if (code >= 0xDC00 && code <= 0xDFFF)
                    return false;
                utf8(code, out);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

  private:
    std::string_view text_;
    std::size_t at_ = 0;
    bool hex(std::uint32_t &code) {
        if (at_ + 4 > text_.size())
            return false;
        code = 0;
        for (int index = 0; index < 4; ++index) {
            const char c = text_[at_++];
            code <<= 4;
            if (c >= '0' && c <= '9')
                code |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                code |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                code |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                return false;
        }
        return true;
    }
    static void utf8(std::uint32_t code, std::string &out) {
        if (code < 0x80)
            out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
};
} // namespace

bool StringTable::parse(std::string_view json, std::string &error) {
    entries_.clear();
    error.clear();
    Reader in(json);
    const auto fail = [&](const char *reason) {
        entries_.clear();
        error = std::string(reason) + " at byte " + std::to_string(in.offset());
        return false;
    };
    if (!in.take('{'))
        return fail("expected '{'");
    if (!in.peek('}'))
        for (;;) {
            std::string key, value;
            if (!in.string(key))
                return fail("expected a quoted key");
            if (!in.take(':'))
                return fail("expected ':'");
            if (!in.string(value))
                return fail("expected a quoted string value");
            if (!entries_.emplace(std::move(key), std::move(value)).second)
                return fail("repeated key");
            if (in.take(','))
                continue;
            break;
        }
    if (!in.take('}'))
        return fail("expected ',' or '}'");
    if (!in.done())
        return fail("unexpected text after the object");
    return true;
}
bool StringTable::load(const std::string &path, std::string &error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        entries_.clear();
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!parse(buffer.str(), error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}
const std::string *StringTable::find(std::string_view key) const {
    const auto found = entries_.find(std::string(key));
    return found == entries_.end() ? nullptr : &found->second;
}
std::vector<std::string> StringTable::keys() const {
    std::vector<std::string> result;
    result.reserve(entries_.size());
    for (const auto &entry : entries_)
        result.push_back(entry.first);
    std::sort(result.begin(), result.end());
    return result;
}
std::string formatText(std::string_view text, const TextArgs &args) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t at = 0; at < text.size();) {
        const std::size_t close = text[at] == '{' ? text.find('}', at + 1) : std::string_view::npos;
        if (close == std::string_view::npos) {
            result += text[at++];
            continue;
        }
        const std::string_view name = text.substr(at + 1, close - at - 1);
        const auto value =
            std::find_if(args.begin(), args.end(), [name](const std::pair<std::string, std::string> &arg) {
                return arg.first == name;
            });
        if (value == args.end())
            result.append(text.substr(at, close - at + 1));
        else
            result += value->second;
        at = close + 1;
    }
    return result;
}
std::vector<std::string> placeholders(std::string_view text) {
    std::vector<std::string> names;
    for (std::size_t at = text.find('{'); at != std::string_view::npos; at = text.find('{', at + 1)) {
        const std::size_t close = text.find('}', at + 1);
        if (close == std::string_view::npos)
            break;
        names.emplace_back(text.substr(at + 1, close - at - 1));
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}
} // namespace thorium::engine
