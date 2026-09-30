#include <coretrace/logger.hpp>

int main() {
  coretrace::enable_logging();
  coretrace::log(coretrace::Level::Info, "consumer {}\n", "ok");
  return 0;
}
