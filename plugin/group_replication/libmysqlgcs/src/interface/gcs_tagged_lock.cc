/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/interface/gcs_tagged_lock.h"

Gcs_tagged_lock::Gcs_tagged_lock() noexcept : m_lock_word(0) {}

Gcs_tagged_lock::~Gcs_tagged_lock() {}

std::uint64_t Gcs_tagged_lock::get_lock_word(
    std::memory_order semantics) const {
  return m_lock_word.load(semantics);
}

/*
 * Retrieves the tag portion of the given lock word.
 */
static Gcs_tagged_lock::Tag get_tag(std::uint64_t const &lock_word) {
  auto tag = (lock_word >> 1);

  return tag;
}

Gcs_tagged_lock::Tag Gcs_tagged_lock::optimistic_read() const {
  // Serialisation point.
  auto lock_word = get_lock_word();

  return get_tag(lock_word);
}

/*
 *Checks whether the lock bit of the given lock word is set.
 */
static bool is_locked_internal(std::uint64_t const &lock_word) {
  auto lock_bit = (lock_word & 1);

  bool const locked = (lock_bit == 1);
  return locked;
}

/*
 * Checks whether the given tag matches the tag of the given lock word.
 */
static bool same_tag(std::uint64_t const &lock_word, Gcs_tagged_lock::Tag tag) {
  auto current_tag = get_tag(lock_word);

  bool const tags_are_the_same = (current_tag == tag);
  return tags_are_the_same;
}

bool Gcs_tagged_lock::validate_optimistic_read(
    Gcs_tagged_lock::Tag const &tag) const {
  // Serialisation point.
  auto lock_word = get_lock_word();

  bool const successful =
      (!is_locked_internal(lock_word) && same_tag(lock_word, tag));
  return successful;
}

/*
 * Sets the lock bit of the given lock word.
 */
static void set_lock_bit(std::uint64_t &lock_word) {
  lock_word = (lock_word | 1);
}

/*
 * Atomically sets the lock bit of the given lock word, if its tag matches the
 * given tag.
 */
static bool try_lock_internal(std::atomic<std::uint64_t> &lock_word,
                              std::uint64_t const &locked_tag) {
  auto unlocked_tag = (get_tag(locked_tag) << 1);
  return lock_word.compare_exchange_strong(unlocked_tag, locked_tag,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed);
}

bool Gcs_tagged_lock::try_lock() {
  auto lock_word = get_lock_word(std::memory_order_relaxed);

  set_lock_bit(lock_word);

  // Serialisation point.
  bool const successful = try_lock_internal(m_lock_word, lock_word);
  return successful;
}

void Gcs_tagged_lock::unlock() {
  // Serialisation point.
  m_lock_word.fetch_add(1, std::memory_order_acq_rel);
}

bool Gcs_tagged_lock::is_locked() const {
  auto lock_word = get_lock_word();
  return is_locked_internal(lock_word);
}
