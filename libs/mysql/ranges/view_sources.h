// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_RANGES_VIEW_SOURCES_H
#define MYSQL_RANGES_VIEW_SOURCES_H

/// @file
/// Experimental API header

#include <optional>                         // optional
#include <ranges>                           // view
#include <type_traits>                      // conditional_t
#include "mysql/iterators/null_iterator.h"  // null_iterator

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges {

/// Type alias to represent the source of a view: resolves to `Type` if
/// `std::ranges::view<Type>`, or to `Type &` otherwise. This is meant to be
/// used by member variables of the view class, or the view's iterator class,
/// that need to reference the source.
///
/// This intends to prevent dangling references by enforcing the following rule:
///
///   "Views and their iterators shall represent sources that are views
///   by-value, and sources that are containers by-reference."
///
/// For full justification, see readme.md.
///
/// Since references are not default-constructible, a view or view iterator that
/// has a member of this type becomes non-default-constructible when the source
/// is not a view. If you need default-constructibility, use View_source
/// instead. The benefit of this class is that it does not require indirection
/// to access the T object.
///
/// Note that types need to be declared explicitly as views, using either
/// `std::ranges::view_base` or `std::ranges::enable_range`.
///
/// @tparam Source_t The source type.
///
/// @tparam owns_source_t Determines if the object owns its source. By default,
/// this is deduced from `std::ranges::view<Source_t>`.
template <class Source_t, bool owns_source_t = std::ranges::view<Source_t>>
using Raw_view_source =
    std::conditional_t<owns_source_t, Source_t, const Source_t &>;

/// Wrapper around an object that is the source for a view: the wrapped object
/// is owned by this object if the wrapped object's type satisfies
/// `std::ranges::view`, and not owned otherwise. This is meant to be used by
/// member variables of the view class, or the view's iterator class, that need
/// to reference the source.
///
/// This intends to prevent dangling references by enforcing the following rule:
///
///   "Views and their iterators shall represent sources that are views
///   by-value, and sources that are containers by-reference."
///
/// For full justification, see readme.md.
///
/// Internally, this class stores a source of type `T` using a member variable
/// of type `T` if `std::ranges::view<T>`, or type `const T *` otherwise. It
/// provides an API similar to `std::optional`, except this object is not
/// optional.
///
/// This object is default-constructible if `T` is default-constructible
/// (although the default-constructed object is in a singular state and can't be
/// used for anything else than as the target of an assignment operation). If
/// default-constructibility is not important, you may use `Raw_view_source`
/// instead.
///
/// Note that types need to be declared explicitly as views, using either
/// `std::ranges::view_base` or `std::ranges::enable_range`.
///
/// @tparam Source_tp The source type.
///
/// @tparam owns_source_tp Determines if the object owns its source. By default,
/// this is deduced from `std::ranges::view<Source_tp>`.
template <class Source_tp, bool owns_source_tp = std::ranges::view<Source_tp>>
class View_source {
 public:
  using Source_t = Source_tp;
  /// True if this object owns a copy of its source; false if it holds a
  /// reference to its source.
  static constexpr bool owns_source = owns_source_tp;

  /// Source_t if `owns_source`, otherwise `Source_t &`.
  using Raw_source_t = Raw_view_source<Source_t, owns_source>;

  /// Internal representation of the source.
  using Source_ref_t =
      std::conditional_t<owns_source, Source_t, const Source_t *>;

  /// Default-construct an object. If the source is not owned, this holds a
  /// nullptr internally, so then the only things you can do with the object is
  /// assign to it.
  View_source() = default;

  /// Construct from a const reference to the source.
  explicit View_source(const Source_t &source) : m_source(from_ref(source)) {}

  /// Return a copy of the stored object if it is owned; otherwise a reference
  /// to it. The behavior is undefined for default-constructed objects.
  [[nodiscard]] const Raw_source_t &get() const { return reference(); }

  /// Return a reference to the stored object. The behavior is undefined
  /// for default-constructed objects. Note that the reference is
  /// only valid as long as this object exists.
  [[nodiscard]] const Source_t &reference() const {
    if constexpr (owns_source)
      return m_source;
    else
      return *m_source;
  }

  /// Arrow operator to access members of the source. The behavior is undefined
  /// for default-constructed objects.
  ///
  /// @note To access begin/end iterators from the source when there is a
  /// source, and objects that behave like end iterators when there is no
  /// source, use `o.begin()` and `o.end()` instead of `o->begin()` and
  /// `o->end()`.
  [[nodiscard]] const Source_t *operator->() const { return &reference(); }

  /// Return begin iterator to the source. The behavior is undefined for
  /// default-constructed objects.
  ///
  /// `x.begin()` is equivalent to `x->begin()`; this function is provided for
  /// API compatibility with Optional_view_source.
  [[nodiscard]] auto begin() const { return reference().begin(); }

  /// Return end iterator to the source. The behavior is undefined for
  /// default-constructed non-owning objects.
  ///
  /// `x.end()` is equivalent to `x->end()`; this function is provided for API
  /// compatibility with Optional_view_source.
  [[nodiscard]] auto end() const { return reference().end(); }

 private:
  /// Return an `Source_ref_t` from the given pointer, holding no object if
  /// the pointer is nullptr, and holding the pointed-to object otherwise.
  [[nodiscard]] static decltype(auto) from_ref(const Source_t &object) {
    if constexpr (owns_source) {
      return object;
    } else {
      return &object;
    }
  }

  /// The source.
  Source_ref_t m_source{};
};  // class View_source

/// `std::optional`-like wrapper around an object that is the source for a view:
/// this may hold an object or not; and the wrapped object is owned by this
/// object if the wrapped object's type satisfies `std::ranges::view`, and not
/// owned otherwise. This is meant to be used by member variables of the view
/// class, or the view's iterator class, that need to reference the source.
///
/// This intends to prevent dangling references by enforcing the following rule:
///
///   "Views and their iterators shall represent sources that are views
///   by-value, and sources that are containers by-reference."
///
/// For full justification, see readme.md.
///
/// Internally, this class stores a source of type `T` using a member variable
/// of type `std::optional<T>` if `std::ranges::view<T>`, or type `const T *`
/// otherwise. It provides an API similar to `std::optional`.
///
/// Note that types need to be declared explicitly as views, using either
/// `std::ranges::view_base` or `std::ranges::enable_range`.
///
/// @tparam Source_tp The source type.
///
/// @tparam owns_source_tp Determines if the object owns its source. By default,
/// this is deduced from `std::ranges::view<Source_tp>`.
template <class Source_tp, bool owns_source_tp = std::ranges::view<Source_tp>>
class Optional_view_source {
 public:
  using Source_t = Source_tp;

  /// True if this object owns a copy of its source; false if it holds a
  /// reference to its source.
  static constexpr bool owns_source = owns_source_tp;

  /// Source_t if `owns_source`, otherwise `Source_t &`.
  using Raw_source_t = Raw_view_source<Source_t, owns_source>;

  /// Internal representation of the source.
  using Optional_source_t =
      std::conditional_t<owns_source, std::optional<Source_t>,
                         const Source_t *>;

  /// Construct an objec that does not hold a source.
  Optional_view_source() = default;

  /// Construct from a const reference to the source.
  explicit Optional_view_source(const Source_t &source)
      : m_source(from_ptr(&source)) {}

  /// Construct from a const pointer to the source. The source may be nullptr,
  /// in which case this object will not hold any sourcd.
  explicit Optional_view_source(const Source_t *source)
      : m_source(from_ptr(source)) {}

  /// Return true if this object holds a source.
  [[nodiscard]] bool has_object() const { return (bool)m_source; }

  /// Return a copy of the stored object if it is owned; otherwise a reference
  /// to it. The behavior is undefined if !has_object().
  [[nodiscard]] const Raw_source_t &get() const { return *m_source; }

  /// Return a reference to the stored object. The behavior is undefined if
  /// !has_object(). Note that the reference is only valid as long as this
  /// object exists.
  [[nodiscard]] const Raw_source_t &reference() const { return *m_source; }

  /// Return a pointer to the source if there is one, or nullptr otherwise. Note
  /// that the pointer is only valid as long as this object exists.
  [[nodiscard]] const Source_t *pointer() const {
    return has_object() ? &reference() : nullptr;
  }

  /// Arrow operator to access members of the source. The behavior is undefined
  /// if !has_object().
  ///
  /// @note To access begin/end iterators from the source when there is a
  /// source, and objects that behave like end iterators when there is no
  /// source, use `o.begin()` and `o.end()` instead of `o->begin()` and
  /// `o->end()`.
  [[nodiscard]] const Source_t *operator->() const { return &*m_source; }

  /// Return a valid begin iterator, even if !has_object().
  ///
  /// If !has_object(), it is defined that the source is an empty range, so then
  /// this function returns the end iterator to a default-constructed source.
  ///
  /// This requires std::ranges::common_range<Source_t>, i.e., begin() and end()
  /// must return the same type.
  [[nodiscard]] auto begin() const
    requires std::ranges::common_range<Source_t>
  {
    if (!has_object()) return null_iterator();
    return reference().begin();
  }

  /// Return a valid end iterator, even if !has_object().
  ///
  /// If !has_object(), this returns the end iterator to a default-constructed
  /// source.
  [[nodiscard]] auto end() const {
    if (!has_object()) return null_iterator();
    return reference().end();
  }

 protected:
  [[nodiscard]] static auto null_iterator() {
    return mysql::iterators::null_iterator<Source_t>();
  }

 private:
  /// Return an `Optional_source_t` from the given pointer, holding no object if
  /// the pointer is nullptr, and holding the pointed-to object otherwise.
  [[nodiscard]] static decltype(auto) from_ptr(const Source_t *object) {
    if constexpr (owns_source) {
      if (object == nullptr) return Optional_source_t{};
      return Optional_source_t(*object);
    } else {
      return object;
    }
  }

  /// The source.
  Optional_source_t m_source{};
};  // class Optional_view_source

/// Factory function to create a View_source wrapping the given object.
template <class Source_t>
[[nodiscard]] auto make_view_source(const Source_t &source) {
  return View_source<Source_t>(source);
}

/// Factory function to create an Optional_view_source wrapping the given
/// object.
template <class Source_t>
[[nodiscard]] auto make_optional_view_source(const Source_t &source) {
  return Optional_view_source<Source_t>(source);
}

/// Factory function to create an Optional_view_source wrapping the pointed-to
/// object.
template <class Source_t>
[[nodiscard]] auto make_optional_view_source(const Source_t *source) {
  return Optional_view_source<Source_t>(source);
}

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_VIEW_SOURCES_H
