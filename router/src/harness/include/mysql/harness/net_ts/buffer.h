/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_BUFFER_H_
#define MYSQL_HARNESS_NET_TS_BUFFER_H_

#include <algorithm>  // copy
#include <array>
#include <limits>     // std::numeric_limits
#include <stdexcept>  // length_error
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "mysql/harness/net_ts/executor.h"               // async_completion
#include "mysql/harness/net_ts/impl/socket_constants.h"  // wait_write
#include "mysql/harness/stdx/expected.h"

namespace net {

// 16.3 [buffer.err]

enum class stream_errc {
  eof = 1,
  not_found,
};
}  // namespace net

namespace std {
template <>
struct is_error_code_enum<net::stream_errc> : public std::true_type {};
}  // namespace std

namespace net {
inline const std::error_category &stream_category() noexcept {
  class stream_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "stream"; }
    std::string message(int ev) const override {
      switch (static_cast<stream_errc>(ev)) {
        case stream_errc::eof:
          return "eof";
        case stream_errc::not_found:
          return "not found";
        default:
          return "unknown";
      }
    }

    bool equivalent(int mycode,
                    const std::error_condition &other) const noexcept override {
      if (*this == other.category() ||
          std::string_view(name()) ==
              std::string_view(other.category().name())) {
        return mycode == other.value();
      }

      return false;
    }

    bool equivalent(const std::error_code &other,
                    int mycode) const noexcept override {
      if (*this == other.category() ||
          std::string_view(name()) ==
              std::string_view(other.category().name())) {
        return mycode == other.value();
      }

      return false;
    }
  };

  static stream_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(net::stream_errc e) noexcept {
  return {static_cast<int>(e), net::stream_category()};
}

inline std::error_condition make_error_condition(net::stream_errc e) noexcept {
  return {static_cast<int>(e), net::stream_category()};
}

// 16.4 [buffer.mutable]

class mutable_buffer {
 public:
  mutable_buffer() noexcept : data_{nullptr}, size_{0} {}

  mutable_buffer(void *p, size_t n) noexcept : data_{p}, size_{n} {}

  void *data() const noexcept { return data_; }
  size_t size() const noexcept { return size_; }

  mutable_buffer &operator+=(size_t n) noexcept {
    data_ = static_cast<char *>(data_) + std::min(n, size_);
    size_ = size_ - std::min(n, size_);
    return *this;
  }

 private:
  void *data_;
  size_t size_;
};

// 16.5 [buffer.const]

class const_buffer {
 public:
  const_buffer() noexcept : data_{nullptr}, size_{0} {}
  const_buffer(const void *p, size_t n) noexcept : data_{p}, size_{n} {}
  const_buffer(const mutable_buffer &b) noexcept
      : data_{b.data()}, size_{b.size()} {}

  const void *data() const noexcept { return data_; }
  size_t size() const noexcept { return size_; }

  const_buffer &operator+=(size_t n) noexcept {
    const size_t inc_size = std::min(n, size_);
    data_ = static_cast<const char *>(data_) + inc_size;
    size_ -= inc_size;
    return *this;
  }

 private:
  const void *data_;
  size_t size_;
};

// 16.6 [buffer.traits]

template <class T>
struct is_const_buffer_sequence;

template <class T>
constexpr bool is_const_buffer_sequence_v = is_const_buffer_sequence<T>::value;

template <class T>
struct is_mutable_buffer_sequence;

template <class T>
constexpr bool is_mutable_buffer_sequence_v =
    is_mutable_buffer_sequence<T>::value;

template <class T>
struct is_dynamic_buffer;

template <class T>
constexpr bool is_dynamic_buffer_v = is_dynamic_buffer<T>::value;

// 16.7 [buffer.seq.access]

inline const const_buffer *buffer_sequence_begin(
    const const_buffer &b) noexcept {
  return std::addressof(b);
}

inline const const_buffer *buffer_sequence_end(const const_buffer &b) noexcept {
  return std::addressof(b) + 1;
}

template <class C>
inline auto buffer_sequence_begin(C &c) noexcept -> decltype(c.begin()) {
  return c.begin();
}

template <class C>
inline auto buffer_sequence_begin(const C &c) noexcept -> decltype(c.begin()) {
  return c.begin();
}

template <class C>
inline auto buffer_sequence_end(C &c) noexcept -> decltype(c.end()) {
  return c.end();
}

template <class C>
inline auto buffer_sequence_end(const C &c) noexcept -> decltype(c.end()) {
  return c.end();
}

inline const mutable_buffer *buffer_sequence_begin(
    const mutable_buffer &b) noexcept {
  return std::addressof(b);
}

inline const mutable_buffer *buffer_sequence_end(
    const mutable_buffer &b) noexcept {
  return std::addressof(b) + 1;
}

namespace impl {

// const_buffer_sequence and mutable_buffer_sequence share the same
// requirements, just with different expected BufferTypes
template <class T, class BufferType,
          class Begin = decltype(net::buffer_sequence_begin(
              std::declval<typename std::add_lvalue_reference<T>::type>())),
          class End = decltype(net::buffer_sequence_end(
              std::declval<typename std::add_lvalue_reference<T>::type>()))>
using buffer_sequence_requirements = std::integral_constant<
    bool,
    std::conjunction<
        // check if buffer_sequence_begin(T &) and buffer_sequence_end(T &)
        // exist and return the same type
        std::is_same<Begin, End>,
        // check if ::value_type of the retval of buffer_sequence_begin() can be
        // converted into a BufferType
        std::is_convertible<typename std::iterator_traits<Begin>::value_type,
                            BufferType>>::value>;

template <class T, class BufferType, class = void>
struct is_buffer_sequence : std::false_type {};

template <class T, class BufferType>
struct is_buffer_sequence<
    T, BufferType, std::void_t<buffer_sequence_requirements<T, BufferType>>>
    : std::true_type {};

template <class T>
struct is_const_buffer_sequence
    : is_buffer_sequence<T, net::const_buffer>::type {};

template <class T>
struct is_mutable_buffer_sequence
    : is_buffer_sequence<T, net::mutable_buffer>::type {};

}  // namespace impl

template <class T>
struct is_mutable_buffer_sequence : impl::is_mutable_buffer_sequence<T> {};

template <class T>
struct is_const_buffer_sequence : impl::is_const_buffer_sequence<T> {};

namespace impl {
template <class T, class = void>
struct is_dynamic_buffer : std::false_type {};

/*
 * helper function for requirement checks.
 *
 * - function isn't implemented, just declared to type-check
 * - function params are default initialized, never have to be specified
 * - function params exist only to apply type-checks against them and pass them
 *   as parameter to decltype() like checks
 */
template <class T, class U = std::remove_const_t<T>>
auto dynamic_buffer_requirements(U *__x = nullptr, const U *__const_x = nullptr,
                                 size_t __n = 0)
    -> std::enable_if_t<std::conjunction<
        // is copy constructible
        std::is_copy_constructible<U>,
        // has a const_buffers_type that's a const_buffer_sequence
        is_const_buffer_sequence<typename T::const_buffers_type>,
        // has a mutable_buffers_type that's a mutable_buffer_sequence
        is_mutable_buffer_sequence<typename T::mutable_buffers_type>,
        // has 'size_t size() const' member
        std::is_same<decltype(__const_x->size()), size_t>,
        // has 'size_t max_size() const' member
        std::is_same<decltype(__const_x->max_size()), size_t>,
        // has 'size_t capacity() const' member
        std::is_same<decltype(__const_x->capacity()), size_t>,
        // has 'const_buffer_type data(size_t, size_t) const' member
        std::is_same<decltype(__const_x->data(__n, __n)),
                     typename T::const_buffers_type>,
        // has 'mutable_buffers_type data(size_t, size_t)' member
        std::is_same<decltype(__x->data(__n, __n)),
                     typename T::mutable_buffers_type>,
        // has 'void grow(size_t)' member
        std::is_void<decltype(__x->grow(__n))>,
        // has 'void shrink(size_t)' member
        std::is_void<decltype(__x->shrink(__n))>,
        // has 'void consume(size_t)' member
        std::is_void<decltype(__x->consume(__n))>>::value>;

template <class T>
struct is_dynamic_buffer<T, decltype(dynamic_buffer_requirements<T>())>
    : std::true_type {};
}  // namespace impl

template <class T>
struct is_dynamic_buffer : impl::is_dynamic_buffer<T>::type {};

// 16.8 [buffer.size]

template <class ConstBufferSequence>
inline size_t buffer_size(const ConstBufferSequence &buffers) noexcept {
  size_t total_size{0};
  auto end = buffer_sequence_end(buffers);

  for (auto i = buffer_sequence_begin(buffers); i != end; ++i) {
    const_buffer b(*i);

    total_size += b.size();
  }

  return total_size;
}

template <>
inline size_t buffer_size<const_buffer>(const const_buffer &b) noexcept {
  return b.size();
}

template <>
inline size_t buffer_size<mutable_buffer>(const mutable_buffer &b) noexcept {
  return b.size();
}

/**
 * copy bytes from a ConstBufferSequence to a MutableBufferSequence.
 *
 * copies min(max_bytes, buffer_size(src), buffer_size(dest)) bytes
 *
 * @param dest buffer-sequence to copy to
 * @param src buffer-sequence to copy from
 * @param max_size max bytes to copy
 * @return bytes transferred from src to dest
 *
 * see: 16.9 [buffer.copy]
 */
template <class MutableBufferSequence, class ConstBufferSequence>
size_t buffer_copy(const MutableBufferSequence &dest,
                   const ConstBufferSequence &src,
                   const size_t max_size) noexcept {
  size_t transfered{0};
  auto dest_cur = buffer_sequence_begin(dest);
  auto const dest_end = buffer_sequence_end(dest);
  auto src_cur = buffer_sequence_begin(src);
  auto const src_end = buffer_sequence_end(src);

  const_buffer src_buf;
  mutable_buffer dest_buf;

  while (transfered < max_size) {
    // if buffer is empty, fetch the next one, if there is one
    if (src_buf.size() == 0) {
      if (src_cur == src_end) break;
      src_buf = const_buffer(*src_cur++);
    }
    if (dest_buf.size() == 0) {
      if (dest_cur == dest_end) break;
      dest_buf = mutable_buffer(*dest_cur++);
    }

    size_t to_copy = std::min(dest_buf.size(), src_buf.size());
    to_copy = std::min(to_copy, max_size - transfered);

    // use std::copy() instead of std::memcpy() as it will be constexpr in C++20
    // and results in the same code as a memcpy() already
    std::copy(static_cast<const char *>(src_buf.data()),
              std::next(static_cast<const char *>(src_buf.data()), to_copy),
              static_cast<char *>(dest_buf.data()));

    src_buf += to_copy;
    dest_buf += to_copy;

    transfered += to_copy;
  }

  return transfered;
}

template <class MutableBufferSequence, class ConstBufferSequence>
size_t buffer_copy(const MutableBufferSequence &dest,
                   const ConstBufferSequence &src) noexcept {
  return buffer_copy(dest, src, std::numeric_limits<size_t>::max());
}

// 16.10 [buffer.arithmetic]

inline mutable_buffer operator+(const mutable_buffer &b, size_t n) noexcept {
  const size_t inc_size = std::min(n, b.size());

  return {static_cast<char *>(b.data()) + inc_size, b.size() - inc_size};
}
inline mutable_buffer operator+(size_t n, const mutable_buffer &b) noexcept {
  return operator+(b, n);
}

inline const_buffer operator+(const const_buffer &b, size_t n) noexcept {
  const size_t inc_size = std::min(n, b.size());
  return {static_cast<const char *>(b.data()) + inc_size, b.size() - inc_size};
}

inline const_buffer operator+(size_t n, const const_buffer &b) noexcept {
  return operator+(b, n);
}

// 16.11 [buffer.creation]

inline mutable_buffer buffer(void *p, size_t n) noexcept { return {p, n}; }
inline const_buffer buffer(const void *p, size_t n) noexcept { return {p, n}; }

inline mutable_buffer buffer(const mutable_buffer &b) noexcept { return b; }
inline mutable_buffer buffer(const mutable_buffer &b, size_t n) noexcept {
  return {b.data(), std::min(b.size(), n)};
}

inline const_buffer buffer(const const_buffer &b) noexcept { return b; }
inline const_buffer buffer(const const_buffer &b, size_t n) noexcept {
  return {b.data(), std::min(b.size(), n)};
}

namespace impl {
template <typename T>
inline mutable_buffer to_mutable_buffer(T *data, size_t n) {
  return {n ? data : nullptr, sizeof(*data) * n};
}

template <typename T>
inline const_buffer to_const_buffer(const T *data, size_t n) {
  return {n ? data : nullptr, sizeof(*data) * n};
}
}  // namespace impl

template <class T, size_t N>
inline mutable_buffer buffer(T (&data)[N]) noexcept {
  return impl::to_mutable_buffer(data, N);
}

template <class T, size_t N>
inline const_buffer buffer(const T (&data)[N]) noexcept {
  return impl::to_const_buffer(data, N);
}

template <class T, size_t N>
inline mutable_buffer buffer(std::array<T, N> &data) noexcept {
  return impl::to_mutable_buffer(data.data(), N);
}

template <class T, size_t N>
inline const_buffer buffer(std::array<const T, N> &data) noexcept {
  return impl::to_const_buffer(data.data(), N);
}

template <class T, size_t N>
inline const_buffer buffer(const std::array<T, N> &data) noexcept {
  return impl::to_const_buffer(data.data(), N);
}

template <class T, class Allocator>
inline mutable_buffer buffer(std::vector<T, Allocator> &data) noexcept {
  return impl::to_mutable_buffer(data.data(), data.size());
}

template <class T, class Allocator>
inline const_buffer buffer(const std::vector<T, Allocator> &data) noexcept {
  return impl::to_const_buffer(data.data(), data.size());
}

template <class CharT, class Traits, class Allocator>
inline mutable_buffer buffer(
    std::basic_string<CharT, Traits, Allocator> &data) noexcept {
  return data.empty() ? mutable_buffer{}
                      : impl::to_mutable_buffer(&data.front(), data.size());
}

template <class CharT, class Traits, class Allocator>
inline const_buffer buffer(
    const std::basic_string<CharT, Traits, Allocator> &data) noexcept {
  return data.empty() ? const_buffer{}
                      : impl::to_const_buffer(&data.front(), data.size());
}

template <class CharT, class Traits>
inline const_buffer buffer(
    const std::basic_string_view<CharT, Traits> &data) noexcept {
  return data.empty() ? const_buffer{}
                      : impl::to_const_buffer(data.data(), data.size());
}

template <class T, size_t N>
inline mutable_buffer buffer(T (&data)[N], size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, size_t N>
inline const_buffer buffer(const T (&data)[N], size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, size_t N>
inline mutable_buffer buffer(std::array<T, N> &data, size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, size_t N>
inline const_buffer buffer(std::array<const T, N> &data, size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, size_t N>
inline const_buffer buffer(const std::array<T, N> &data, size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, class Allocator>
inline mutable_buffer buffer(std::vector<T, Allocator> &data,
                             size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class T, class Allocator>
inline const_buffer buffer(const std::vector<T, Allocator> &data,
                           size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class CharT, class Traits, class Allocator>
inline mutable_buffer buffer(std::basic_string<CharT, Traits, Allocator> &data,
                             size_t n) noexcept {
  return buffer(buffer(data), n);
}

template <class CharT, class Traits, class Allocator>
inline const_buffer buffer(
    const std::basic_string<CharT, Traits, Allocator> &data,
    size_t n) noexcept {
  return buffer(buffer(data), n);
}

namespace impl {
// base for net::dynamic_vector_buffer and net::dynamic_string_buffer who share
// the same interface, just differ by storage type
template <class T>
class dynamic_buffer_base {
 public:
  using const_buffers_type = const_buffer;
  using mutable_buffers_type = mutable_buffer;

  explicit dynamic_buffer_base(T &v) noexcept
      : v_{v}, max_size_{v.max_size()} {}
  dynamic_buffer_base(T &v, size_t max_size) noexcept
      : v_{v}, max_size_{max_size} {}

  /**
   * number of bytes.
   */
  size_t size() const noexcept { return std::min(v_.size(), max_size_); }

  /**
   * max number of bytes.
   */
  size_t max_size() const noexcept { return max_size_; }

  /**
   * max number of bytes without requiring reallocation.
   */
  size_t capacity() const noexcept {
    return std::min(v_.capacity(), max_size_);
  }

  const_buffers_type data(size_t pos, size_t n) const noexcept {
    return buffer(buffer(v_, max_size_) + pos, n);
  }

  mutable_buffers_type data(size_t pos, size_t n) {
    return buffer(buffer(v_, max_size_) + pos, n);
  }

  /**
   * append bytes at the end.
   */
  void grow(size_t n) {
    if (size() > max_size() || max_size() - size() < n) {
      throw std::length_error("overflow");
    }
    v_.resize(v_.size() + n);
  }

  /**
   * remove bytes at the end.
   */
  void shrink(size_t n) {
    if (n >= size()) {
      v_.clear();
    } else {
      v_.resize(size() - n);
    }
  }
  /**
   * remove bytes at the start.
   */
  void consume(size_t n) {
    size_t m = std::min(n, size());
    if (m == size()) {
      v_.clear();
    } else {
      v_.erase(v_.begin(), std::next(v_.begin(), m));
    }
  }

 private:
  T &v_;
  const size_t max_size_;
};

}  // namespace impl

// 16.12 buffer.dynamic.vector

template <class T, class Allocator>
class dynamic_vector_buffer
    : public impl::dynamic_buffer_base<std::vector<T, Allocator>> {
 public:
  using impl::dynamic_buffer_base<
      std::vector<T, Allocator>>::dynamic_buffer_base;
};

// 16.13 buffer.dynamic.string

template <class CharT, class Traits, class Allocator>
class dynamic_string_buffer : public impl::dynamic_buffer_base<
                                  std::basic_string<CharT, Traits, Allocator>> {
 public:
  using impl::dynamic_buffer_base<
      std::basic_string<CharT, Traits, Allocator>>::dynamic_buffer_base;
};

// 16.14 [buffer.dynamic.creation]

template <class T, class Allocator>
dynamic_vector_buffer<T, Allocator> dynamic_buffer(
    std::vector<T, Allocator> &vec) noexcept {
  return dynamic_vector_buffer<T, Allocator>(vec);
}

template <class T, class Allocator>
dynamic_vector_buffer<T, Allocator> dynamic_buffer(
    std::vector<T, Allocator> &vec, size_t n) noexcept {
  return dynamic_vector_buffer<T, Allocator>(vec, n);
}

template <class CharT, class Traits, class Allocator>
dynamic_string_buffer<CharT, Traits, Allocator> dynamic_buffer(
    std::basic_string<CharT, Traits, Allocator> &str) noexcept {
  return dynamic_string_buffer<CharT, Traits, Allocator>(str);
}

template <class CharT, class Traits, class Allocator>
dynamic_string_buffer<CharT, Traits, Allocator> dynamic_buffer(
    std::basic_string<CharT, Traits, Allocator> &str, size_t n) noexcept {
  return dynamic_string_buffer<CharT, Traits, Allocator>(str, n);
}

// 17.2 [buffer.stream.transfer.all]

class transfer_all {
 public:
  size_t operator()(const std::error_code &ec, size_t /* unused */) const {
    if (!ec) return std::numeric_limits<size_t>::max();

    return 0;
  }
};

// 17.3 [buffer.stream.transfer.at.least]
class transfer_at_least {
 public:
  explicit transfer_at_least(size_t m) : minimum_{m} {}

  size_t operator()(const std::error_code &ec, size_t n) const {
    if (!ec && n < minimum_) return std::numeric_limits<size_t>::max();

    return 0;
  }

 private:
  size_t minimum_;
};

// 17.4 [buffer.stream.transfer.exactly]

class transfer_exactly {
 public:
  explicit transfer_exactly(size_t m) : exact_{m} {}

  /**
   * @returns bytes to transfer
   */
  size_t operator()(const std::error_code &ec, size_t n) const {
    // "unspecified non-zero number"
    constexpr size_t N = std::numeric_limits<size_t>::max();

    if (!ec && n < exact_) return std::min(exact_ - n, N);

    return 0;
  }

 private:
  size_t exact_;
};

// BufferSequence of prepared buffers
template <class BufferType>
class prepared_buffers {
 public:
  using value_type = BufferType;

  // for writev()/sendv() a std::array<> is a good enough as it maps to IOV_MAX
  // (which may be 16)
  //
  // note: for protocol decoding something similar is needed, but it shouldn't
  // be limited to the number of parts. A Generator would be nice.
  using storage_type = std::array<BufferType, 16>;
  using const_iterator = typename storage_type::const_iterator;
  using iterator = typename storage_type::iterator;

  const_iterator begin() const { return bufs_.begin(); }
  const_iterator end() const { return std::next(begin(), used_); }

  void push_back(value_type &&v) {
    if (size() == max_size()) {
      throw std::length_error("size() MUST be less than max_size().");
    }
    bufs_[used_++] = std::move(v);
  }

  // number of buffers
  size_t size() const noexcept { return used_; }
  // max number of buffers
  constexpr size_t max_size() const noexcept { return bufs_.size(); }

 private:
  storage_type bufs_;
  size_t used_{0};
};

// get sequence of buffers given byte-size from another sequence of buffers.
//
// input buffer sequence is unchanged.
//
// - BufferSequence: ConstBufferSequence or MutableBufferSequence
// - BufferType: net::const_buffer or net::mutable_buffer
//
// Note: mostly used for net::write(), net::read()
template <class BufferSequence, class BufferType>
class consuming_buffers {
 public:
  using prepared_buffer_type = prepared_buffers<BufferType>;

  consuming_buffers(const BufferSequence &buffers) : buffers_{buffers} {}

  /**
   * prepare a buffer sequence, skipping the already consumed bytes.
   *
   * @param max_size max bytes to take from the buffer sequence
   *
   * note: nax_size may be larger than the size of the buffer-sequence
   */
  prepared_buffer_type prepare(size_t max_size) {
    prepared_buffer_type to_bufs;

    auto from_cur = buffer_sequence_begin(buffers_);
    auto const from_end = buffer_sequence_end(buffers_);

    size_t to_skip = total_consumed();

    for (; (from_cur != from_end) && to_bufs.size() < to_bufs.max_size() &&
           max_size > 0;
         ++from_cur) {
      if (from_cur->size() > to_skip) {
        const size_t avail = from_cur->size() - to_skip;
        const size_t to_use = std::min(avail, max_size);
        to_bufs.push_back(
            net::buffer(BufferType(net::buffer(*from_cur)) + to_skip, to_use));
        to_skip = 0;
        max_size -= to_use;
      } else {
        to_skip -= from_cur->size();
      }
    }
    return to_bufs;
  }

  /**
   * mark bytes as consumed from the beginning of the unconsumed sequence.
   *
   * @param n bytes to consume
   *
   * note: n may be larger than the size of the buffer-sequence
   */
  void consume(size_t n) { total_consumed_ += n; }

  // sum of all consume()ed bytes
  size_t total_consumed() const { return total_consumed_; }

 private:
  const BufferSequence &buffers_;
  size_t total_consumed_{0};
};

// 17.5 [buffer.read]

template <class SyncReadStream, class MutableBufferSequence>
std::enable_if_t<is_mutable_buffer_sequence<MutableBufferSequence>::value,
                 stdx::expected<size_t, std::error_code>>
read(SyncReadStream &stream, const MutableBufferSequence &buffers) {
  static_assert(net::is_mutable_buffer_sequence<MutableBufferSequence>::value,
                "");
  return read(stream, buffers, transfer_all());
}

template <class SyncReadStream, class MutableBufferSequence,
          class CompletionCondition>
std::enable_if_t<is_mutable_buffer_sequence<MutableBufferSequence>::value,
                 stdx::expected<size_t, std::error_code>>
read(SyncReadStream &stream, const MutableBufferSequence &buffers,
     CompletionCondition cond) {
  static_assert(net::is_mutable_buffer_sequence<MutableBufferSequence>::value,
                "");

  std::error_code ec{};

  consuming_buffers<MutableBufferSequence, mutable_buffer> consumable(buffers);

  const size_t total_size = buffer_size(buffers);
  size_t to_transfer;

  while (0 != (to_transfer = cond(ec, consumable.total_consumed())) &&
         (consumable.total_consumed() < total_size)) {
    auto res = stream.read_some(consumable.prepare(to_transfer));
    if (!res) return res;

    consumable.consume(*res);
  }

  return {consumable.total_consumed()};
}

template <class SyncReadStream, class DynamicBuffer>
std::enable_if_t<is_dynamic_buffer<std::decay_t<DynamicBuffer>>::value,
                 stdx::expected<size_t, std::error_code>>
read(SyncReadStream &stream, DynamicBuffer &&b) {
  return read(stream, b, transfer_all());
}

template <class SyncReadStream, class DynamicBuffer, class CompletionCondition>
std::enable_if_t<is_dynamic_buffer<std::decay_t<DynamicBuffer>>::value,
                 stdx::expected<size_t, std::error_code>>
read(SyncReadStream &stream, DynamicBuffer &&b, CompletionCondition cond) {
  std::error_code ec{};

  size_t transferred{};
  size_t to_transfer;

  while (0 != (to_transfer = cond(ec, transferred)) &&
         b.size() != b.max_size()) {
    auto orig_size = b.size();
    // if there is space available, use that, if not grow by 4k
    auto avail = b.capacity() - orig_size;
    size_t grow_size = avail ? avail : 4 * 1024;
    size_t space_left = b.max_size() - b.size();
    // limit grow-size by possible space-left
    grow_size = std::min(grow_size, space_left);
    // limit grow-size by how much data we still have to read
    grow_size = std::min(grow_size, to_transfer);

    b.grow(grow_size);
    auto res = stream.read_some(b.data(orig_size, grow_size));
    if (!res) {
      b.shrink(grow_size);

      // if socket was non-blocking and some bytes where already read, return
      // the success
      const auto ec = res.error();
      if ((ec == make_error_condition(
                     std::errc::resource_unavailable_try_again) ||
           ec == make_error_condition(std::errc::operation_would_block) ||
           ec == net::stream_errc::eof) &&
          transferred != 0) {
        return transferred;
      }
      return res;
    }

    transferred += *res;

    b.shrink(grow_size - *res);
  }

  return {transferred};
}

// 17.6 [buffer.async.read]
template <class AsyncReadStream, class DynamicBuffer, class CompletionCondition,
          class CompletionToken>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value, void> async_read(
    AsyncReadStream &stream, DynamicBuffer &&b,
    CompletionCondition completion_condition, CompletionToken &&token) {
  async_completion<CompletionToken, void(std::error_code, size_t)> init{token};

  using compl_handler_type = typename decltype(init)::completion_handler_type;

  class Completor {
   public:
    Completor(AsyncReadStream &stream, DynamicBuffer &&b,
              CompletionCondition compl_cond,
              compl_handler_type &&compl_handler)
        : stream_{stream},
          b_{std::forward<DynamicBuffer>(b)},
          compl_cond_{compl_cond},
          compl_handler_(std::forward<compl_handler_type>(compl_handler)) {}

    Completor(const Completor &) = delete;
    Completor(Completor &&) = default;

    void operator()(std::error_code ec) {
      if (ec) {
        compl_handler_(ec, 0);
        return;
      }

      const auto res = net::read(stream_, b_, compl_cond_);

      if (!res) {
        compl_handler_(res.error(), 0);
      } else {
        compl_handler_({}, res.value());
      }

      return;
    }

   private:
    AsyncReadStream &stream_;
    DynamicBuffer b_;
    CompletionCondition compl_cond_;
    compl_handler_type compl_handler_;
  };

  stream.async_wait(
      net::impl::socket::wait_type::wait_read,
      Completor(stream, std::forward<DynamicBuffer>(b), completion_condition,
                std::move(init.completion_handler)));

  return init.result.get();
}

template <class AsyncReadStream, class DynamicBuffer, class CompletionToken>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value, void> async_read(
    AsyncReadStream &stream, DynamicBuffer &&b, CompletionToken &&token) {
  return async_read(stream, std::forward<DynamicBuffer>(b), net::transfer_all(),
                    std::forward<CompletionToken>(token));
}

// 17.7 [buffer.write]

template <class SyncWriteStream, class ConstBufferSequence>
std::enable_if_t<is_const_buffer_sequence<ConstBufferSequence>::value,
                 stdx::expected<size_t, std::error_code>>
write(SyncWriteStream &stream, const ConstBufferSequence &buffers) {
  return write(stream, buffers, transfer_all());
}

template <class SyncWriteStream, class ConstBufferSequence,
          class CompletionCondition>
std::enable_if_t<is_const_buffer_sequence<ConstBufferSequence>::value,
                 stdx::expected<size_t, std::error_code>>
write(SyncWriteStream &stream, const ConstBufferSequence &buffers,
      CompletionCondition cond) {
  std::error_code ec{};

  consuming_buffers<ConstBufferSequence, const_buffer> consumable(buffers);

  const size_t total_size = buffer_size(buffers);
  size_t to_transfer;

  while (0 != (to_transfer = cond(ec, consumable.total_consumed())) &&
         (consumable.total_consumed() < total_size)) {
    auto res = stream.write_some(consumable.prepare(to_transfer));
    if (!res) {
      ec = res.error();
    } else {
      consumable.consume(*res);
    }
  }

  // if there is an error and it isn't EAGAIN|EWOULDBLOCK, return it.
  // if it is EAGAIN|EWOULDBLOCK return it only if nothing was transferred.
  if (ec &&
      ((ec != make_error_condition(std::errc::resource_unavailable_try_again) &&
        ec != make_error_condition(std::errc::operation_would_block)) ||
       consumable.total_consumed() == 0)) {
    return stdx::make_unexpected(ec);
  } else {
    return {consumable.total_consumed()};
  }
}

template <class SyncWriteStream, class DynamicBuffer>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value,
                 stdx::expected<size_t, std::error_code>>
write(SyncWriteStream &stream, DynamicBuffer &&b) {
  return write(stream, std::forward<DynamicBuffer>(b), transfer_all());
}

template <class SyncWriteStream, class DynamicBuffer, class CompletionCondition>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value,
                 stdx::expected<size_t, std::error_code>>
write(SyncWriteStream &stream, DynamicBuffer &&b, CompletionCondition cond) {
  std::error_code ec{};

  size_t to_transfer;
  size_t transferred{};

  while (0 != (to_transfer = cond(ec, transferred)) && (b.size() != 0)) {
    auto res = stream.write_some(b.data(0, std::min(b.size(), to_transfer)));
    if (!res) {
      ec = res.error();
    } else {
      transferred += *res;

      b.consume(*res);
    }
  }

  // if there is an error and it isn't EAGAIN|EWOULDBLOCK, return it.
  // if it is EAGAIN|EWOULDBLOCK return it only if nothing was transferred.
  if (ec &&
      ((ec != make_error_condition(std::errc::resource_unavailable_try_again) &&
        ec != make_error_condition(std::errc::operation_would_block)) ||
       transferred == 0)) {
    return stdx::make_unexpected(ec);
  } else {
    return transferred;
  }
}

// 17.8 [buffer.async.write]

template <class AsyncWriteStream, class DynamicBuffer,
          class CompletionCondition, class CompletionToken>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value, void> async_write(
    AsyncWriteStream &stream, DynamicBuffer &&b, CompletionCondition cond,
    CompletionToken &&token) {
  async_completion<CompletionToken, void(std::error_code, size_t)> init{token};

  using compl_handler_type = typename decltype(init)::completion_handler_type;

  class Completor {
   public:
    Completor(AsyncWriteStream &stream, DynamicBuffer &&b,
              CompletionCondition cond, compl_handler_type &&compl_handler)
        : stream_{stream},
          b_{std::forward<DynamicBuffer>(b)},
          cond_{cond},
          compl_handler_(std::forward<compl_handler_type>(compl_handler)) {}

    Completor(const Completor &) = delete;
    Completor(Completor &&) = default;

    void operator()(std::error_code ec) {
      if (ec) {
        compl_handler_(ec, 0);
        return;
      }

      const auto res =
          net::write(stream_, std::forward<DynamicBuffer>(b_), cond_);

      if (!res) {
        compl_handler_(res.error(), 0);
      } else {
        compl_handler_({}, res.value());
      }

      return;
    }

   private:
    AsyncWriteStream &stream_;
    DynamicBuffer b_;
    CompletionCondition cond_;
    compl_handler_type compl_handler_;
  };

  stream.async_wait(net::impl::socket::wait_type::wait_write,
                    Completor(stream, std::forward<DynamicBuffer>(b), cond,
                              std::move(init.completion_handler)));

  return init.result.get();
}
template <class AsyncWriteStream, class DynamicBuffer, class CompletionToken>
std::enable_if_t<is_dynamic_buffer<DynamicBuffer>::value, void> async_write(
    AsyncWriteStream &stream, DynamicBuffer &&b, CompletionToken &&token) {
  return async_write(stream, std::forward<DynamicBuffer>(b),
                     net::transfer_all(), std::forward<CompletionToken>(token));
}

// 17.9 [buffer.read.until] not-implemented-ye

// 17.10 [buffer.async.read.until] not-implemented-yet

}  // namespace net

#endif
