// FEAViz C++ binding - parallel runtime.
//
// RAII wrappers over the FEAViz executor/future and the parallel-for runtime.
// FVizExecutor owns a worker pool; FVizFuture tracks one task with
// cancellation, progress and value transfer.

#ifndef FVIZ_CPP_PARALLEL_HPP
#define FVIZ_CPP_PARALLEL_HPP

#include <FViz/Parallel/FVizExecutor.h>
#include <FViz/Parallel/FVizParallel.h>

#include "FVizCppObject.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace fviz {

// ---------------------------------------------------------------------------
// CancellationToken - cooperative cancellation flag.
// ---------------------------------------------------------------------------
class CancellationToken {
public:
    CancellationToken() = default;
    explicit CancellationToken(FVizCancellationToken* owned) : token_(owned) {}
    ~CancellationToken()
    {
        if (token_) fviz_cancellation_token_destroy(token_);
    }
    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;
    CancellationToken(CancellationToken&& other) noexcept : token_(other.token_) { other.token_ = nullptr; }
    CancellationToken& operator=(CancellationToken&& other) noexcept
    {
        if (this != &other)
        {
            if (token_) fviz_cancellation_token_destroy(token_);
            token_ = other.token_;
            other.token_ = nullptr;
        }
        return *this;
    }

    static CancellationToken create()
    {
        FVizCancellationToken* token = nullptr;
        detail::checkResult(fviz_cancellation_token_create(&token));
        return CancellationToken(token);
    }

    void cancel() noexcept { if (token_) fviz_cancellation_token_cancel(token_); }
    void reset() noexcept { if (token_) fviz_cancellation_token_reset(token_); }
    bool isCancelled() const noexcept { return token_ ? fviz_cancellation_token_is_cancelled(token_) != FVIZ_FALSE : false; }
    FVizCancellationToken* get() const noexcept { return token_; }

private:
    FVizCancellationToken* token_ = nullptr;
};

// ---------------------------------------------------------------------------
// Future - an async task submitted to an executor.
// ---------------------------------------------------------------------------
class Future {
public:
    Future() = default;
    explicit Future(FVizFuture* owned) : future_(owned) {}
    ~Future()
    {
        if (future_) fviz_future_destroy(future_);
    }
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&& other) noexcept : future_(other.future_) { other.future_ = nullptr; }
    Future& operator=(Future&& other) noexcept
    {
        if (this != &other)
        {
            if (future_) fviz_future_destroy(future_);
            future_ = other.future_;
            other.future_ = nullptr;
        }
        return *this;
    }

    bool ready() const noexcept { return future_ ? fviz_future_ready(future_) != FVIZ_FALSE : false; }
    double progress() const noexcept { return future_ ? fviz_future_progress(future_) : 0.0; }
    void cancel() noexcept { if (future_) fviz_future_cancel(future_); }
    void wait() { detail::checkResult(fviz_future_wait(future_)); }

    // Transfers the task value exactly once after success.
    void* takeValue()
    {
        void* value = nullptr;
        detail::checkResult(fviz_future_take_value(future_, &value));
        return value;
    }

    FVizFuture* get() const noexcept { return future_; }

private:
    FVizFuture* future_ = nullptr;
};

// ---------------------------------------------------------------------------
// Executor - a worker pool.
// ---------------------------------------------------------------------------
class Executor {
public:
    Executor() = default;
    explicit Executor(FVizExecutor* owned) : executor_(owned) {}
    ~Executor()
    {
        if (executor_) fviz_executor_destroy(executor_);
    }
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    Executor(Executor&& other) noexcept : executor_(other.executor_) { other.executor_ = nullptr; }
    Executor& operator=(Executor&& other) noexcept
    {
        if (this != &other)
        {
            if (executor_) fviz_executor_destroy(executor_);
            executor_ = other.executor_;
            other.executor_ = nullptr;
        }
        return *this;
    }

    // Default thread count (system limit) and queue capacity (1024).
    static Executor createDefault()
    {
        FVizExecutorOptions options;
        fviz_executor_options_initialize(&options);
        FVizExecutor* executor = nullptr;
        detail::checkResult(fviz_executor_create(&options, &executor));
        return Executor(executor);
    }

    static Executor create(uint32_t thread_count, FVizSize queue_capacity = 0u)
    {
        FVizExecutorOptions options;
        fviz_executor_options_initialize(&options);
        options.thread_count = thread_count;
        options.queue_capacity = queue_capacity;
        FVizExecutor* executor = nullptr;
        detail::checkResult(fviz_executor_create(&options, &executor));
        return Executor(executor);
    }

    // Submits a task; fn returns FVizResult. Returns a Future owning the task.
    template <typename Fn>
    Future submit(Fn&& fn)
    {
        auto state = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(fn));
        auto holder = new std::shared_ptr<std::decay_t<Fn>>(state);
        auto task_fn = [](FVizCancellationToken*, void* user_data, void** out_value) -> FVizResult {
            auto* shared = static_cast<std::shared_ptr<std::decay_t<Fn>>*>(user_data);
            return (*shared)();
        };
        auto destroy = [](void* user_data) {
            delete static_cast<std::shared_ptr<std::decay_t<Fn>>*>(user_data);
        };
        FVizFuture* future = nullptr;
        detail::checkResult(fviz_executor_submit(executor_, 0, task_fn, holder, destroy, nullptr, &future));
        return Future(future);
    }

    void getStatistics(FVizExecutorStatistics& out_statistics) const noexcept
    {
        (void)memset(&out_statistics, 0, sizeof(out_statistics));
        if (executor_) fviz_executor_get_statistics(executor_, &out_statistics);
    }

    FVizExecutor* get() const noexcept { return executor_; }

private:
    FVizExecutor* executor_ = nullptr;
};

// ---------------------------------------------------------------------------
// Parallel helpers.
// ---------------------------------------------------------------------------
namespace parallel {

inline uint32_t hardwareThreadCount() noexcept { return fviz_parallel_hardware_thread_count(); }
inline uint32_t threadLimit() noexcept { return fviz_parallel_thread_limit(); }
inline void setThreadLimit(uint32_t limit) noexcept { fviz_parallel_set_thread_limit(limit); }

// Runs fn(begin, end) over [begin, end) in chunks. fn must return FVizResult.
template <typename Fn>
void forEach(FVizSize begin, FVizSize end, FVizSize grain_size, Fn&& fn)
{
    struct Callback {
        Fn fn;
        FVizResult operator()(FVizSize b, FVizSize e, void*) { return fn(b, e); }
    };
    auto* callback = new Callback{std::forward<Fn>(fn)};
    auto trampoline = [](FVizSize b, FVizSize e, void* user_data) {
        auto* cb = static_cast<Callback*>(user_data);
        cb->fn(b, e);
    };
    const FVizResult result = fviz_parallel_for(begin, end, grain_size, trampoline, callback);
    delete callback;
    detail::checkResult(result);
}

} // namespace parallel

} // namespace fviz

#endif // FVIZ_CPP_PARALLEL_HPP
