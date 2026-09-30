#include "check.hpp"

#include <coretrace/logger.hpp>

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

  // The whole line must reach the first sink, although it switches sinks.
  CHECK(first_output.find("whole line\n") != std::string::npos);
  CHECK(second_output.empty());
  return ct_test::result();
}
