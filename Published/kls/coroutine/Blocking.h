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
#include "Traits.h"

namespace kls::coroutine::detail {
	class Blocking: public AddressSensitive {
	public:
		Blocking();
		~Blocking();
    protected:
		void start();
		void stop();
    private:
		class Executor;
		Executor* mTheExec;
	};
}

namespace kls::coroutine {
    template <class Fn>
    auto run_blocking(Fn fn) {
        using return_type = awaitable_result_t<std::invoke_result_t<Fn>>;
        class Blocking : public detail::Blocking {
        public:
            // drain the queue until the awaiting operation has completed.
            // any item submitted to the executor after the completion of the awaited
            // item is invalid and will result in undefined behaviour
            return_type await(const Fn &fn) {
                launch(fn);
                start();
                return m_storage.get();
            }
        private:
            FutureStore<return_type> m_storage;

            ValueAsync<void> launch(const Fn &fn) noexcept {
                try {
                    if constexpr(!std::is_same_v<void, return_type>) {
                        m_storage.set(co_await fn());
                    } else {
                        co_await fn();
                        m_storage.set();
                    }
                }
                catch (...) {
                    m_storage.fail(std::current_exception());
                }
                stop();
            }
        };
        return Blocking().await(fn);
    }
}