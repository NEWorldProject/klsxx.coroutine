#pragma once

#include "Executor.h"
#include "kls/coroutine/ValueAsync.h"

namespace kls::coroutine {
    class ManualDrainExecutor {
    public:
        ManualDrainExecutor();

        ManualDrainExecutor(ManualDrainExecutor&&) = delete;
        ManualDrainExecutor(const ManualDrainExecutor&) = delete;
        ManualDrainExecutor& operator=(ManualDrainExecutor&&) = delete;
        ManualDrainExecutor& operator=(const ManualDrainExecutor&) = delete;

        ~ManualDrainExecutor();

        void DrainOnce();
    private:
        class Executor;
        Executor* mTheExec;
    };
}