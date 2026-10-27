#include "ea/repository/EapxImporter.hpp"
#include "ea/core/Utils.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <array>
#include <sqlite3.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace ea {

static std::string execCommand(const std::string& cmd, const std::string& ldPath = "") {
    std::string fullCmd = cmd;
    if (!ldPath.empty()) {
        fullCmd = "LD_LIBRARY_PATH=" + ldPath + " " + fullCmd;
    }

    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

static bool endsWithIgnoreCase(std::string_view str, std::string_view suffix) {
    if (str.size() < suffix.size()) return false;
    auto s1 = str.substr(str.size() - suffix.size());
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s1[i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

bool EapxImporter::IsEapxFile(const std::string& path) {
    if (endsWithIgnoreCase(path, ".eap") || endsWithIgnoreCase(path, ".eapx")) {
        return true;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char header[32] = {0};
    file.read(header, sizeof(header));
    std::string_view h(header, sizeof(header));

    if (h.find("Standard Jet DB") != std::string_view::npos ||
        h.find("Standard ACE DB") != std::string_view::npos) {
        return true;
    }

    return false;
}

std::string EapxImporter::ConvertToSqlite(const std::string& eapx_path, std::string target_qea_path) {
    if (!fs::exists(eapx_path)) return "";

    // Determine target path
    if (target_qea_path.empty()) {
        std::string cache_dir = "/tmp/onlyarchitect_cache";
        fs::create_directories(cache_dir);

        std::string stem = fs::path(eapx_path).stem().string();
        std::hash<std::string> hasher;
        size_t h = hasher(eapx_path);
        target_qea_path = cache_dir + "/" + stem + "_" + std::to_string(h) + ".qea";
    }

    // Check if target is newer than eapx
    if (fs::exists(target_qea_path)) {
        auto eapx_time = fs::last_write_time(eapx_path);
        auto qea_time = fs::last_write_time(target_qea_path);
        if (qea_time >= eapx_time && fs::file_size(target_qea_path) > 10000) {
            return target_qea_path;
        }
    }

    // Locate mdbtools binaries
    std::string mdb_dir = "/home/iran/EA/ea_cpp/tools/mdbtools";
    std::string bin_dir = mdb_dir + "/bin";
    std::string lib_dir = mdb_dir + "/lib";

    std::string schema_bin = bin_dir + "/mdb-schema";
    std::string tables_bin = bin_dir + "/mdb-tables";
    std::string export_bin = bin_dir + "/mdb-export";

    if (!fs::exists(schema_bin)) {
        schema_bin = "mdb-schema";
        tables_bin = "mdb-tables";
        export_bin = "mdb-export";
        lib_dir = "";
    }

    // 1. Export schema
    std::string schema_sql = execCommand(schema_bin + " \"" + eapx_path + "\" sqlite", lib_dir);
    if (schema_sql.empty() || schema_sql.find("CREATE TABLE") == std::string::npos) {
        std::cerr << "EapxImporter: Failed to extract schema from " << eapx_path << "\n";
        return "";
    }

    // 2. Open SQLite target database
    if (fs::exists(target_qea_path)) {
        fs::remove(target_qea_path);
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(target_qea_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "EapxImporter: Failed to create SQLite database at " << target_qea_path << "\n";
        return "";
    }

    // Execute schema DDL
    char* err_msg = nullptr;
    if (sqlite3_exec(db, schema_sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "EapxImporter: Schema DDL error: " << (err_msg ? err_msg : "unknown") << "\n";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return "";
    }

    // 3. Get table names
    std::string tables_out = execCommand(tables_bin + " -1 \"" + eapx_path + "\"", lib_dir);
    std::istringstream stream(tables_out);
    std::string table_name;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    while (std::getline(stream, table_name)) {
        // Trim whitespace
        while (!table_name.empty() && (table_name.back() == '\r' || table_name.back() == ' ' || table_name.back() == '\n')) {
            table_name.pop_back();
        }
        while (!table_name.empty() && (table_name.front() == ' ' || table_name.front() == '\t')) {
            table_name.erase(table_name.begin());
        }
        if (table_name.empty()) continue;

        std::string insert_sql = execCommand(export_bin + " -I sqlite \"" + eapx_path + "\" \"" + table_name + "\"", lib_dir);
        if (!insert_sql.empty()) {
            sqlite3_exec(db, insert_sql.c_str(), nullptr, nullptr, nullptr);
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);

    return target_qea_path;
}

} // namespace ea
