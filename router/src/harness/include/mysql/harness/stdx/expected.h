/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_EXPECTED_H_
#define MYSQL_HARNESS_STDX_EXPECTED_H_

// implementation of http://wg21.link/p0323
//
// see http://wg21.link/p0762

#include <new>      // ::operator new
#include <utility>  // std::forward

#include "mysql/harness/stdx/type_traits.h"
#if defined(__GNUC__) || defined(__clang__) || defined(__SUNPRO_CC)
#define RESO_ASSUME(x) \
  if (!(x)) __builtin_unreachable();
#elif defined(_MSC_VER)
#define RESO_ASSUME(x) \
  if (!(x)) __assume(0);
#else
#define RESO_ASSUME(x) \
  if (!(x)) {          \
  };
#endif

/* workaround default-constructor of std::unique_ptr<T, D> triggering a
 * static-exception when it is tested for "std::is_default_constructible"
 *
 * The problem exists in GCC's libstdc++ up to 7.0.0 and is tracked by C++ std
 * as DR 2801
 *
 *   http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#2801
 *
 * It is fixed in GCC-7.1.0 and later:
 *
 * https://gcc.gnu.org/legacy-ml/gcc-cvs/2017-01/msg00068.html
 */

#include <memory>
namespace std {
template <class T>
struct is_default_constructible<std::unique_ptr<T, void (*)(T *)>>
    : std::false_type {};
}  // namespace std

namespace stdx {

template <typename E>
class unexpected {
 public:
  static_assert(!std::is_same<E, void>::value, "E must not be void");

  using error_type = E;

  unexpected() = delete;

  constexpr explicit unexpected(error_type &&e) : error_{std::move(e)} {}

  constexpr explicit unexpected(const error_type &e) : error_{e} {}

  constexpr error_type &value() &noexcept { return error_; }
  constexpr const error_type &value() const &noexcept { return error_; }
  constexpr error_type &&value() &&noexcept { return std::move(error_); }
  constexpr const error_type &&value() const &&noexcept {
    return std::move(error_);
  }

 private:
  error_type error_;
};

// if E is void, we need no storage, but we need the wrapper
template <>
class unexpected<void> {};

template <typename E>
constexpr auto make_unexpected(E &&e) -> unexpected<std::decay_t<E>> {
  return unexpected<std::decay_t<E>>(std::forward<E>(e));
}

constexpr auto make_unexpected() { return unexpected<void>{}; }

namespace base {
template <class T, class E>
union storage_t {
  using value_type = T;
  using error_type = E;

  storage_t() {}
  ~storage_t() {}

  template <bool B = std::is_default_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value() {
    new (&value_) value_type();
  }

  template <bool B = std::is_copy_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value(value_type const &e) {
    new (&value_) value_type(e);
  }

  template <bool B = std::is_move_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value(value_type &&e) {
    new (&value_) value_type(std::move(e));
  }

  // enable inplace construction of value_type, if the T supports it
  template <class... Args,
            typename std::enable_if_t<
                std::is_constructible<T, Args &&...>::value, void *> = nullptr>
  void construct_value(in_place_t, Args &&... args) {
    new (&value_) value_type(std::forward<Args>(args)...);
  }

  void destruct_value() { value_.~value_type(); }

  void construct_error(error_type const &e) { new (&error_) error_type(e); }
  void construct_error(error_type &&e) {
    new (&error_) error_type(std::move(e));
  }

  // enable inplace construction of error, if the E supports it
  template <class... Args, typename std::enable_if_t<std::is_constructible<
                               E, Args &&...>::value> * = nullptr>
  void construct_error(in_place_t, Args &&... args) {
    new (&error_) error_type(std::forward<Args>(args)...);
  }

  void destruct_error() { error_.~error_type(); }

  constexpr const value_type &value() const & { return value_; }
  constexpr const value_type &&value() const && { return std::move(value_); }
  value_type &value() & { return value_; }
  constexpr value_type &&value() && { return std::move(value_); }

  const value_type *value_ptr() const { return &value_; }
  value_type *value_ptr() { return &value_; }

  constexpr const error_type &error() const & { return error_; }
  constexpr const error_type &&error() const && { return std::move(error_); }
  constexpr error_type &error() & { return error_; }
  constexpr error_type &&error() && { return std::move(error_); }

 private:
  value_type value_;
  error_type error_;
};

template <class T>
union storage_t<T, void> {
  using value_type = T;
  using error_type = void;

  storage_t() {}
  ~storage_t() {}

  template <bool B = std::is_default_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value() {
    new (&value_) value_type();
  }

  template <bool B = std::is_copy_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value(value_type const &e) {
    new (&value_) value_type(e);
  }

  template <bool B = std::is_move_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  void construct_value(value_type &&e) {
    new (&value_) value_type(std::move(e));
  }

  // enable inplace construction of value_type, if the T supports it
  template <class... Args,
            typename std::enable_if_t<
                std::is_constructible<T, Args &&...>::value, void *> = nullptr>
  void construct_value(in_place_t, Args &&... args) {
    new (&value_) value_type(std::forward<Args>(args)...);
  }

  void destruct_value() { value_.~value_type(); }

  constexpr const value_type &value() const & { return value_; }
  constexpr const value_type &&value() const && { return std::move(value_); }
  value_type &value() & { return value_; }
  constexpr value_type &&value() && { return std::move(value_); }

  const value_type *value_ptr() const { return &value_; }
  value_type *value_ptr() { return &value_; }

 private:
  value_type value_;
};

/**
 * specialized storage for <void, E>.
 *
 * as 'value' is void, all related functions are removed.
 */
template <typename E>
union storage_t<void, E> {
  using value_type = void;
  using error_type = E;

  static_assert(!std::is_void<E>::value, "E must not be void");

  storage_t() {}
  ~storage_t() {}

  void construct_error(error_type const &e) { new (&error_) error_type(e); }
  void construct_error(error_type &&e) {
    new (&error_) error_type(std::move(e));
  }

  // enable inplace construction of error, if the E supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible<E, Args &&...>::value> * = nullptr>
  void construct_error(in_place_t, Args &&... args) {
    new (&error_) error_type(std::forward<Args>(args)...);
  }

  void destruct_error() { error_.~error_type(); }

  const error_type &error() const & { return error_; }
  error_type &error() & { return error_; }
  constexpr const error_type &&error() const && { return std::move(error_); }
  constexpr error_type &&error() && { return std::move(error_); }

 private:
  error_type error_;
};

// member_policy to disable implicit constructors and assignment operations
enum class member_policy {
  none = 0,
  copy = 1 << 1,
  move = 1 << 2,
};

constexpr member_policy operator|(member_policy x, member_policy y) {
  using int_type = std::underlying_type<member_policy>::type;

  return static_cast<member_policy>(static_cast<int_type>(x) |
                                    static_cast<int_type>(y));
}

// control creation of default-constructors
template <bool = true>
struct default_ctor_base {
  constexpr default_ctor_base() noexcept = default;

  constexpr default_ctor_base(const default_ctor_base &) = default;
  constexpr default_ctor_base(default_ctor_base &&) = default;

  default_ctor_base &operator=(const default_ctor_base &) noexcept = default;
  default_ctor_base &operator=(default_ctor_base &&) noexcept = default;
};

template <>
struct default_ctor_base<false> {
  constexpr default_ctor_base() noexcept = delete;

  constexpr default_ctor_base(const default_ctor_base &) = default;
  constexpr default_ctor_base(default_ctor_base &&) = default;

  default_ctor_base &operator=(const default_ctor_base &) noexcept = default;
  default_ctor_base &operator=(default_ctor_base &&) noexcept = default;
};

// control creation of copy and move-constructors
template <member_policy>
struct ctor_base;

template <>
struct ctor_base<member_policy::none> {
  constexpr ctor_base() noexcept = default;

  constexpr ctor_base(const ctor_base &) = delete;
  constexpr ctor_base(ctor_base &&) = delete;

  ctor_base &operator=(const ctor_base &) noexcept = default;
  ctor_base &operator=(ctor_base &&) noexcept = default;
};

template <>
struct ctor_base<member_policy::copy> {
  constexpr ctor_base() noexcept = default;

  constexpr ctor_base(const ctor_base &) = default;
  constexpr ctor_base(ctor_base &&) = delete;

  ctor_base &operator=(const ctor_base &) noexcept = default;
  ctor_base &operator=(ctor_base &&) noexcept = default;
};

template <>
struct ctor_base<member_policy::move> {
  constexpr ctor_base() noexcept = default;

  constexpr ctor_base(const ctor_base &) = delete;
  constexpr ctor_base(ctor_base &&) = default;

  ctor_base &operator=(const ctor_base &) noexcept = default;
  ctor_base &operator=(ctor_base &&) noexcept = default;
};

template <>
struct ctor_base<member_policy::move | member_policy::copy> {
  constexpr ctor_base() noexcept = default;

  constexpr ctor_base(const ctor_base &) = default;
  constexpr ctor_base(ctor_base &&) = default;

  ctor_base &operator=(const ctor_base &) noexcept = default;
  ctor_base &operator=(ctor_base &&) noexcept = default;
};

// control creation of copy and move-assignment operators
template <member_policy>
struct assign_base;

template <>
struct assign_base<member_policy::none> {
  constexpr assign_base() noexcept = default;

  constexpr assign_base(const assign_base &) = default;
  constexpr assign_base(assign_base &&) = default;

  assign_base &operator=(const assign_base &) noexcept = delete;
  assign_base &operator=(assign_base &&) noexcept = delete;
};

template <>
struct assign_base<member_policy::copy> {
  constexpr assign_base() noexcept = default;

  constexpr assign_base(const assign_base &) = default;
  constexpr assign_base(assign_base &&) = default;

  assign_base &operator=(const assign_base &) noexcept = default;
  assign_base &operator=(assign_base &&) noexcept = delete;
};

template <>
struct assign_base<member_policy::move> {
  constexpr assign_base() noexcept = default;

  constexpr assign_base(const assign_base &) = default;
  constexpr assign_base(assign_base &&) = default;

  assign_base &operator=(const assign_base &) noexcept = delete;
  assign_base &operator=(assign_base &&) noexcept = default;
};

template <>
struct assign_base<member_policy::copy | member_policy::move> {
  constexpr assign_base() noexcept = default;

  constexpr assign_base(const assign_base &) = default;
  constexpr assign_base(assign_base &&) = default;

  assign_base &operator=(const assign_base &) noexcept = default;
  assign_base &operator=(assign_base &&) noexcept = default;
};

template <class B>
using not_ = stdx::negation<B>;

template <class... B>
using and_ = stdx::conjunction<B...>;

template <class... B>
using or_ = stdx::disjunction<B...>;

// enable copy constructor if T and E are copy-constructible or void
// enable move constructor if T and E are move-constructible or void
template <class T, class E>
using select_ctor_base =
    ctor_base<(and_<or_<std::is_void<T>, std::is_copy_constructible<T>>,
                    or_<std::is_void<E>, std::is_copy_constructible<E>>>::value
                   ? member_policy::copy
                   : member_policy::none) |
              (and_<or_<std::is_void<T>, std::is_move_constructible<T>>,
                    or_<std::is_void<E>, std::is_move_constructible<E>>>::value
                   ? member_policy::move
                   : member_policy::none)>;

// enable copy assignment if T and E are (copy-constructible and
// copy-assignable) or void
//
// enable move assignment if T and E are
// (move-constructible and move-assignable) or void
template <class T, class E>
using select_assign_base = assign_base<
    (and_<or_<std::is_void<T>,
              and_<std::is_copy_constructible<T>, std::is_copy_assignable<T>>>,
          or_<std::is_void<E>, and_<std::is_copy_constructible<E>,
                                    std::is_copy_assignable<E>>>>::value
         ? member_policy::copy
         : member_policy::none) |
    (or_<std::is_void<T>,
         and_<std::is_move_constructible<T>, std::is_move_assignable<T>>>::value
         ? member_policy::move
         : member_policy::none)>;

}  // namespace base

class ExpectedImplBase {
 public:
  constexpr explicit ExpectedImplBase(bool has_value) noexcept
      : has_value_{has_value} {}

  constexpr bool has_value() const { return has_value_; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  void swap(ExpectedImplBase &other) noexcept {
    using std::swap;

    swap(has_value_, other.has_value_);
  }

 private:
  bool has_value_;
};

template <class T, class E>
class ExpectedImpl : public ExpectedImplBase {
 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  template <bool B = std::is_default_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  constexpr ExpectedImpl() : ExpectedImplBase{true} {
    storage_.construct_value();
  }

  constexpr ExpectedImpl(const value_type &v) : ExpectedImplBase{true} {
    storage_.construct_value(v);
  }
  constexpr ExpectedImpl(value_type &&v) : ExpectedImplBase{true} {
    storage_.construct_value(std::move(v));
  }

  // enable inplace construction of value_type, if the T supports it
  template <
      class... Args
#if !defined(__SUNPRO_CC)
      // disabled the 'is_constructible' check as it triggers:
      //
      //  >> Assertion:   (../lnk/substitute.cc, line 1131)
      //	      while processing ./test_expected.cc at line 4.
      //
      // with devstudio-12.6's CC
      //
      // That means:
      //
      // stdx::expected<std::string, void> res(stdx::in_place, 1.0); will report
      // the error for:
      //
      //   Could not find a match for stdx::base::storage_t<std::string,
      //   double>::construct_value(const stdx::in_place_t, double)
      //
      // Instead for
      //
      //   Could not find a match for stdx::base::ExpectedImpl<std::string,
      //   double>::ExpectedImpl(const stdx::in_place_t, double)
      //
      // as the same check is done in construct_value() and does not trigger
      // the assertion.
      ,
      typename std::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
          nullptr
#endif
      >
  constexpr ExpectedImpl(in_place_t, Args &&... args) : ExpectedImplBase{true} {
    storage_.construct_value(stdx::in_place, std::forward<Args>(args)...);
  }

  constexpr ExpectedImpl(const ExpectedImpl &other)
      : ExpectedImplBase{other.has_value()} {
    if (has_value()) {
      storage_.construct_value(other.storage_.value());
    } else {
      storage_.construct_error(other.storage_.error());
    }
  }

  constexpr ExpectedImpl(ExpectedImpl &&other) noexcept(
      std::is_nothrow_move_constructible<E>::value
          &&std::is_nothrow_move_constructible<T>::value)
      : ExpectedImplBase{other.has_value()} {
    if (has_value()) {
      storage_.construct_value(std::move(other.storage_.value()));
    } else {
      storage_.construct_error(std::move(other.storage_.error()));
    }
  }

  constexpr ExpectedImpl(const unexpected<E> &e) : ExpectedImplBase{false} {
    storage_.construct_error(e.value());
  }

  constexpr ExpectedImpl(unexpected<E> &&e) : ExpectedImplBase{false} {
    storage_.construct_error(std::move(e.value()));
  }

  ExpectedImpl &operator=(ExpectedImpl const &other) {
    ExpectedImpl(other).swap(*this);

    return *this;
  }

  ExpectedImpl &operator=(ExpectedImpl &&other) {
    ExpectedImpl(std::move(other)).swap(*this);

    return *this;
  }

  // destruct
  ~ExpectedImpl() {
    if (has_value()) {
      storage_.destruct_value();
    } else {
      storage_.destruct_error();
    }
  }

  //
  template <class U = T, class G = E>
  typename std::enable_if_t<
#if defined(__cpp_lib_is_swappable)
      std::is_swappable<U>::value && std::is_swappable<G>::value &&
#endif
      (std::is_move_constructible<U>::value ||
       std::is_move_constructible<G>::value)>
  swap(ExpectedImpl &other) noexcept(
      std::is_nothrow_move_constructible<T>::value
          &&std::is_nothrow_move_constructible<E>::value
#if defined(__cpp_lib_is_swappable)
              &&std::is_nothrow_swappable<T &>::value
                  &&std::is_nothrow_swappable<E &>::value
#endif
  ) {
    using std::swap;

    if (bool(*this) && bool(other)) {
      swap(storage_.value(), other.storage_.value());
    } else if (!bool(*this) && !bool(other)) {
      swap(storage_.error(), other.storage_.error());
    } else if (bool(*this) && !bool(other)) {
      error_type t{std::move(other.error())};

      other.storage_.destruct_error();
      other.storage_.construct_value(std::move(storage_.value()));
      storage_.destruct_value();
      storage_.construct_error(std::move(t));

      swap(static_cast<ExpectedImplBase &>(*this),
           static_cast<ExpectedImplBase &>(other));
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  // value accessors

  constexpr const value_type &value() const & { return storage_.value(); }
  constexpr const value_type &&value() const && {
    return std::move(storage_.value());
  }
  value_type &value() & { return storage_.value(); }
  value_type &&value() && { return std::move(storage_.value()); }

  // unchecked value access
  value_type &operator*() & {
    RESO_ASSUME(has_value());

    return storage_.value();
  }
  constexpr const value_type &operator*() const & {
    RESO_ASSUME(has_value());

    return storage_.value();
  }

  value_type *operator->() {
    RESO_ASSUME(has_value());

    return storage_.value_ptr();
  }
  constexpr const value_type *operator->() const {
    RESO_ASSUME(has_value());

    return storage_.value_ptr();
  }

  template <class U>
  constexpr value_type value_or(U &&v) const & {
    static_assert(std::is_copy_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be copy-constructible and convertible from U&&");

    return has_value() ? **this : static_cast<T>(std::forward<U>(v));
  }

  template <class U>
  constexpr value_type value_or(U &&v) && {
    static_assert(std::is_move_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be move-constructible and convertible from U&&");

    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
  }

  // error accessors
  constexpr const error_type &error() const & {
    RESO_ASSUME(!has_value());
    return storage_.error();
  }
  constexpr const error_type &&error() const && {
    RESO_ASSUME(!has_value());
    return std::move(storage_.error());
  }
  constexpr error_type &error() & {
    RESO_ASSUME(!has_value());
    return storage_.error();
  }
  constexpr error_type &&error() && {
    RESO_ASSUME(!has_value());
    return std::move(storage_.error());
  }

  constexpr unexpected_type get_unexpected() const {
    return make_unexpected(storage_.error());
  }

 private:
  base::storage_t<T, E> storage_;
};

// specialization for T=void
template <class E>
class ExpectedImpl<void, E> : public ExpectedImplBase {
 public:
  using value_type = void;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  constexpr ExpectedImpl() noexcept : ExpectedImplBase{true} {}

  constexpr ExpectedImpl(const ExpectedImpl &other)
      : ExpectedImplBase{other.has_value()} {
    if (!has_value()) {
      storage_.construct_error(other.storage_.error());
    }
  }

  constexpr ExpectedImpl(ExpectedImpl &&other) noexcept(
      std::is_nothrow_move_constructible<E>::value)
      : ExpectedImplBase{other.has_value()} {
    if (!has_value()) {
      storage_.construct_error(std::move(other.storage_.error()));
    }
  }

  ExpectedImpl &operator=(ExpectedImpl const &other) {
    ExpectedImpl(other).swap(*this);

    return *this;
  }

  ExpectedImpl &operator=(ExpectedImpl &&other) {
    ExpectedImpl(std::move(other)).swap(*this);

    return *this;
  }

  constexpr ExpectedImpl(const unexpected<E> &e) : ExpectedImplBase{false} {
    storage_.construct_error(e.value());
  }

  constexpr ExpectedImpl(unexpected<E> &&e) : ExpectedImplBase{false} {
    storage_.construct_error(std::move(e.value()));
  }

  // destruct
  ~ExpectedImpl() {
    if (!has_value()) {
      storage_.destruct_error();
    }
  }

  // swap
  template <class G = E, std::enable_if_t<
#if defined(__cpp_lib_is_swappable)
                             std::is_swappable<G>::value &&
#endif
                                 std::is_move_constructible<G>::value,
                             void *> = nullptr>
  void swap(ExpectedImpl &other) noexcept(
      std::is_nothrow_move_constructible<G>::value
#if defined(__cpp_lib_is_swappable)
          &&std::is_nothrow_swappable<G &>::value
#endif
  ) {
    using std::swap;

    if (bool(*this) && bool(other)) {
      // both types have void value, nothing to swap
    } else if (!bool(*this) && !bool(other)) {
      swap(storage_.error(), other.storage_.error());
    } else if (bool(*this) && !bool(other)) {
      // we are value, but have no storage. Nothing to destroy on our side
      // before we move the error-type over
      storage_.construct_error(std::move(other.error()));

      swap(static_cast<ExpectedImplBase &>(*this),
           static_cast<ExpectedImplBase &>(other));
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  // error accesors
  constexpr const error_type &error() const & {
    RESO_ASSUME(!has_value());
    return storage_.error();
  }
  constexpr const error_type &&error() const && {
    RESO_ASSUME(!has_value());
    return std::move(storage_.error());
  }
  constexpr error_type &error() & {
    RESO_ASSUME(!has_value());
    return storage_.error();
  }
  constexpr error_type &&error() && {
    RESO_ASSUME(!has_value());
    return std::move(storage_.error());
  }

  constexpr unexpected_type get_unexpected() const {
    return make_unexpected(storage_.error());
  }

 private:
  base::storage_t<void, E> storage_;
};

template <class T>
class ExpectedImpl<T, void> : public ExpectedImplBase {
 public:
  using value_type = T;
  using error_type = void;
  using unexpected_type = unexpected<error_type>;

  template <bool B = std::is_default_constructible<T>::value,
            std::enable_if_t<B> * = nullptr>
  constexpr ExpectedImpl() : ExpectedImplBase{true} {
    storage_.construct_value();
  }

  constexpr ExpectedImpl(const value_type &v) : ExpectedImplBase{true} {
    storage_.construct_value(v);
  }
  constexpr ExpectedImpl(value_type &&v) : ExpectedImplBase{true} {
    storage_.construct_value(std::move(v));
  }

  // enable inplace construction of value_type, if the T supports it
  template <class... Args
#if !defined(__SUNPRO_CC)
            // see the generic ExpectedImpl(in_place_t, ...) why this check is
            // disabled.
            ,
            typename std::enable_if_t<
                std::is_constructible<T, Args &&...>::value> * = nullptr
#endif
            >
  constexpr ExpectedImpl(in_place_t, Args &&... args) : ExpectedImplBase{true} {
    storage_.construct_value(stdx::in_place, std::forward<Args>(args)...);
  }

  constexpr ExpectedImpl(const ExpectedImpl &other)
      : ExpectedImplBase{other.has_value()} {
    if (has_value()) {
      storage_.construct_value(other.storage_.value());
    }
  }

  constexpr ExpectedImpl(ExpectedImpl &&other) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : ExpectedImplBase{other.has_value()} {
    if (has_value()) {
      storage_.construct_value(std::move(other.storage_.value()));
    }
  }

  constexpr ExpectedImpl(const unexpected_type &) : ExpectedImplBase{false} {}

  constexpr ExpectedImpl(unexpected_type &&) : ExpectedImplBase{false} {}

  ExpectedImpl &operator=(ExpectedImpl const &other) {
    ExpectedImpl(other).swap(*this);

    return *this;
  }

  ExpectedImpl &operator=(ExpectedImpl &&other) {
    ExpectedImpl(std::move(other)).swap(*this);

    return *this;
  }

  // destruct
  ~ExpectedImpl() {
    if (has_value()) {
      storage_.destruct_value();
    }
  }

  //
  template <class U = T>
  typename std::enable_if_t<
#if defined(__cpp_lib_is_swappable)
      std::is_swappable<U>::value &&
#endif
      std::is_move_constructible<U>::value>
  swap(ExpectedImpl &other) noexcept(
      std::is_nothrow_move_constructible<T>::value
#if defined(__cpp_lib_is_swappable)
          &&std::is_nothrow_swappable<T &>::value
#endif
  ) {
    using std::swap;

    if (bool(*this) && bool(other)) {
      swap(storage_.value(), other.storage_.value());
    } else if (!bool(*this) && !bool(other)) {
      // no storage for error
    } else if (bool(*this) && !bool(other)) {
      other.storage_.construct_value(std::move(storage_.value()));
      // storage_.destruct_value();

      swap(static_cast<ExpectedImplBase &>(*this),
           static_cast<ExpectedImplBase &>(other));
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  // value accessors

  constexpr const value_type &value() const & { return storage_.value(); }
  constexpr const value_type &&value() const && {
    return std::move(storage_.value());
  }
  value_type &value() & { return storage_.value(); }
  value_type &&value() && { return std::move(storage_.value()); }

  // uncheck value access
  value_type &operator*() & {
    RESO_ASSUME(has_value());

    return storage_.value();
  }
  constexpr const value_type &operator*() const & {
    RESO_ASSUME(has_value());

    return storage_.value();
  }

  value_type *operator->() {
    RESO_ASSUME(has_value());

    return storage_.value_ptr();
  }
  constexpr const value_type *operator->() const {
    RESO_ASSUME(has_value());

    return storage_.value_ptr();
  }

  template <class U>
  constexpr value_type value_or(U &&v) const & {
    static_assert(std::is_copy_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be copy-constructible and convertible from U&&");

    return has_value() ? **this : static_cast<T>(std::forward<U>(v));
  }

  template <class U>
  constexpr value_type value_or(U &&v) && {
    static_assert(std::is_move_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be move-constructible and convertible from U&&");

    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
  }

  constexpr unexpected_type get_unexpected() const { return make_unexpected(); }

 private:
  base::storage_t<value_type, error_type> storage_;
};

// specialization for T=void, E=void
template <>
class ExpectedImpl<void, void> : public ExpectedImplBase {
 public:
  using value_type = void;
  using error_type = void;
  using unexpected_type = unexpected<void>;

  ExpectedImpl() : ExpectedImplBase{true} {}

  constexpr ExpectedImpl(const ExpectedImpl &other)
      : ExpectedImplBase{other.has_value()} {}

  constexpr ExpectedImpl(ExpectedImpl &&other) noexcept
      : ExpectedImplBase{other.has_value()} {}

  ExpectedImpl &operator=(ExpectedImpl const &other) {
    ExpectedImpl(other).swap(*this);

    return *this;
  }

  ExpectedImpl &operator=(ExpectedImpl &&other) {
    ExpectedImpl(std::move(other)).swap(*this);

    return *this;
  }

  constexpr ExpectedImpl(const unexpected<error_type> &)
      : ExpectedImplBase{false} {}

  constexpr ExpectedImpl(unexpected<error_type> &&) : ExpectedImplBase{false} {}

  // swap
  void swap(ExpectedImpl &other) noexcept {
    using std::swap;

    if (bool(*this) && bool(other)) {
      // both types have void value, nothing to swap
    } else if (!bool(*this) && !bool(other)) {
      // nothing to swap
    } else if (bool(*this) && !bool(other)) {
      swap(static_cast<ExpectedImplBase &>(*this),
           static_cast<ExpectedImplBase &>(other));
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  constexpr unexpected_type get_unexpected() const { return make_unexpected(); }
};

template <class T, class E>
class expected : public ExpectedImpl<T, E>,
                 private base::select_assign_base<T, E>,
                 private base::select_ctor_base<T, E> {
 public:
  static_assert(!std::is_reference<T>::value, "T must not be a reference");
  static_assert(!std::is_same<T, std::remove_cv<in_place_t>>::value,
                "T must not be in_place_t");
  static_assert(!std::is_same<T, std::remove_cv<unexpected<E>>>::value,
                "T must not be unexpected<E>");
  static_assert(!std::is_reference<E>::value, "E must not be a reference");

#if defined(__SUNPRO_CC)
  // sunpro generates a wrong default assign-move which forces use
  // to declare all the implicit constructors/assign-ops explicitly
  // as default which leads to slightly worse error-messages:
  //
  // clang:
  //
  //  call to implicitly-deleted default constructor of
  //  'stdx::expected<no_default_construct, void>'
  //
  //  note: default constructor of
  //  'expected<no_default_construct, void>' is implicitly deleted
  //    because base class 'ExpectedImpl<no_default_construct, void>' has no
  //    default constructor
  //
  // vs.
  //
  //  call to implicitly-deleted default constructor of
  //  'stdx::expected<no_default_construct, void>'
  //
  //  explicitly defaulted function was implicitly deleted here
  //  expected() = default;
  //
  //  note: default constructor of
  //  'expected<no_default_construct, void>' is implicitly deleted
  //    because base class 'ExpectedImpl<no_default_construct, void>' has no
  //    default constructor
  //
  expected() = default;

  expected(const expected &) = default;
  expected &operator=(const expected &) = default;

  // sunpro generates the default move-assign, move-construct which:
  //
  // 1. ExpectedImpl<T, E>::operator=(std::move(other))
  // 2. memmove(this, &other, sizeof(other));
  //
  // where the memmove trashes the just moved value

#if 0
  // this constructor should be only visible if T and E and move-constructible
  // but with sunpro it leads to
  //
  //    CC: Fatal error in .../developerstudio12.6/lib/compilers/bin/ccfe : Signal number = 139
  //
  // Therefore, it is left unconditionally enabled which makes this type
  // look move-constructible even though it may not be.
  template <
      bool B = std::is_move_constructible<base::select_ctor_base<T, E>>::value,
      std::enable_if_t<B> * = nullptr>
#endif
  expected(expected &&other) : ExpectedImpl<T, E>{std::move(other)} {}

  template <bool B = std::is_move_assignable<T>::value,
            std::enable_if_t<B> * = nullptr>
  expected &operator=(expected &&other) {
    ExpectedImpl<T, E>::operator=(std::move(other));
    return *this;
  }
#endif

  // inherit all the constructors of our base
  using ExpectedImpl<T, E>::ExpectedImpl;
};

template <class E1, class E2>
inline bool operator==(const unexpected<E1> &a, const unexpected<E2> &b) {
  return a.value() == b.value();
}

template <>
inline bool operator==(const unexpected<void> & /* a */,
                       const unexpected<void> & /* b */) {
  return true;
}

template <class E1, class E2>
inline bool operator!=(const unexpected<E1> &a, const unexpected<E2> &b) {
  return !(a == b);
}

template <class T1, class E1, class T2, class E2>
inline bool operator==(const expected<T1, E1> &a, const expected<T2, E2> &b) {
  if (a.has_value() != b.has_value()) return false;

  if (!a.has_value()) return a.error() == b.error();
  return *a == *b;
}

template <class T1, class T2>
inline
    typename std::enable_if_t<base::and_<base::not_<std::is_void<T1>>,
                                         base::not_<std::is_void<T2>>>::value,
                              bool>
    operator==(const expected<T1, void> &a, const expected<T2, void> &b) {
  if (a.has_value() != b.has_value()) return false;

  if (!a.has_value()) return true;
  return *a == *b;
}

template <class E1, class E2>
inline
    typename std::enable_if_t<base::and_<base::not_<std::is_void<E1>>,
                                         base::not_<std::is_void<E2>>>::value,
                              bool>
    operator==(const expected<void, E1> &a, const expected<void, E2> &b) {
  if (a.has_value() != b.has_value()) return false;

  if (!a.has_value()) return a.error() == b.error();
  return true;
}

template <>
inline bool operator==<void, void, void, void>(const expected<void, void> &a,
                                               const expected<void, void> &b) {
  return a.has_value() == b.has_value();
}

template <class T1, class E1, class T2, class E2>
inline bool operator!=(const expected<T1, E1> &a, const expected<T2, E2> &b) {
  return !(a == b);
}

template <class T1, class E1, class E2>
inline bool operator==(const expected<T1, E1> &a, const unexpected<E2> &b) {
  if (a.has_value()) return false;

  return a.get_unexpected() == b;
}

template <class T1, class E1, class E2>
inline bool operator==(const unexpected<E2> &a, const expected<T1, E1> &b) {
  return b == a;
}

template <class T1, class E1, class E2>
inline bool operator!=(const expected<T1, E1> &a, const unexpected<E2> &b) {
  return !(a == b);
}

template <class T1, class E1, class E2>
bool operator!=(const unexpected<E2> &a, const expected<T1, E1> &b) {
  return !(b == a);
}

}  // namespace stdx

#endif
