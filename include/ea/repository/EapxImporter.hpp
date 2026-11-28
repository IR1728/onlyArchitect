#pragma once

#include <functional>
#include <string>

namespace ea {

enum class RepositoryFileFormat {
    Unknown,
    Sqlite,   // Modern .qea (or SQLite renamed as .eap/.eapx)
    Jet3,     // MS Access 97 / Jet 3.5 (.eap)
    Jet4,     // MS Access 2000-2003 / Jet 4.0 (.eap / .eapx)
    Ace,      // MS Access 2007+ / ACE 12-16 (.eapx)
    Firebird  // Firebird Embedded (.feap / Firebird .eapx)
};

class EapxImporter {
public:
    using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;

    struct ConversionResult {
        bool ok{false};
        std::string outputPath;
        std::string detectedFormat;
        int tablesProcessed{0};
        int tablesSkippedEmpty{0};
        int rowsInserted{0};
        int failedStatements{0};
        std::string error;
    };

    // Inspects file header magic bytes to determine the real underlying database engine.
    static RepositoryFileFormat DetectFormat(const std::string& path);
    static std::string FormatName(RepositoryFileFormat fmt);

    // Returns true if the file is an MS Access Jet/ACE database requiring conversion to SQLite.
    // Returns false for native SQLite files even if their extension is .eap/.eapx.
    static bool IsEapxFile(const std::string& path);

    // Converts an .eapx/.eap file to an SQLite .qea repository with rich diagnostics and progress.
    static ConversionResult ConvertToSqliteDetailed(const std::string& eapx_path,
                                                    std::string target_qea_path = "",
                                                    ProgressCallback progress = nullptr);

    // Convenience wrapper returning the converted SQLite path, or empty string on failure.
    static std::string ConvertToSqlite(const std::string& eapx_path, std::string target_qea_path = "");
};

} // namespace ea
