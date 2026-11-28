#pragma once

#include <string>
#include "ea/repository/Repository.hpp"

namespace ea {

class ModelDocExporter {
public:
    static std::string exportMarkdown(Repository& repo);
    static bool exportMarkdownToFile(Repository& repo, const std::string& filePath);
};

} // namespace ea
