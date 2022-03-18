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

#include <queue>
#include <mutex>
#include "kls/thread/SpinLock.h"
#include "kls/thread/Semaphore.h"
#include "kls/coroutine/Timed.h"

using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;

namespace kls::coroutine::detail {
    struct Unit {
        Time time;
        SingleExecutorTrigger *await;

        auto operator<=>(const Unit &r) const noexcept { return time <=> r.time; }
    };

    class Timed {
    public:
        void add(Unit unit) {
            std::lock_guard lk{m_lock};
            if (m_queue.empty()) {
                m_queue.push(unit);
                m_signal.signal();
            }
            else {
                Unit top = m_queue.top();
                m_queue.push(unit);
                if (top > unit) m_signal.signal();
            }
        }

        static Timed &get() {
            static Timed instance{};
            return instance;
        }

    private:
        std::atomic_bool m_stop{false};
        std::thread m_thread{[this] { run(); }};
        thread::SpinLock m_lock;
        thread::Semaphore m_signal;
        std::priority_queue<Unit, std::vector<Unit>, std::greater<>> m_queue;

        ~Timed() {
            m_stop.store(true);
            m_signal.signal();
            m_thread.join();
        }

        void run() noexcept {
            for(;;) {
                std::unique_lock lk{m_lock};
                if (m_queue.empty()) {
                    lk.unlock();
                    if (m_stop.load()) break;
                    m_signal.wait();
                }
                else {
                    const auto now = Clock::now();
                    if (const auto top = m_queue.top(); now >= top.time) {
                        m_queue.pop();
                        top.await->pull();
                    } else {
                        lk.unlock();
                        m_signal.wait_until(top.time);
                    }
                }
            }
        }
    };
}

namespace kls::coroutine {
    DelayAwait delay_until(std::chrono::steady_clock::time_point tp) {
        return DelayAwait{[tp](SingleExecutorTrigger *ths) { detail::Timed::get().add({tp, ths}); }};
    }
}
