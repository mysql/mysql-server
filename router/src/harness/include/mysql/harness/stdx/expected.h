/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

// implementation of C++23's std::expected<> in C++17
//
// and http://wg21.link/p2505 (r0) for .and_then(), .or_else() and .transform()
//
// See http://wg21.link/p0323
//
// missing: trivial destructors if T and E are trivial.

#include <functional>  // invoke
#include <initializer_list>
#include <new>  // ::operator new
#include <type_traits>
#include <utility>  // std::forward

#include "my_compiler.h"
#include "mysql/harness/stdx/type_traits.h"

#if defined(__GNUC__) || defined(__clang__)
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

// inplace construction of unexpected values.
struct unexpect_t {
  explicit unexpect_t() = default;
};

inline constexpr unexpect_t unexpect{};

template <typename E>
class unexpected {
 public:
  static_assert(!std::is_same<E, void>::value, "E must not be void");

  using error_type = E;

  unexpected() = delete;

  constexpr explicit unexpected(error_type &&e) : error_{std::move(e)} {}
  // GCC 8.3.1, 8.4.0 report:
  //
  // '*((void*)& accu +64)' may be used uninitialized in this function
  // [-Werror=maybe-uninitialized]
  MY_COMPILER_DIAGNOSTIC_PUSH()
  MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wmaybe-uninitialized")
  constexpr explicit unexpected(const error_type &e) : error_{e} {}
  MY_COMPILER_DIAGNOSTIC_POP()

  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<E, Args &&...>> * = nullptr>
  constexpr explicit unexpected(std::in_place_t, Args &&... args) {
    error_(std::forward<Args>(args)...);
  }

  constexpr error_type &value() &noexcept { return error_; }
  constexpr const error_type &value() const &noexcept { return error_; }
  constexpr error_type &&value() &&noexcept { return std::move(error_); }
  constexpr const error_type &&value() const &&noexcept {
    return std::move(error_);
  }

 private:
  error_type error_;
};

template <typename E>
constexpr auto make_unexpected(E &&e) -> unexpected<std::decay_t<E>> {
  return unexpected<std::decay_t<E>>(std::forward<E>(e));
}

// deduction guide
template <class E>
unexpected(E) -> unexpected<E>;

template <class T, class E>
class expected;

namespace base {
template <class T, class E>
union storage_t {
  using value_type = T;
  using error_type = E;

  storage_t() {}   // NOLINT(modernize-use-equals-default)
  ~storage_t() {}  // NOLINT(modernize-use-equals-default)

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
            std::enable_if_t<std::is_constructible_v<T, Args &&...>, void *> =
                nullptr>
  void construct_value(std::in_place_t, Args &&... args) {
    new (&value_) value_type(std::forward<Args>(args)...);
  }

  template <class U, class... Args,
            std::enable_if_t<std::is_constructible_v<
                                 T, std::initializer_list<U> &, Args &&...>,
                             void *> = nullptr>
  void construct_value(std::in_place_t, std::initializer_list<U> il,
                       Args &&... args) {
    new (&value_) value_type(il, std::forward<Args>(args)...);
  }

  void destruct_value() { value_.~value_type(); }

  void construct_error(error_type const &e) { new (&error_) error_type(e); }
  void construct_error(error_type &&e) {
    new (&error_) error_type(std::move(e));
  }

  // enable inplace construction of error, if the E supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<E, Args &&...>> * = nullptr>
  void construct_error(std::in_place_t, Args &&... args) {
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

  storage_t() {}   // NOLINT(modernize-use-equals-default)
  ~storage_t() {}  // NOLINT(modernize-use-equals-default)

  void construct_error(error_type const &e) { new (&error_) error_type(e); }
  void construct_error(error_type &&e) {
    new (&error_) error_type(std::move(e));
  }

  // enable inplace construction of error, if the E supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<E, Args &&...>> * = nullptr>
  void construct_error(std::in_place_t, Args &&... args) {
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
using not_ = std::negation<B>;

template <class... B>
using and_ = std::conjunction<B...>;

template <class... B>
using or_ = std::disjunction<B...>;

// enable copy constructor if T and E are copy-constructible or void
// enable move constructor if T and E are move-constructible or void
template <class T, class E>
using select_ctor_base =
    ctor_base<(and_<or_<std::is_void<T>, std::is_copy_constructible<T>>,
                    std::is_copy_constructible<E>>::value
                   ? member_policy::copy
                   : member_policy::none) |
              (and_<or_<std::is_void<T>, std::is_move_constructible<T>>,
                    std::is_move_constructible<E>>::value
                   ? member_policy::move
                   : member_policy::none)>;

// enable copy assignment if T and E are (copy-constructible and
// copy-assignable) or void
//
// enable move assignment if T and E are
// (move-constructible and move-assignable) or void
template <class T, class E>
using select_assign_base = assign_base<
    (and_<
         or_<std::is_void<T>,
             and_<std::is_copy_constructible<T>, std::is_copy_assignable<T>>>,
         and_<std::is_copy_constructible<E>, std::is_copy_assignable<E>>>::value
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

  template <class UF, class GF>
  using constructor_is_explicit =
      std::bool_constant<!std::is_convertible_v<UF, T> ||
                         !std::is_convertible_v<GF, E>>;

  //
  template <class U, class G, class UF, class GF>
  using can_value_convert_construct = std::bool_constant<
      std::is_constructible_v<T, UF> && std::is_constructible_v<E, GF> &&
      !std::is_constructible_v<T, ExpectedImpl<U, G> &> &&
      !std::is_constructible_v<T, ExpectedImpl<U, G>> &&
      !std::is_constructible_v<T, const ExpectedImpl<U, G> &> &&
      !std::is_constructible_v<T, const ExpectedImpl<U, G>> &&
      !std::is_convertible_v<ExpectedImpl<U, G> &, T> &&
      !std::is_convertible_v<ExpectedImpl<U, G>, T> &&
      !std::is_convertible_v<const ExpectedImpl<U, G> &, T> &&
      !std::is_convertible_v<const ExpectedImpl<U, G>, T> &&
      !std::is_constructible_v<unexpected<E>, ExpectedImpl<U, G> &> &&
      !std::is_constructible_v<unexpected<E>, ExpectedImpl<U, G>> &&
      !std::is_constructible_v<unexpected<E>, const ExpectedImpl<U, G> &> &&
      !std::is_constructible_v<unexpected<E>, const ExpectedImpl<U, G>>>;

  template <
      class U, class G, class UF = const U &, class GF = const G &,
      std::enable_if_t<can_value_convert_construct<U, G, UF, GF>::value &&
                       !constructor_is_explicit<UF, GF>::value> * = nullptr>
  constexpr ExpectedImpl(const ExpectedImpl<U, G> &rhs)
      : ExpectedImplBase{rhs.has_value()} {
    if (rhs.has_value()) {
      storage_.construct_value(std::forward<UF>(*rhs));
    } else {
      storage_.construct_error(rhs.error());
    }
  }

  template <
      class U, class G, class UF = const U &, class GF = const G &,
      std::enable_if_t<can_value_convert_construct<U, G, UF, GF>::value &&
                       constructor_is_explicit<UF, GF>::value> * = nullptr>
  explicit constexpr ExpectedImpl(const ExpectedImpl<U, G> &rhs)
      : ExpectedImplBase{rhs.has_value()} {
    if (rhs.has_value()) {
      storage_.construct_value(std::forward<UF>(*rhs));
    } else {
      storage_.construct_error(rhs.error());
    }
  }

  template <
      class U, class G, class UF = U, class GF = G,
      std::enable_if_t<can_value_convert_construct<U, G, UF, GF>::value &&
                       !constructor_is_explicit<UF, GF>::value> * = nullptr>
  constexpr ExpectedImpl(ExpectedImpl<U, G> &&rhs)
      : ExpectedImplBase{rhs.has_value()} {
    if (rhs.has_value()) {
      storage_.construct_value(std::forward<UF>(*rhs));
    } else {
      storage_.construct_error(rhs.error());
    }
  }

  template <
      class U, class G, class UF = U, class GF = G,
      std::enable_if_t<can_value_convert_construct<U, G, UF, GF>::value &&
                       constructor_is_explicit<UF, GF>::value> * = nullptr>
  explicit constexpr ExpectedImpl(ExpectedImpl<U, G> &&rhs)
      : ExpectedImplBase{rhs.has_value()} {
    if (rhs.has_value()) {
      storage_.construct_value(std::forward<UF>(*rhs));
    } else {
      storage_.construct_error(rhs.error());
    }
  }

  template <class U>
  using can_construct_from_value_type = std::conjunction<
      std::negation<std::is_same<std::in_place_t, stdx::remove_cvref_t<U>>>,
      std::negation<std::is_same<ExpectedImpl<T, E>, stdx::remove_cvref_t<U>>>,
      std::negation<std::is_same<unexpected<E>, stdx::remove_cvref_t<U>>>,
      std::is_constructible<T, U>>;

  template <class U>
  using can_construct_from_value_type_explicit =
      std::negation<std::is_convertible<U, T>>;

  template <class U = T,
            std::enable_if_t<can_construct_from_value_type<U>::value &&
                             !can_construct_from_value_type_explicit<U>::value>
                * = nullptr>
  constexpr ExpectedImpl(U &&v) : ExpectedImplBase{true} {
    storage_.construct_value(std::forward<U>(v));
  }

  template <class U = T,
            std::enable_if_t<can_construct_from_value_type<U>::value &&
                             can_construct_from_value_type_explicit<U>::value>
                * = nullptr>
  explicit constexpr ExpectedImpl(U &&v) : ExpectedImplBase{true} {
    storage_.construct_value(std::in_place, std::forward<U>(v));
  }

  // enable inplace construction of value_type, if the T supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<T, Args &&...>> * = nullptr>
  constexpr ExpectedImpl(std::in_place_t, Args &&... args)
      : ExpectedImplBase{true} {
    storage_.construct_value(std::in_place, std::forward<Args>(args)...);
  }

  // enable inplace construction of error_type, if the E supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<E, Args &&...>> * = nullptr>
  constexpr ExpectedImpl(stdx::unexpect_t, Args &&... args)
      : ExpectedImplBase{false} {
    storage_.construct_error(std::in_place, std::forward<Args>(args)...);
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

  template <class G,
            std::enable_if_t<std::is_constructible_v<E, const G &>> * = nullptr>
  constexpr ExpectedImpl(const unexpected<G> &e) : ExpectedImplBase{false} {
    storage_.construct_error(e.value());
  }

  template <class G,
            std::enable_if_t<std::is_constructible_v<E, G>> * = nullptr>
  constexpr ExpectedImpl(unexpected<G> &&e) : ExpectedImplBase{false} {
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

  template <class U = T,
            std::enable_if_t<
                !std::is_same_v<ExpectedImpl<T, E>, stdx::remove_cvref_t<U>> &&
                !std::conjunction_v<std::is_scalar<T>,
                                    std::is_same<T, std::decay_t<U>>> &&
                std::is_constructible_v<T, U> && std::is_assignable_v<T &, U>>
                * = nullptr>
  ExpectedImpl &operator=(U &&v) {
    if (bool(*this)) {
      storage_.value() = std::forward<U>(v);
    } else {
      ExpectedImpl(std::forward<U>(v)).swap(*this);
    }

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
  std::enable_if_t<
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

  // enable inplace construction of error_type, if the E supports it
  template <
      class... Args,
      std::enable_if_t<std::is_constructible_v<E, Args &&...>> * = nullptr>
  constexpr ExpectedImpl(stdx::unexpect_t, Args &&... args)
      : ExpectedImplBase{false} {
    storage_.construct_error(std::in_place, std::forward<Args>(args)...);
  }

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
  base::storage_t<void, E> storage_;
};

template <class T>
struct is_expected_impl : std::false_type {};

template <class T, class E>
struct is_expected_impl<expected<T, E>> : std::true_type {};

template <class T>
using is_expected = is_expected_impl<std::decay_t<T>>;

namespace base {

template <
    class Exp, class Func,
    typename value_type = typename std::decay_t<Exp>::value_type,
    std::enable_if_t<std::is_void_v<value_type>
                         ? std::is_invocable_v<Func>
                         : std::is_invocable_v<Func, value_type>> * = nullptr>
constexpr auto and_then_impl(Exp &&exp, Func &&func) {
  if constexpr (std::is_void_v<value_type>) {
    using Ret = std::invoke_result_t<Func>;

    static_assert(stdx::is_expected<Ret>::value,
                  "Func must return a stdx::expected<>");

    if (exp.has_value()) {
      return std::invoke(func);
    } else {
      return Ret{stdx::unexpect, std::forward<Exp>(exp).error()};
    }
  } else {
    using Ret = std::invoke_result_t<Func, value_type>;

    static_assert(stdx::is_expected<Ret>::value,
                  "Func must return a stdx::expected<>");

    if (exp.has_value()) {
      return std::invoke(func, *std::forward<Exp>(exp));
    } else {
      return Ret{stdx::unexpect, std::forward<Exp>(exp).error()};
    }
  }
}

template <class Exp, class Func,
          typename error_type = typename std::decay_t<Exp>::error_type,
          std::enable_if_t<std::is_invocable_v<Func, error_type>> * = nullptr>
constexpr auto or_else_impl(Exp &&exp, Func &&func) {
  static_assert(
      std::is_same_v<
          stdx::remove_cvref_t<std::invoke_result_t<Func, error_type>>, Exp>,
      "Func must return an expected<>");

  if (exp.has_value()) {
    return std::forward<Exp>(exp);
  }

  return std::invoke(std::forward<Func>(func), std::forward<Exp>(exp).error());
}

}  // namespace base

template <class T, class E>
class expected : public ExpectedImpl<T, E>,
                 private base::select_assign_base<T, E>,
                 private base::select_ctor_base<T, E> {
 public:
  static_assert(!std::is_void<E>::value, "E must not be void");
  static_assert(!std::is_reference<T>::value, "T must not be a reference");
  static_assert(!std::is_same<T, std::remove_cv<std::in_place_t>>::value,
                "T must not be std::in_place_t");
  static_assert(!std::is_same<T, std::remove_cv<unexpected<E>>>::value,
                "T must not be unexpected<E>");
  static_assert(!std::is_reference<E>::value, "E must not be a reference");

  // inherit all the constructors of our base
  using ExpectedImpl<T, E>::ExpectedImpl;

  //
  // and_then
  //

  template <class Func>
  constexpr auto and_then(Func &&func) & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // or_else
  //

  template <class Func>
  constexpr auto or_else(Func &&func) & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // transform
  //

  template <class Func>
  constexpr auto transform(Func &&func) & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }
};

template <class Exp, class Func>
constexpr auto expected_transform_impl(Exp &&exp, Func &&func) {
  // type of the value that's passed to func
  using func_value_type = typename std::decay_t<Exp>::value_type;

  if constexpr (std::is_void_v<func_value_type>) {
    using func_return_type = std::invoke_result_t<Func>;
    using result_type =
        stdx::expected<func_return_type, typename Exp::error_type>;

    if (!exp.has_value()) {
      return result_type{stdx::unexpect, std::forward<Exp>(exp).error()};
    }

    if constexpr (std::is_void_v<func_return_type>) {
      std::invoke(func);
      return result_type();
    } else {
      return result_type(std::invoke(func));
    }
  } else {
    using func_return_type = std::invoke_result_t<Func, func_value_type>;
    using result_type =
        stdx::expected<func_return_type, typename Exp::error_type>;

    if (!exp.has_value()) {
      return result_type{stdx::unexpect, std::forward<Exp>(exp).error()};
    }

    if constexpr (std::is_void_v<func_return_type>) {
      std::invoke(func, *std::forward<Exp>(exp));
      return result_type();
    } else {
      return result_type(std::invoke(func, *std::forward<Exp>(exp)));
    }
  }
}

template <class E1, class E2>
inline bool operator==(const unexpected<E1> &a, const unexpected<E2> &b) {
  return a.value() == b.value();
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

template <class E1, class E2>
inline bool operator==(const expected<void, E1> &a,
                       const expected<void, E2> &b) {
  if (a.has_value() != b.has_value()) return false;

  if (!a.has_value()) return a.error() == b.error();
  return true;
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
