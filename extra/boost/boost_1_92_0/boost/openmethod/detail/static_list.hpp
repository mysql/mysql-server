// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_DETAIL_STATIC_LIST_HPP
#define BOOST_OPENMETHOD_DETAIL_STATIC_LIST_HPP

#include <algorithm>
#include <boost/assert.hpp>

namespace boost::openmethod {

namespace detail {

template<typename T>
class static_list {
  public:
    static_list(static_list&) = delete;
    static_list() = default;

    class static_link {
      public:
        static_link(const static_link&) = delete;
        static_link() = default;

        auto next() -> T* {
            return next_ptr;
        }

      protected:
        friend class static_list;
        T* prev_ptr;
        T* next_ptr;
    };

    void push_back(T& node) {
        BOOST_ASSERT(node.prev_ptr == nullptr);
        BOOST_ASSERT(node.next_ptr == nullptr);

        if (!first) {
            first = &node;
            node.prev_ptr = &node;
            return;
        }

        auto last = first->prev_ptr;
        last->next_ptr = &node;
        node.prev_ptr = last;
        first->prev_ptr = &node;
    }

    void remove(T& node) {
        BOOST_ASSERT(first != nullptr);

        auto prev = node.prev_ptr;
        auto next = node.next_ptr;
        auto last = first->prev_ptr;

        node.prev_ptr = nullptr;
        node.next_ptr = nullptr;

        if (&node == last) {
            if (&node == first) {
                first = nullptr;
                return;
            }

            first->prev_ptr = prev;
            prev->next_ptr = nullptr;
            return;
        }

        if (&node == first) {
            first = next;
            first->prev_ptr = last;
            return;
        }

        prev->next_ptr = next;
        next->prev_ptr = prev;
    }

    void clear() {
        auto next = first;
        first = nullptr;

        while (next) {
            auto cur = next;
            next = cur->next_ptr;
            cur->prev_ptr = nullptr;
            cur->next_ptr = nullptr;
        }
    }

    class iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : ptr(nullptr) {
        }
        explicit iterator(T* p) : ptr(p) {
        }

        auto operator*() -> reference {
            return *ptr;
        }
        auto operator->() -> pointer {
            return ptr;
        }

        auto operator++() -> iterator& {
            BOOST_ASSERT(ptr);
            ptr = ptr->next_ptr;
            return *this;
        }

        auto operator++(int) -> iterator {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto operator==(const iterator& a, const iterator& b) -> bool {
            return a.ptr == b.ptr;
        }

        friend auto operator!=(const iterator& a, const iterator& b) -> bool {
            return a.ptr != b.ptr;
        }

      private:
        T* ptr;
    };

    auto begin() -> iterator {
        return iterator(first);
    }

    auto end() -> iterator {
        return iterator(nullptr);
    }

    class const_iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const T;
        using pointer = value_type*;
        using reference = value_type&;

        const_iterator() : ptr(nullptr) {
        }
        explicit const_iterator(T* p) : ptr(p) {
        }

        auto operator*() -> reference {
            return *ptr;
        }
        auto operator->() -> pointer {
            return ptr;
        }

        auto operator++() -> const_iterator& {
            BOOST_ASSERT(ptr);
            ptr = ptr->next_ptr;
            return *this;
        }

        auto operator++(int) -> const_iterator {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto
        operator==(const const_iterator& a, const const_iterator& b) -> bool {
            return a.ptr == b.ptr;
        }

        friend auto
        operator!=(const const_iterator& a, const const_iterator& b) -> bool {
            return a.ptr != b.ptr;
        }

      private:
        T* ptr;
    };

    auto begin() const -> const_iterator {
        return const_iterator(first);
    }

    auto end() const -> const_iterator {
        return const_iterator(nullptr);
    }

    auto size() const -> std::size_t {
        return std::distance(begin(), end());
    }

    auto empty() const -> bool {
        return !first;
    }

  protected:
    T* first;
};

} // namespace detail
} // namespace boost::openmethod

#endif
