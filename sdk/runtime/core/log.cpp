#include "core/log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace logx {

static std::mutex g_mutex;
static FILE* g_file = nullptr;
static std::string g_path;
static auto g_start = std::chrono::steady_clock::now();

constexpr int kKeep = 30;

// the run number of prefix_NNN.log, 0 for another name
static int RunNumber(const fs::path& file, const std::string& prefix) {
    std::string name = file.filename().string();
    if (file.extension() != ".log" || name.size() <= prefix.size() + 1 || name.compare(0, prefix.size() + 1, prefix + "_"))
        return 0;
    return std::atoi(name.c_str() + prefix.size() + 1);
}

void Init(const std::string& dir, const std::string& prefix, bool append) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::error_code ec;
    fs::create_directories(dir, ec);
    g_path = (fs::path(dir) / (prefix + ".log")).string();
    if (!append) {
        g_file = std::fopen(g_path.c_str(), "w");
        if (g_file) {
            std::fclose(g_file);
            g_file = nullptr;
        }
    }
}

const std::string& Path() { return g_path; }

void Write(Level lv, const char* fmt, ...) {
    if (lv != Level::Error) return;
    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    auto tid = std::hash<std::thread::id>()(std::this_thread::get_id()) & 0xFFFF;
    std::lock_guard<std::mutex> lock(g_mutex);
    char line[4300];
    int n = std::snprintf(line, sizeof(line), "[%9.3f] [error] [t%04zx] %s\n", t, size_t(tid), msg);
    n = std::min(n, int(sizeof(line)) - 1);
    if (!g_file && !g_path.empty()) {
        g_file = std::fopen(g_path.c_str(), "a");
    }
    if (g_file) {
        std::fwrite(line, 1, size_t(n), g_file);
        std::fflush(g_file);
    }
    std::fwrite(line, 1, size_t(n), stderr);
}

void Flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) std::fflush(g_file);
}

}  // namespace logx
