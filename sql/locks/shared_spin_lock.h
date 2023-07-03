/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef LOCK_SHARED_SPIN_LOCK_INCLUDED
#define LOCK_SHARED_SPIN_LOCK_INCLUDED

#include <atomic>
#include <map>
#include <memory>
#include <type_traits>

#include "sql/memory/aligned_atomic.h"

/**
  Provides atomic access in shared-exclusive modes. Shared mode allows for
  several threads to share lock acquisition. Exclusive mode will allow for
  a single thread to acquire the lock.

  The implementation also provides re-entrance, meaning that a thread is
  allowed to acquire the lock in the same mode several times without
  blocking. Re-entrance is symmetric, meaning, in the case the lock is
  acquired several times by the same thread, it should be released the same
  amount of times.

  Acquisition request priority management is implemented to avoid
  starvation, meaning:

  1) When no thread is holding the lock, acquisition is granted to the
     first thread to request it.

  2) If the lock is being held in shared mode and an exclusive acquisition
     request is made, no more shared or exclusive acquisition requests are
     granted until the exclusivity request is granted and released.

  The acquisition relation given to concurrent requests is as follows:

                   -------------------------------------------------------------
                   |              S2             |              E2             |
                   +-----------------------------+-----------------------------+
                   |   REQUEST    |   ACQUIRED   |   REQUEST    |   ACQUIRED   |
  -----------------+--------------+--------------------------------------------+
  |      | REQUEST |   S1 & S2    |   S1 & S2    |   S1 | E2    |      E2      |
  |  S1  |---------+--------------+--------------+--------------+--------------+
  |      | ACQUIRED|   S1 & S2    |   S1 & S2    |      S1      |      -       |
  -------+---------+--------------+--------------+--------------+--------------+
  |      | REQUEST |      E1      |      S2      |   E1 | E2    |      E2      |
  |  E1  |---------+--------------+--------------+--------------+--------------+
  |      | ACQUIRED|      E1      |      -       |      E1      |      -       |
  ------------------------------------------------------------------------------

  Legend:
  - S1: Thread that is requesting or has acquired in shared mode
  - S2: Thread that is requesting or has acquired in shared mode
  - E1: Thread that is requesting or has acquired in exclusive mode
  - E2: Thread that is requesting or has acquired in exclusive mode


 */
namespace lock {
class Shared_spin_lock {
 public:
  enum class enum_lock_acquisition {
    SL_EXCLUSIVE = 0,
    SL_SHARED = 1,
    SL_NO_ACQUISITION = 2
  };

  /**
    Sentry class for `Shared_spin_lock` to deliver RAII pattern usability.
   */
  class Guard {
   public:
    friend class Shared_spin_lock;

    /**
      Class constructor that receives the target spin-lock, whether or not
      it can be a shared acquisition and whether or not it should be a
      try-and-fail lock attempt, instead of a blocking attempt.

      @param target The target spin-lock.
      @param acquisition the acquisition type, SHARED, EXCLUSIVE or
             NO_ACQUISITION
      @param try_and_fail whether or not the lock attempt should be
                          blocking (only used if acquisition type is SHARED
                          or EXCLUSIVE).
     */
    Guard(Shared_spin_lock &target,
          enum_lock_acquisition acquisition = enum_lock_acquisition::SL_SHARED,
          bool try_and_fail = false);
    // Delete copy and move constructors
    Guard(Shared_spin_lock::Guard const &) = delete;
    Guard(Shared_spin_lock::Guard &&) = delete;
    //
    /**
      Destructor for the sentry. It will release any acquisition, shared or
      exclusive.
     */
    virtual ~Guard();

    // Delete copy and move operators
    Shared_spin_lock::Guard &operator=(Shared_spin_lock::Guard const &) =
        delete;
    Shared_spin_lock::Guard &operator=(Shared_spin_lock::Guard &&) = delete;
    //

    /**
      Arrow operator to access the underlying lock.

      @return A pointer to the underlying lock.
     */
    Shared_spin_lock *operator->();
    /**
      Star operator to access the underlying lock.

      @return A reference to the underlying lock.
     */
    Shared_spin_lock &operator*();
    /**
      If this instance was initialized without acquiring the lock
      (`NO_ACQUISITION ` passed to constructor) or the acquisition request
      wasn't granted (passing `try_and_fail = true` to the constructor),
      invoking this method will try to acquire the lock in the provided
      mode.

      @param acquisition the acquisition type, SHARED or EXCLUSIVE
      @param try_and_fail whether or not the lock attempt should be
                          blocking

      @return A reference to `this` object, for chaining purposes.
     */
    Shared_spin_lock::Guard &acquire(enum_lock_acquisition acquisition,
                                     bool try_and_fail = false);
    /**
      Releases the underlying lock acquisition, if any.

      @return A reference to `this` object, for chaining purposes.
     */
    Shared_spin_lock::Guard &release();

   private:
    /** The underlying lock */
    Shared_spin_lock &m_target;
    /** The type of lock acquisition to be requested */
    enum_lock_acquisition m_acquisition{enum_lock_acquisition::SL_SHARED};
  };
  friend class Shared_spin_lock::Guard;

  /**
    Default class constructor.
   */
  Shared_spin_lock() = default;
  /**
    Default class destructor.
   */
  virtual ~Shared_spin_lock() = default;

  /**
    Blocks until the lock is acquired in shared mode.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &acquire_shared();
  /**
    Blocks until the lock is acquired in exclusive mode.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &acquire_exclusive();
  /**
    Tries to acquire the lock in shared mode.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &try_shared();
  /**
    Tries to acquire the lock in exclusive mode.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &try_exclusive();
  /**
    Releases the previously granted shared acquisition request.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &release_shared();
  /**
    Releases the previously granted exclusive acquisition request.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &release_exclusive();
  /**
    Returns whether the lock is acquired for shared access by the invoking
    thread.

    @return true if the lock was acquired in shared mode by the invoking
            thread
   */
  bool is_shared_acquisition();
  /**
    Returns whether the lock is acquired for exclusive access by the
    invoking thread.

    @return true if the lock was acquired in exclusive mode by the invoking
            thread
   */
  bool is_exclusive_acquisition();

 private:
  /** The total amount of threads accessing in shared mode  */
  memory::Aligned_atomic<long> m_shared_access{0};
  /** Whether or not any thread is accessing in or waiting for exclusive mode */
  memory::Aligned_atomic<bool> m_exclusive_access{false};

  /**
    Tries to lock or waits for locking in shared mode and increases the
    thread-local lock acquisition shared counter.

    @param try_and_fail Whether or not to try to lock of wait for acquiring.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &try_or_spin_shared_lock(bool try_and_fail);
  /**
    Tries to lock or waits for locking in shared mode and increases the
    thread-local lock acquisition shared counter.

    @param try_and_fail Whether or not to try to lock of wait for acquiring.

    @return A reference to `this` object, for chaining purposes.
   */
  Shared_spin_lock &try_or_spin_exclusive_lock(bool try_and_fail);
  /**
    Tries to acquire in shared mode.

    @return `true` if the attempt to acquire the lock in shared mode was
            successful.
   */
  bool try_shared_lock();
  /**
    Tries to acquire in exclusive mode.

    @return `true` if the attempt to acquire the lock in exclusive mode was
            successful.
   */
  bool try_exclusive_lock();
  /**
    Blocks until the lock is acquired in shared mode.
   */
  void spin_shared_lock();
  /**
    Blocks until the lock is acquired in exclusive mode.
   */
  void spin_exclusive_lock();
  /**
    Returns the thread-local lock counter map.
   */
  static std::map<Shared_spin_lock *, long> &acquired_spins();
};
}  // namespace lock

#endif  // LOCK_SHARED_SPIN_LOCK_INCLUDED
