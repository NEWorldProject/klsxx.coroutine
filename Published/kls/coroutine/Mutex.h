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

#include <mutex>
#include <atomic>
#include <cstdint>
#include "Executor.h"

namespace kls::coroutine {
    class Mutex;

    class MutexAcquire {
    public:
        explicit MutexAcquire(Mutex &mutex) noexcept: m_mutex(mutex) {}
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
        [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) noexcept;
        Mutex &await_resume() const noexcept { return m_mutex; } // NOLINT, can discard
    private:
        friend class Mutex;

        void resume() { if (m_exec) m_exec->enqueue(m_handle); else m_handle.resume(); }

        Mutex &m_mutex;
        MutexAcquire *m_next{};
        IExecutor *m_exec{this_executor()};
        std::coroutine_handle<> m_handle{};
    };

    class MutexLock {
    public:
        explicit MutexLock(Mutex &mutex, std::adopt_lock_t) noexcept: m_mutex(&mutex) {}
        MutexLock(MutexLock &&other) noexcept: m_mutex(other.m_mutex) { other.m_mutex = nullptr; }
        MutexLock(const MutexLock &other) = delete;
        MutexLock &operator=(const MutexLock &other) = delete;
        ~MutexLock();
    private:
        Mutex *m_mutex;
    };

    class ScopedMutexAcquire {
    public:
        explicit ScopedMutexAcquire(Mutex &mutex) noexcept: m_acquire(mutex) {}
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
        [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) noexcept { return m_acquire.await_suspend(h); }
        [[nodiscard]] MutexLock await_resume() const noexcept {
            return MutexLock{m_acquire.await_resume(), std::adopt_lock};
        }
    private:
        MutexAcquire m_acquire;
    };

    // This is basically the same stuff from cppcoro, except that we backed in the executor
    class Mutex {
    public:
        Mutex() noexcept;
        ~Mutex();
        bool try_lock() noexcept;
        MutexAcquire lock_async() noexcept { return MutexAcquire{*this}; }
        ScopedMutexAcquire scoped_lock_async() noexcept { return ScopedMutexAcquire{*this}; }
        void unlock();
    private:
        friend class MutexAcquire;

        static constexpr std::uintptr_t not_locked = 1;
        static constexpr std::uintptr_t locked_no_waiters = 0;

        // This field provides synchronisation for the mutex.
        //
        // It can have three kinds of values:
        // - not_locked
        // - locked_no_waiters
        // - a pointer to the head of a singly linked list of recently
        //   queued MutexAcquire objects. This list is
        //   in most-recently-queued order as new items are pushed onto
        //   the front of the list.
        std::atomic<std::uintptr_t> m_state;

        // Linked list of async lock operations that are waiting to acquire
        // the mutex. These operations will acquire the lock in the order
        // they appear in this list. Waiters in this list will acquire the
        // mutex before waiters added to the m_newWaiters list.
        MutexAcquire *m_waiters;
    };
}
