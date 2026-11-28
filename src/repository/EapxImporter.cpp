#include "ea/repository/EapxImporter.hpp"
#include "ea/core/Utils.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <sqlite3.h>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace fs = std::filesystem;

namespace ea {

namespace {

fs::path executableDir() {
    std::error_code ec;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return fs::path(buf).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size > 0) {
        std::string buf(size, '\0');
        if (_NSGetExecutablePath(buf.data(), &size) == 0) {
            return fs::weakly_canonical(fs::path(buf.c_str()), ec).parent_path();
        }
    }
#else
    fs::path exe_path = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && !exe_path.empty()) return exe_path.parent_path();
#endif
    return {};
}

struct CmdOutput {
    std::string out;
    int exitCode{-1};
};

#ifdef _WIN32
// Quotes an argument according to Win32 CreateProcess / CommandLineToArgvW rules (no cmd.exe shell!).
std::string win32QuoteArg(const std::string& arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) {
        return arg;
    }
    std::string out = "\"";
    for (size_t i = 0; i < arg.size(); ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == '\\') {
            ++backslashes;
            ++i;
        }
        if (i == arg.size()) {
            out.append(backslashes * 2, '\\');
        } else if (arg[i] == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(arg[i]);
        }
    }
    out.push_back('"');
    return out;
}
#endif

// Launches a subprocess directly (execvp on POSIX, CreateProcessA on Windows) WITHOUT invoking
// /bin/sh or cmd.exe, eliminating OS command injection even for adversarial filenames.
CmdOutput execProcess(const std::string& exe, const std::vector<std::string>& args,
                      const std::string& ldPath = "", bool silenceStderr = false) {
    CmdOutput res;
#ifdef _WIN32
    (void)ldPath;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return res;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nulHandle = INVALID_HANDLE_VALUE;
    if (silenceStderr) {
        nulHandle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = (silenceStderr && nulHandle != INVALID_HANDLE_VALUE) ? nulHandle : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::string cmdLine = win32QuoteArg(exe);
    for (const auto& a : args) {
        cmdLine.push_back(' ');
        cmdLine += win32QuoteArg(a);
    }
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (nulHandle != INVALID_HANDLE_VALUE) CloseHandle(nulHandle);

    if (!ok) {
        CloseHandle(readPipe);
        return res;
    }

    std::array<char, 8192> buf;
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr) && bytesRead > 0) {
        res.out.append(buf.data(), bytesRead);
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        res.exitCode = static_cast<int>(exitCode);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return res;
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) return res;

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return res;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (silenceStderr) {
            const int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
        }

        if (!ldPath.empty()) {
#ifdef __APPLE__
            setenv("DYLD_LIBRARY_PATH", ldPath.c_str(), 1);
#else
            setenv("LD_LIBRARY_PATH", ldPath.c_str(), 1);
#endif
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 2);
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(exe.c_str(), argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    std::array<char, 8192> buf;
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
        res.out.append(buf.data(), static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) > 0) {
        if (WIFEXITED(status)) res.exitCode = WEXITSTATUS(status);
        else res.exitCode = -1;
    }
    return res;
#endif
}

bool endsWithIgnoreCase(std::string_view str, std::string_view suffix) {
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

struct MdbToolsPaths {
    std::string schemaBin;
    std::string tablesBin;
    std::string exportBin;
    std::string libDir;
    bool valid{false};
};

bool probeMdbSchema(const std::string& schemaBin, const std::string& libDir) {
    auto r = execProcess(schemaBin, {"--version"}, libDir, true);
    if (r.exitCode == 0) return true;
    auto r2 = execProcess(schemaBin, {"--help"}, libDir, true);
    return !r2.out.empty() && r2.exitCode != 127 && r2.exitCode != -1;
}

MdbToolsPaths locateMdbTools() {
#ifdef _WIN32
    constexpr const char* kBinSuffix = ".exe";
#else
    constexpr const char* kBinSuffix = "";
#endif
    std::vector<fs::path> candidateDirs;
#ifdef ONLYARCHITECT_MDBTOOLS_DIR
    candidateDirs.emplace_back(ONLYARCHITECT_MDBTOOLS_DIR);
#endif
    std::error_code ec;
    candidateDirs.emplace_back(fs::current_path(ec) / "tools" / "mdbtools");
    candidateDirs.emplace_back(fs::current_path(ec).parent_path() / "tools" / "mdbtools");
    const fs::path exeDir = executableDir();
    if (!exeDir.empty()) {
        candidateDirs.emplace_back(exeDir / "tools" / "mdbtools");
        candidateDirs.emplace_back(exeDir.parent_path() / "tools" / "mdbtools");
    }

    // 1. Prefer bundled mdbtools ONLY if it executes cleanly on this OS/distro
    for (const auto& dir : candidateDirs) {
        const fs::path candSchema = dir / "bin" / (std::string("mdb-schema") + kBinSuffix);
        if (!fs::exists(candSchema, ec)) continue;
        MdbToolsPaths p;
        p.schemaBin = candSchema.string();
        p.tablesBin = (dir / "bin" / (std::string("mdb-tables") + kBinSuffix)).string();
        p.exportBin = (dir / "bin" / (std::string("mdb-export") + kBinSuffix)).string();
        p.libDir = (dir / "lib").string();
        if (probeMdbSchema(p.schemaBin, p.libDir)) {
            p.valid = true;
            return p;
        }
    }

    // 2. Fallback to system PATH (`apt install mdbtools` / `brew install mdbtools`)
    MdbToolsPaths sys;
    sys.schemaBin = std::string("mdb-schema") + kBinSuffix;
    sys.tablesBin = std::string("mdb-tables") + kBinSuffix;
    sys.exportBin = std::string("mdb-export") + kBinSuffix;
    if (probeMdbSchema(sys.schemaBin, "")) {
        sys.valid = true;
        return sys;
    }

    return {};
}

void addDefaultToNotNull(std::string& sql, const std::string& typeToken, const std::string& defaultVal) {
    const std::string needle = typeToken + " NOT NULL";
    const std::string replacement = needle + " DEFAULT " + defaultVal;
    size_t pos = 0;
    while ((pos = sql.find(needle, pos)) != std::string::npos) {
        const size_t after = pos + needle.size();
        if (sql.compare(after, 8, " DEFAULT") != 0) {
            sql.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        } else {
            pos = after;
        }
    }
}

void sanitizeSchemaSql(std::string& schemaSql) {
    addDefaultToNotNull(schemaSql, "INTEGER", "0");
    addDefaultToNotNull(schemaSql, "REAL", "0");
    addDefaultToNotNull(schemaSql, "NUMERIC", "0");
    addDefaultToNotNull(schemaSql, "BOOLEAN", "0");
    addDefaultToNotNull(schemaSql, "TEXT", "''");
}

void executeSqlBatchTolerant(sqlite3* db, const std::string& sql, bool retryIgnoreOnInsert,
                             int& okCount, int& failCount) {
    const char* cursor = sql.c_str();
    const char* end = cursor + sql.size();

    while (cursor < end) {
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (cursor >= end) break;

        sqlite3_stmt* stmt = nullptr;
        const char* tail = nullptr;
        int rc = sqlite3_prepare_v2(db, cursor, static_cast<int>(end - cursor), &stmt, &tail);
        if (rc != SQLITE_OK) {
            const char* nextSemi = static_cast<const char*>(std::memchr(cursor, ';', static_cast<size_t>(end - cursor)));
            const char* nextNl = static_cast<const char*>(std::memchr(cursor, '\n', static_cast<size_t>(end - cursor)));
            if (nextSemi && (!nextNl || nextSemi < nextNl)) cursor = nextSemi + 1;
            else if (nextNl) cursor = nextNl + 1;
            else break;
            ++failCount;
            continue;
        }

        if (!stmt) {
            cursor = (tail && tail > cursor) ? tail : end;
            continue;
        }

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE || rc == SQLITE_ROW) {
            const int ch = sqlite3_changes(db);
            okCount += (ch > 0 ? ch : 1);
        } else if (retryIgnoreOnInsert && tail > cursor) {
            std::string single(cursor, static_cast<size_t>(tail - cursor));
            size_t insPos = single.find("INSERT INTO ");
            if (insPos != std::string::npos) {
                single.replace(insPos, 12, "INSERT OR IGNORE INTO ");
                if (sqlite3_exec(db, single.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK) {
                    const int ch = sqlite3_changes(db);
                    okCount += (ch > 0 ? ch : 1);
                } else {
                    ++failCount;
                }
            } else {
                ++failCount;
            }
        } else {
            ++failCount;
        }

        cursor = (tail && tail > cursor) ? tail : end;
    }
}

} // namespace

RepositoryFileFormat EapxImporter::DetectFormat(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return RepositoryFileFormat::Unknown;

    unsigned char header[32] = {0};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    const auto n = file.gcount();
    if (n < 16) return RepositoryFileFormat::Unknown;

    std::string_view h(reinterpret_cast<const char*>(header), static_cast<size_t>(n));
    if (h.rfind("SQLite format 3", 0) == 0) {
        return RepositoryFileFormat::Sqlite;
    }
    if (h.find("Standard Jet DB") != std::string_view::npos) {
        return (n > 20 && header[20] == 0x00) ? RepositoryFileFormat::Jet3 : RepositoryFileFormat::Jet4;
    }
    if (h.find("Standard ACE DB") != std::string_view::npos) {
        return RepositoryFileFormat::Ace;
    }
    if (endsWithIgnoreCase(path, ".feap") ||
        (header[0] == 0x01 && header[1] == 0x00 && header[2] == 0x39 && header[3] == 0x30)) {
        return RepositoryFileFormat::Firebird;
    }
    return RepositoryFileFormat::Unknown;
}

std::string EapxImporter::FormatName(RepositoryFileFormat fmt) {
    switch (fmt) {
        case RepositoryFileFormat::Sqlite: return "SQLite 3 (.qea)";
        case RepositoryFileFormat::Jet3: return "MS Access Jet 3.5 (.eap)";
        case RepositoryFileFormat::Jet4: return "MS Access Jet 4.0 (.eap/.eapx)";
        case RepositoryFileFormat::Ace: return "MS Access ACE 12+ (.eapx)";
        case RepositoryFileFormat::Firebird: return "Firebird Embedded (.feap)";
        default: return "Desconocido";
    }
}

bool EapxImporter::IsEapxFile(const std::string& path) {
    const RepositoryFileFormat fmt = DetectFormat(path);
    if (fmt == RepositoryFileFormat::Sqlite) return false;
    if (fmt == RepositoryFileFormat::Jet3 || fmt == RepositoryFileFormat::Jet4 ||
        fmt == RepositoryFileFormat::Ace || fmt == RepositoryFileFormat::Firebird) {
        return true;
    }
    return endsWithIgnoreCase(path, ".eap") || endsWithIgnoreCase(path, ".eapx") || endsWithIgnoreCase(path, ".feap");
}

EapxImporter::ConversionResult EapxImporter::ConvertToSqliteDetailed(const std::string& eapx_path,
                                                                     std::string target_qea_path,
                                                                     ProgressCallback progress) {
    ConversionResult result;
    if (!fs::exists(eapx_path)) {
        result.error = "El archivo de origen no existe: " + eapx_path;
        return result;
    }

    const RepositoryFileFormat fmt = DetectFormat(eapx_path);
    result.detectedFormat = FormatName(fmt);

    if (fmt == RepositoryFileFormat::Sqlite) {
        result.ok = true;
        result.outputPath = eapx_path;
        return result;
    }

    if (fmt == RepositoryFileFormat::Firebird) {
        result.error = "Este archivo usa el motor Firebird Embedded (.feap). "
                       "Para abrirlo en onlyArchitect, expórtalo primero a .qea (SQLite) o XMI 2.5.1.";
        return result;
    }

    if (fmt == RepositoryFileFormat::Unknown) {
        result.error = "No se reconoció un encabezado válido de MS Access (Jet 3.5, Jet 4.0 o ACE) ni SQLite en: " + eapx_path;
        return result;
    }

    std::error_code ec;
    if (target_qea_path.empty()) {
        fs::path cache_dir = fs::temp_directory_path(ec) / "onlyarchitect_cache";
        fs::create_directories(cache_dir, ec);

        std::string stem = fs::path(eapx_path).stem().string();
        std::hash<std::string> hasher;
        size_t h = hasher(fs::weakly_canonical(eapx_path, ec).string());
        target_qea_path = (cache_dir / (stem + "_" + std::to_string(h) + ".qea")).string();
    }

    if (fs::exists(target_qea_path, ec)) {
        auto eapx_time = fs::last_write_time(eapx_path, ec);
        auto qea_time = fs::last_write_time(target_qea_path, ec);
        if (!ec && qea_time >= eapx_time && fs::file_size(target_qea_path, ec) > 10000) {
            if (progress) progress(100, 100, "Usando caché SQLite actualizada...");
            result.ok = true;
            result.outputPath = target_qea_path;
            return result;
        }
    }

    const MdbToolsPaths tools = locateMdbTools();
    if (!tools.valid) {
        result.error = "No se encontraron las utilidades de mdbtools (mdb-schema / mdb-export) para leer " +
                       result.detectedFormat + ". Instálalas con 'sudo apt install mdbtools' (Linux) o 'brew install mdbtools' (macOS).";
        return result;
    }

    if (progress) progress(5, 100, "Extrayendo esquema (" + result.detectedFormat + ")...");

    // 1. Export schema (direct exec without shell)
    auto schemaRes = execProcess(tools.schemaBin, {eapx_path, "sqlite"}, tools.libDir, true);
    if (schemaRes.out.empty() || schemaRes.out.find("CREATE TABLE") == std::string::npos) {
        result.error = "mdb-schema no pudo leer el esquema de " + result.detectedFormat +
                       ". Puede ser una versión ACE cifrada/comprimida no soportada por mdbtools; "
                       "como alternativa, exporta el modelo a XMI 2.5.1 o .qea.";
        return result;
    }
    sanitizeSchemaSql(schemaRes.out);

    // 2. Write into temporary SQLite DB first so a failed run never corrupts target_qea_path
    const std::string tmp_qea_path = target_qea_path + ".migrating.tmp";
    fs::remove(tmp_qea_path, ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(tmp_qea_path.c_str(), &db) != SQLITE_OK) {
        result.error = "No se pudo crear la base de datos SQLite temporal en: " + tmp_qea_path;
        if (db) sqlite3_close(db);
        return result;
    }

    sqlite3_exec(db, "PRAGMA synchronous = OFF; PRAGMA journal_mode = MEMORY; PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);

    int ddlOk = 0, ddlFail = 0;
    executeSqlBatchTolerant(db, schemaRes.out, false, ddlOk, ddlFail);
    if (ddlOk == 0) {
        result.error = "No se pudo crear ninguna tabla SQLite a partir del esquema extraído.";
        sqlite3_close(db);
        fs::remove(tmp_qea_path, ec);
        return result;
    }

    // 3. Enumerate tables
    auto tablesRes = execProcess(tools.tablesBin, {"-1", eapx_path}, tools.libDir, true);
    std::vector<std::string> tables;
    {
        std::istringstream stream(tablesRes.out);
        std::string line;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\n' || line.back() == '\t')) line.pop_back();
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
            if (!line.empty()) tables.push_back(std::move(line));
        }
    }

    if (tables.empty()) {
        result.error = "mdb-tables no encontró tablas en el archivo " + result.detectedFormat + ".";
        sqlite3_close(db);
        fs::remove(tmp_qea_path, ec);
        return result;
    }

    // 4. Parallel mdb-export across up to 4 worker threads, serialized writes into SQLite
    const int totalTables = static_cast<int>(tables.size());
    const unsigned int hw = std::thread::hardware_concurrency();
    const unsigned int workerCount = std::clamp(hw == 0 ? 4u : hw, 1u, 4u);

    std::atomic<size_t> nextIndex{0};
    std::atomic<int> doneCount{0};
    std::mutex dbMutex;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    auto worker = [&]() {
        while (true) {
            const size_t idx = nextIndex.fetch_add(1);
            if (idx >= tables.size()) break;
            const std::string& tableName = tables[idx];

            auto exp = execProcess(tools.exportBin, {"-I", "sqlite", eapx_path, tableName}, tools.libDir, true);

            int inserted = 0;
            int failed = 0;
            const bool empty = exp.out.find("INSERT") == std::string::npos;
            if (!empty) {
                std::lock_guard<std::mutex> lock(dbMutex);
                executeSqlBatchTolerant(db, exp.out, true, inserted, failed);
                result.tablesProcessed += 1;
                result.rowsInserted += inserted;
                result.failedStatements += failed;
            } else {
                std::lock_guard<std::mutex> lock(dbMutex);
                result.tablesSkippedEmpty += 1;
            }

            const int completed = doneCount.fetch_add(1) + 1;
            if (progress) {
                const int pct = 10 + (completed * 85) / std::max(1, totalTables);
                progress(pct, 100, "Migrando tabla " + std::to_string(completed) + "/" +
                                       std::to_string(totalTables) + " (" + tableName + ")...");
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(workerCount);
    for (unsigned int i = 0; i < workerCount; ++i) pool.emplace_back(worker);
    for (auto& t : pool) t.join();

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA journal_mode = DELETE; PRAGMA synchronous = FULL;", nullptr, nullptr, nullptr);
    sqlite3_close(db);

    fs::remove(target_qea_path, ec);
    fs::rename(tmp_qea_path, target_qea_path, ec);
    if (ec) {
        fs::copy_file(tmp_qea_path, target_qea_path, fs::copy_options::overwrite_existing, ec);
        fs::remove(tmp_qea_path, ec);
    }

    if (progress) progress(100, 100, "Migración completada.");

    result.ok = fs::exists(target_qea_path, ec);
    result.outputPath = result.ok ? target_qea_path : "";
    if (!result.ok) {
        result.error = "No se pudo guardar el archivo convertido en: " + target_qea_path;
    }
    return result;
}

std::string EapxImporter::ConvertToSqlite(const std::string& eapx_path, std::string target_qea_path) {
    auto res = ConvertToSqliteDetailed(eapx_path, std::move(target_qea_path), nullptr);
    if (!res.ok && !res.error.empty()) {
        std::cerr << "EapxImporter: " << res.error << "\n";
    }
    return res.outputPath;
}

} // namespace ea
