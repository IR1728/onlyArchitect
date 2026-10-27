#pragma once

#include <string>

namespace ea {

class EapxImporter {
public:
    // Checks if the given file is an MS Access / Jet database (.eap or .eapx)
    static bool IsEapxFile(const std::string& path);

    // Converts an .eapx/.eap file to an SQLite .qea repository.
    // If target_qea_path is empty, creates a cached file in /tmp/onlyarchitect_cache/.
    // Returns the path to the converted SQLite database, or empty string on failure.
    static std::string ConvertToSqlite(const std::string& eapx_path, std::string target_qea_path = "");
};

} // namespace ea
