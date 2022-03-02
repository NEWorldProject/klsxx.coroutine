#pragma once

#include <coroutine>

namespace kls::coroutine::detail {
    template<template<class> class Queue, class Task>
    class QueueDrain {
    public:
        void Add(const Task &t) { mQueue.Add(t); }

        void Drain() noexcept {
            for (;;) if (auto exec = mQueue.Get(); exec) std::coroutine_handle<>::from_address(exec).resume(); else return;
        }

        [[nodiscard]] bool ShouldActive() noexcept { return mQueue.SnapshotNotEmpty(); }

        void Finalize() { mQueue.Finalize(); }
    private:
        Queue<Task> mQueue;
    };
}
