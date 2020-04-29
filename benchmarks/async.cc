#include <yamail/resource_pool/async/pool.hpp>

#include <benchmark/benchmark.h>

#include <boost/asio/post.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <iomanip>
#include <random>
#include <thread>

namespace {

using namespace yamail::resource_pool;

class benchmark_args {
public:
    constexpr benchmark_args sequences(std::size_t value) const {
        auto copy = *this;
        copy.sequences_ = value;
        return copy;
    }

    constexpr std::size_t sequences() const {
        return sequences_;
    }

    constexpr benchmark_args threads(std::size_t value) const {
        auto copy = *this;
        copy.threads_ = value;
        return copy;
    }

    constexpr std::size_t threads() const {
        return threads_;
    }

    constexpr benchmark_args resources(std::size_t value) const {
        auto copy = *this;
        copy.resources_ = value;
        return copy;
    }

    constexpr std::size_t resources() const {
        return resources_;
    }

    constexpr benchmark_args queue_size(std::size_t value) const {
        auto copy = *this;
        copy.queue_size_ = value;
        return copy;
    }

    constexpr std::size_t queue_size() const {
        return queue_size_;
    }

private:
    std::size_t sequences_ = 0;
    std::size_t threads_ = 0;
    std::size_t resources_ = 0;
    std::size_t queue_size_ = 0;
};

struct resource {
    std::int64_t value = 0;
};

template <class Mutex = std::mutex>
using benchmark_pool = async::pool<
    resource,
    Mutex,
    boost::asio::io_context,
    async::detail::pool_impl<
        resource,
        Mutex,
        boost::asio::io_context,
        typename async::default_pool_queue<resource, Mutex, boost::asio::io_context>::type
    >
>;

struct stub_mutex {
    stub_mutex() = default;
    stub_mutex(const stub_mutex&) = delete;
    void lock() {}
    void unlock() {}
};

struct single_thread {};
struct multi_thread {};

template <class Threading>
struct context {
    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard = boost::asio::make_work_guard(io_context);
    std::conditional_t<std::is_same_v<Threading, multi_thread>, std::atomic_bool, bool> stop {false};
    time_traits::duration timeout {std::chrono::milliseconds(100)};
    std::vector<std::chrono::steady_clock::duration> durations;
    std::mutex get_next_mutex;
    std::condition_variable get_next;
    std::unique_lock<std::mutex> get_next_lock {get_next_mutex};
    std::conditional_t<std::is_same_v<Threading, multi_thread>, std::atomic<std::int64_t>, std::int64_t> ready_count {0};

    void wait_next() {
        if (--ready_count < 0) {
            get_next.wait(get_next_lock);
        }
    }

    void allow_next() {
        ++ready_count;
        if constexpr (std::is_same_v<Threading, multi_thread>) {
            get_next.notify_one();
        }
    }

    void finish() {
        stop = true;
        guard.reset();
    }
};

template <class Threading>
struct callback {
    using mutex_t = std::conditional_t<std::is_same_v<Threading, multi_thread>, std::mutex, stub_mutex>;
    using pool_t = benchmark_pool<mutex_t>;
    using handle_t = typename pool_t::handle;

    context<Threading>& ctx;
    pool_t& pool;

    void operator ()(const boost::system::error_code& ec, handle_t handle) {
        impl(ec, std::move(handle));
        if (!ctx.stop) {
            pool.get_auto_waste(ctx.io_context, *this, ctx.timeout);
        }
        ctx.allow_next();
    }

    void impl(const boost::system::error_code& ec, handle_t handle) {
        static thread_local std::minstd_rand generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
        static std::uniform_real_distribution<> distrubution(0, 1);
        constexpr const double recycle_probability = 0.5;
        if (!ec) {
            if (handle.empty()) {
                handle.reset(resource {});
            }
            benchmark::DoNotOptimize(++handle->value);
            if (distrubution(generator) < recycle_probability) {
                handle.recycle();
            }
        }
    }
};

const std::array<benchmark_args, 17> benchmarks{{
    benchmark_args().sequences(1).threads(1).resources(1).queue_size(0), // 0
    benchmark_args().sequences(2).threads(1).resources(1).queue_size(1), // 1
    benchmark_args().sequences(2).threads(1).resources(2).queue_size(0), // 2
    benchmark_args().sequences(10).threads(1).resources(10).queue_size(0), // 3
    benchmark_args().sequences(10).threads(1).resources(1).queue_size(9), // 4
    benchmark_args().sequences(10).threads(1).resources(5).queue_size(5), // 5
    benchmark_args().sequences(10).threads(1).resources(9).queue_size(1), // 6
    benchmark_args().sequences(10).threads(2).resources(5).queue_size(5), // 7
    benchmark_args().sequences(100).threads(1).resources(100).queue_size(0), // 8
    benchmark_args().sequences(100).threads(1).resources(10).queue_size(90), // 9
    benchmark_args().sequences(100).threads(1).resources(50).queue_size(50), // 10
    benchmark_args().sequences(100).threads(1).resources(90).queue_size(10), // 11
    benchmark_args().sequences(100).threads(2).resources(50).queue_size(50), // 12
    benchmark_args().sequences(1000).threads(1).resources(10).queue_size(990), // 13
    benchmark_args().sequences(1000).threads(2).resources(10).queue_size(990), // 14
    benchmark_args().sequences(10000).threads(1).resources(10).queue_size(9990), // 15
    benchmark_args().sequences(10000).threads(2).resources(10).queue_size(9990), // 16
}};

void get_auto_waste_callbacks_st(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    context<single_thread> ctx;
    benchmark_pool<stub_mutex> pool(args.resources(), args.queue_size());
    callback<single_thread> cb {ctx, pool};
    for (std::size_t i = 0; i < args.sequences(); ++i) {
        pool.get_auto_waste(ctx.io_context, cb, ctx.timeout);
    }
    while (state.KeepRunning()) {
        const auto ready_count = ctx.ready_count;
        do {
            ctx.io_context.run_one();
        } while (ready_count == ctx.ready_count);
    }
    ctx.finish();
}

struct thread_context {
    context<multi_thread> impl;
    std::thread thread;

    thread_context()
        : thread([this] { this->impl.io_context.run(); }) {}
};

void get_auto_waste_callbacks_mt(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    std::vector<std::unique_ptr<thread_context>> threads;
    for (std::size_t i = 0; i < args.threads(); ++i) {
        threads.emplace_back(std::make_unique<thread_context>());
    }
    benchmark_pool<> pool(args.resources(), args.queue_size());
    for (const auto& ctx : threads) {
        callback<multi_thread> cb {ctx->impl, pool};
        for (std::size_t i = 0; i < args.sequences(); ++i) {
            pool.get_auto_waste(ctx->impl.io_context, cb, ctx->impl.timeout);
        }
    }
    while (state.KeepRunning()) {
        std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->impl.wait_next(); });
    }
    std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->impl.finish(); });
    std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->thread.join(); });
}

void get_auto_waste_callbacks(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    if (args.threads() > 1) {
        get_auto_waste_callbacks_mt(state);
    } else {
        get_auto_waste_callbacks_st(state);
    }
}

void get_auto_waste_coroutines_st(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    context<single_thread> ctx;
    benchmark_pool<stub_mutex> pool(args.resources(), args.queue_size());
    for (std::size_t i = 0; i < args.sequences(); ++i) {
        boost::asio::spawn(ctx.io_context, [&] (boost::asio::yield_context yield) {
            static thread_local std::minstd_rand generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
            std::uniform_real_distribution<> distrubution(0, 1);
            constexpr const double recycle_probability = 0.5;
            while (!ctx.stop) {
                boost::system::error_code ec;
                auto handle = pool.get_auto_waste(ctx.io_context, yield[ec], ctx.timeout);
                if (!ec) {
                    if (handle.empty()) {
                        handle.reset(resource {});
                    }
                    benchmark::DoNotOptimize(++handle->value);
                    if (distrubution(generator) < recycle_probability) {
                        handle.recycle();
                    }
                    ctx.allow_next();
                }
            }
        });
    }
    while (state.KeepRunning()) {
        const auto ready_count = ctx.ready_count;
        do {
            ctx.io_context.run_one();
        } while (ready_count == ctx.ready_count);
    }
    ctx.finish();
}

void get_auto_waste_coroutines_mt(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    std::vector<std::unique_ptr<thread_context>> threads;
    for (std::size_t i = 0; i < args.threads(); ++i) {
        threads.emplace_back(std::make_unique<thread_context>());
    }
    benchmark_pool<> pool(args.resources(), args.queue_size());
    for (const auto& ctx : threads) {
        for (std::size_t i = 0; i < args.sequences(); ++i) {
            boost::asio::spawn(ctx->impl.io_context, [&] (boost::asio::yield_context yield) {
                static thread_local std::minstd_rand generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
                std::uniform_real_distribution<> distrubution(0, 1);
                constexpr const double recycle_probability = 0.5;
                while (!ctx->impl.stop) {
                    boost::system::error_code ec;
                    auto handle = pool.get_auto_waste(ctx->impl.io_context, yield[ec], ctx->impl.timeout);
                    if (!ec) {
                        if (handle.empty()) {
                            handle.reset(resource {});
                        }
                        benchmark::DoNotOptimize(++handle->value);
                        if (distrubution(generator) < recycle_probability) {
                            handle.recycle();
                        }
                        ctx->impl.allow_next();
                    }
                }
            });
        }
    }
    while (state.KeepRunning()) {
        std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->impl.wait_next(); });
    }
    std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->impl.finish(); });
    std::for_each(threads.begin(), threads.end(), [] (const auto& ctx) { ctx->thread.join(); });
}

void get_auto_waste_coroutines(benchmark::State& state) {
    const auto& args = benchmarks[static_cast<std::size_t>(state.range(0))];
    if (args.threads() > 1) {
        get_auto_waste_coroutines_st(state);
    } else {
        get_auto_waste_coroutines_mt(state);
    }
}

void all_benchmarks(benchmark::internal::Benchmark* b) {
    for (std::size_t n = 0; n < benchmarks.size(); ++n) {
        b->Arg(static_cast<int>(n));
    }
}

}

BENCHMARK(get_auto_waste_callbacks)->Apply(all_benchmarks);
BENCHMARK(get_auto_waste_coroutines)->Apply(all_benchmarks);

BENCHMARK_MAIN();
