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

#include <exception>
#include "Trigger.h"
#include "Executor.h"

namespace kls::coroutine {
    template<class T>
    class ValueStore;

    template<>
    class ValueStore<void> {
    public:
        void set() {}
        void fail(std::exception_ptr e) { m_fail = std::move(e); }
        void get() { if (m_fail) std::rethrow_exception(m_fail); }
        void ref() { get(); }
        void copy() { get(); }
    private:
        std::exception_ptr m_fail{nullptr}; //NOLINT
    };

    template<class T>
    class ValueStore {
    public:
        ValueStore() noexcept = default;
        ~ValueStore() { if (!m_fail) std::destroy_at(&m_store.value); }
        void fail(std::exception_ptr e) { m_fail = std::move(e); }
        template<class ...U> requires requires(U ...v) { sizeof...(v) == 1; }
        void set(U &&... v) { std::construct_at(&m_store.value, std::forward<U>(v)...); }
        T&& get() { if (m_fail) std::rethrow_exception(m_fail); else return std::move(m_store.value); }
        T& ref() { if (m_fail) std::rethrow_exception(m_fail); else return m_store.value; }
        T copy() { if (m_fail) std::rethrow_exception(m_fail); else return m_store.value; }
    private:
        std::exception_ptr m_fail{nullptr}; //NOLINT
        Storage<T> m_store;
    };

    template<class T, class ContControl>
    class ContinuableValueMedia : public ContControl {
    public:
        template<class ...U>
        explicit ContinuableValueMedia(U &&... v): ContControl(std::forward<U>(v)...) {}
        template<class ...U>
        void set(U &&... v) { m_storage.set(std::forward<U>(v)...), ContControl::resume(); }
        void fail() { m_storage.fail(std::current_exception()), ContControl::resume(); }
        T get() { return m_storage.get(); }
        decltype(auto) ref() { return m_storage.ref(); }
        decltype(auto) copy() { return m_storage.copy(); }
    private:
        ValueStore<T> m_storage;
    };

    // helper class for generating actual await client class.
    // the 'Core' class should have a trap(coroutine_handle<>) and a get() function
    template <class Core>
    struct Await: private Core {
        template<class ...U>
        explicit Await(U &&... v): Core(std::forward<U>(v)...) {}

        // the short-path check should be done in Core::Transit
        // this is done to make sure that the state transition is properly handled
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
        [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) { return Core::trap(h); }
        decltype(auto) await_resume() { return Core::get(); }
    };
}

namespace kls::coroutine::detail {
    struct FlexAsyncControl : private FifoExecutorTrigger {
        bool trap(FifoExecutorAwaitEntry *next) noexcept { return FifoExecutorTrigger::trap(*next); }
        void resume() noexcept { FifoExecutorTrigger::pull(); }
    };

    template<class T>
    using FlexAsyncValueMedia = ContinuableValueMedia<T, FlexAsyncControl>;

    template<class T>
    using FlexAsyncValueMediaHandle = std::shared_ptr<FlexAsyncValueMedia<T>>;

    template<class T>
    class FlexAsyncPromiseValueMedia {
    public:
        explicit FlexAsyncPromiseValueMedia(FlexAsyncValueMediaHandle<T> h) noexcept: m_handle(std::move(h)) {}
        template<class ...U>
        void return_value(U &&... v) { m_handle->set(std::forward<U>(v)...); }
        void unhandled_exception() { m_handle->fail(); }
    private:
        FlexAsyncValueMediaHandle<T> m_handle;
    };

    template<>
    class FlexAsyncPromiseValueMedia<void> {
    public:
        explicit FlexAsyncPromiseValueMedia(FlexAsyncValueMediaHandle<void> h) noexcept: m_handle(std::move(h)) {}
        void return_void() { m_handle->set(); }
        void unhandled_exception() { m_handle->fail(); }
    private:
        FlexAsyncValueMediaHandle<void> m_handle;
    };
}

namespace kls::coroutine {
    template<class T>
    class FlexAsync {
        using State = detail::FlexAsyncValueMedia<T>;
        using StateHandle = detail::FlexAsyncValueMediaHandle<T>;
        using PromiseMedia = detail::FlexAsyncPromiseValueMedia<T>;

        class FlexAwaitCore : ExecutorAwaitEntry {
        public:
            explicit FlexAwaitCore(StateHandle state) : m_state(state) {}
            FlexAwaitCore(StateHandle state, IExecutor *next) noexcept: ExecutorAwaitEntry(next), m_state(state) {}
            bool trap(std::coroutine_handle<> h) { return (set_handle(h), m_state->trap(this)); }
            T get() { return m_state->copy(); }
        private:
            StateHandle m_state;
        };
    public:
        using MyAwait = Await<FlexAwaitCore>;

        class promise_type : public PromiseMedia {
            static StateHandle make_state() noexcept { return std::make_shared<State>(); }
        public:
            promise_type() : PromiseMedia(make_state()) {}
            FlexAsync get_return_object() { return FlexAsync(this); }
            constexpr std::suspend_never initial_suspend() noexcept { return {}; }
            constexpr std::suspend_never final_suspend() noexcept { return {}; }
        };

        FlexAsync(FlexAsync &&) noexcept = default;
        FlexAsync(const FlexAsync &) noexcept = default;
        FlexAsync &operator=(FlexAsync &&) noexcept = default;
        FlexAsync &operator=(const FlexAsync &) noexcept = default;
        auto operator co_await()&& { return MyAwait(std::move(m_state)); }
        auto operator co_await() const& { return MyAwait(m_state); }
        auto configure(IExecutor *next) &&{ return MyAwait(std::move(m_state), next); }
        auto configure(IExecutor *next) const &{ return MyAwait(m_state, next); }
    private:
        StateHandle m_state;

        explicit FlexAsync(std::shared_ptr<State> state) noexcept: m_state(std::move(state)) {}
    };
}

namespace kls::coroutine::detail {
    struct LazyAsyncControl : private FifoExecutorTrigger {
        bool trap(FifoExecutorAwaitEntry *next) noexcept { return FifoExecutorTrigger::trap(*next); }
        void resume() noexcept { FifoExecutorTrigger::pull(); }
    };

    template<class T>
    using LazyAsyncValueMedia = ContinuableValueMedia<T, LazyAsyncControl>;

    template<class T>
    using LazyAsyncValueMediaHandle = LazyAsyncValueMedia<T>*;

    template<class T>
    class LazyAsyncPromiseValueMedia {
    public:
        LazyAsyncPromiseValueMedia() = default;
        template<class ...U>
        void return_value(U &&... v) { m_handle->set(std::forward<U>(v)...); }
        void unhandled_exception() { m_handle->fail(); }
    protected:
        LazyAsyncValueMediaHandle<T> m_handle{nullptr};
    };

    template<>
    class LazyAsyncPromiseValueMedia<void> {
    public:
        LazyAsyncPromiseValueMedia() = default;
        void return_void() { m_handle->set(); }
        void unhandled_exception() { m_handle->fail(); }
    protected:
        LazyAsyncValueMediaHandle<void> m_handle{nullptr};
    };
}

namespace kls::coroutine {
    template<class T>
    class LazyAsync {
        using State = detail::LazyAsyncValueMedia<T>;
        using StateHandle = detail::LazyAsyncValueMediaHandle<T>;
        using PromiseMedia = detail::LazyAsyncPromiseValueMedia<T>;

        class LazyAwaitCore : ExecutorAwaitEntry {
        public:
            explicit LazyAwaitCore(StateHandle state) : m_state(state) {}
            LazyAwaitCore(StateHandle state, IExecutor* next) noexcept : ExecutorAwaitEntry(next), m_state(state) {}
            bool trap(std::coroutine_handle<> h) { return (set_handle(h), m_state->trap(this)); }
            decltype(auto) get() { return m_state->ref(); }
        private:
            StateHandle m_state;
        };
    public:
        using MyAwait = Await<LazyAwaitCore>;

        struct promise_type : PromiseMedia {
            promise_type() = default;
            constexpr std::suspend_never initial_suspend() noexcept { return {}; }
            constexpr std::suspend_never final_suspend() noexcept { return {}; }
            LazyAsync get_return_object() noexcept {
                return {[this](StateHandle s) noexcept { PromiseMedia::m_handle = s; }};
            }
        };

        auto operator co_await() { return MyAwait(m_state); }
        auto configure(IExecutor* next) { return MyAwait(m_state, next); }
    private:
        State m_state;

        template<class Fn> requires requires(StateHandle h, Fn fn) { fn(h); }
        explicit LazyAsync(Fn fn) noexcept { fn(&m_state); }
    };
}

namespace kls::coroutine::detail {
    struct ValueAsyncControl: AddressSensitive {
        void resume() noexcept { m_state.pull(); }
        bool trap(ExecutorAwaitEntry *next) noexcept { return m_state.trap(*next); }
        void drop_task() noexcept { m_lifecycle.drop(); }
        bool drop_host(std::coroutine_handle<> h) noexcept { return m_lifecycle.trap(h); }
    private:
        SingleTrigger m_lifecycle{};
        SingleExecutorTrigger m_state{};
    };

    struct ValueAsyncFinalAwait final {
        ValueAsyncControl& control;
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> h) noexcept { return control.drop_host(h); }
        constexpr void await_resume() const noexcept {}
    };

    template<class T>
    using ValueAsyncValueMedia = ContinuableValueMedia<T, ValueAsyncControl>;

    template<class T>
    struct ValueAsyncPromiseValueMedia : public ValueAsyncValueMedia<T> {
        template<class ...U>
        void return_value(U &&... v) { ValueAsyncValueMedia<T>::set(std::forward<U>(v)...); }
        void unhandled_exception() { ValueAsyncValueMedia<T>::fail(); }
    };

    template<>
    struct ValueAsyncPromiseValueMedia<void> : public ValueAsyncValueMedia<void> {
        void return_void() { ValueAsyncValueMedia<void>::set(); }
        void unhandled_exception() { ValueAsyncValueMedia<void>::fail(); }
    };
}

namespace kls::coroutine {
    template<class T>
    class ValueAsync {
        using Media = detail::ValueAsyncValueMedia<T>;
        using PromiseMedia = detail::ValueAsyncPromiseValueMedia<T>;

        class ValueAwaitCore : ExecutorAwaitEntry {
        public:
            explicit ValueAwaitCore(Media* media) : mMedia(media) {}
            ValueAwaitCore(Media* media, IExecutor* next) noexcept : ExecutorAwaitEntry(next), mMedia(media) {}
            ~ValueAwaitCore() { mMedia->drop_task(); }
            T get() { return mMedia->get(); }
            bool trap(std::coroutine_handle<> h) { return (set_handle(h), mMedia->trap(this)); }
        private:
            Media* mMedia;
        };
    public:
        using MyAwait = Await<ValueAwaitCore>;

        struct promise_type : public PromiseMedia {
            ValueAsync get_return_object() { return ValueAsync(this); }
            constexpr std::suspend_never initial_suspend() noexcept { return {}; }
            auto final_suspend() noexcept { return detail::ValueAsyncFinalAwait{ *this }; }
        };

        constexpr ValueAsync() noexcept = default;
        ValueAsync(ValueAsync&& o) noexcept : mMedia(std::exchange(o.mMedia, nullptr)) {}
        ValueAsync(const ValueAsync& o) = delete;
        ValueAsync& operator=(ValueAsync&& o) noexcept { return delegate_to_move_construct(this, std::move(o)); } // NOLINT
        ValueAsync& operator=(const ValueAsync& other) = delete;
        ~ValueAsync() noexcept { if (mMedia) { mMedia->drop_task(); } }
        auto operator co_await()&& { return MyAwait(std::exchange(mMedia, nullptr)); }
        auto configure(IExecutor* next)&& { return MyAwait(std::exchange(mMedia, nullptr), next); }
    private:
        Media* mMedia{ nullptr };

        explicit ValueAsync(Media* state) noexcept : mMedia(state) {}
    };
}
