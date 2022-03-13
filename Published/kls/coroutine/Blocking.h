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

#include <variant>
#include "Traits.h"
#include "Executor.h"
#include "ValueAsync.h"

namespace kls::coroutine::detail {
	class Blocking {
	public:
		// creates the executor queue, mark the current thread as the executor
		Blocking();

		Blocking(Blocking&&) = delete;
		Blocking(const Blocking&) = delete;
		Blocking& operator=(Blocking&&) = delete;
		Blocking& operator=(const Blocking&) = delete;

		~Blocking();

    protected:
		void Start();
		void Stop();
    private:

		class Executor;
		Executor* mTheExec;
	};
}

namespace kls::coroutine {
    template <class Fn>
    auto run_blocking(Fn fn) {
        using return_type = awaitable_result_t<std::invoke_result_t<Fn>>;
        if constexpr(!std::is_same_v<void, return_type>) {
            class Blocking : public detail::Blocking {
            public:
                // drain the queue until the awaiting operation has completed.
                // any item submitted to the executor after the completion of the awaited
                // item is invalid and will result in undefined behaviour
                return_type Await(const Fn &fn) {
                    CoAwaitToStop(fn); // Call a coroutine to await the awaitable which calls Stop() afterwards
                    Start();
                    if(auto index = m_result.index(); index == 1)
                        return std::move(*std::get_if<return_type>(&m_result));
                    else
                        std::rethrow_exception(std::move(*std::get_if<std::exception_ptr>(&m_result)));
                }

            private:
                std::variant<std::monostate, return_type, std::exception_ptr> m_result{};

                ValueAsync<void> CoAwaitToStop(const Fn &fn) noexcept {
                    try {
                        m_result = co_await fn();
                    }
                    catch (...) {
                        m_result = std::current_exception();
                    }
                    Stop();
                }
            };
            return Blocking().Await(fn);
        }
        else {
            class Blocking : public detail::Blocking {
            public:
                return_type Await(const Fn &fn) {
                    CoAwaitToStop(fn);
                    Start();
                    if (m_result) std::rethrow_exception(std::move(*m_result));
                }

            private:
                std::optional<std::exception_ptr> m_result = std::nullopt;

                ValueAsync<void> CoAwaitToStop(const Fn &fn) noexcept {
                    try {
                        co_await fn();
                    }
                    catch (...) {
                        m_result = std::current_exception();
                    }
                    Stop();
                }
            };
            return Blocking().Await(fn);
        }
    }
}