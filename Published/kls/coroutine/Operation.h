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

#include "Async.h"

namespace kls::coroutine {
    class SwitchTo {
    public:
        explicit SwitchTo(IExecutor* next) noexcept : mNext(next) {}

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { mNext->enqueue(handle); }

        constexpr void await_resume() noexcept {}
    private:
        IExecutor* mNext;
    };

    struct Redispatch {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) { this_executor()->enqueue(handle); }

        constexpr void await_resume() noexcept {}
    };

    template <class ...U>
    ValueAsync<void> awaits(U&&... c) { (..., co_await std::move(c)); }

    template <class Container>
    ValueAsync<void> await_all(Container c) { for (auto&& x : c) co_await std::move(x); }
}
