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

#include <utility>
#include <optional>
#include <coroutine>
#include <exception>
#include "Executor.h"
#include "kls/Object.h"

namespace kls::coroutine {
    template<class T>
    class ValueStore;

    template<>
    class ValueStore<void> {
    public:
        void Set() {}

        void Fail(std::exception_ptr e) { mFail = std::move(e); }

        void Get() { if (mFail) std::rethrow_exception(mFail); }

        void GetCopy() { if (mFail) std::rethrow_exception(mFail); }
    private:
        std::exception_ptr mFail{nullptr};
    };

    template<class T>
    class ValueStore {
    public:
        template<class ...U>
        void Set(U &&... v) {
            static_assert(sizeof...(v) == 1, "Too may values to set in a ValueStore Object");
            mValue.emplace(std::forward<U>(v)...);
        }

        void Fail(std::exception_ptr e) { mFail = std::move(e); }

        T &&Get() { if (mFail) std::rethrow_exception(mFail); else return std::move(mValue).value(); }

        T GetCopy() { if (mFail) std::rethrow_exception(mFail); else return mValue.value(); }
    private:
        std::exception_ptr mFail{nullptr};
        std::optional<T> mValue{std::nullopt};
    };

    template<class T, class ContControl>
    class ContinuableValueMedia : public ContControl {
    public:
        template<class ...U>
        explicit ContinuableValueMedia(U &&... v): ContControl(std::forward<U>(v)...) {}

        template<class ...U>
        void Set(U &&... v) {
            mValueStore.Set(std::forward<U>(v)...);
            ContControl::DispatchContinuation();
        }

        void Fail() {
            mValueStore.Fail(std::current_exception());
            ContControl::DispatchContinuation();
        }

        T Get() { return mValueStore.Get(); }

        T GetCopy() { return mValueStore.GetCopy(); }
    private:
        ValueStore<T> mValueStore;
    };

    class AwaitCore: public Object {
    public:
        AwaitCore() noexcept: mExec(CurrentExecutor()) {}

        explicit AwaitCore(IExecutor *next) noexcept: mExec(next) {}

        // Awaits-related classes are not supposed to be copied nor moved
        AwaitCore(AwaitCore &&) = delete;

        AwaitCore(const AwaitCore &) = delete;

        AwaitCore &operator=(AwaitCore &&) = delete;

        AwaitCore &operator=(const AwaitCore &) = delete;

        bool CanExecInPlace(IExecutor *exec = CurrentExecutor()) const noexcept { return (exec == mExec) || (!mExec); }

        // Note: this call will invalidate "this"
        void Dispatch() { if (mExec) mExec->Enqueue(mCo); else mCo.resume(); }

        void SetHandle(std::coroutine_handle<> handle) noexcept { mCo = handle; }
    private:
        IExecutor *mExec;
        std::coroutine_handle<> mCo{};
    };

    // helper class for generating actual await client class.
    // the 'Core' class should have a Transit(coroutine_handle<>) and a Get() function
    template <class Core>
    struct Await: private Core {
        template<class ...U>
        explicit Await(U &&... v): Core(std::forward<U>(v)...) {}

        // the short-path check should be done in Core::Transit
        // this is done to make sure that the state transition is properly handled
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) { return Core::Transit(h); }

        auto await_resume() { return Core::Get(); }
    };
}