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

#include <cassert>
#include "kls/coroutine/Mutex.h"

namespace kls::coroutine {
    Mutex::Mutex() noexcept: m_state(not_locked), m_waiters(nullptr) {}

    Mutex::~Mutex() {
        [[maybe_unused]] auto state = m_state.load(std::memory_order_relaxed);
        assert(state == not_locked || state == locked_no_waiters);
        assert(m_waiters == nullptr);
    }

    bool Mutex::try_lock() noexcept {
        // Try to atomically transition from nullptr (not-locked) -> this (locked-no-waiters).
        auto oldState = not_locked;
        return m_state.compare_exchange_strong(
                oldState, locked_no_waiters, std::memory_order_acquire, std::memory_order_relaxed
        );
    }

    void Mutex::unlock() {
        assert(m_state.load(std::memory_order_relaxed) != not_locked);
        MutexAcquire *waitersHead = m_waiters;
        if (waitersHead == nullptr) {
            auto oldState = locked_no_waiters;
            const bool releasedLock = m_state.compare_exchange_strong(
                    oldState, not_locked, std::memory_order_release, std::memory_order_relaxed
            );
            if (releasedLock) return;

            // At least one new waiter.
            // Acquire the list of new waiter operations atomically.
            oldState = m_state.exchange(locked_no_waiters, std::memory_order_acquire);

            assert(oldState != locked_no_waiters && oldState != not_locked);

            // Transfer the list to m_waiters, reversing the list in the process so
            // that the head of the list is the first to be resumed.
            auto *next = reinterpret_cast<MutexAcquire *>(oldState);
            do {
                auto *temp = next->m_next;
                next->m_next = waitersHead;
                waitersHead = next;
                next = temp;
            } while (next != nullptr);
        }

        assert(waitersHead != nullptr);

        m_waiters = waitersHead->m_next;

        // Resume the waiter.
        // This will pass the ownership of the lock on to that operation/coroutine.
        waitersHead->resume();
    }

    bool MutexAcquire::await_suspend(std::coroutine_handle<> h) noexcept {
        m_handle = h;
        std::uintptr_t oldState = m_mutex.m_state.load(std::memory_order_acquire);
        while (true) {
            if (oldState == Mutex::not_locked) {
                if (m_mutex.m_state.compare_exchange_weak(
                        oldState, Mutex::locked_no_waiters,
                        std::memory_order_acquire, std::memory_order_relaxed
                ))
                    return false; // Acquired lock, don't suspend.
            } else {
                // Try to push this operation onto the head of the waiter stack.
                m_next = reinterpret_cast<MutexAcquire *>(oldState);
                if (m_mutex.m_state.compare_exchange_weak(
                        oldState, reinterpret_cast<std::uintptr_t>(this),
                        std::memory_order_release, std::memory_order_relaxed
                ))
                    return true; // Queued operation to waiters list, suspend now.
            }
        }
    }

    MutexLock::~MutexLock() { if (m_mutex != nullptr) m_mutex->unlock(); }
}