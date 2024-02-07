/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_TAGGED_LOCK_INCLUDED
#define GCS_TAGGED_LOCK_INCLUDED

#include <atomic>   // std::atomic
#include <cstdint>  // std::uint64_t

/**
 * @brief The Gcs_tagged_lock class
 * Implements a tagged lock for optimistic read-side sections.
 *
 * In a nutshell, the tagged lock is a read-write spin lock which offers the
 * following API:
 *
 *     try_lock() -> bool
 *     unlock()
 *     optimistic_read() -> tag
 *     validate_optimistic_read(tag) -> bool
 *
 * For the write-side section, one uses it as a typical spin lock, e.g.:
 *
 *     do:
 *       lock_acquired := try_lock()
 *     while (not lock_acquired)
 *     write-side section
 *     unlock()
 *
 * For the read-side section, one can use it as follows:
 *
 *     done := false
 *     while (not done):
 *       tag := optimistic_read()
 *       unsynchronised read-side section
 *       done := validate_optimistic_read(tag)
 *       if (not done):
 *         rollback unsynchronized read-side section
 *
 * The idea is to allow an optimistic read-side section that does not perform
 * any memory stores.
 * This is in contrast with a typical read-write lock, where the read side
 * performs some memory stores to account for the reader, e.g. keeping a reader
 * counter.
 * The trade off is that:
 *
 *   a. the execution of the read-side of a tagged lock may be concurrent with
 *      the write-side section if meanwhile the tagged lock is acquired
 *   b. the read-side of a tagged lock may fail if meanwhile the tagged lock is
 *      acquired, in which case one may want to rollback the effects of the
 *      failed read-side section
 *
 * The tagged lock is implemented over a single atomic 64-bit word with the
 * following bit layout:
 *
 *     bit #    64  63  62        3   2   1
 *            +---+---+---+-...-+---+---+---+
 *            |   |   |   |     |   |   |   |
 *            +---+---+---+-...-+---+---+---+
 *             \__________  ___________/ \ /
 *                        \/              v
 *                        tag            locked?
 */
class Gcs_tagged_lock {
 public:
  using Tag = std::uint64_t;

  Gcs_tagged_lock() noexcept;
  ~Gcs_tagged_lock();

  /**
   * Starts an optimistic read-side section.
   * @returns the tag associated with the optimistic execution.
   */
  Tag optimistic_read() const;

  /**
   * Validates an optimistic read-side section.
   * @param tag The tag returned by the corresponding @c optimistic_read
   * @returns true if the optimistic read-side was atomically executed while the
   * lock was free, false otherwise
   */
  bool validate_optimistic_read(Tag const &tag) const;

  /**
   * Attempts to start a write-side section, i.e. acquire the lock.
   * @returns true if the write-side section was successfully started, i.e. we
   * acquired the lock, false otherwise
   */
  bool try_lock();

  /**
   * Finishes the write-side section, i.e. releases the lock.
   */
  void unlock();

  /**
   * Checks whether the lock is currently acquired.
   * @returns true if the lock is currently acquired, false otherwise
   */
  bool is_locked() const;

 private:
  /*
   * Atomically loads the underlying lock word.
   */
  std::uint64_t get_lock_word(
      std::memory_order order = std::memory_order_acquire) const;

  /* The underlying lock word. */
  std::atomic<std::uint64_t> m_lock_word;
};

#endif  // GCS_TAGGED_LOCK_INCLUDED
