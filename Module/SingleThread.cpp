#include <mutex>
#include "FifoQueue.h"
#include "Executor.hpp"
#include "kls/thread/Semaphore.h"
#include "kls/coroutine/Coroutine.h"

namespace kls::coroutine {
    std::shared_ptr<IExecutor> CreateSingleThreadExecutor() {
        class Executor final : public IExecutor {
        public:
            Executor() :
                IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)),
                mRunning(true), mThread([this]()noexcept { ThreadRun(); })
            {}
            
            ~Executor() {
                [this]() -> ValueAsync<void> {
                    co_await SwitchTo{ this };
                    mRunning = false;
                }();
                mThread.join();
            }

        private:
            void ThreadRun() noexcept {
                while (mRunning) {
                    detail::SetCurrentExecutor(this);
                    DoWorks();
                    if (mRunning) Rest();
                }
            }

            void EnqueueRawImpl(void* address) noexcept {
                mQueue.Add(address);
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
            detail::FifoQueue<void*> mQueue;
            std::atomic_int mPark{ 0 };
            thread::Semaphore mSignal{};
            std::thread mThread;
        };
        return std::make_shared<Executor>();
    }
}
