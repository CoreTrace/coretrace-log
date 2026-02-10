#include <coretrace/logger.hpp>

#include <cstdio>
#include <string>

namespace {

std::string g_capture;

void capture_sink(const char *data, size_t size) {
  g_capture.append(data, size);
}

} // namespace

int main() {
  using namespace coretrace;

  set_sink(capture_sink);
  enable_logging();
  set_min_level(Level::Info);

  enable_all_modules();
  enable_module("alloc");
  enable_module("trace");

  log(Level::Info, Module("alloc"), "alloc accepted\n");
  log(Level::Info, Module("network"), "network filtered\n");

  const bool alloc_seen = g_capture.find("alloc accepted") != std::string::npos;
  const bool network_seen =
      g_capture.find("network filtered") != std::string::npos;

  disable_module("alloc");

  const size_t before = g_capture.size();
  log(Level::Info, Module("alloc"), "alloc filtered\n");
  log(Level::Info, Module("trace"), "trace accepted\n");

  const bool alloc_filtered_seen =
      g_capture.find("alloc filtered", before) != std::string::npos;
  const bool trace_seen =
      g_capture.find("trace accepted", before) != std::string::npos;

  reset_sink();

  if (!alloc_seen || network_seen || alloc_filtered_seen || !trace_seen) {
    std::fprintf(stderr,
                 "alloc_seen=%d network_seen=%d alloc_filtered_seen=%d "
                 "trace_seen=%d\\n%s\\n",
                 alloc_seen ? 1 : 0, network_seen ? 1 : 0,
                 alloc_filtered_seen ? 1 : 0, trace_seen ? 1 : 0,
                 g_capture.c_str());
    return 1;
  }

  return 0;
}
