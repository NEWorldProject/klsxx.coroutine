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
