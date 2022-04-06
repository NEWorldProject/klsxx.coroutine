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

#include <memory>
#include <iterator>
#include "Trigger.h"
#include "ValueStore.h"
#include "kls/essential/Final.h"

namespace kls::coroutine {
    template<class T>
    class AsyncGenerator {
    public:
        class promise_type {
        public:
            promise_type() noexcept = default;
            ~promise_type() noexcept = default;

            // coroutine lifetime promise functions
            auto get_return_object() noexcept { return AsyncGenerator(this); }
            auto initial_suspend() noexcept { return std::suspend_never{}; }
            auto final_suspend() noexcept { return std::suspend_always{}; }
            void unhandled_exception() { m_future.fail(std::current_exception()); }
            void return_void() { m_future.set(), m_trigger.pull(); }

            // value yielding for generator
            decltype(auto) get_value() { return m_yield.get(); }
            auto yield_value(T &&ref) noexcept(std::is_nothrow_move_constructible_v<T>) {
                return m_yield.set(std::forward<T>(ref)), m_trigger.pull(), std::suspend_always{};
            }

            // continuation
            bool trap(coroutine::ExecutorAwaitEntry &h) { return m_trigger.trap(h); }
            void reset() { m_yield.reset(), std::destroy_at(&m_trigger), std::construct_at(&m_trigger); }
            void resume() { if (auto h = handle_t::from_promise(*this); m_exec) m_exec->enqueue(h); else h.resume(); }
            void final() { m_future.get(); }
        private:
            coroutine::ValueStore<T> m_yield;
            coroutine::FutureStore<void> m_future;
            coroutine::SingleExecutorTrigger m_trigger;
            coroutine::IExecutor *m_exec = coroutine::this_executor();
        };

        using handle_t = std::coroutine_handle<promise_type>;

        class forward_await : public AddressSensitive {
        public:
            explicit forward_await(promise_type *p) noexcept: m_p(*p) {}
            [[nodiscard]] constexpr bool await_ready() noexcept { return false; }
            [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) { return m_e.set_handle(h), m_p.trap(m_e); }
            [[nodiscard]] bool await_resume() {
                const auto done = handle_t::from_promise(m_p).done();
                if (done) m_p.final(), handle_t::from_promise(m_p).destroy(); else m_p.reset();
                return !done;
            }
        private:
            promise_type &m_p;
            coroutine::ExecutorAwaitEntry m_e;
        };

        [[nodiscard]] auto forward() noexcept { return forward_await(m_promise); }
        [[nodiscard]] decltype(auto) next() {
            essential::Final final{[this]() { m_promise->resume(); }};
            return m_promise->get_value();
        }
    private:
        explicit AsyncGenerator(promise_type *promise) noexcept: m_promise(promise) {}
        promise_type *m_promise;
    };
}

