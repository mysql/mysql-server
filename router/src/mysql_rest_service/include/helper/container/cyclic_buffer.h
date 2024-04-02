/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_CONTAINER_CYCLIC_BUFFER_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_CONTAINER_CYCLIC_BUFFER_H_

#include <array>
#include <iterator>

namespace helper {
namespace container {

template <uint32_t buffer_size, typename Container>
class CycleBuffer {
  using Cbuffer = CycleBuffer<buffer_size, Container>;
  using Cinterator = typename Container::iterator;
  using Cpointer = typename Container::pointer;
  using Cconst_interator = typename Container::const_iterator;
  using Creference = typename Container::reference;
  using Cconst_reference = typename Container::const_reference;
  using value_type = typename Container::value_type;

  class Position {
   public:
    Cinterator it_;
    uint32_t flips_{0};

    bool operator==(const Position &other) const {
      return it_ == other.it_ && flips_ == other.flips_;
    }
  };

  class Const_position {
   public:
    Const_position() {}
    Const_position(const Const_position &p) : it_{p.it_}, flips_{p.flips_} {}
    Const_position(const Position &p) : it_{p.it_}, flips_{p.flips_} {}

    Cconst_interator it_;
    uint32_t flips_{0};

    bool operator==(const Const_position &other) const {
      return it_ == other.it_ && flips_ == other.flips_;
    }
  };

  void try_resize(std::array<value_type, buffer_size> &) {}

  template <typename C>
  void try_resize(C &c) {
    c.resize(buffer_size);
  }

 public:
  template <typename Pos = Position, typename Buffer = CycleBuffer,
            typename Reference = Creference>
  class Iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = CycleBuffer::value_type;
    using difference_type = long;
    using pointer = Cpointer;
    using reference = Creference;

    explicit Iterator(Pos position, Buffer *buffer)
        : position_(position), buffer_{buffer} {}

    Iterator &operator++() {
      ++position_.it_;
      if (position_.it_ == buffer_->container_.end()) {
        position_.it_ = buffer_->container_.begin();
        ++position_.flips_;
      }
      return *this;
    }

    Iterator &operator--() {
      if (position_.it_ == buffer_->container_.begin()) {
        position_.it_ = buffer_->container_.end();
        --position_.it_;
        --position_.flips_;
      } else {
        --position_.it_;
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator retval = *this;
      ++(*this);
      return retval;
    }
    Iterator operator--(int) {
      Iterator retval = *this;
      --(*this);
      return retval;
    }

    bool operator==(const Iterator &other) const {
      return position_ == other.position_;
    }

    bool operator!=(Iterator other) const { return !(*this == other); }

    Reference operator*() const { return *position_.it_; }

   private:
    Pos position_;
    Buffer *buffer_;
  };

  using iterator = Iterator<Position>;
  using const_iterator =
      Iterator<Const_position, const CycleBuffer, Cconst_reference>;

 public:
  CycleBuffer() {
    try_resize(container_);
    begin_.it_ = container_.begin();
    end_.it_ = container_.begin();
  }

  Container &container() { return container_; }

  iterator begin() { return iterator(begin_, this); }
  iterator end() { return iterator(end_, this); }

  const_iterator begin() const { return const_iterator(begin_, this); }
  const_iterator end() const { return const_iterator(end_, this); }

  template <typename V>
  void push_back(V &&v) {
    Iterator<Position &> e{end_, this};
    *e = std::forward<V>(v);
    ++e;
    if (elements_ != buffer_size) {
      ++elements_;
    } else {
      Iterator<Position &> b{begin_, this};
      ++b;
    }
  }

  uint32_t size() const { return elements_; }

  bool empty() const { return 0 == elements_; }

  value_type &front() { return *begin(); }

  value_type &back() { return *--end(); }

  void pop_front() {
    if (!elements_) return;

    Iterator<Position &> b{begin_, this};
    b++;
    --elements_;
  }

  void pop_back() {
    if (!elements_) return;

    Iterator<Position &> e{end_, this};
    e--;
    --elements_;
  }

 private:
  Container container_;
  uint32_t elements_{0};
  Position begin_{container_.begin(), 0};
  Position end_{container_.begin(), 0};
};

template <typename Type, uint32_t buffer_size>
class CycleBufferArray
    : public CycleBuffer<buffer_size, std::array<Type, buffer_size>> {};

}  // namespace container
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_CONTAINER_CYCLIC_BUFFER_H_
