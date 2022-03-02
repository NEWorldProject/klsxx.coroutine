#pragma once

#include <atomic>
#include <memory>
#include "CoroDetail.h"
#include "kls/essential/Final.h"

namespace kls::coroutine::detail::value_async {
    class StateLifeCycleControl {
        inline static void *INVALID_PTR = std::bit_cast<void *>(~uintptr_t(0));
    public:
        void ClientRelease() {
            const auto address = mAddress.exchange(INVALID_PTR);
            if (address) std::coroutine_handle<>::from_address(address).destroy();
        }

        bool RoutineRelease(std::coroutine_handle<> h) {
            const auto address = h.address();
            for (;;) {
                auto val = mAddress.load();
                if (val == INVALID_PTR) return false; // client state released, continue final
                if (mAddress.compare_exchange_weak(val, address)) return true;
                if (val != INVALID_PTR && val != nullptr) std::abort();
            }
        }
    private:
        std::atomic<void*> mAddress{ nullptr };
    };

    class ContinuationControl: public StateLifeCycleControl {
        inline static AwaitCore *INVALID_PTR = std::bit_cast<AwaitCore *>(~uintptr_t(0));
    public:
        bool Transit(AwaitCore *next) {
            const auto exec = CurrentExecutor();
            for (;;) {
                auto val = mNext.load();
                // if the state has been finalized, direct dispatch
                if (val == INVALID_PTR) {
                    if (next->CanExecInPlace(exec)) return false;
                    return (next->Dispatch(), true);
                }
                if (val) std::abort();
                if (mNext.compare_exchange_weak(val, next)) return true;
            }
        }

        void DispatchContinuation() { if (auto ths = mNext.exchange(INVALID_PTR); ths) ths->Dispatch(); }
    private:
        std::atomic<AwaitCore *> mNext{nullptr};
    };

    struct StateLifeCycleRoutineFinalAwait final {
        StateLifeCycleControl& Control;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        bool await_suspend(std::coroutine_handle<> h) noexcept { return Control.RoutineRelease(h); }

        constexpr void await_resume() const noexcept {}
    };

    template<class T>
    using ValueContinuableValueMedia = ContinuableValueMedia<T, ContinuationControl>;

    template<class T>
    struct ValuePromiseValueMedia : public ValueContinuableValueMedia<T> {
        template<class ...U>
        void return_value(U &&... v) { ValueContinuableValueMedia<T>::Set(std::forward<U>(v)...); }

        void unhandled_exception() { ValueContinuableValueMedia<T>::Fail(); }
    };

    template<>
    struct ValuePromiseValueMedia<void> : public ValueContinuableValueMedia<void> {
        void return_void() { ValueContinuableValueMedia<void>::Set(); }

        void unhandled_exception() { ValueContinuableValueMedia<void>::Fail(); }
    };
}

namespace kls::coroutine {
    template<class T>
    class ValueAsync {
        using Media = detail::value_async::ValueContinuableValueMedia<T>;
        using PromiseMedia = detail::value_async::ValuePromiseValueMedia<T>;

        class ValueAwaitCore : detail::AwaitCore {
        public:
            explicit ValueAwaitCore(Media* media) : mMedia(media) {}

            ValueAwaitCore(Media* media, IExecutor* next) noexcept : AwaitCore(next), mMedia(media) {}

            bool Transit(std::coroutine_handle<> h) {
                SetHandle(h);
                return mMedia->Transit(this);
            }

            T Get() {
                const auto scope = ScopeGuard([m = mMedia]() noexcept { m->ClientRelease(); });
                return mMedia->Get();
            }
        private:
            Media* mMedia;
        };
    public:
        using Await = detail::Await<ValueAwaitCore>;

        struct promise_type : public PromiseMedia {
            ValueAsync get_return_object() { return ValueAsync(this); }

            constexpr auto initial_suspend() noexcept { return std::suspend_never{}; }

            auto final_suspend() noexcept { return detail::value_async::StateLifeCycleRoutineFinalAwait{ *this }; }
        };

        constexpr ValueAsync() noexcept = default;

        ValueAsync(ValueAsync&& other) noexcept : mMedia(std::exchange(other.mMedia, nullptr)) {}

        ValueAsync(const ValueAsync& other) = delete;

        ValueAsync& operator=(ValueAsync&& other) noexcept {
            this->~ValueAsync();
            mMedia = std::exchange(other.mMedia, nullptr);
            return *this;
        }

        ValueAsync& operator=(const ValueAsync& other) = delete;

        ~ValueAsync() noexcept { if (mMedia) { mMedia->ClientRelease(); } }

        auto operator co_await()&& { return Await(std::exchange(mMedia, nullptr)); }

        auto Configure(IExecutor* next)&& { return Await(std::exchange(mMedia, nullptr), next); }

    private:
        Media* mMedia{ nullptr };

        explicit ValueAsync(Media* state) noexcept : mMedia(state) {}
    };
}
