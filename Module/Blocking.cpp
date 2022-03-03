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

#include "FifoQueue.h"
#include "Executor.hpp"
#include "kls/thread/Semaphore.h"
#include "kls/coroutine/Blocking.h"

namespace kls::coroutine {
    class Blocking::Executor final : public IExecutor {
    public:
        Executor() :
            IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)),
            mRunning(true) {
            detail::SetCurrentExecutor(this);
        }

        void Start() {
            while (mRunning) {
                DoWorks();
                if (mRunning) Rest();
            }
            detail::SetCurrentExecutor(nullptr);
        }

        void Stop() noexcept { mRunning = false; }

    private:
        void EnqueueRawImpl(void* handle) noexcept {
            mQueue.Add(handle);
            WakeOne();
        }

        void WakeOne() noexcept {
            for (;;) {
                if (auto c = mPark.load(); c) {
                    if (mPark.compare_exchange_strong(c, c - 1)) return mSignal.signal();
                }
                else return;
            }
        }

        void Rest() noexcept {
            mPark.fetch_add(1); // enter protected region
            if (mQueue.SnapshotNotEmpty()) {
                // it is possible that a task was added during function invocation period of this function and the WakeOne
                // did not notice this thread is going to sleep. To prevent system stalling, we will invoke WakeOne again
                // to wake a worker (including this one)
                WakeOne();
            }
            // to keep integrity, this thread will enter sleep state regardless of whether if the snapshot check is positive
            mSignal.wait();
        }

        void DoWorks() noexcept {
            for (;;) if (auto exec = mQueue.Get(); exec) std::coroutine_handle<>::from_address(exec).resume(); else return;
        }

        std::atomic_bool mRunning;
        detail::FifoQueue<void*, true> mQueue;
        std::atomic_int mPark{ 0 };
        thread::Semaphore mSignal{};
    };

    Blocking::Blocking() : mTheExec(new Executor()) {}

    Blocking::~Blocking() { delete mTheExec; }

    void Blocking::Start() { mTheExec->Start(); }

    void Blocking::Stop() { mTheExec->Stop(); }
}
