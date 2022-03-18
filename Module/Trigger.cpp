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

#include <bit>
#include <mutex>
#include "kls/coroutine/Trigger.h"

using namespace kls::thread;
using namespace kls::coroutine;

static void *INVALID_PTR = std::bit_cast<void *>(~uintptr_t(0));

static bool trap_address(std::atomic<void*>& capture, void* address) noexcept {
    for (;;) {
        auto val = capture.load();
        // trigger state is already set, no trapping
        if (val == INVALID_PTR) return false;
        // trigger state is not set, try to trap atomically
        if (!capture.compare_exchange_weak(val, address)) {
            // if the trigger state is neither set nor empty, abort as the state is corrupted
            if (val != INVALID_PTR && val != nullptr) std::abort();
            // the exchange failed spuriously, possibly due to busy memory line. IDLE then retry
            IDLE;
        }
        else return true;
    }
}

static auto set_trap(std::atomic<void*>& capture) noexcept { return capture.exchange(INVALID_PTR); }

bool SingleTrigger::trap(std::coroutine_handle<> h) noexcept {
    return trap_address(m_captured, h.address());
}

void SingleTrigger::drop() noexcept {
    if (const auto h = set_trap(m_captured); h) std::coroutine_handle<>::from_address(h).destroy();
}

void SingleTrigger::pull() noexcept {
    if (const auto h = set_trap(m_captured); h) std::coroutine_handle<>::from_address(h).resume();
}

bool SingleExecutorTrigger::trap(ExecutorAwaitEntry &h) {
    if (trap_address(m_captured, &h)) return true;
    if (const auto now = this_executor(); h.resumable_inplace(now)) return false;
    return (h.resume_async(), true);
}

void SingleExecutorTrigger::drop() noexcept {
    if (const auto h = set_trap(m_captured); h) static_cast<ExecutorAwaitEntry *>(h)->destroy();
}

void SingleExecutorTrigger::pull() {
    if (const auto h = set_trap(m_captured); h) static_cast<ExecutorAwaitEntry *>(h)->resume_async();
}

static bool fifo_trap(std::atomic_bool& flag, SpinLock& lock, auto& head, auto& tail, auto next) noexcept {
    if (flag) return false;
    std::lock_guard lk{lock};
    // able to direct pull, no need to hold lock.
    if (flag) return false;
    // double-checked that we cannot pull now, chain it to the tail
    if (!head) head = next; else tail->set_next(next);
    return (tail = next, true);
}

static auto fifo_set_trigger(std::atomic_bool& flag, SpinLock& lock, auto& head) noexcept {
    flag.store(true);
    std::lock_guard lk{lock};
    return head;
    // as the ready flag is set, any other call using the state should not touch the cont chain
}

static void fifo_handle_list(auto it, auto handler) noexcept {
    for (; it;) {
        const auto current = it;
        it = it->get_next();
        handler(current); // expect this to invalidate the current pointer
    }
}

bool FifoTrigger::trap(FifoAwaitEntry& next) noexcept {
    return fifo_trap(m_done, m_lock, m_head, m_tail, &next);
}

void FifoTrigger::drop() noexcept {
    fifo_handle_list(fifo_set_trigger(m_done, m_lock, m_head), [](auto h) noexcept { h->destroy(); });
}

void FifoTrigger::pull() noexcept {
    fifo_handle_list(fifo_set_trigger(m_done, m_lock, m_head), [](auto h) noexcept { h->resume(); });
}

bool FifoExecutorTrigger::trap(FifoExecutorAwaitEntry &next) {
    if (fifo_trap(m_done, m_lock, m_head, m_tail, &next)) return true;
    if (const auto now = this_executor(); next.resumable_inplace(now)) return false;
    return (next.resume_async(), true);
}

void FifoExecutorTrigger::drop() noexcept {
    fifo_handle_list(fifo_set_trigger(m_done, m_lock, m_head), [](auto h) noexcept { h->destroy(); });
}

void FifoExecutorTrigger::pull() {
    fifo_handle_list(fifo_set_trigger(m_done, m_lock, m_head), [](auto h) noexcept { h->resume_async(); });
}
