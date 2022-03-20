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

#include <exception>
#include "kls/Object.h"

namespace kls::coroutine {
    template<class T>
    class ValueStore;

    template<>
    class ValueStore<void> {
    public:
        void set() {}
        void fail(std::exception_ptr e) { m_fail = std::move(e); }
        void get() { if (m_fail) std::rethrow_exception(m_fail); }
        void ref() { get(); }
        void copy() { get(); }
    private:
        std::exception_ptr m_fail{nullptr}; //NOLINT
    };

    template<class T>
    class ValueStore {
    public:
        ValueStore() noexcept = default;
        ~ValueStore() { if (!m_fail) std::destroy_at(&m_store.value); }
        template<class ...U> requires requires(U ...v) { sizeof...(v) == 1; }
        void set(U &&... v) { std::construct_at(&m_store.value, std::forward<U>(v)...); }
        void fail(std::exception_ptr e) { m_fail = std::move(e); }
        T&& get() { if (m_fail) std::rethrow_exception(m_fail); else return std::move(m_store.value); }
        T& ref() { if (m_fail) std::rethrow_exception(m_fail); else return m_store.value; }
        T copy() { if (m_fail) std::rethrow_exception(m_fail); else return m_store.value; }
    private:
        std::exception_ptr m_fail{nullptr}; //NOLINT
        Storage<T> m_store;
    };
}
