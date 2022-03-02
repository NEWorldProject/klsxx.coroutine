#pragma once

#include <mutex>
#include "kls/thread/TSS.h"
#include "kls/thread/SpinLock.h"
#include "WorkStealingQueue.h"

namespace kls::coroutine::detail {
    // TODO(improve the design. The current implementation is slow as hell)
    template<class Task>
    class BagQueue {
        class Context : public WorkStealingQueue<Task> {
        public:
            std::atomic<Context *> Next{nullptr};

            Context *FinalizationAlternativeQueue{nullptr};

            bool Use() { return !mUsed.exchange(true); }

            void Reset() { mUsed.store(false); }

        private:
            std::atomic_bool mUsed{false};
        };

    public:
        ~BagQueue() {
            std::lock_guard lk{mLocalLock};
            for (auto it = mListHead.load(); it;) {
                const auto release = it;
                it = it->Next;
                if (!release->empty()) std::abort();
                delete release;
            }
        }

        void Add(const Task &item) { WriteContext()->push(item); }

        [[nodiscard]] Task Get() noexcept {
            const auto ctx = ReadContext();
            if (auto local = ctx->pop(); local) return *std::move(local);
            if (mFinal) {
                auto &alt = ctx->FinalizationAlternativeQueue;
                for (;;) {
                    if (!alt) FinalizationSelectQueue();
                    if (!alt) break; // no more external abandoned queues need to be collected
                    if (auto local = alt->pop(); local) return *std::move(local);
                    alt = nullptr; // if current alt queue fails, we clear the field without removing lock
                }
            }
            if (auto steal = Steal(ctx); steal) return *std::move(steal);
            return Task{};
        }

        bool SnapshotNotEmpty() noexcept {
            for (auto it = mListHead.load(); it; it = it->Next) if (!it->empty()) return true;
            return false;
        }

        void Finalize() {
            {
                // remove the tss storage for external threads to reset queue status
                auto scoped = std::move(mListTss);
                (void) scoped;
            }
            mFinal.store(true);
        }

    private:
        std::atomic_bool mFinal;
        thread::SpinLock mLocalLock;
        std::atomic<Context *> mListHead = nullptr;
        Context *mListTail = nullptr;
        thread::Pointer<Context, void> mListTss{&LooseReset, nullptr};

        static void LooseReset(void *p, void *) noexcept { if (p) reinterpret_cast<Context *>(p)->Reset(); }

        Context *AssignList() {
            std::lock_guard lk{mLocalLock};
            // try to recycle an abandoned unit
            for (auto it = mListHead.load(); it; it = it->Next) if (it->Use()) return it;
            // add a new list if recycling failed
            const auto newList = new Context();
            if (mListTail) mListTail->Next = newList; else mListHead = newList;
            return (newList->Use(), mListTail = newList);
        }

        Context *FinalizationSelectQueue() {
            std::lock_guard lk{mLocalLock};
            // look for an abandoned unit
            for (auto it = mListHead.load(); it; it = it->Next) {
                if (!it->Use()) continue;
                // unit locked, so that other queues can only steal from it
                if (it->empty()) continue;
                // the unit is not already empty, use this unit as the alternative
                return it;
            }
            return nullptr; // no queue is found
        }

        // For use in executors, reading threads need to have list cached for maximum performance
        // We can avoid the expensive lookup by just using an id check
        static Context *ExecContext(BagQueue *parent, bool reading) {
            struct ListHolder {
                BagQueue *Parent{nullptr};
                Context *Held{nullptr};

                ~ListHolder() { if (Held) Held->Reset(); };
            };
            static thread_local ListHolder holder;
            if (parent == holder.Parent) return holder.Held; // we have a unique list for this parent
            if (reading) { // assign a new list to the fast TLS cache
                holder.Parent = parent;
                return holder.Held = parent->AssignList();
            }
            return nullptr; // fallback to instance local tls check
        }

        Context *ReadContext() { return ExecContext(this, true); }

        Context *WriteContext() {
            if (const auto list = ExecContext(this, false); list) return list;
            if (const auto list = mListTss.get(); list) return list;
            const auto newList = AssignList();
            return (mListTss.reset(newList), newList);
        }

        std::optional<Task> Steal(Context *ctx) {
            for (auto it = mListHead.load(); it; it = it->Next) {
                if (it == ctx) continue;
                if (auto r = it->steal(); r) return r;
            }
            return std::nullopt;
        }
    };
}
