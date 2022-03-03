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
#include "kls/temp/Queue.h"
#include "kls/thread/SpinLock.h"

namespace kls::coroutine::detail {
    template<class Task, bool Fast = false>
    class FifoQueue {
    public:
        void Add(const Task& t) {
            std::lock_guard lk{ mSpin };
            mTasks.Push(t);
        }

        [[nodiscard]] Task Get() noexcept {
            if (auto exec = LockedPop(); exec) return exec;
            if constexpr (!Fast) {
                thread::SpinWait spinner{};
                for (auto i = 0u; i < thread::SpinWait::SpinCountForSpinBeforeWait; ++i) {
                    spinner.once();
                    if (auto exec = LockedPop(); exec) return exec;
                }
            }
            return {};
        }

        [[nodiscard]] bool SnapshotNotEmpty() const noexcept { return !mTasks.Empty(); }

        void Finalize() noexcept {}
    private:
        thread::SpinLock mSpin{};
        temp::Queue<Task> mTasks{};

        auto LockedPop() {
            std::lock_guard lk{ mSpin };
            return mTasks.Pop();
        }
    };
}
