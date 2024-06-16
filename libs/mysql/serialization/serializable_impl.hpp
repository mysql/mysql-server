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

/// @file
/// Experimental API header

namespace mysql::serialization {

template <class Derived_serializable_type>
template <typename Serializer_type>
std::size_t Serializable<Derived_serializable_type>::get_size_internal() const {
  std::size_t calculated_size = 0;
  auto add_size_s = [&calculated_size](const auto &field,
                                       auto field_id) -> auto{
    calculated_size += Serializer_type::get_size_serializable(field_id, field);
  };
  auto add_size_f = [&calculated_size](const auto &field,
                                       auto field_id) -> auto{
    calculated_size += Serializer_type::get_size_field_def(field_id, field);
  };
  do_for_each_field(add_size_s, add_size_f);
  return calculated_size;
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type>
void Serializable<Derived_serializable_type>::do_for_each_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f) {
  using Tuple_type =
      decltype(std::declval<Derived_serializable_type>().define_fields());
  do_for_each_field(
      std::forward<Serializable_functor_type>(func_s),
      std::forward<Field_functor_type>(func_f),
      static_cast<Derived_serializable_type *>(this)->define_fields(),
      std::make_index_sequence<std::tuple_size_v<Tuple_type>>{});
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type>
void Serializable<Derived_serializable_type>::do_for_each_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f) const {
  using Tuple_type =
      decltype(std::declval<const Derived_serializable_type>().define_fields());
  const auto *derived_ptr =
      static_cast<const Derived_serializable_type *>(this);
  do_for_each_field(std::forward<Serializable_functor_type>(func_s),
                    std::forward<Field_functor_type>(func_f),
                    (derived_ptr)->define_fields(),
                    std::make_index_sequence<std::tuple_size_v<Tuple_type>>{});
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Tuple_type, std::size_t... Is>
void Serializable<Derived_serializable_type>::do_for_each_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f,
    Tuple_type &&tuple, std::index_sequence<Is...>) {
  (do_for_one_field(std::forward<Serializable_functor_type>(func_s),
                    std::forward<Field_functor_type>(func_f),
                    std::get<Is>(tuple), Is),
   ...);
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f,
    Field_type &field, std::size_t field_id) {
  using Current_field_type_bare = typename std::decay<Field_type>::type;
  using Current_tag_type = typename Current_field_type_bare::Tag;
  do_for_one_field(std::forward<Serializable_functor_type>(func_s),
                   std::forward<Field_functor_type>(func_f), field, field_id,
                   Current_tag_type{});
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&func_s, Field_functor_type &&,
    Field_type &field, std::size_t field_id, Serializable_tag) {
  func_s(field, field_id);
  // we only call only function for serializable
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&, Field_functor_type &&func_f,
    Field_type &field, std::size_t field_id, Field_definition_tag) {
  func_f(field, field_id);
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Tuple_type, std::size_t... Is>
void Serializable<Derived_serializable_type>::do_for_each_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f,
    Tuple_type &&tuple, std::index_sequence<Is...>) const {
  (do_for_one_field(std::forward<Serializable_functor_type>(func_s),
                    std::forward<Field_functor_type>(func_f),
                    std::get<Is>(tuple), Is),
   ...);
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&func_s, Field_functor_type &&func_f,
    const Field_type &field, std::size_t field_id) const {
  using Current_field_type_bare = typename std::decay<Field_type>::type;
  using Current_tag_type = typename Current_field_type_bare::Tag;
  do_for_one_field(std::forward<Serializable_functor_type>(func_s),
                   std::forward<Field_functor_type>(func_f), field, field_id,
                   Current_tag_type{});
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&func_s, Field_functor_type &&,
    const Field_type &field, std::size_t field_id, Serializable_tag) const {
  func_s(field, field_id);
  // we only call only function for serializable
}

template <class Derived_serializable_type>
template <class Serializable_functor_type, class Field_functor_type,
          class Field_type>
void Serializable<Derived_serializable_type>::do_for_one_field(
    Serializable_functor_type &&, Field_functor_type &&func_f,
    const Field_type &field, std::size_t field_id, Field_definition_tag) const {
  func_f(field, field_id);
}

template <class Derived_serializable_type>
bool Serializable<Derived_serializable_type>::is_any_field_provided() const {
  bool is_provided = false;
  auto func_is_provided_s = [&is_provided](const auto &serializable,
                                           const auto &) -> void {
    is_provided = serializable.is_any_field_provided();
  };
  auto func_is_provided_f = [&is_provided](const auto &field,
                                           const auto &) -> void {
    if (field.run_encode_predicate()) {
      is_provided = true;
    }
  };
  using Tuple_type =
      decltype(std::declval<Derived_serializable_type>().define_fields());
  do_for_each_field(
      func_is_provided_s, func_is_provided_f,
      static_cast<const Derived_serializable_type *>(this)->define_fields(),
      std::make_index_sequence<std::tuple_size_v<Tuple_type>>{});
  return is_provided;
}

}  // namespace mysql::serialization
