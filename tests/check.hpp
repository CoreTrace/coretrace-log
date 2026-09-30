#ifndef CORETRACE_LOGGER_TESTS_CHECK_HPP
#define CORETRACE_LOGGER_TESTS_CHECK_HPP

// Assertion and capture helpers shared by the test programs. Each test is its
// own process: CHECK() records failures, and main() returns result().

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ct_test {

inline int failures = 0;

/// Everything written through capture_sink().
inline std::string captured;

inline void capture_sink(const char *data, size_t size) {
  captured.append(data, size);
}

/// True when `text` appears in the captured output at or after `from`.
[[nodiscard]] inline bool logged(std::string_view text, size_t from = 0) {
  return captured.find(text, from) != std::string::npos;
}

inline bool set_env(const char *key, const char *value) {
#if defined(_WIN32)
  return _putenv_s(key, value) == 0;
#else
  return setenv(key, value, 1) == 0;
#endif
}

inline void check(bool ok, const char *expr, const char *file, int line) {
  if (ok)
    return;
  ++failures;
  std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expr);
}

/// Exit code for main(): non-zero when a CHECK failed. Prints the captured
/// output on failure to help diagnose it.
[[nodiscard]] inline int result() {
  if (failures == 0)
    return 0;
  if (!captured.empty())
    std::fprintf(stderr, "captured output:\n%s\n", captured.c_str());
  return 1;
}

} // namespace ct_test

#define CHECK(expr)                                                            \
  ::ct_test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#endif // CORETRACE_LOGGER_TESTS_CHECK_HPP
