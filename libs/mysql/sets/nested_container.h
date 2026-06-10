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

#ifndef MYSQL_SETS_NESTED_CONTAINER_H
#define MYSQL_SETS_NESTED_CONTAINER_H

/// @file
/// Experimental API header

#include <cassert>                             // assert
#include <utility>                             // forward
#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/meta/not_decayed.h"            // Not_decayed
#include "mysql/sets/binary_operation.h"       // Binary_operation
#include "mysql/sets/nested_set_category.h"    // Nested_set_category_tag
#include "mysql/sets/nested_set_interface.h"   // Nested_set_interface
#include "mysql/sets/nested_set_meta.h"        // Is_nested_set_over_traits
#include "mysql/sets/set_container_helpers.h"  // handle_inplace_op_trivial_cases
#include "mysql/utils/forward_like.h"          // forward_like
#include "mysql/utils/return_status.h"         // void_to_ok

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Represents the subset of a Cartesian product "L x R" of two sets, using a
/// map data structure that maps elements of type L to containers holding values
/// of type R.
///
/// This never throws exceptions. All member functions that can fail, return a
/// status and have the [[nodiscard]] attribute.
///
/// @tparam Storage_tp Underlying storage
template <Is_nested_storage Storage_tp>
class Nested_container
    : public Basic_nested_container_wrapper<Nested_container<Storage_tp>,
                                            Storage_tp> {
 public:
  using Storage_t = Storage_tp;
  using This_t = Nested_container<Storage_t>;
  using Base_t =
      Basic_nested_container_wrapper<Nested_container<Storage_tp>, Storage_tp>;

  using Set_category_t = Nested_set_category_tag;

  using typename Base_t::Const_iterator_t;
  using typename Base_t::Iterator_t;
  using typename Base_t::Iterator_value_t;
  using typename Base_t::Key_t;
  using typename Base_t::Mapped_t;
  using typename Base_t::Set_traits_t;

  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Return_status_t = mysql::utils::Return_status;
  static constexpr Return_status_t return_ok = Return_status_t::ok;

  /// Construct a new, empty Nested_container.
  ///
  /// @param args any arguments are passed to the base class.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Nested_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}

  /// Return non-const reference to the underlying storage.
  [[nodiscard]] auto &storage() noexcept { return this->wrapped(); }

  /// Return const reference to the underlying storage.
  [[nodiscard]] const auto &storage() const noexcept { return this->wrapped(); }

  /// Return const iterator to the pair having the given key in the first
  /// component, or end() if the key is not in the set.
  ///
  /// @param key Key to look for.
  [[nodiscard]] Const_iterator_t find(const Key_t &key) const noexcept {
    return storage().find(key);
  }

  /// Return iterator to the pair having the given key in the first component,
  /// or end() if the key is not in the set.
  ///
  /// @param key Key to look for.
  [[nodiscard]] Iterator_t find(const Key_t &key) noexcept {
    return storage().find(key);
  }

  /// Return const iterator to the pair having the given key in the first
  /// component, or end() if the key is not in the set.
  ///
  /// @param key Key to look for.
  ///
  /// @param[in,out] cursor Iterator hint. If this is greater than the lower
  /// bound for key, the behavior is undefined. It will be updated to the upper
  /// bound for the key. Thus, it is suitable to use in successive calls to this
  /// function with increasing keys. This is not nodiscard because someone may
  /// want to only advance the cursor.
  Const_iterator_t find(Const_iterator_t &cursor,
                        const Key_t &key) const noexcept {
    return storage().find(cursor, key);
  }

  /// Return iterator to the pair having the given key in the first component,
  /// or end() if the key is not in the set.
  ///
  /// @param key Key to look for.
  ///
  /// @param[in,out] cursor Iterator hint. If this is greater than the lower
  /// bound for key, the behavior is undefined. It will be updated to the upper
  /// bound for the key. Thus, it is suitable to use in successive calls to this
  /// function with increasing keys. This is not nodiscard because someone may
  /// want to only advance the cursor.
  Iterator_t find(Iterator_t &cursor, const Key_t &key) noexcept {
    return storage().find(cursor, key);
  }

  /// Insert the given element (inplace union).
  ///
  /// This will create the value pair with the given key and an empty mapped
  /// container if it does not exist. Then it will invoke the `insert` member
  /// function of the mapped container.
  ///
  /// @param key Key of the element to insert.
  ///
  /// @param mapped_args Arguments passed to the `insert` member function of the
  /// mapped container.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can either when inserting into the
  /// storage for this object, or when inserting into the mapped container. On
  /// error, the container is left unchanged: in particular, if an error
  /// occurred after inserting the value pair into the map, the value pair is
  /// removed again.
  template <class... Mapped_args_t>
  [[nodiscard]] Return_status_t insert(
      const Key_t &key, Mapped_args_t &&...mapped_args) noexcept {
    return insert_or_union(
        [](Mapped_t &mapped_set, Mapped_args_t &&...mapped_args_fwd) {
          return mapped_set.insert(
              std::forward<Mapped_args_t>(mapped_args_fwd)...);
        },
        storage().emplace(key), std::forward<Mapped_args_t>(mapped_args)...);
  }

  /// Remove the given element from the set, if it is there.
  ///
  /// This will lookup the value pair in this set. If the key was found, remove
  /// the element from the mapped container by invoking its `remove` member
  /// function. If the resulting mapped container becomes empty, the value pair
  /// is removed from this container.
  ///
  /// @param key Key of the element to insert.
  ///
  /// @param value Arguments passed to the `remove` member function of the
  /// mapped container.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can happen if removing from the
  /// mapped container required an allocation, for example, to split an
  /// interval. On error, the container is left unchanged.
  Return_status_t remove(const Key_t &key, const auto &...value) noexcept {
    auto it = find(key);
    if (it != this->end()) {
      auto ret = it->second.remove(value...);
      if (ret != return_ok) return ret;
      if (!it->second) {
        storage().erase(it);
      }
    }
    return return_ok;
  }

  /// Insert the given set (inplace union).
  ///
  /// This will create the value pair with the given key and an empty mapped
  /// container if it does not exist. Then it will invoke the `inplace_union`
  /// member function of the mapped container.
  ///
  /// @param key Key of the element to insert.
  ///
  /// @param mapped_args Arguments passed to the `inplace_union` member function
  /// of the mapped container.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can either when inserting into the
  /// storage for this object, or when invoking `inplace_union` for the mapped
  /// container. This may leave the container as a superset of the previous set
  /// and a subset of the union.
  template <class... Mapped_args_t>
  [[nodiscard]] auto inplace_union(const Key_t &key,
                                   Mapped_args_t &&...mapped_args) noexcept {
    return insert_or_union(
        [](Mapped_t &mapped_set, Mapped_args_t &&...mapped_args_fwd) {
          return mapped_set.inplace_union(
              std::forward<Mapped_args_t>(mapped_args_fwd)...);
        },
        storage().emplace(key), std::forward<Mapped_args_t>(mapped_args)...);
  }

  /// Insert the given set (inplace union), reading and updating the given
  /// cursor.
  ///
  /// This will create the value pair with the given key and an empty mapped
  /// container if it does not exist. Then it will invoke the `inplace_union`
  /// member function of the mapped container.
  ///
  /// @param[in,out] cursor Hint for the insertion position. If this is greater
  /// than the lower bound for key, the behavior is undefined. It will be
  /// updated to the element after the inserted one, which makes it good to
  /// reuse for future calls to this function, with keys following this one.
  ///
  /// @param key Key of the element to insert.
  ///
  /// @param mapped_args Arguments passed to the `inplace_union` member function
  /// of the mapped container.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can either when inserting into the
  /// storage for this object, or when invoking `inplace_union` for the mapped
  /// container. This may leave the container as a superset of the previous set
  /// and a subset of the union.
  template <class... Mapped_args_t>
  [[nodiscard]] auto inplace_union(Iterator_t &cursor, const Key_t &key,
                                   Mapped_args_t &&...mapped_args) noexcept {
    return insert_or_union(
        [](Mapped_t &mapped_set, Mapped_args_t &&...mapped_args_fwd) {
          return mapped_set.inplace_union(
              std::forward<Mapped_args_t>(mapped_args_fwd)...);
        },
        storage().emplace(cursor, key),
        std::forward<Mapped_args_t>(mapped_args)...);
  }

  /// Inplace-insert the given set (inplace union) into this container.
  ///
  /// This will iterate over the input set and repeatedly invoke
  /// inplace_union(cursor, key, mapped).
  ///
  /// If other_set is an rvalue reference, and its allocator compares equal with
  /// this Nested_container's allocator, and the set types satisfy
  /// Can_donate_set_elements, this steals elements from other and does not
  /// allocate elements in the Nested_set. The same applies on all levels of
  /// nested containers, i.e., the mapped sets may steal if their allocators
  /// compare equal and their types satisfy Can_donate_set_elements; and so on
  /// on all nesting levels. If stealing is possible on all levels, this cannot
  /// fail.
  ///
  /// @param other_set Arguments passed to the `inplace_union` member function
  /// of the mapped container.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred, either when inserting into the storage
  /// for this object, or when invoking `inplace_union` for the mapped
  /// container. This may leave the container as a superset of the previous set
  /// and a subset of the union.
  template <Is_nested_set_over_traits_unqualified<Set_traits_t> Other_set_t>
  [[nodiscard]] auto inplace_union(Other_set_t &&other_set) noexcept {
    if (detail::handle_inplace_op_trivial_cases<Binary_operation::op_union>(
            *this, std::forward<Other_set_t>(other_set)))
      return return_ok;
    constexpr bool types_allow_donation =
        Can_donate_set_elements<decltype(std::forward<Other_set_t>(other_set)),
                                This_t>;
    bool can_donate = false;
    if constexpr (types_allow_donation) {
      can_donate = (this->get_allocator() == other_set.get_allocator());
    }

    auto this_cursor = this->begin();
    auto other_it = other_set.begin();
    while (other_it != other_set.end()) {
      auto &&[other_key, other_mapped] = *other_it;
      auto next_other_it = std::next(other_it);
      auto this_it = this->find(this_cursor, other_key);
      if (this_it == this->end()) {
        // Key not found in `this`. Then move or copy "mapped" from source.
        if (can_donate) {
          // Use move semantics.
          // Guard by `if constexpr (types_allow_donation)` since the call to
          // `steal_and_insert` may not compile otherwise.
          if constexpr (types_allow_donation) {
            this_cursor = storage().steal_and_insert(
                this_cursor, other_set.storage(), other_it);
          }
        } else {
          // Use copy semantics.
          auto ret = inplace_union(this_cursor, other_key, other_mapped);
          if (ret != return_ok) return ret;
        }
      } else {
        // Key found in `this`. Use Mapped_t::inplace_union.
        auto ret = this_it->second.inplace_union(
            mysql::utils::forward_like<Other_set_t>(*other_it).second);
        if (ret != return_ok) return ret;
      }
      other_it = next_other_it;
    }
    return return_ok;
  }

  /// Inplace-remove the given key and associated mapped set.
  ///
  /// @param key Key to remove.
  ///
  /// This operation cannot fail.
  void inplace_subtract(const Key_t &key) noexcept {
    auto it = find(key);
    if (it != this->end()) {
      storage().erase(it);
    }
  }

  /// Inplace-remove the given mapped set from the mapped set associated with
  /// the given key.
  ///
  /// @param key Key to find.
  ///
  /// @param mapped_args Arguments passed to `inplace_subtract` of the mapped
  /// set.
  ///
  /// @return Return_status::ok on success, or Return_status::error on
  /// out-of-memory error. This can occur if the `inplace_subtract` member
  /// function of the mapped container type can fail. It may leave this set as a
  /// subset of the old set and a superset of the difference set.
  template <class... Mapped_args_t>
  [[nodiscard]] auto inplace_subtract(const Key_t &key,
                                      Mapped_args_t &&...mapped_args) noexcept {
    auto cursor = this->begin();
    return inplace_subtract(cursor, key,
                            std::forward<Mapped_args_t>(mapped_args)...);
  }

  /// Inplace-remove the given mapped set from the mapped set associated with
  /// the given key, reading and updating the given cursor.
  ///
  /// @param[in,out] cursor Hint for the position of the key. If this is greater
  /// than the lower bound for key, the behavior is undefined. It will be
  /// updated to point to the element following key, which makes it good to
  /// reuse for future calls to this function, with keys following this one.
  ///
  /// @param key Key to find.
  ///
  /// @param mapped_args Arguments passed to `inplace_subtract` of the mapped
  /// set.
  ///
  /// @return Return_status::ok on success, or Return_status::error on
  /// out-of-memory error. This can occur if the `inplace_subtract` member
  /// function of the mapped container type can fail. It may leave this set as a
  /// subset of the old set and a superset of the difference set.
  template <class... Mapped_args_t>
  [[nodiscard]] auto inplace_subtract(Iterator_t &cursor, const Key_t &key,
                                      Mapped_args_t &&...mapped_args) noexcept {
    // The call to inplace_subtract may return void or Return_t, depending on
    // Mapped_args_t. Deduce which is the case adn propagate the same return
    // type from this function.
    using Return_t = decltype(std::declval<Mapped_t>().inplace_subtract(
        std::declval<Mapped_args_t &&>()...));
    constexpr bool can_fail = !std::same_as<Return_t, void>;

    auto ret = return_ok;
    auto it = find(cursor, key);
    if (it != this->end()) {
      ret = mysql::utils::void_to_ok([&it, &mapped_args...] {
        return it->second.inplace_subtract(
            std::forward<Mapped_args_t>(mapped_args)...);
      });
      advance_and_erase_if_empty(it);
      cursor = it;
    }
    if constexpr (can_fail) {
      return ret;
    } else {
      assert(ret == return_ok);
    }
  }

  /// Inplace-remove the given set from this container.
  ///
  /// This iterates over value pairs of this container and value pairs of the
  /// other set, where the key part of the two pairs are equal. In each step, it
  /// invokes `inplace_subtract` on the mapped container, passing the mapped
  /// other set as parameter. The number of iterations over value pairs is
  /// bounded by min(this->size, other_set.size()).
  ///
  /// If other_set is an rvalue reference, any time this function makes a
  /// recursive call to inplace_subtract on the mapped sets, it will pass an
  /// rvalue reference to a mapped set in other_set. This possibly enables
  /// stealing, if allowed by the types and allocators of those sets. If
  /// stealing is possible on all nesting levels, this cannot fail.
  ///
  /// @param other_set Set to subtract
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can happen if the
  /// `inplace_subtract` member of the mapped set fails. This may leave the
  /// container as a subset of the previous set and a superset of the
  /// difference.
  template <Is_nested_set_over_traits_unqualified<Set_traits_t> Other_set_t>
  [[nodiscard]] auto inplace_subtract(Other_set_t &&other_set) noexcept {
    if (detail::handle_inplace_op_trivial_cases<
            Binary_operation::op_subtraction>(
            *this, std::forward<Other_set_t>(other_set)))
      return return_ok;
    // Given iterators to this and other, pointing to the same key, subtract
    // the other iterator's mapped set from this operator's mapped set.
    auto do_subtract = [this](auto &this_it, auto &other_it) {
      auto ret = this_it->second.inplace_subtract(
          mysql::utils::forward_like<Other_set_t>(*other_it).second);
      this->advance_and_erase_if_empty(this_it);
      return ret;
    };

    auto other_cursor = other_set.begin();
    auto cursor = this->begin();
    while (cursor != this->end()) {
      auto other_it = other_set.find(other_cursor, cursor->first);
      if (other_it != other_set.end()) {
        // Found `cursor`'s key in other. Subtract the mapped set.
        auto ret = do_subtract(cursor, other_it);
        if (ret != return_ok) return ret;
      } else {
        // Did not find `cursor`'s key in `other`.

        // `cursor`'s key is greater than all keys in `other`. I.e., we have
        // exhausted `other` already and are done.
        if (other_cursor == other_set.end()) return return_ok;

        // `other_cursor` is the next-greater key in `other`. Try to find the
        // same key in this.
        auto it = this->find(cursor, other_cursor->first);
        if (it != this->end()) {
          auto ret = do_subtract(it, other_cursor);
          if (ret != return_ok) return ret;
          cursor = it;
        }
      }
    }
    return return_ok;
  }

  /// Inplace-remove all value pairs, except the one for the given key.
  ///
  /// @param key Key to keep.
  ///
  /// This operation cannot fail.
  void inplace_intersect(const Key_t &key) noexcept {
    auto it = storage().find(key);
    if (it == storage().end()) {
      storage().clear();
    } else {
      storage().erase(std::ranges::next(it), storage().end());
      storage().erase(storage().begin(), it);
    }
  }

  /// Inplace-remove all value pairs, except the one for the given key, and
  /// inplace-intersect the mapped container for the given key with the given
  /// mapped set.
  ///
  /// @param key Key to keep.
  ///
  /// @param mapped_args Arguments to pass to inplace_intersect on the mapped
  /// set.
  ///
  /// @return Return_status::ok if the operation succeeds, or
  /// Return_status::error if an out-of-memory condition occurs. This can happen
  /// if the  inplace_intersect operation of the mapped container fails. This
  /// may leave the container as a subset of the previous set and a superset of
  /// the intersection.
  template <class... Mapped_args_t>
  [[nodiscard]] auto inplace_intersect(
      const Key_t &key, Mapped_args_t &&...mapped_args) noexcept {
    // The call to inplace_intersect may return void or Return_t, depending on
    // Mapped_args_t. Deduce which is the case adn propagate the same return
    // type from this function.
    using Return_t = decltype(std::declval<Mapped_t>().inplace_intersect(
        std::declval<Mapped_args_t>()...));
    constexpr bool can_fail = !std::same_as<Return_t, void>;

    inplace_intersect(key);
    auto ret = return_ok;
    if (!storage().empty()) {
      auto &&mapped = storage().begin()->second;
      ret = mysql::utils::void_to_ok([&mapped, &mapped_args...] {
        return mapped.inplace_intersect(
            std::forward<Mapped_args_t>(mapped_args)...);
      });
      if (mapped.empty()) storage().clear();
    }
    if constexpr (can_fail) {
      return ret;
    } else {
      assert(ret == return_ok);
    }
  }

  /// Inplace-intersect this set with the given set.
  ///
  /// If other_set is an rvalue reference, any time this function makes a
  /// recursive call to inplace_intersect on the mapped sets, it will pass an
  /// rvalue reference to a mapped set in other_set. This possibly enables
  /// stealing, if allowed by the types and allocators of those sets. If
  /// stealing is possible on all nesting levels, this cannot fail.
  ///
  /// @param other_set Set to intersect with.
  ///
  /// @return Return_status::ok on success, or Return_status::error if an
  /// out-of-memory condition occurred. This can happen if the
  /// `inplace_intersect` member of the mapped set fails. This may leave the
  /// container as a subset of the previous set and a superset of the
  /// intersection.
  template <Is_nested_set_over_traits_unqualified<Set_traits_t> Other_set_t>
  [[nodiscard]] auto inplace_intersect(Other_set_t &&other_set) noexcept {
    if (detail::handle_inplace_op_trivial_cases<
            Binary_operation::op_intersection>(
            *this, std::forward<Other_set_t>(other_set)))
      return return_ok;
    // Iterate over all key-mapped pairs of this set.
    auto it = this->begin();
    auto other_cursor = other_set.begin();
    while (it != this->end()) {
      auto other_it = other_set.find(other_cursor, it->first);
      if (other_it == other_set.end()) {
        // If other_set does not have any element with this key, just erase
        // the key from this set.
        it = storage().erase(it);
      } else {
        // If other_set has an element with this key, intersect the mapped
        // sets.
        auto ret = it->second.inplace_intersect(
            mysql::utils::forward_like<Other_set_t>(*other_it).second);
        advance_and_erase_if_empty(it);
        if (ret != return_ok) return ret;
      }
    }
    return return_ok;
  }

 private:
  /// Common implementation for `insert` and several of the `inplace_union`
  /// operations: it performs the insertion operation, given an iterator to the
  /// key-value pair and a mapped set.
  ///
  /// @tparam Mapped_args_t Type of parameters passed to the lambda.
  ///
  /// @param func Callable that will perform the insert or inplace_union
  /// operation in the mapped set. It will be passed the mapped set, followed by
  /// the arguments in `mapped_args`.
  ///
  /// @param opt_it std::optional that may hold an iterator to the value pair on
  /// which the `insert` or `inplace_union` operation should operate. If this
  /// does not hold a value, this function returns Return_status::error
  /// immediately; otherwise it attempts to insert into the mapped container it
  /// points to.
  ///
  /// @param mapped_args Parameters to pass to `func`.
  ///
  /// @return Return_status::ok on success; Return_status::error if either
  /// `opt_it` does not hold a value, or `func` fails.
  template <class... Mapped_args_t>
  [[nodiscard]] auto insert_or_union(const auto &func, const auto &opt_it,
                                     Mapped_args_t &&...mapped_args) noexcept {
    if (!opt_it.has_value()) return Return_status_t::error;
    auto &it = opt_it.value();
    auto &mapped_set = it->second;
    auto ret = mysql::utils::void_to_ok(
        func, mapped_set, std::forward<Mapped_args_t>(mapped_args)...);
    if (mapped_set.empty()) {
      // Can happen if emplace had to insert a new mapped_set, and then
      // mapped_set.insert caused an out-of-memory condition.
      storage().erase(it);
    }
    return ret;
  }

  /// Helper that will erase the value pair that the iterator points to *if* it
  /// is empty, and, regardless if it erased or not, make the iterator point to
  /// the next element.
  ///
  /// This will point to the next element even if an erase operation was
  /// performed which invalidated iterators.
  ///
  /// @param iterator Iterator to operate on.
  void advance_and_erase_if_empty(Iterator_t &iterator) noexcept {
    if (iterator->second.empty()) {
      iterator = storage().erase(iterator);
    } else {
      ++iterator;
    }
  }
};  // class Nested_container

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_CONTAINER_H
