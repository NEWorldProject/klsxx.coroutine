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

#include "Trigger.h"
#include "ValueStore.h"

namespace kls::coroutine {
    template <class T, class Trigger, class Ext>
    class FuturePromise;

    template <class T, class Trigger>
    class FuturePromise<T, Trigger, void> final {
    public:
        template<class ...U>
        void set(U &&... v) { (m_storage.set(std::forward<U>(v)...), m_trigger.pull()); }
        void fail(std::exception_ptr e = std::current_exception()) { (m_storage.fail(e), m_trigger.pull()); }
        T get() { return m_storage.get(); }
        decltype(auto) ref() { return m_storage.ref(); }
        decltype(auto) copy() { return m_storage.copy(); }
        auto& trigger() noexcept { return m_trigger; }
    private:
        ValueStore<T> m_storage;
        Trigger m_trigger;
    };

    template <class T, class Trigger, class Ext>
    class FuturePromise final {
    public:
        template<class ...U>
        void set(U &&... v) { (m_storage.set(std::forward<U>(v)...), m_trigger.pull()); }
        void fail(std::exception_ptr e = std::current_exception()) { (m_storage.fail(e), m_trigger.pull()); }
        T get() { return m_storage.get(); }
        decltype(auto) ref() { return m_storage.ref(); }
        decltype(auto) copy() { return m_storage.copy(); }
        Ext* extension() const noexcept { return &m_extension.value; }
        auto& trigger() noexcept { return m_trigger; }
    private:
        Storage<Ext> m_extension;
        ValueStore<T> m_storage;
        Trigger m_trigger;
    };

    template <class T = void, class Ext = void>
    class FlexFuture {
    public:
        using PromiseType = FuturePromise<T, FifoExecutorTrigger, Ext>;
        using PromiseHandle = std::shared_ptr<PromiseType>;

        class Await: public AddressSensitive {
        public:
            explicit Await(FlexFuture& flex) noexcept : m_promise(flex.m_promise) {}
            explicit Await(FlexFuture&& flex) noexcept : m_promise(std::move(flex.m_promise)) {}
            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
            T await_resume() noexcept { return m_promise->copy(); }
            bool await_suspend(std::coroutine_handle<> h) {
                return (m_entry.set_handle(h), m_promise->trigger().trap(m_entry));
            }
        private:
            PromiseHandle m_promise;
            FifoExecutorAwaitEntry m_entry {};
        };

        template<class Fn>
        requires requires(PromiseHandle t, Fn f) { f(t); }
        explicit FlexFuture(Fn fn): m_promise{std::make_shared<PromiseType>()} { fn(PromiseHandle(m_promise)); }

        Await operator co_await() & noexcept { return Await(*this); }
        Await operator co_await() && noexcept { return Await(std::move(*this)); }
    private:
        PromiseHandle m_promise;
    };

    template <class T = void, class Ext = void>
    class ValueFuture: public AddressSensitive {
    public:
        using PromiseType = FuturePromise<T, SingleExecutorTrigger, Ext>;
        using PromiseHandle = PromiseType*;

        template<class Fn>
        requires requires(PromiseHandle t, Fn f) { f(t); }
        explicit ValueFuture(Fn fn) { fn(&m_promise); }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        bool await_suspend(std::coroutine_handle<> h) {
            return (m_entry.set_handle(h), m_promise.trigger().trap(m_entry));
        }

        T await_resume() noexcept { return m_promise.get(); }
    private:
        ExecutorAwaitEntry m_entry {};
        PromiseType m_promise {};
    };

    template <class T = void, class Ext = void>
    class LazyFuture: public AddressSensitive {
    public:
        using PromiseType = FuturePromise<T, SingleExecutorTrigger, Ext>;
        using PromiseHandle = PromiseType*;

        template<class Fn>
        requires requires(PromiseHandle t, Fn f) { f(t); }
        explicit LazyFuture(Fn fn) { fn(&m_promise); }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        bool await_suspend(std::coroutine_handle<> h) {
            return (m_entry.set_handle(h), m_promise.trigger().trap(m_entry));
        }

        decltype(auto) await_resume() noexcept { return m_promise.ref(); }
    private:
        ExecutorAwaitEntry m_entry {};
        PromiseType m_promise {};
    };
}