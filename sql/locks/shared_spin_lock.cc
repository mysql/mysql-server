/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <type_traits>

#include "sql/locks/shared_spin_lock.h"

lock::Shared_spin_lock::Guard::Guard(
    lock::Shared_spin_lock &target,
    lock::Shared_spin_lock::enum_lock_acquisition acquisition,
    bool try_and_fail)
    : m_target{target},
      m_acquisition{enum_lock_acquisition::SL_NO_ACQUISITION} {
  if (acquisition != enum_lock_acquisition::SL_NO_ACQUISITION) {
    this->acquire(acquisition, try_and_fail);
  }
}

lock::Shared_spin_lock::Guard::~Guard() { this->release(); }

lock::Shared_spin_lock *lock::Shared_spin_lock::Guard::operator->() {
  return &this->m_target;
}

lock::Shared_spin_lock &lock::Shared_spin_lock::Guard::operator*() {
  return this->m_target;
}

lock::Shared_spin_lock::Guard &lock::Shared_spin_lock::Guard::acquire(
    enum_lock_acquisition acquisition, bool try_and_fail) {
  assert(this->m_acquisition == enum_lock_acquisition::SL_NO_ACQUISITION);
  assert(acquisition == enum_lock_acquisition::SL_SHARED ||
         acquisition == enum_lock_acquisition::SL_EXCLUSIVE);

  this->m_acquisition = acquisition;

  switch (this->m_acquisition) {
    case enum_lock_acquisition::SL_SHARED: {
      if (try_and_fail) {
        this->m_target.try_shared();
        if (!this->m_target.is_shared_acquisition()) {
          this->m_acquisition = enum_lock_acquisition::SL_NO_ACQUISITION;
        }
      } else {
        this->m_target.acquire_shared();
      }
      break;
    }
    case enum_lock_acquisition::SL_EXCLUSIVE: {
      if (try_and_fail) {
        this->m_target.try_exclusive();
        if (!this->m_target.is_exclusive_acquisition()) {
          this->m_acquisition = enum_lock_acquisition::SL_NO_ACQUISITION;
        }
      } else {
        this->m_target.acquire_exclusive();
      }
      break;
    }
    default:
      break; /* purecov: inspected */
  }
  return (*this);
}

lock::Shared_spin_lock::Guard &lock::Shared_spin_lock::Guard::release() {
  if (this->m_acquisition == enum_lock_acquisition::SL_NO_ACQUISITION) {
    return (*this);
  }
  switch (this->m_acquisition) {
    case enum_lock_acquisition::SL_SHARED: {
      this->m_target.release_shared();
      this->m_acquisition = enum_lock_acquisition::SL_NO_ACQUISITION;
      break;
    }
    case enum_lock_acquisition::SL_EXCLUSIVE: {
      this->m_target.release_exclusive();
      this->m_acquisition = enum_lock_acquisition::SL_NO_ACQUISITION;
      break;
    }
    default:
      break; /* purecov: inspected */
  }
  return (*this);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::acquire_shared() {
  return this->try_or_spin_shared_lock(false);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::acquire_exclusive() {
  return this->try_or_spin_exclusive_lock(false);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_shared() {
  return this->try_or_spin_shared_lock(true);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_exclusive() {
  return this->try_or_spin_exclusive_lock(true);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::release_shared() {
  auto found = lock::Shared_spin_lock::acquired_spins().find(this);
  assert(this->is_shared_acquisition());
  if (!this->is_shared_acquisition()) {
    // shared spin lock not acquired by thread
    return (*this); /* purecov: inspected */
  }
  --found->second;
  if (found->second == 0) {
    lock::Shared_spin_lock::acquired_spins().erase(found);
    this->m_shared_access->fetch_sub(1, std::memory_order_release);
  }
  return (*this);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::release_exclusive() {
  auto found = lock::Shared_spin_lock::acquired_spins().find(this);
  assert(this->is_exclusive_acquisition());
  if (!this->is_exclusive_acquisition()) {
    // exclusive spin lock not acquired by thread
    return (*this); /* purecov: inspected */
  }
  ++found->second;
  if (found->second == 0) {
    lock::Shared_spin_lock::acquired_spins().erase(found);
    this->m_exclusive_access->store(false, std::memory_order_release);
  }
  return (*this);
}

bool lock::Shared_spin_lock::is_shared_acquisition() {
  auto found = lock::Shared_spin_lock::acquired_spins().find(this);
  return found != lock::Shared_spin_lock::acquired_spins().end() &&
         found->second > 0;
}

bool lock::Shared_spin_lock::is_exclusive_acquisition() {
  auto found = lock::Shared_spin_lock::acquired_spins().find(this);
  return found != lock::Shared_spin_lock::acquired_spins().end() &&
         found->second < 0;
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_or_spin_shared_lock(
    bool try_and_fail) {
  auto &count = lock::Shared_spin_lock::acquired_spins()[this];
  if (count == 0) {
    if (try_and_fail) {
      if (!this->try_shared_lock()) {
        return (*this);
      }
    } else {
      this->spin_shared_lock();
    }
  } else if (count < 0) {
    // lock already by thread acquired NOT in shared mode
    return (*this);
  }
  ++count;
  return (*this);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_or_spin_exclusive_lock(
    bool try_and_fail) {
  auto &count = lock::Shared_spin_lock::acquired_spins()[this];
  if (count == 0) {
    if (try_and_fail) {
      if (!this->try_exclusive_lock()) {
        return (*this);
      }
    } else {
      this->spin_exclusive_lock();
    }
  } else if (count > 0) {
    // lock already acquired by thread NOT in exclusive mode
    return (*this);
  }
  --count;
  return (*this);
}

bool lock::Shared_spin_lock::try_shared_lock() {
  if (this->m_exclusive_access->load(std::memory_order_seq_cst)) {
    return false;
  }

  this->m_shared_access->fetch_add(1, std::memory_order_release);

  if (this->m_exclusive_access->load(std::memory_order_seq_cst)) {
    this->m_shared_access->fetch_sub(1, std::memory_order_release);
    return false;
  }
  return true;
}

bool lock::Shared_spin_lock::try_exclusive_lock() {
  if (this->m_exclusive_access->exchange(true, std::memory_order_seq_cst)) {
    return false;
  }
  if (this->m_shared_access->load(std::memory_order_acquire) != 0) {
    this->m_exclusive_access->store(false, std::memory_order_seq_cst);
    return false;
  }
  return true;
}

void lock::Shared_spin_lock::spin_shared_lock() {
  do {
    if (this->m_exclusive_access->load(std::memory_order_seq_cst)) {
      std::this_thread::yield();
      continue;
    }

    this->m_shared_access->fetch_add(1, std::memory_order_release);

    if (this->m_exclusive_access->load(std::memory_order_seq_cst)) {
      this->m_shared_access->fetch_sub(1, std::memory_order_release);
      std::this_thread::yield();
      continue;
    }

    break;
  } while (true);
}

void lock::Shared_spin_lock::spin_exclusive_lock() {
  while (this->m_exclusive_access->exchange(true, std::memory_order_seq_cst)) {
    std::this_thread::yield();
  }
  while (this->m_shared_access->load(std::memory_order_acquire) != 0) {
    std::this_thread::yield();
  }
}

std::map<lock::Shared_spin_lock *, long>
    &lock::Shared_spin_lock::acquired_spins() {
  // TODO: garbage collect this if spin-locks start to be used more dynamically
  static thread_local std::map<lock::Shared_spin_lock *, long> acquired_spins;
  return acquired_spins;
}
