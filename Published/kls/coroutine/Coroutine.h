#pragma once

#include "FlexAsync.h"
#include "ValueAsync.h"

namespace kls::coroutine {
    class SwitchTo {
    public:
        explicit SwitchTo(IExecutor* next) noexcept : mNext(next) {}

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { mNext->Enqueue(handle); }

        constexpr void await_resume() noexcept {}
    private:
        IExecutor* mNext;
    };

    struct Redispatch {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { CurrentExecutor()->Enqueue(handle); }

        constexpr void await_resume() noexcept {}
    };

    template <class ...U>
    ValueAsync<void> Await(U&&... c) { (..., co_await std::move(c)); }

    template <class Container>
    ValueAsync<void> AwaitAll(Container c) { for (auto&& x : c) co_await std::move(x); }
}
