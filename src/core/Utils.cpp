#include "ea/core/Utils.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace ea::utils {

std::string generate_guid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t p1 = dis(gen);
    uint64_t p2 = dis(gen);

    // Set UUID version 4 and variant
    p1 = (p1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    p2 = (p2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream ss;
    ss << '{'
       << std::hex << std::uppercase << std::setfill('0')
       << std::setw(8) << ((p1 >> 32) & 0xFFFFFFFF) << '-'
       << std::setw(4) << ((p1 >> 16) & 0xFFFF) << '-'
       << std::setw(4) << (p1 & 0xFFFF) << '-'
       << std::setw(4) << ((p2 >> 48) & 0xFFFF) << '-'
       << std::setw(12) << (p2 & 0xFFFFFFFFFFFFULL)
       << '}';
    return ss.str();
}

std::string trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
}

std::vector<std::string> split(std::string_view str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string_view::npos) {
        tokens.emplace_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    if (start < str.size()) {
        tokens.emplace_back(str.substr(start));
    }
    return tokens;
}

std::string replace_all(std::string_view str, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(str);
    std::string result;
    result.reserve(str.size());
    size_t start = 0;
    size_t pos = str.find(from);
    while (pos != std::string_view::npos) {
        result.append(str.substr(start, pos - start));
        result.append(to);
        start = pos + from.size();
        pos = str.find(from, start);
    }
    result.append(str.substr(start));
    return result;
}

std::string escape_xml(std::string_view str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string escape_json(std::string_view str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream ss;
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    result += ss.str();
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

} // namespace ea::utils
