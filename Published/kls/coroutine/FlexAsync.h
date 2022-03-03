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
#include <memory>
#include "CoroDetail.h"
#include "kls/thread/SpinLock.h"

namespace kls::coroutine::detail::flex_async {
    class AwaitCoreChained : public AwaitCore {
    public:
        AwaitCoreChained() noexcept = default;

        explicit AwaitCoreChained(IExecutor *next) noexcept: AwaitCore(next) {}

        [[nodiscard]] AwaitCoreChained *GetNext() const noexcept { return mNext; }

        AwaitCoreChained *SetNext(AwaitCoreChained *next) noexcept { return mNext = next; }

    private:
        AwaitCoreChained *mNext{nullptr};
    };

    class ContinuationControl {
    public:
        bool Transit(AwaitCoreChained *next) {
            const auto current = CurrentExecutor();
            if (mReady) { if (next->CanExecInPlace(current)) return false; else return (next->Dispatch(), true); }
            else {
                std::unique_lock lk{mContLock};
                if (mReady) {
                    lk.unlock(); // able to direct dispatch, no need to hold lock.
                    if (next->CanExecInPlace(current)) return false; else return (next->Dispatch(), true);
                }
                // double-checked that we cannot dispatch now, chain it to the tail
                if (!mHead) mHead = next; else mTail->SetNext(next);
                mTail = next;
                return true;
            }
        }

        void DispatchContinuation() {
            mReady.store(true);
            std::unique_lock lk{mContLock};
            auto it = mHead;
            // as the ready flag is set, any other call using the state should not touch the cont chain
            lk.unlock();
            for (; it;) {
                auto current = it;
                it = it->GetNext();
                current->Dispatch(); // this will invalidate the current pointer
            }
        }

    private:
        std::atomic_bool mReady{false};
        thread::SpinLock mContLock{};
        AwaitCoreChained *mHead{nullptr}, *mTail{nullptr};
    };

    template<class T>
    using SharedContinuableValueMedia = ContinuableValueMedia<T, ContinuationControl>;

    template<class T>
    using SharedContinuableValueMediaHandle = std::shared_ptr<SharedContinuableValueMedia<T>>;

    template<class T>
    class PromiseValueMedia {
    public:
        explicit PromiseValueMedia(SharedContinuableValueMediaHandle<T> h) noexcept: mHandle(std::move(h)) {}

        template<class ...U>
        void return_value(U &&... v) { mHandle->Set(std::forward<U>(v)...); }

        void unhandled_exception() { mHandle->Fail(); }

    private:
        SharedContinuableValueMediaHandle<T> mHandle;
    };

    template<>
    class PromiseValueMedia<void> {
    public:
        explicit PromiseValueMedia(SharedContinuableValueMediaHandle<void> h) noexcept: mHandle(std::move(h)) {}

        void return_void() { mHandle->Set(); }

        void unhandled_exception() { mHandle->Fail(); }

    private:
        SharedContinuableValueMediaHandle<void> mHandle;
    };
}

namespace kls::coroutine {
    template<class T>
    class FlexAsync {
        using State = detail::flex_async::SharedContinuableValueMedia<T>;
        using StateHandle = detail::flex_async::SharedContinuableValueMediaHandle<T>;
        using PromiseMedia = detail::flex_async::PromiseValueMedia<T>;

        class FlexAwaitCore : detail::flex_async::AwaitCoreChained {
        public:
            explicit FlexAwaitCore(StateHandle state) : mState(state) {}

            FlexAwaitCore(StateHandle state, IExecutor* next) noexcept : AwaitCoreChained(next), mState(state) {}

            bool Transit(std::coroutine_handle<> h) {
                SetHandle(h);
                return mState->Transit(this);
            }

            T Get() { return mState->GetCopy(); }

        private:
            StateHandle mState;
        };

    public:
        using Await = detail::Await<FlexAwaitCore>;

        class promise_type : public PromiseMedia {
            static StateHandle make_state() noexcept {
                return std::make_shared<State>();
            }

        public:
            promise_type() : PromiseMedia(make_state()) {}

            FlexAsync get_return_object() { return FlexAsync(this); }

            constexpr auto initial_suspend() noexcept { return std::suspend_never{}; }

            constexpr auto final_suspend() noexcept { return std::suspend_never{}; }
        };

        auto operator co_await()&& { return Await(std::move(mState)); }

        auto operator co_await() const& { return Await(mState); }

        auto Configure(IExecutor* next)&& { return Await(std::move(mState), next); }

        auto Configure(IExecutor* next) const& { return Await(mState, next); }

    private:
        StateHandle mState;

        explicit FlexAsync(std::shared_ptr<State> state) noexcept : mState(std::move(state)) {}
    };
}
