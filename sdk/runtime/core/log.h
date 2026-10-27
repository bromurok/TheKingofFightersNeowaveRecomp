#pragma once
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <string>

namespace logx {

// a release build writes Report and Error lines only
enum class Level { Debug, Info, Warn, Error, Report };

// release: dir/prefix.log, carried on when append; debug: dir/prefix_NNN.log per run, newest 30 kept
void Init(const std::string& dir, const std::string& prefix, bool append = false);
void Write(Level lv, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void Flush();
const std::string& Path();

inline constexpr bool kVerbose = false;

}  // namespace logx

#define LOGD(...) do {} while (0)
#define LOGI(...) do {} while (0)
#define LOGW(...) do {} while (0)
#define LOGE(...) ::logx::Write(::logx::Level::Error, __VA_ARGS__)
#define LOGR(...) do {} while (0)

// logs only the first n times a call site is reached
#define LOG_FIRST(n, ...) do {} while (0)
