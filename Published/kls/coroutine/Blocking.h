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