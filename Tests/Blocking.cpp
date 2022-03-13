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

#include <string>
#include <gtest/gtest.h>
#include "kls/coroutine/Blocking.h"

TEST(kls_coroutine, VoidBlockingSuccess) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    run_blocking([&]() -> ValueAsync<void> { co_return; });
}

TEST(kls_coroutine, VoidBlockingFailure) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    EXPECT_ANY_THROW(run_blocking([&]() -> ValueAsync<void> { throw std::runtime_error(""); }));
}

TEST(kls_coroutine, TrivialBlockingSuccess) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    EXPECT_EQ(run_blocking([&]() -> ValueAsync<int> { co_return 42; }), 42);
}

TEST(kls_coroutine, TrivialBlockingFailure) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    EXPECT_ANY_THROW(run_blocking([&]() -> ValueAsync<int> { throw std::runtime_error(""); }));
}

TEST(kls_coroutine, NonTrivialBlockingSuccess) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    EXPECT_EQ(run_blocking([&]() -> ValueAsync<std::string> { co_return "TEST"; }), std::string("TEST"));
}

TEST(kls_coroutine, NonTrivialBlockingFailure) {
    using namespace kls::essential;
    using namespace kls::coroutine;
    EXPECT_ANY_THROW(run_blocking([&]() -> ValueAsync<std::string> { throw std::runtime_error(""); }));
}
