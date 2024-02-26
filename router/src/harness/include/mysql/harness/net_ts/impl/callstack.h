/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_CALLSTACK_H_
#define MYSQL_HARNESS_NET_TS_IMPL_CALLSTACK_H_

namespace net::impl {

/**
 * callstack of a thread.
 *
 * use-cases:
 * - track debuginfo of call chains
 * - check if the function calls itself
 *
 * Each new stackframe links to the previous stackframe and sets itself as
 * stacktop. As callstacks are per-thread, no locking is needed.
 *
 * # Usage
 *
 * @code
 * struct Frame {
 *   const char *filename;
 *   int line;
 *   const char *func;
 *
 *   Frame(const char *f, int l, const char *fun):
 *     filename(f), line(l), func(fun) {}
 * };
 * void a() {
 *   Frame frame(__FILE__, __LINE__, __func__);
 *   Callstack<Frame>::Context stackframe(&frame);
 * }
 *
 * void b() {
 *   // store a frame on the functions stack
 *   Frame frame(__FILE__, __LINE__, __func__);
 *   // link the 'frame' to the callstack
 *   Callstack<Frame>::Context stackframe(&frame);
 *
 *   a();
 * }
 * @endcode
 *
 * @tparam Key   key-type
 * @tparam Value value-type to assign to the key
 */
template <class Key, class Value = unsigned char>
class Callstack {
 public:
  class Context;
  class Iterator;

  using value_type = Context *;
  using iterator = Iterator;
  using const_iterator = Iterator;

  class Context {
   public:
    /**
     * construct a stackframe.
     *
     * sets top of stack to this frame.
     */
    explicit Context(const Key *k)
        : Context(k, reinterpret_cast<Value &>(*this)) {}

    Context(const Key *k, Value &v)
        : key_{k}, value_{&v}, next_{Callstack<Key, Value>::stack_top_} {
      Callstack<Key, Value>::stack_top_ = this;
    }

    /**
     * destruct a stackframe.
     *
     * sets top of stack to the previous stackframe.
     */
    ~Context() { Callstack<Key, Value>::stack_top_ = next_; }

    // disable copy construct and copy-assign
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    const Key *key() const { return key_; }
    Value *value() const { return value_; }

   private:
    friend class Callstack<Key, Value>;
    friend class Callstack<Key, Value>::Iterator;

    const Key *key_;
    Value *value_;

    Context *next_; /**!< next stackframe */
  };

  /**
   * forward-iterator over stack frames.
   *
   * just enough to implement for-range loops
   */
  class Iterator {
   public:
    Iterator(Context *ctx) : ctx_{ctx} {}

    Iterator &operator++() {
      if (ctx_ != nullptr) ctx_ = ctx_->next_;
      return *this;
    }

    bool operator!=(const Iterator &other) const { return ctx_ != other.ctx_; }

    Context *operator*() { return ctx_; }

   private:
    Context *ctx_;
  };

  /**
   * check if a callstack contains a pointer already.
   *
   * walks the stack from the top the last element and checks if a frames key
   * makes k
   *
   * @param k key to search for in the callstack
   * @returns stored value if key is found
   * @retval nullptr if not found.
   */
  static constexpr Value *contains(const Key *k) {
    for (auto *e : Callstack<Key, Value>()) {
      if (e->key() == k) return e->value();
    }

    return nullptr;
  }

  /** begin() iterator */
  static Iterator begin() { return Iterator(stack_top_); }
  /** end() iterator */
  static Iterator end() { return Iterator(nullptr); }

 private:
  static thread_local Context *stack_top_;
};

// define thread-local initial stack-top
template <class Key, class Value>
thread_local
    typename Callstack<Key, Value>::Context *Callstack<Key, Value>::stack_top_ =
        nullptr;
}  // namespace net::impl

#endif
