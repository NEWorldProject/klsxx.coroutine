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

#include "Executor.h"
#include "kls/coroutine/ValueAsync.h"

namespace kls::coroutine {
	class Blocking {
	public:
		// creates the executor queue, mark the current thread as the executor
		Blocking();

		Blocking(Blocking&&) = delete;
		Blocking(const Blocking&) = delete;
		Blocking& operator=(Blocking&&) = delete;
		Blocking& operator=(const Blocking&) = delete;

		// destruct everthing
		~Blocking();

		// drain the queue until the awaiting operation has completed.
		// any item submitted to the executer after the completion of the awaited
		// item is invalid and will result in undefined behaviour
		template <class T>
		void Await(T&& awaitable) {
			CoAwaitToStop(std::forward<T>(awaitable)); // Call a coroutine to await the awaitable which calls Stop() afterwards
			Start();
		}
	private:
		void Start();
		void Stop();

		template <class T>
		ValueAsync<void> CoAwaitToStop(T&& awaitable) {
			co_await std::forward<T>(awaitable);
			Stop();
		}

		class Executor;
		Executor* mTheExec;
	};
}