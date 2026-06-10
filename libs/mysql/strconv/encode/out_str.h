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

#ifndef MYSQL_STRCONV_ENCODE_OUT_STR_H
#define MYSQL_STRCONV_ENCODE_OUT_STR_H

/// @file
/// Experimental API header

#include <cassert>                             // assert
#include <concepts>                            // invokable
#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/meta/is_charlike.h"            // Is_charlike
#include "mysql/meta/is_either.h"              // Is_either
#include "mysql/meta/is_pointer.h"             // Is_pointer_to
#include "mysql/meta/is_specialization.h"      // Is_specialization
#include "mysql/meta/not_decayed.h"            // Not_decayed
#include "mysql/ranges/buffer_interface.h"     // Buffer_interface
#include "mysql/utils/call_and_catch.h"        // call_and_catch
#include "mysql/utils/char_cast.h"             // char_cast
#include "mysql/utils/return_status.h"         // Return_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{
///
/// Wrappers around output buffers for string-producing functions, enabling
/// a single string producer to accept multiple string representations and
/// allocation policies.
///
/// Anyone that defines a string-producing function usually has to make several
/// decisions, and this framework makes it easy to support all variants without
/// duplicating/repeating code:
///
/// - Who allocates the string: the function or the caller?
///
/// - How are strings represented: std::string or raw pointer? For raw pointers,
///   how is the length represented: an integer length, or a raw pointer to the
///   end? Are characters `char`, `unsigned char`, or `std::byte`? Which integer
///   type is used for the length?
///
/// - Should the string be null-terminated or not?
///
/// If the string producer will only be used in code that follows one defined
/// convention, the API can just use that. Otherwise, in more heterogeneous
/// environments, use this library as follows:
///
/// - The string-producing function should take an `Out_str &` as out-parameter:
///   @code
///   Return_status produce(Out_str &out);
///   @endcode
///
/// - The user code should pass an object returned by any of the `out_str_*`
///   functions, which return a wrapper around the output parameters. For
///   example:
///   @code
///   std::string str;
///   // Allocate and write to `str`; return error if allocation fails.
///   if (produce(out_str_growable(str)) == Return_status::error) { ... }
///   @endcode
///   or
///   @code
///   char *str = nullptr;
///   char *end = str;
///   // Allocate memory and write to `str`.
///   // If allocation fails, return error.
///   // Make the string null-terminated (the _z suffix).
///   // Make `end` point to the null character.
///   if (produce(out_str_growable_z(str, end)) == Return_status::error) {...}
///   @endcode
///   or
///   @code
///   unsigned char str[1024];
///   size_t size = 1024;
///   // We know the produced string is at most 1024 bytes (otherwise, behavior
///   // is undefined). Write the string to `str`.
///   // Do not make the string null-terminated (the _nz suffix).
///   // Set `size` to the actual number of bytes written.
///   if (produce(out_str_fixed_nz(str, size)) == Return_status::Error) { ... }
///   @endcode
///   or
///   @code
///   std::byte str[1024];
///   size_t size = 1024;
///   // We know the produced string is at most 1024 bytes (otherwise, behavior
///   // is undefined). Write the string to `str`.
///   // Do not make the string null-terminated (the _nz suffix).
///   // Set `size` to the actual number of bytes written.
///   produce(out_str_fixed_nz(str, size));
///   @endcode
///   These are only examples: each of `_growable`, `_fixed` can be combined
///   with all string representations (std::string, begin+end, or begin+length).
///   Raw pointer representations can be either null-terminated or not (the
///   `_z`/`_nz` suffixes); optionally take a Memory_resource parameter; use
///   `char`, `unsigned char`, or `std::byte` for the character type; and their
///   sizes can be any integer type.
///
/// - To write to Output String Wrappers, while taking both the resize policy
///   and the zero-termination policy into account, the implementation of
///   `produce` may use the function `out_str_write`. `out_str_write` takes
///   an invocable as argument; the invocable is what actually generates the
///   string, and `out_str_write` performs the allocation (and checking for
///   out-of-memory errors), and writes the null-termination byte if required.

// ==== Overview of implementation ====
//
// We implement out-strings using a class hierarchy with the following levels:
//
// - Out_str_base is the common base class. It is empty, and is used only to
//   identify that classes belong to the hierarchy.
//
// - `Representation_string`, `Representation_strstr`, and
//   `Representation_strsize` define the three representations of strings which
//   use std::string, begin pointer+end pointer, and begin pointer+size,
//   respectively. They all inherit from `Out_str_base` and `Buffer_interface`,
//   and provide the following members:
//
//   public:
//     // Return pointer to the first character, using three alternative
//     // representations of characters.
//     char *data() const;
//     unsigned char *udata() const;
//     std::byte *bdata() const;
//     // Before `store_size` has been invoked, return the initial capacity;
//     // otherwise undefined.
//     std::size_t initial_capacity() const;
//     // After `store_size` has been invoked, return the output size; otherwise
//     // undefined.
//     std::size_t size() const;
//
//   protected:
//     // Stores the given value in the size field.
//     void store_size(size_t);
//
//   (Functionality common to `Representation_ptrsize` and
//   `Representation_ptrstr` is defined in their common base class
//   `Represntation_ptr_base`. All `Representation_*` classes use
//   `Buffer_interface`.)
//
// - `Policy_fixed`, `Policy_growable_string`, `Policy_growable_ptr` define the
//   resize policy and the null-termination policy. All take a template
//   parameter that specifies a base class among the three `Representation_*`
//   classes above. This allows the next level in the hierarchy to compose a
//   policy with a representation (see below) . The `Policy_*` provide the
//   following members:
//
//   public:
//     // `growable` for `Policy_growable_*`; `fixed` for `Policy_fixed`.
//     static constexpr Resize_policy resize_policy;
//     // Change size by according to the resize policy, call `store_size`, and
//     // set the null-termination byte if required.
//     Return_status resize(size_t);
//
// - `Out_str_{fixed|growable}_{string|ptrsize|ptrptr}_alias` are type aliases
//   that combine one of the representations with one of the resize policies, by
//   passing the representation class as template argument to the policy class.
//
// - `Out_str_{fixed|growable}_{string|ptrsize|ptrptr}` are classes
//   corresponding to each alias. They do not provide any new functionality
//   compared to the aliases. However, for technical reasons the Policy classes,
//   and therefore also the aliases, have the Memory_resoruce constructor
//   argument before other arguments, which is a little inconvenient/unnatural
//   since this argument is optional. These classes fix that. Also, the names
//   of these classes are more succinct than the aliases when they appear in
//   stack traces and compilation errors.
//
// - `out_str{growable|fixed{_nz|z}}` are factory functions that users call
//   to wrap their output string arguments.

namespace mysql::strconv {

// ==== Common base ====

/// Top of the hierarchy
class Out_str_base {};

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint

/// Indicates whether an Ouput String Wrapper is growable or fixed-size.
enum class Resize_policy { growable, fixed };

/// Indicates whether an Output String Wrapper requires that the string shall
/// be null-terminated or not.
enum class Null_terminated { no, yes };

/// Indicates the type of string represention used by an Output String Wrapper.
enum class Representation_type {
  /// String represented as `std::string`
  string,
  /// String represented using raw pointer to beginning and raw pointer to end.
  ptrptr,
  /// String represented using raw pointer to beginning and integral size.
  ptrsize
};

// NOLINTEND(performance-enum-size)

/// True if Test is an Output String Wrapper, i.e., derived from Out_str_base.
template <class Test>
concept Is_out_str =
    std::derived_from<Test, Out_str_base> &&  // this comment helps clang-format
    requires(Test test, std::size_t new_size) {
      { test.initial_capacity() } -> std::same_as<std::size_t>;
      { test.size() } -> std::same_as<std::size_t>;
      {
        test.resize(new_size)
        } -> mysql::meta::Is_either<void, mysql::utils::Return_status>;
      { test.data() } -> std::same_as<char *>;
      { test.udata() } -> std::same_as<unsigned char *>;
      { test.bdata() } -> std::same_as<std::byte *>;
      { Test::resize_policy } -> std::convertible_to<Resize_policy>;
    };

/// True if Test is an Output String Wrapper with Resize_policy *fixed*.
template <class Test>
concept Is_out_str_fixed = Is_out_str<Test> &&
                           (Test::resize_policy == Resize_policy::fixed);

/// True if Test is an Output String Wrapper with Resize_policy *growable*.
template <class Test>
concept Is_out_str_growable = Is_out_str<Test> &&
                              (Test::resize_policy == Resize_policy::growable);

}  // namespace mysql::strconv

// ==== Classes implementing string representations ====

namespace mysql::strconv::detail {

/// Represent a string as an object, typically std::string.
///
/// This stores a reference to the object. The object can be a specialization of
/// std::string, or any other class implementing `data`, `size`, and `resize`
/// members.
template <class String_tp>
class Representation_string
    : public mysql::ranges::Buffer_interface<Representation_string<String_tp>>,
      public Out_str_base {
 public:
  static constexpr auto representation_type = Representation_type::string;

  /// Construct a new object wrapping the given `str` object.
  explicit Representation_string(String_tp &str) : m_str(str) {}

  /// Return a raw pointer to the data.
  [[nodiscard]] char *data() const {
    return mysql::utils::char_cast(m_str.data());
  }

  /// Before `resize`, return the capacity of the string.
  [[nodiscard]] std::size_t initial_capacity() const {
    return m_str.capacity();
  }

  /// After `resize`, return the size of the string.
  [[nodiscard]] std::size_t size() const { return m_str.size(); }

 protected:
  /// Store the string size.
  void store_size(std::size_t size_arg) const { m_str.resize(size_arg); }

  /// Reference to string storage.
  String_tp &m_str;
};

/// Common CRTP base class for Representation_ptrptr and Representation_ptrsize.
///
/// This stores either a pointer to the beginning of the string, or a reference
/// to a pointer to the beginning of the string; the template argument decides
/// which.
///
/// @tparam Self_tp Class implementing `size` member.
///
/// @tparam Ptr_tp Type of char pointer: either `char *&`, if the buffer is
/// resizable, or `char *`, for fixed-size buffers.
template <class Self_tp, class Ptr_tp>
class Representation_ptr_base : public mysql::ranges::Buffer_interface<Self_tp>,
                                public Out_str_base {
 public:
  /// Construct a new object wrapping the buffer at the given position.
  explicit Representation_ptr_base(Ptr_tp first, std::size_t capacity)
      : m_first(first), m_initial_capacity(capacity) {}

  /// Return pointer to the data.
  [[nodiscard]] char *data() const { return mysql::utils::char_cast(m_first); }

  /// Before `resize`, return the value of the size field.
  [[nodiscard]] std::size_t initial_capacity() const {
    return std::size_t(m_initial_capacity);
  }

 protected:
  /// Pointer to first character.
  Ptr_tp m_first;

  /// Initial capacity.
  std::size_t m_initial_capacity;
};

/// Represent a string as raw pointer to the beginning and raw pointer to the
/// end.
///
/// This stores both pointers. The end is always a reference (determined by the
/// template arguent); the beginning is optionally a reference.
///
/// @tparam Ptr_tp Type of char pointer to the beginning: either `char *&`, if
/// the buffer is resizable, or `char *`, for fixed-size buffers.
template <class Ptr_tp>
class Representation_ptrptr
    : public Representation_ptr_base<Representation_ptrptr<Ptr_tp>, Ptr_tp> {
  using Base_t = Representation_ptr_base<Representation_ptrptr<Ptr_tp>, Ptr_tp>;

 public:
  static constexpr auto representation_type = Representation_type::ptrptr;

  /// Construct a new object from the given pointer to begin and pointer to end
  /// of capacity, and the given reference to pointer to the end.
  explicit Representation_ptrptr(Ptr_tp first, Ptr_tp &last,
                                 Ptr_tp capacity_end)
      : Base_t(first,
               std::size_t((capacity_end == nullptr ? last : capacity_end) -
                           first)),
        m_last(last) {
    if (capacity_end == nullptr)
      assert(first <= last);
    else
      assert(first <= capacity_end);
  }

  // After `resize`, return the distance between the begin pointer and the end
  // pointer.
  [[nodiscard]] std::size_t size() const { return m_last - this->m_first; }

 protected:
  /// Alter the end pointer.
  void store_size(std::size_t size_arg) const {
    m_last = this->m_first + size_arg;
  }

  /// Reference to end pointer.
  Ptr_tp &m_last;
};

/// Represent a string as raw pointer to the beginning, and integer size.
///
/// This stores the pointer and integer. The size is always a reference; the
/// beginning is optinally a reference (determined by the template argument).
///
/// @tparam Ptr_tp Type of char pointer to the beginning: either `char *&`, if
/// the buffer is resizable, or `char *`, for fixed-size buffers.
///
/// @tparam Size_tp Type of size field.
template <class Ptr_tp, std::integral Size_tp>
class Representation_ptrsize
    : public Representation_ptr_base<Representation_ptrsize<Ptr_tp, Size_tp>,
                                     Ptr_tp> {
  using Base_t =
      Representation_ptr_base<Representation_ptrsize<Ptr_tp, Size_tp>, Ptr_tp>;

 public:
  static constexpr auto representation_type = Representation_type::ptrsize;

  /// Construct a new object from the given begin pointer, reference to size,
  /// and capacity.
  explicit Representation_ptrsize(Ptr_tp first, Size_tp &size_arg,
                                  Size_tp capacity)
      : Base_t(first, std::size_t(capacity == 0 ? size_arg : capacity)),
        m_size(size_arg) {}

  /// After `resize`, return the value of the size field.
  [[nodiscard]] std::size_t size() const { return std::size_t(m_size); }

 protected:
  /// Alter the size field.
  void store_size(std::size_t size_arg) const { m_size = Size_tp(size_arg); }

  /// Reference to integral size.
  Size_tp &m_size;
};

// ==== Classes implementing resize policies and null termination ====
//
// The three CRTP base classes:
// Policy_growable_string
// Policy_growable_ptr
// Policy_fixed
//
// Each implements resize() (which uses implementations of initial_capacity(),
// store_size(), and data() provided by the subclass).

/// Base class for Out_str_growable_string.
///
/// @tparam Representation_tp base class to inherit from, which provides the
/// string representation.
template <class Representation_tp>
class Policy_growable_string : public Representation_tp {
 public:
  static constexpr auto resize_policy = Resize_policy::growable;
  static constexpr auto null_terminated = Null_terminated::yes;

  /// Construct a new object, forwarding all arguments to the base class.
  explicit Policy_growable_string(auto &...args) : Representation_tp(args...) {}

  /// Resize the std::string object.
  ///
  /// @param size The new size
  ///
  /// @return Return_status::ok on success, Return_status::error if an
  /// out-of-memory error occurs.
  [[nodiscard]] mysql::utils::Return_status resize(std::size_t size) const {
    return mysql::utils::call_and_catch([&] { this->m_str.resize(size); });
  }
};

/// Base class for all Out_str_growable_ptr* classes.
///
/// @tparam Representation_tp base class to inherit from, which provides the
/// string representation.
///
/// @tparam null_terminated_tp Indicate whether the string should be
/// null-terminated or not.
template <class Char_tp, class Representation_tp,
          Null_terminated null_terminated_tp>
class Policy_growable_ptr : public Representation_tp {
  static constexpr size_t null_size =
      (null_terminated_tp == Null_terminated::yes) ? 1 : 0;
  using This_t =
      Policy_growable_ptr<Char_tp, Representation_tp, null_terminated_tp>;

 public:
  static constexpr auto resize_policy = Resize_policy::growable;
  static constexpr auto null_terminated = null_terminated_tp;

  /// Construct a new object, forwarding all arguments to the base class.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Policy_growable_ptr(Args_t &&...args)
      : Representation_tp(std::forward<Args_t>(args)...) {}

  /// Construct a new object with the given Memory_resource, forwarding all
  /// remaining arguments to the base class.
  template <class... Args_t>
  explicit Policy_growable_ptr(
      mysql::allocators::Memory_resource memory_resource, Args_t &&...args)
      : Representation_tp(std::forward<Args_t>(args)...),
        m_memory_resource(std::move(memory_resource)) {}

  /// Return the Memory_resource.
  [[nodiscard]] mysql::allocators::Memory_resource get_memory_resource() const {
    return m_memory_resource;
  }

  /// Resize the character buffer and store the new size.
  ///
  /// @param size_arg The new size
  ///
  /// @return Return_status::ok on success, Return_status::error if an
  /// out-of-memory error occurs.
  [[nodiscard]] mysql::utils::Return_status resize(std::size_t size_arg) const {
    if (size_arg > this->initial_capacity() || this->m_first == nullptr) {
      auto *new_first = reinterpret_cast<Char_tp *>(
          m_memory_resource.allocate(size_arg + null_size));
      if (new_first == nullptr) return mysql::utils::Return_status::error;
      m_memory_resource.deallocate(this->m_first);
      this->m_first = new_first;
    }
    this->store_size(size_arg);
    if constexpr (null_terminated_tp == Null_terminated::yes &&
                  Representation_tp::representation_type !=
                      Representation_type::string) {
      this->data()[size_arg] = '\0';
    }
    return mysql::utils::Return_status::ok;
  }

 private:
  mysql::allocators::Memory_resource m_memory_resource;
};

/// Base class for all Out_str_fixed* classes.
///
/// @tparam Representation_tp base class to inherit from, which provides the
/// string representation.
///
/// @tparam null_terminated_tp Indicate whether the string should be
/// null-terminated or not.
template <class Representation_tp, Null_terminated null_terminated_tp>
class Policy_fixed : public Representation_tp {
 private:
  using This_t = Policy_fixed<Representation_tp, null_terminated_tp>;

 public:
  static constexpr auto resize_policy = Resize_policy::fixed;
  static constexpr auto null_terminated = null_terminated_tp;

  /// Construct a new object, forwarding all arguments to the base class.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Policy_fixed(Args_t &&...args)
      : Representation_tp(std::forward<Args_t>(args)...) {}

  /// Assume that the character buffer has at least the given new size, and
  /// store the new size.
  ///
  /// @param size_arg The new size.
  void resize(std::size_t size_arg) const {
    assert(size_arg <= this->initial_capacity());
    this->store_size(size_arg);
    if constexpr (null_terminated_tp == Null_terminated::yes &&
                  Representation_tp::representation_type !=
                      Representation_type::string) {
      this->data()[size_arg] = '\0';
    }
  }
};

// ==== Type aliases that combine a representation and a resize policy ====

template <class String_tp>
using Out_str_fixed_string_alias =
    Policy_fixed<Representation_string<String_tp>, Null_terminated::yes>;

template <class Size_tp>
using Out_str_fixed_ptrsize_z_alias =
    Policy_fixed<Representation_ptrsize<char *, Size_tp>, Null_terminated::yes>;

template <class Size_tp>
using Out_str_fixed_ptrsize_nz_alias =
    Policy_fixed<Representation_ptrsize<char *, Size_tp>, Null_terminated::no>;

template <class Char_tp>
using Out_str_fixed_ptrptr_z_alias =
    Policy_fixed<Representation_ptrptr<Char_tp *>, Null_terminated::yes>;

template <class Char_tp>
using Out_str_fixed_ptrptr_nz_alias =
    Policy_fixed<Representation_ptrptr<Char_tp *>, Null_terminated::no>;

template <class String_tp>
using Out_str_growable_string_alias =
    Policy_growable_string<Representation_string<String_tp>>;

template <mysql::meta::Is_charlike Char_t, std::integral Size_tp>
using Out_str_growable_ptrsize_z_alias =
    Policy_growable_ptr<Char_t, Representation_ptrsize<Char_t *&, Size_tp>,
                        Null_terminated::yes>;

template <mysql::meta::Is_charlike Char_t, std::integral Size_tp>
using Out_str_growable_ptrsize_nz_alias =
    Policy_growable_ptr<Char_t, Representation_ptrsize<Char_t *&, Size_tp>,
                        Null_terminated::no>;

template <mysql::meta::Is_charlike Char_t>
using Out_str_growable_ptrptr_z_alias =
    Policy_growable_ptr<Char_t, Representation_ptrptr<Char_t *&>,
                        Null_terminated::yes>;

template <mysql::meta::Is_charlike Char_t>
using Out_str_growable_ptrptr_nz_alias =
    Policy_growable_ptr<Char_t, Representation_ptrptr<Char_t *&>,
                        Null_terminated::no>;

}  // namespace mysql::strconv::detail

// ==== Classes wrapping the aliases ====

namespace mysql::strconv {

// ---- Fixed-size output buffers ----

/// Non-growable output buffer wrapper, represented as std::string.
template <class String_tp>
class Out_str_fixed_string
    : public detail::Out_str_fixed_string_alias<String_tp> {
 public:
  explicit Out_str_fixed_string(String_tp &str)
      : detail::Out_str_fixed_string_alias<String_tp>(str) {}
};

/// Non-growable output buffer wrapper, represented as raw pointers to the
/// beginning and end, null-terminated.
template <class Char_tp>
class Out_str_fixed_ptrptr_z
    : public detail::Out_str_fixed_ptrptr_z_alias<Char_tp> {
  using This_t = Out_str_fixed_ptrptr_z<Char_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Out_str_fixed_ptrptr_z(Args_t &&...args)
      : detail::Out_str_fixed_ptrptr_z_alias<Char_tp>(
            std::forward<Args_t>(args)...) {}
};

/// Non-growable output buffer wrapper, represented as raw pointers to the
/// beginning and end, non-null-terminated.
template <class Char_tp>
class Out_str_fixed_ptrptr_nz
    : public detail::Out_str_fixed_ptrptr_nz_alias<Char_tp> {
  using This_t = Out_str_fixed_ptrptr_nz<Char_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Out_str_fixed_ptrptr_nz(Args_t &&...args)
      : detail::Out_str_fixed_ptrptr_nz_alias<Char_tp>(
            std::forward<Args_t>(args)...) {}
};

/// Non-growable output buffer wrapper, represented as raw pointer to the
/// beginning, and integer size, null-terminated.
template <class Size_tp>
class Out_str_fixed_ptrsize_z
    : public detail::Out_str_fixed_ptrsize_z_alias<Size_tp> {
  using This_t = Out_str_fixed_ptrsize_z<Size_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Out_str_fixed_ptrsize_z(Args_t &&...args)
      : detail::Out_str_fixed_ptrsize_z_alias<Size_tp>(
            std::forward<Args_t>(args)...) {}
};

/// Non-growable output buffer wrapper, represented as raw pointer to the
/// beginning, and integer size, non-null-terminated.
template <class Size_tp>
class Out_str_fixed_ptrsize_nz
    : public detail::Out_str_fixed_ptrsize_nz_alias<Size_tp> {
  using This_t = Out_str_fixed_ptrsize_nz<Size_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Out_str_fixed_ptrsize_nz(Args_t &&...args)
      : detail::Out_str_fixed_ptrsize_nz_alias<Size_tp>(
            std::forward<Args_t>(args)...) {}
};

// ---- Growable output buffers ----

/// Growable output buffer wrapper, represented as std::string.
template <class String_tp>
class Out_str_growable_string
    : public detail::Out_str_growable_string_alias<String_tp> {
 public:
  explicit Out_str_growable_string(String_tp &str)
      : detail::Out_str_growable_string_alias<String_tp>(str) {}
};

/// Growable output buffer wrapper, represented as raw pointer to the
/// beginning, and integer size, null-terminated.
template <mysql::meta::Is_charlike Char_t, std::integral Size_tp>
class Out_str_growable_ptrsize_z
    : public detail::Out_str_growable_ptrsize_z_alias<Char_t, Size_tp> {
 public:
  explicit Out_str_growable_ptrsize_z(
      Char_t *&first, Size_tp &size, Size_tp capacity,
      const mysql::allocators::Memory_resource &memory_resource)
      : detail::Out_str_growable_ptrsize_z_alias<Char_t, Size_tp>(
            memory_resource, first, size, capacity) {}
};

/// Growable output buffer wrapper, represented as raw pointer to the
/// beginning, and integer size, non-null-terminated.
template <mysql::meta::Is_charlike Char_t, std::integral Size_tp>
class Out_str_growable_ptrsize_nz
    : public detail::Out_str_growable_ptrsize_nz_alias<Char_t, Size_tp> {
 public:
  explicit Out_str_growable_ptrsize_nz(
      Char_t *&first, Size_tp &size, Size_tp capacity,
      const mysql::allocators::Memory_resource &memory_resource)
      : detail::Out_str_growable_ptrsize_nz_alias<Char_t, Size_tp>(
            memory_resource, first, size, capacity) {}
};

/// Growable output buffer wrapper, represented as raw pointers to the beginning
/// and end, null-terminated.
template <mysql::meta::Is_charlike Char_t>
class Out_str_growable_ptrptr_z
    : public detail::Out_str_growable_ptrptr_z_alias<Char_t> {
 public:
  explicit Out_str_growable_ptrptr_z(
      Char_t *&first, Char_t *&last, Char_t *&capacity_end,
      const mysql::allocators::Memory_resource &memory_resource)
      : detail::Out_str_growable_ptrptr_z_alias<Char_t>(memory_resource, first,
                                                        last, capacity_end) {}
};

/// Growable output buffer wrapper, represented as raw pointers to the beginning
/// and end, non-null-terminated.
template <mysql::meta::Is_charlike Char_t>
class Out_str_growable_ptrptr_nz
    : public detail::Out_str_growable_ptrptr_nz_alias<Char_t> {
 public:
  explicit Out_str_growable_ptrptr_nz(
      Char_t *&first, Char_t *&last, Char_t *&capacity_end,
      const mysql::allocators::Memory_resource &memory_resource)
      : detail::Out_str_growable_ptrptr_nz_alias<Char_t>(memory_resource, first,
                                                         last, capacity_end) {}
};

// ==== API factory functions ====

// ---- API factory functions with resize policy "fixed", string argument ----

/// Return a wrapper around a non-growable output buffer, represented as a
/// std::string or similar type.
///
/// @param[in,out] str Reference to std::string, or other type that provides
/// similar `capacity`, `resize`, and `data` members.
///
/// @return Object of subclass of `Out_str_base`, which wraps `str`.
template <mysql::meta::Is_specialization<std::basic_string> String_t>
[[nodiscard]] auto out_str_fixed(String_t &str) {
  return Out_str_fixed_string<String_t>(str);
}

// ---- API factory functions with resize policy "fixed", raw pointers ----

/// Return a wrapper around a non-growable, null-terminated output buffer,
/// represented using a raw pointer to the beginning and a reference to an
/// integral size.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `length` to point to the zero byte.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters not counting the null-termination character.
///
/// @param capacity Available capacity to write string characters, not counting
/// the null-termination character. Note that this should be one less than the
/// actual buffer size. May be omitted, in which case this value is taken from
/// `length` instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <std::integral Size_t>
[[nodiscard]] auto out_str_fixed_z(
    mysql::meta::Is_pointer_to_charlike auto first, Size_t &length,
    Size_t capacity = 0) {
  // In case `first` is `unsigned char *`, cast to `char *` in order to collapse
  // template instantiations for `unsigned char *` and `char *`.
  //
  // The cast to `char *` is safe because C++ strict aliasing rules specifically
  // allow `char *` to alias other types. (In other cases in this file, casting
  // one argument to `char *` would require casting other arguments to `char
  // *&`, which is not allowed.)
  return Out_str_fixed_ptrsize_z<Size_t>(mysql::utils::char_cast(first), length,
                                         capacity);
}

/// Return a wrapper around a non-growable, null-terminated output buffer,
/// represented using raw a pointer to the beginning and a reference to a raw
/// pointer to the end.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `last` to point to the zero byte.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to the null-termination character.
///
/// @param capacity_end Pointer to the last character that may be used for the
/// null-termination character. Note that this is the last character *included*
/// in the buffer, not one-past-the-last buffer character. May be omitted, in
/// which case this value is taken from `last` instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t,
          mysql::meta::Is_pointer_to<Char_t> Charptr_t>
[[nodiscard]] auto out_str_fixed_z(Charptr_t first, Char_t *&last,
                                   Char_t *capacity_end = nullptr) {
  return Out_str_fixed_ptrptr_z<Char_t>(first, last, capacity_end);
}

/// Return a wrapper around a non-growable, non-null-terminated output buffer,
/// represented using a raw pointer to the beginning and a reference to an
/// integral size.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `length` to the past-the-end character.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters written. May be omitted, in which case this
/// value is taken from `length` instead.
///
/// @param capacity Available capacity to write string characters.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <std::integral Size_t>
[[nodiscard]] auto out_str_fixed_nz(
    mysql::meta::Is_pointer_to_charlike auto first, Size_t &length,
    Size_t capacity = 0) {
  // In case `first` is `unsigned char *`, cast to `char *` in order to collapse
  // template instantiations for `unsigned char *` and `char *`.
  //
  // The cast to `char *` is safe because C++ strict aliasing rules specifically
  // allow `char *` to alias other types. (In other cases in this file, casting
  // one argument to `char *` would require casting other arguments to `char
  // *&`, which is not allowed.)
  return Out_str_fixed_ptrsize_nz<Size_t>(mysql::utils::char_cast(first),
                                          length, capacity);
}

/// Return a wrapper around a non-growable, non-null-terminated output buffer,
/// represented using raw a pointer to the beginning and a reference to a raw
/// pointer to the end.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `last` to the past-the-end character.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to one-past-the-last character written.
///
/// @param capacity_end Pointer to one-past-the-last character that may be
/// written. May be omitted, in which case this value is taken from `last`
/// instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t,
          mysql::meta::Is_pointer_to<Char_t> Charptr_t>
[[nodiscard]] auto out_str_fixed_nz(Charptr_t first, Char_t *&last,
                                    Char_t *capacity_end = nullptr) {
  return Out_str_fixed_ptrptr_nz<Char_t>(first, last, capacity_end);
}

// ---- API factory functions with resize policy "fixed", arrays ----

// These are equivalent to the versions that take a "pointer", except they take
// an array whose length is known at compile-time as argument, and assert that
// the array size is sufficient.

/// Return a wrapper around a non-growable, null-terminated output buffer,
/// represented as an array and a reference to an integral size.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `length` to point to the zero byte.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters not counting the null-termination character.
///
/// @param capacity Available capacity to write string characters, not counting
/// the null-termination character. Note that this should be one less than the
/// actual buffer size. May be omitted, in which case this value is taken from
/// `length` instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <std::integral Size_t, std::ptrdiff_t array_size>
[[nodiscard]] auto out_str_fixed_z(
    mysql::meta::Is_charlike auto(&first)[array_size], Size_t &length,
    Size_t capacity = 0) {
  if (capacity == 0)
    assert(length <= array_size - 1);
  else
    assert(capacity <= array_size - 1);
  return out_str_fixed_z(reinterpret_cast<char *>(first), length, capacity);
}

/// Return a wrapper around a non-growable, null-terminated output buffer,
/// represented as an array and a reference to a raw pointer to the end.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `last` to point to the zero byte.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to the null-termination character.
///
/// @param capacity_end Pointer to the last character that may be used for the
/// null-termination character. Note that this is the last character *included*
/// in the buffer, not one-past-the-last buffer character. May be omitted, in
/// which case this value is taken from `last` instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t, std::ptrdiff_t array_size>
[[nodiscard]] auto out_str_fixed_z(Char_t (&first)[array_size], Char_t *&last,
                                   Char_t *capacity_end = nullptr) {
  if (capacity_end == nullptr)
    assert(last - first <= array_size - 1);
  else
    assert(capacity_end - first <= array_size - 1);
  return out_str_fixed_z(reinterpret_cast<Char_t *>(first), last, capacity_end);
}

/// Return a wrapper around a non-growable, non-null-terminated output buffer,
/// represented as an array and a reference to an integral size.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `length` to the past-the-end character.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters written.
///
/// @param capacity Available capacity to write string characters. May be
/// omitted, in which case this value is taken from `length` instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <std::integral Size_t, std::ptrdiff_t array_size>
[[nodiscard]] auto out_str_fixed_nz(
    mysql::meta::Is_charlike auto(&first)[array_size], Size_t &length,
    Size_t capacity = 0) {
  if (capacity == 0)
    assert(length <= array_size);
  else
    assert(capacity <= array_size);
  return out_str_fixed_nz(reinterpret_cast<char *>(first), length, capacity);
}

/// Return a wrapper around a non-growable, non-null-terminated output buffer,
/// represented as an array and a reference to a raw pointer to the end.
///
/// Functions accepting this type as argument will typically write to the
/// character buffer and alter `last` to the past-the-end character.
///
/// @param[in,out] first Raw pointer to the beginning of a character buffer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to one-past-the-last character written.
///
/// @param capacity_end Pointer to one-past-the-last character that may be
/// written. May be omitted, in which case this value is taken from `last`
/// instead.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t, std::ptrdiff_t array_size>
[[nodiscard]] auto out_str_fixed_nz(Char_t (&first)[array_size], Char_t *&last,
                                    Char_t *capacity_end = nullptr) {
  if (capacity_end == nullptr)
    assert(last - first <= array_size);
  else
    assert(capacity_end - first <= array_size);
  return out_str_fixed_nz(reinterpret_cast<Char_t *>(first), last,
                          capacity_end);
}

// ---- API factory functions with resize policy "growable" ----

/// Return a wrapper around a growable output buffer, represented as a
/// std::string or similar type.
///
/// @param[in,out] str Reference to std::string, or other type that provides
/// similar `capacity`, `reserve`, `resize`, and `data` members.
///
/// @return Object of subclass of `Out_str_base`, which wraps `str`.
template <mysql::meta::Is_specialization<std::basic_string> String_t>
[[nodiscard]] auto out_str_growable(String_t &str) {
  return Out_str_growable_string<String_t>(str);
}

/// Return a wrapper around a growable, null-terminated output buffer,
/// represented using a reference to a raw pointer to the beginning and a
/// reference to an integral size.
///
/// Functions accepting this type as argument will typically allocate a new
/// buffer and replace `first` by it if the given one is too small; then write
/// to the character buffer and alter `length` to point to the zero byte.
///
/// @param[in,out] first Reference to raw pointer to the beginning of a
/// character buffer. May be nullptr if length is 0; otherwise must be a valid
/// pointer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters not counting the null-termination character.
///
/// @param capacity Available capacity to write string characters, not counting
/// the null-termination character. Note that this should be one less than the
/// actual buffer size. May be omitted, in which case this value is taken from
/// `length` instead.
///
/// @param memory_resource Memory_resource to allocate bytes. If omitted, uses
/// the default-constructed Memory_resource, which uses std::malloc and
/// std::free.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t, std::integral Size_t>
[[nodiscard]] auto out_str_growable_z(
    Char_t *&first, Size_t &length, Size_t capacity = 0,
    const mysql::allocators::Memory_resource &memory_resource = {}) {
  return Out_str_growable_ptrsize_z<Char_t, Size_t>(first, length, capacity,
                                                    memory_resource);
}

/// Return a wrapper around a growable, null-terminated output buffer,
/// represented using reference to a raw a pointer to the beginning and a
/// reference to a raw pointer to the end.
///
/// Functions accepting this type as argument will typically allocate a new
/// buffer and replace `first` by it if the given one is too small; then write
/// to the character buffer and alter `last` to point to the zero byte.
///
/// @param[in,out] first Reference to raw pointer to the beginning of a
/// character buffer. May be nullptr if last is nullptr; otherwise must be a
/// valid pointer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to the null-termination character.
///
/// @param capacity_end Pointer to the last character that may be used for the
/// null-termination character. Note that this is the last character *included*
/// in the buffer, not one-past-the-last buffer character. May be omitted, in
/// which case this value is taken from `last` instead.
///
/// @param memory_resource Memory_resource to allocate bytes. If omitted, uses
/// the default-constructed Memory_resource, which uses std::malloc and
/// std::free.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t>
[[nodiscard]] auto out_str_growable_z(
    Char_t *&first, Char_t *&last, Char_t *capacity_end = nullptr,
    const mysql::allocators::Memory_resource &memory_resource = {}) {
  return Out_str_growable_ptrptr_z<Char_t>(first, last, capacity_end,
                                           memory_resource);
}

/// Return a wrapper around a growable, non-null-terminated output buffer,
/// represented using a reference to a raw pointer to the beginning and a
/// reference to an integral size.
///
/// Functions accepting this type as argument will typically allocate a new
/// buffer and replace `first` by it if the given one is too small; then write
/// to the character buffer and alter `length` to the past-the-end character.
///
/// @param[in,out] first Reference to raw pointer to the beginning of a
/// character buffer. May be nullptr if length is 0; otherwise must be a valid
/// pointer.
///
/// @param[out] length Reference to integer, which writers may update to the
/// number of string characters written.
///
/// @param capacity Available capacity to write string characters. May be
/// omitted, in which case this value is taken from `length` instead.
///
/// @param memory_resource Memory_resource to allocate bytes. If omitted, uses
/// the default-constructed Memory_resource, which uses std::malloc and
/// std::free.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t, std::integral Size_t>
[[nodiscard]] auto out_str_growable_nz(
    Char_t *&first, Size_t &length, Size_t capacity = 0,
    const mysql::allocators::Memory_resource &memory_resource = {}) {
  return Out_str_growable_ptrsize_nz<Char_t, Size_t>(first, length, capacity,
                                                     memory_resource);
}

/// Return a wrapper around a growable, non-null-terminated output buffer,
/// represented using reference to a raw a pointer to the beginning and a
/// reference to a raw pointer to the end
///
/// Functions accepting this type as argument will typically allocate a new
/// buffer and replace `first` by it if the given one is too small; then write
/// to the character buffer and alter `last` to point to the past-the-end
/// character.
///
/// @param[in,out] first Reference to raw pointer to the beginning of a
/// character buffer. May be nullptr if last is nullptr; otherwise must be a
/// valid pointer.
///
/// @param[out] last Reference to raw pointer, which writers may update to point
/// to one-past-the-last character written.
///
/// @param capacity_end Pointer to one-past-the-last character that may be
/// written. May be omitted, in which case this value is taken from `last`
/// instead.
///
/// @param memory_resource Memory_resource to allocate bytes. If omitted, uses
/// the default-constructed Memory_resource, which uses std::malloc and
/// std::free.
///
/// @return Object of subclass of `Out_str_base`, which wraps the given range of
/// characters.
template <mysql::meta::Is_charlike Char_t>
[[nodiscard]] auto out_str_growable_nz(
    Char_t *&first, Char_t *&last, Char_t *capacity_end = nullptr,
    const mysql::allocators::Memory_resource &memory_resource = {}) {
  return Out_str_growable_ptrptr_nz<Char_t>(first, last, capacity_end,
                                            memory_resource);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_OUT_STR_H
