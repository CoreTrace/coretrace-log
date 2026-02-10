#include <coretrace/logger.hpp>

#include <cstdlib>
#include <string>

namespace
{

std::string g_capture;

void capture_sink(const char* data, size_t size)
{
    g_capture.append(data, size);
}

} // namespace

int main()
{
    setenv("CT_LOG_LEVEL", "info", 1);

    coretrace::set_sink(capture_sink);
    coretrace::enable_logging();

    // Explicit API config must win over env defaults.
    coretrace::set_min_level(coretrace::Level::Error);

    coretrace::log(coretrace::Level::Warn, "warn should be filtered\n");
    coretrace::log(coretrace::Level::Error, "error should pass\n");

    coretrace::reset_sink();
    unsetenv("CT_LOG_LEVEL");

    const bool has_warn = g_capture.find("[WARN]") != std::string::npos;
    const bool has_error = g_capture.find("[ERROR]") != std::string::npos;

    if (has_warn || !has_error)
        return 1;

    return 0;
}
