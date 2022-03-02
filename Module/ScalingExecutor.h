#pragma once

#include "QueueDrain.h"
#include "Executor.hpp"
#include "kls/thread/Semaphore.h"
#include "kls/coroutine/Coroutine.h"

namespace kls::coroutine::detail {
    template<template<class> class Queue>
    class ScalingExecutor : public IExecutor {
    public:
        ScalingExecutor(int min, int max, int linger) :
                IExecutor(static_cast<FnEnqueue>(&ScalingExecutor::EnqueueRawImpl)),
                mMin(min), mMax(max), mLinger(linger) {
            mTotal.store(min);
            for (int i = 0; i < mMin; ++i) Spawn();
        }

        ~ScalingExecutor() {
            [this]() -> ValueAsync<void> {
                co_await SwitchTo{ this };
                // notify the underlying queue to join all non-executor que
                mDrainer.Finalize();
                // tell the executors that they should join after finishing whatever they are doing
                mRun = false;
                // wake all parked executors
                while (TryWake());
            }();
            mFinal.wait();
        }

    private:
        std::atomic_bool mRun{true};
        std::atomic_int mPark{0}, mTotal{0};
        thread::Semaphore mSignal{}, mFinal{};
        const int mMin, mMax, mLinger;
        QueueDrain<Queue, void*> mDrainer{};

        void EnqueueRawImpl(void* handle) noexcept { Add(handle); }

        void Add(void* task) {
            mDrainer.Add(task);
            Notify();
        }

        bool TryWake() {
            for (;;) {
                // try to awake an existing thread
                if (auto c = mPark.load(); c) {
                    if (mPark.compare_exchange_strong(c, c - 1)) return (mSignal.signal(), true);
                } else return false;
            }
        }

        void Notify() {
            if (!TryWake()) {
                for (;;) {
                    auto t = mTotal.load();
                    if (mTotal == mMax) return; // maximum thread alive, do not scale up
                    if (mTotal.compare_exchange_strong(t, t + 1)) {
                        Spawn(); // counter bump success, spin up the actual thread
                    }
                }
            }
        }

        void Spawn() {
            std::thread([this]()noexcept {
                SetCurrentExecutor(this);
                do {
                    mDrainer.Drain();
                    if (mRun) continue;
                    if (Rest()) continue;
                    // this is a scale down decision, counter already modified, handles special case
                    if (mMin != 0) continue;
                    // settings allows scaling to zero
                    if (mRun) continue;
                    // the executor has been commanded to stop. as stop is set by the last added task,
                    // all tasks added before should be already drained.
                    if (mTotal == 0) mFinal.signal(); // this is the last thread. notify final
                } while(mRun);
                // this is not a scale down operation, we need to check the counter and notify final
                if (mTotal.fetch_sub(1) == 1) mFinal.signal(); // this is the last thread. notify final
            }).detach();
        }

        bool Rest() noexcept {
            mPark.fetch_add(1); // enter protected region
            if (mDrainer.ShouldActive() || !mRun) {
                // it is possible that a task was added during function invocation period of this function and the WakeOne
                // did not notice this thread is going to sleep. To prevent system stalling, we will unconditionally
                // signal to wake a worker (including this one)
                TryWake();
            }
            // to keep integrity, this thread will enter sleep state regardless of whether if the snapshot check is positive
            const auto result = mSignal.wait_for(std::chrono::milliseconds(mLinger));
            if (!result) {
                mPark.fetch_sub(1); // the thread is not awoken by signal, a signal is remained on the sem
                // determine if we should scale down
                for (;;) {
                    auto c = mTotal.load();
                    if (mTotal == mMin) return true; // minimal thread alive, do not scale down
                    if (mTotal.compare_exchange_strong(c, c - 1)) return false; // scaling counter success
                }
            }
            return true;
        }
    };
}
