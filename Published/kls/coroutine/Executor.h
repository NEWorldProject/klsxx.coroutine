#pragma once

#include <memory>
#include <coroutine>

namespace kls::coroutine {
    class IExecutor {
    public:
        void Enqueue(std::coroutine_handle<> handle) noexcept { (*this.*EnqueueRaw)(handle.address()); }

    protected:
        using FnEnqueue = void (IExecutor::*)(void* coroutine) noexcept;

        explicit IExecutor(FnEnqueue enqueue) : EnqueueRaw{ enqueue } {}

    private:
        FnEnqueue EnqueueRaw;
    };

    IExecutor* CurrentExecutor() noexcept;

    std::shared_ptr<IExecutor> CreateSingleThreadExecutor();

    std::shared_ptr<IExecutor> CreateScalingFIFOExecutor(int min, int max, int linger);

    std::shared_ptr<IExecutor> CreateScalingBagExecutor(int min, int max, int linger);
}
