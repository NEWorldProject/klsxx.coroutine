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
#include "kls/coroutine/ManualDrainExecutor.h"

namespace kls::coroutine {
    class ManualDrainExecutor::Executor final : public IExecutor {
    public:
        Executor() : IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)) {}

        void DrainOnce() {
            detail::SetCurrentExecutor(this);
            for (;;) if (auto exec = mQueue.Get(); exec) std::coroutine_handle<>::from_address(exec).resume(); else return;
            detail::SetCurrentExecutor(nullptr);
        }
    private:
        void EnqueueRawImpl(void* handle) noexcept {
            mQueue.Add(handle);
        }

        detail::FifoQueue<void*, true> mQueue;
    };

    ManualDrainExecutor::ManualDrainExecutor() : mTheExec(new Executor()) {}

    ManualDrainExecutor::~ManualDrainExecutor() { delete mTheExec; }

    void ManualDrainExecutor::DrainOnce() { mTheExec->DrainOnce(); }
}
