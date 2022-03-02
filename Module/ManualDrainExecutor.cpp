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
