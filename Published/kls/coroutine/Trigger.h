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

#include "Executor.h"
#include "kls/thread/SpinLock.h"

namespace kls::coroutine {
    class SingleTrigger: public AddressSensitive {
    public:
        bool trap(std::coroutine_handle<> h) noexcept;

        void drop() noexcept;

        void pull() noexcept;
    private:
        std::atomic<void *> m_captured{nullptr};
    };

    struct FifoAwaitEntry: AddressSensitive {
        void resume() noexcept { m_handle.resume(); }

        void destroy() noexcept { m_handle.destroy(); }

        void set_handle(std::coroutine_handle<> handle) noexcept { m_handle = handle; }

        [[nodiscard]] auto get_next() const noexcept { return m_next; }

        auto set_next(FifoAwaitEntry* const next) noexcept { return m_next = next; }
    private:
        FifoAwaitEntry* m_next{nullptr};
        std::coroutine_handle<> m_handle{};
    };

    class FifoTrigger: public AddressSensitive {
    public:
        bool trap(FifoAwaitEntry& next) noexcept;

        void drop() noexcept;

        void pull() noexcept;
    private:
        thread::SpinLock m_lock{};
        std::atomic_bool m_done{false};
        FifoAwaitEntry *m_head{nullptr}, *m_tail{nullptr};
    };

    class ExecutorAwaitEntry: public AddressSensitive {
    public:
        ExecutorAwaitEntry() noexcept: m_exec(this_executor()) {}

        explicit ExecutorAwaitEntry(IExecutor *next) noexcept: m_exec(next) {}

        void destroy() noexcept { m_handle.destroy(); }

        void set_handle(std::coroutine_handle<> handle) noexcept { m_handle = handle; }

        void resume_async() { if (m_exec) m_exec->enqueue(m_handle); else m_handle.resume(); }

        bool resumable_inplace(IExecutor *now) const noexcept { return (now == m_exec) || (!m_exec); }

        void resume_exec(IExecutor *now) { if (now != m_exec) m_exec->enqueue(m_handle); else m_handle.resume(); }
    private:
        IExecutor *m_exec;
        std::coroutine_handle<> m_handle{};
    };

    class SingleExecutorTrigger: public AddressSensitive {
    public:
        bool trap(ExecutorAwaitEntry& h);

        void drop() noexcept;

        void pull();
    private:
        std::atomic<void *> m_captured{nullptr};
    };

    class FifoExecutorAwaitEntry: public ExecutorAwaitEntry {
    public:
        [[nodiscard]] auto get_next() const noexcept { return m_next; }

        auto set_next(FifoExecutorAwaitEntry* const next) noexcept { return m_next = next; }
    private:
        FifoExecutorAwaitEntry* m_next{nullptr};
    };

    class FifoExecutorTrigger: public AddressSensitive {
    public:
        bool trap(FifoExecutorAwaitEntry& next);

        void drop() noexcept;

        void pull();
    private:
        thread::SpinLock m_lock{};
        std::atomic_bool m_done{false};
        FifoExecutorAwaitEntry *m_head{nullptr}, *m_tail{nullptr};
    };
}