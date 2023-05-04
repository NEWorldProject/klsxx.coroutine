#pragma once

#include <type_traits>
#include <kls/Span.h>
#include <kls/coroutine/Future.h>

namespace kls::channel {
    namespace internal {
        struct Rx {
            virtual coroutine::ValueFuture<Span<>> poll() = 0;

            virtual void consume() = 0;

            virtual ~Rx() = 0;
        };

        struct Tx {
            virtual int try_push(Span<> o) = 0;

            virtual coroutine::ValueFuture<> push(Span<> o) = 0;

            virtual ~Tx() = 0;
        };

        struct RxSrc {
            virtual std::unique_ptr<Rx> get() = 0;

            virtual ~RxSrc() = 0;
        };

        struct TxSrc {
            virtual std::unique_ptr<Tx> get() = 0;

            virtual ~TxSrc() = 0;
        };

        struct Src {
            std::unique_ptr<RxSrc> rx_src;
            std::unique_ptr<TxSrc> tx_src;
        };

        struct Ti {
            uintptr_t size, align;
        };

        Src bounded(Ti ti, int count, bool sp, bool sc);

        Src unbounded(Ti ti, bool sp, bool sc);

        Src rendezvous(Ti ti, bool sp, bool sc);
    }

    template<class T>
    struct Rx {
        ~Rx() = default;

        coroutine::ValueFuture<Span<T>> poll() {
            auto result = co_await m_rx->poll();
            co_return static_span_cast<T>(result);
        }

        void consume() { m_rx->consume(); }

    private:
        std::unique_ptr<internal::Rx> m_rx;

        template<class U>
        friend
        class Src;

        explicit Rx(std::unique_ptr<internal::Rx> &&u) : m_rx(std::move(u)) {}
    };

    template<class T>
    struct Tx {
        ~Tx() = default;

        bool try_push(T &&o) { return try_push(Span<T>(&o, 1)) == 1; };

        int try_push(Span<T> o) { return m_tx->try_push(o); }

        coroutine::ValueFuture<> push(T &&o) { return push(Span<T>(&o, 1)); }

        coroutine::ValueFuture<> push(Span<T> o) { return m_tx->push(o); }

    private:
        std::unique_ptr<internal::Tx> m_tx;

        template<class U>
        friend
        class Src;

        explicit Tx(std::unique_ptr<internal::Tx> &&u) : m_tx(std::move(u)) {}
    };

    template<class T>
    class Src {
    public:
        explicit Src(internal::Src &&src) : m_src(std::move(src)) {}

        Rx<T> rx() { return Rx<T>(m_src.rx_src->get()); }

        Tx<T> tx() { return Tx<T>(m_src.tx_src->get()); }

    private:
        internal::Src m_src;
    };

    template<class T>
    Src<T> bounded(int count, bool sp, bool sc) {
        return Src<T>(internal::bounded({sizeof(T), alignof(T)}, count, sp, sc));
    }

    template<class T>
    Src<T> unbounded(bool sp, bool sc) {
        return Src<T>(internal::unbounded({sizeof(T), alignof(T)}, sp, sc));
    }

    template<class T>
    Src<T> rendezvous(bool sp, bool sc) {
        return Src<T>(internal::rendezvous({sizeof(T), alignof(T)}, sp, sc));
    }
}