#include "kls/coroutine/Executor.h"

namespace kls::coroutine::detail {
	void SetCurrentExecutor(IExecutor* exec) noexcept;
}
