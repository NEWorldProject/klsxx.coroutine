#pragma once

#include <memory>
#include <iterator>
#include "Trigger.h"
#include "ValueStore.h"

namespace kls::coroutine {
    template<class T>
    class AsyncGenerator {
    public:
        class promise_type {
        public:
            promise_type() noexcept = default;
            ~promise_type() noexcept = default;
            auto get_return_object() noexcept { return AsyncGenerator(this); }
            auto initial_suspend() noexcept { return std::suspend_always{}; }
            auto final_suspend() noexcept { return std::suspend_always{}; }
            void unhandled_exception() { m_store.fail(std::current_exception()); }
            bool trap(coroutine::ExecutorAwaitEntry& h) { m_trigger.trap(h); }
            decltype(auto) get() { return m_store.get(); }
            void return_void() {}

            auto yield_value(T&& ref) noexcept(std::is_nothrow_move_constructible_v<T>) {
                return m_store.set(std::forward<T>(ref)), m_trigger.pull(), std::suspend_always{};
            }

            void restart_async() {
                std::destroy_at(&m_store), std::construct_at(&m_store);
                std::destroy_at(&m_trigger), std::construct_at(&m_trigger);
                auto handle = handle_t::from_address(this);
                if (m_exec) m_exec->enqueue(handle); else handle.resume();
            }
        private:
            coroutine::ValueStore<T> m_store;
            coroutine::SingleExecutorTrigger m_trigger;
            coroutine::IExecutor *m_exec = coroutine::this_executor();
        };

        using handle_t = std::coroutine_handle<promise_type>;

        struct sentinel {
        };

        class reference: public AddressSensitive {
        public:
            explicit reference(handle_t h) noexcept: m_handle(h) {}
            [[nodiscard]] constexpr bool await_ready() && noexcept { return false; }
            [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) && {
                if (!m_handle) throw std::logic_error("async generator reference should only be awaited once");
                return m_entry.set_handle(h), m_handle.promise().trap(m_entry);
            }
            [[nodiscard]] constexpr T await_resume() && { return std::exchange(m_handle, {}).promise().get(); }
        private:
            handle_t m_handle;
            coroutine::ExecutorAwaitEntry m_entry;
        };

        class iterator {
        public:
            using reference = reference;
            using value_type = T;
            using pointer = std::add_pointer_t<T>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::input_iterator_tag;

            iterator() noexcept = default;
            explicit iterator(handle_t h) noexcept: m_handle(h) {}
            reference operator*() const { return reference(m_handle); }
            iterator &operator++() { return m_handle.promise().restart_async(), *this; }
            void operator++(int) { m_handle.promise().restart_async(); }

            friend bool operator==(const iterator &it, sentinel) noexcept { return it.m_handle.done(); }
            friend bool operator==(sentinel, const iterator &it) noexcept { return it.m_handle.done(); }
            friend bool operator!=(const iterator &it, sentinel) noexcept { return !it.m_handle.done(); }
            friend bool operator!=(sentinel, const iterator &it) noexcept { return !it.m_handle.done(); }
        private:
            handle_t m_handle;
        };

        AsyncGenerator(AsyncGenerator &&g) noexcept: m_handle(std::exchange(g.m_handle, {})) {}
        ~AsyncGenerator() { if (m_handle) m_handle.destroy(); }

        iterator begin() {
            m_handle.resume();
            return iterator{m_handle};
        }

        sentinel end() { return {}; }
    private:
        explicit AsyncGenerator(promise_type* promise) noexcept: m_handle(handle_t::from_promise(promise)) {}
        handle_t m_handle;
    };
}

