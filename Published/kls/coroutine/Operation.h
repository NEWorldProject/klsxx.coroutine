/*
* Copyright (c) 2022 DWVoid and Infinideastudio Team
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "Async.h"
#include "Traits.h"
#include "Blocking.h"

namespace kls::coroutine {
    class SwitchTo {
    public:
        explicit SwitchTo(IExecutor *next) noexcept: mNext(next) {}

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { mNext->enqueue(handle); }

        constexpr void await_resume() noexcept {}
    private:
        IExecutor *mNext;
    };

    struct Redispatch {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { this_executor()->enqueue(handle); }

        constexpr void await_resume() noexcept {}
    };

    template<class U, class Fn>
    requires requires(U &o) {
        { o.close() };
        std::is_nothrow_move_constructible_v<U>;
    }
    ValueAsync<awaitable_result_t<std::invoke_result_t<Fn, decltype(std::declval<U &>())>>> uses(U &obj, Fn fn) {
        try {
            if constexpr(std::is_same_v<
                    void,
                    awaitable_result_t<std::invoke_result_t<Fn, decltype(std::declval<U &>())>>
            >) {
                co_await fn(obj);
                co_await obj.close();
            } else {
                auto ret = co_await fn(obj);
                co_await obj.close();
                co_return ret;
            }
        }
        catch (...) {
            run_blocking([&]() -> ValueAsync<> { co_await obj.close(); });
            throw;
        }
    }

    template<class U, class Fn>
    requires requires(U o) {
        { o.operator*() };
        { o.operator->() };
        { o->close() };
    }
    ValueAsync<awaitable_result_t<std::invoke_result_t<Fn, decltype(*std::declval<U>())>>> uses(U &obj, Fn fn) {
        try {
            if constexpr(std::is_same_v<
                    void,
                    awaitable_result_t<std::invoke_result_t<Fn, decltype(*std::declval<U>())>>
            >) {
                co_await fn(*obj);
                co_await obj->close();
            } else {
                auto ret = co_await fn(*obj);
                co_await obj->close();
                co_return ret;
            }
        }
        catch (...) {
            run_blocking([&]() -> ValueAsync<> { co_await obj->close(); });
            throw;
        }
    }

    namespace detail {
        template<class ...U>
        ValueAsync<void> awaits_impl(U... c) { (..., co_await std::move(c)); }
    }

    template<class ...U>
    ValueAsync<void> awaits(U &&... c) { return detail::awaits_impl<std::decay_t<U>...>(std::forward<U>(c)...); }

    template<class Container>
    ValueAsync<void> await_all(Container c) { for (auto &&x: c) co_await std::move(x); }
}
