#include <coretrace/logger.hpp>

#include <cstdio>
#include <string>

namespace {

std::string captured;
bool logged_nested = false;

void reentrant_sink(const char *data, size_t size) {
  captured.append(data, size);
  if (!logged_nested) {
    logged_nested = true;
    coretrace::log(coretrace::Level::Error, "nested\n");
  }
}

} // namespace

int main() {
  coretrace::set_sink(reentrant_sink);
  coretrace::enable_logging();
  coretrace::log(coretrace::Level::Error, "outer\n");
  coretrace::reset_sink();

  if (captured.find("outer\n") == std::string::npos ||
      captured.find("nested\n") == std::string::npos) {
    std::fprintf(stderr, "reentrant sink did not receive both messages\n");
    return 1;
  }
  return 0;
}
