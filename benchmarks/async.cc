#include <yamail/resource_pool/async/pool.hpp>

#include <benchmark/benchmark.h>

#include <boost/numeric/conversion/cast.hpp>

#include <condition_variable>
#include <iomanip>
#include <random>
#include <thread>

namespace {

using namespace yamail::resource_pool;

struct benchmark_args {
    std::size_t sequences;
    std::size_t threads;
    std::size_t resources;
    std::size_t queue_size;
};

struct resource {};

struct context {
    boost::asio::io_service io_service;
    boost::asio::io_service::work work {io_service};
    std::atomic_bool stop {false};
    time_traits::duration timeout {std::chrono::milliseconds(100)};
    double recycle_probability {0};
    std::vector<std::chrono::steady_clock::duration> durations;
    std::condition_variable get_next;
    std::mutex get_next_mutex;
    std::unique_lock<std::mutex> get_next_lock {get_next_mutex};

    void wait_next() {
        get_next.wait(get_next_lock);
    }

    void allow_next() {
        get_next.notify_one();
    }

    void finish() {
        stop = true;
        io_service.stop();
    }
};

struct callback {
    context& ctx;
    async::pool<resource>& pool;

    void operator ()(const boost::system::error_code& ec, async::pool<resource>::handle handle) {
        impl(ec, std::move(handle));
        if (!ctx.stop) {
            pool.get_auto_waste(ctx.io_service, *this, ctx.timeout);
        }
        ctx.allow_next();
    }

    void impl(const boost::system::error_code& ec, async::pool<resource>::handle handle) {
        static thread_local std::minstd_rand generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
        static std::uniform_real_distribution<> distrubution(0, 1);
        static constexpr const double recycle_probability = 0.5;
        if (!ec) {
            if (handle.empty()) {
                handle.reset(resource {});
            }
            if (distrubution(generator) < recycle_probability) {
                handle.recycle();
            }
        }
    }
};

struct io_service_post_callback {
    context& ctx;

    void operator ()() {
        if (!ctx.stop) {
            ctx.io_service.post(*this);
        }
        ctx.allow_next();
    }
};

static const std::vector<benchmark_args> benchmarks({
    {1, 1, 1, 0}, // 0
    {2, 1, 1, 1}, // 1
    {2, 1, 2, 0}, // 2
    {10, 1, 10, 0}, // 3
    {10, 1, 1, 9}, // 4
    {10, 1, 5, 5}, // 5
    {10, 1, 9, 1}, // 6
    {10, 2, 5, 5}, // 7
    {100, 1, 100, 0}, // 8
    {100, 1, 10, 90}, // 9
    {100, 1, 50, 50}, // 10
    {100, 1, 90, 10}, // 11
    {100, 2, 50, 50}, // 12
    {1000, 1, 10, 990}, // 13
    {1000, 2, 10, 990}, // 14
    {10000, 1, 10, 9990}, // 15
    {10000, 2, 10, 9990}, // 16
});

void get_auto_waste(benchmark::State& state) {
    const auto& args = benchmarks[boost::numeric_cast<std::size_t>(state.range(0))];
    context ctx;
    std::vector<std::thread> workers;
    for (std::size_t i = 0; i < args.threads; ++i) {
        workers.emplace_back(std::thread([&] { return ctx.io_service.run(); }));
    }
    async::pool<resource> pool(args.resources, args.queue_size);
    callback cb {ctx, pool};
    for (std::size_t i = 0; i < args.sequences; ++i) {
        pool.get_auto_waste(ctx.io_service, cb, ctx.timeout);
    }
    while (state.KeepRunning()) {
        ctx.wait_next();
    }
    ctx.finish();
    std::for_each(workers.begin(), workers.end(), std::mem_fn(&std::thread::join));
}

void all_benchmarks(benchmark::internal::Benchmark* b) {
    for (std::size_t n = 0; n < benchmarks.size(); ++n) {
        b->Arg(int(n));
    }
}

}

BENCHMARK(get_auto_waste)->Apply(all_benchmarks);

BENCHMARK_MAIN()
