#include <coretrace/logger.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace
{

void noop_sink(const char*, size_t)
{
}

} // namespace

int main()
{
    using namespace coretrace;

    set_sink(noop_sink);
    enable_logging();
    set_min_level(Level::Info);

    std::atomic<bool> start{false};
    std::atomic<int> ready{0};

    auto logger_worker = [&]() {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (!start.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (int i = 0; i < 12000; ++i)
        {
            log(Level::Info, "msg {}\n", i);
            log(Level::Info, Module("stress"), "module {}\n", i);
        }
    };

    auto config_worker = [&]() {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (!start.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (int i = 0; i < 12000; ++i)
        {
            set_thread_safe((i % 2) == 0);
            set_prefix((i % 2) == 0 ? "==alpha==" : "==beta==");

            enable_module("stress");
            if ((i % 3) == 0)
                disable_module("stress");

            (void)module_is_enabled("stress");
        }

        set_thread_safe(true);
    };

    std::vector<std::thread> threads;
    threads.emplace_back(logger_worker);
    threads.emplace_back(logger_worker);
    threads.emplace_back(config_worker);

    while (ready.load(std::memory_order_acquire) != 3)
        std::this_thread::yield();
    start.store(true, std::memory_order_release);

    for (auto& t : threads)
        t.join();

    reset_sink();
    return 0;
}
