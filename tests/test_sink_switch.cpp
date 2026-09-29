#include <coretrace/logger.hpp>

#include <cstdio>
#include <string>

namespace {

std::string first_output;
std::string second_output;

void second_sink(const char *data, size_t size) {
  second_output.append(data, size);
}

void first_sink(const char *data, size_t size) {
  first_output.append(data, size);
  coretrace::set_sink(second_sink);
}

} // namespace

int main() {
  coretrace::set_sink(first_sink);
  coretrace::enable_logging();
  coretrace::log(coretrace::Level::Info, "whole line\n");
  coretrace::reset_sink();

  if (first_output.find("whole line\n") == std::string::npos ||
      !second_output.empty()) {
    std::fprintf(stderr, "one line was split across two sinks\n");
    return 1;
  }
  return 0;
}
