#include <coretrace/logger.hpp>

#include <cstdio>
#include <cstdlib>

int main() {
#if defined(_WIN32)
  if (_putenv_s("CT_LOG_LEVEL", "debug") != 0 ||
      _putenv_s("CT_DEBUG", "selected") != 0)
    return 1;
#else
  if (setenv("CT_LOG_LEVEL", "debug", 1) != 0 ||
      setenv("CT_DEBUG", "selected", 1) != 0)
    return 1;
#endif

  if (coretrace::min_level() != coretrace::Level::Debug ||
      !coretrace::module_is_enabled("selected") ||
      coretrace::module_is_enabled("other")) {
    std::fprintf(stderr, "environment defaults were not loaded by getters\n");
    return 1;
  }

  return 0;
}
