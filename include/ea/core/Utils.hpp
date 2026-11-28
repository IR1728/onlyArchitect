#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>

namespace ea::utils {

// Generates a valid uppercase EA-style GUID: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
std::string generate_guid();

// String helpers
std::string trim(std::string_view str);
std::vector<std::string> split(std::string_view str, char delimiter);
std::string replace_all(std::string_view str, std::string_view from, std::string_view to);

// Escaping
std::string escape_xml(std::string_view str);
std::string escape_json(std::string_view str);

} // namespace ea::utils
