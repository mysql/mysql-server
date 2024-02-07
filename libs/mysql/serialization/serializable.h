// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SERIALIZATION_SERIALIZABLE_H
#define MYSQL_SERIALIZATION_SERIALIZABLE_H

#include <tuple>
#include <unordered_set>

#include "mysql/serialization/field_definition.h"
#include "mysql/serialization/field_functor.h"
#include "mysql/serialization/serializable_size_calculator.h"
#include "mysql/serialization/serializable_type_tags.h"
#include "mysql/serialization/serialization_format_version.h"
#include "mysql/serialization/unknown_field_policy.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

// forward declarations

template <typename Serializer_type, typename... Args>
struct Serializable_size_calculator;

/// @brief Interface for serializable data structures. Classes that implement
/// serializable interface may be serialized automatically by the serializer
/// @note To be implemented by derived:
///   decltype(auto) define_fields { return std::make_tuple(...); } const and
///   non-const version
/// @details Please have a look at examples enclosed in unittest
/// The base class provides:
///  - methods to traverse through fields defined by the user in the
///    Derived_serializable_type
///  - methods to compute a compile-time upper bound on the size of message
///    defined by the user in Derived_serializable_type
///  - methods to compute the exact size of a particular message at runtime,
///    taking into account the current values of fields defined by the user
///    in the Derived_serializable_type
/// Derived class should provide:
///  - decltype(auto) define_fields { return std::make_tuple(...); } const
///    defines fields to be encoded. To define fields,
///    helpers defined in the "field_definition_helpers.h" shall be used.
///    Sufficient information about the field is the field reference. The
///    message designer can also define:
///     - fixed size of an integer / upper bound of the string length
///     - encode predicate, which will be called by the encoding functions
///       to determine whether field should be encoded
///     - an unknown_field_policy, defined by the encoder and applied by the
///       decoder in case decoder does not recognize a field
///  - decltype(auto) define_fields { return std::make_tuple(...); }
///    defines fields to be decoded. To define fields,
///    helpers defined in the "field_definition_helpers.h" shall be used.
///    Sufficient information about the field is the field reference. The
///    message designer can also define:
///     - fixed size of an integer / upper bound of the string length
///     - field missing functor, which will be called by the decoding functions
///       in case field is not encoded in the message
///     - an unknown_field_policy, defined by the encoder and applied by the
///       decoder in case decoder does not recognize a field
template <class Derived_serializable_type>
class Serializable {
 public:
  /// @brief calls functor for each field
  /// @tparam Serializable_functor_type Type of functor to be applied on the
  /// serializable
  /// @tparam Field_functor_type Type of functor to be applied on the field
  /// @param func_s Functor to be called for each serializable field
  /// @param func_f Functor to be called for each non-serializable field
  template <class Serializable_functor_type, class Field_functor_type>
  void do_for_each_field(Serializable_functor_type &&func_s,
                         Field_functor_type &&func_f);

  /// @brief calls functor for each field, const version
  /// @tparam Serializable_functor_type Type of functor to be applied on the
  /// serializable
  /// @tparam Field_functor_type Type of functor to be applied on the field
  /// @param func_s Functor to be called for each serializable field
  /// @param func_f Functor to be called for each non-serializable field
  template <class Serializable_functor_type, class Field_functor_type>
  void do_for_each_field(Serializable_functor_type &&func_s,
                         Field_functor_type &&func_f) const;

  // available for serializer derived

  /// @brief This function calculates last field id of this type
  /// @return last field id
  static constexpr Field_id_type get_last_field_id() {
    using Tuple_type_captured =
        decltype(std::declval<Derived_serializable_type>().define_fields());
    return std::tuple_size_v<Tuple_type_captured> - 1;
  }

  /// @brief Returns serializable object fields size, internal function
  /// (without serializable metadata size)
  /// @tparam Serializer_type Type of serializer used to format fields
  /// overhead added by serializer, including overhead of nested types
  /// @returns Serializable internal size
  template <typename Serializer_type>
  std::size_t get_size_internal() const;

  /// @brief Performs iteration over all of the serializable fields and checks
  /// whether any of the fields in this serializable is provided
  /// @return True in case any of the fields in all levels, starting from this
  /// one, is provided
  bool is_any_field_provided() const;

  /// @brief Returns serializable object fields maximum size, internal function
  /// (without serializable metadata size)
  /// @tparam Serializer_type Type of serializer used to format fields
  /// object will have this maximum size
  /// @return This class maximum declared size
  template <typename Serializer_type>
  static constexpr std::size_t get_max_size_internal() {
    using Tuple_type_captured =
        decltype(std::declval<Derived_serializable_type>().define_fields());
    return Serializable_size_calculator<Serializer_type,
                                        Tuple_type_captured>::get_max_size();
  }

  /// @brief Sets unknown field policy for this object
  /// @param policy Chosen policy, determines what decoder should do in case it
  /// encounters this object in the stream but this object definition is
  /// unknown to the decoder
  void set_unknown_field_policy(const Unknown_field_policy &policy) {
    m_unknown_field_policy = policy;
  }
  bool is_ignorable() const {
    return m_unknown_field_policy == Unknown_field_policy::ignore;
  }

 protected:
  using Tag = Serializable_tag;

  template <class Serializer_derived_type, class Archive_type>
  friend class Serializer;
  template <class Derived_serializable_type_current>
  friend class Serializable;

  /// @brief do_for_each_field helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Tuple_type, std::size_t... Is>
  void do_for_each_field(Serializable_functor_type &&func_s,
                         Field_functor_type &&func_f, Tuple_type &&tuple,
                         std::index_sequence<Is...>);

  /// @brief do_for_each_field helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&func_s,
                        Field_functor_type &&func_f, Field_type &field,
                        std::size_t field_id);

  /// @brief do_for_each_field helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&func_s,
                        Field_functor_type &&func_f, Field_type &field,
                        std::size_t field_id, Serializable_tag);

  /// @brief do_for_each_field helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&,
                        Field_functor_type &&func_f, Field_type &field,
                        std::size_t field_id, Field_definition_tag);

  /// @brief do_for_each_field (const) helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Tuple_type, std::size_t... Is>
  void do_for_each_field(Serializable_functor_type &&func_s,
                         Field_functor_type &&func_f, Tuple_type &&tuple,
                         std::index_sequence<Is...>) const;

  /// @brief do_for_each_field (const) helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&func_s,
                        Field_functor_type &&func_f, const Field_type &field,
                        std::size_t field_id) const;

  /// @brief do_for_each_field (const) helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&func_s,
                        Field_functor_type &&func_f, const Field_type &field,
                        std::size_t field_id, Serializable_tag) const;

  /// @brief do_for_each_field (const) helper
  template <class Serializable_functor_type, class Field_functor_type,
            class Field_type>
  void do_for_one_field(Serializable_functor_type &&,
                        Field_functor_type &&func_f, const Field_type &field,
                        std::size_t field_id, Field_definition_tag) const;

  // copy-move semantics
  Serializable() = default;
  Serializable(const Serializable &) = default;
  Serializable(Serializable &&) = default;
  Serializable &operator=(const Serializable &) = default;
  Serializable &operator=(Serializable &&) = default;
  /// @brief Creation of Serializable objects is prohibited
  virtual ~Serializable() = default;

 private:
  /// Unknown field policy for this serializable object
  Unknown_field_policy m_unknown_field_policy = Unknown_field_policy::ignore;
};

}  // namespace mysql::serialization

/// @}

#include "mysql/serialization/serializable_impl.hpp"

#endif  // MYSQL_SERIALIZATION_SERIALIZABLE_H
