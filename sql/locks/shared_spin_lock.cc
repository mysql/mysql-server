/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include <iostream>
#include <map>
#include <memory>

#include "locks/shared_spin_lock.h"

lock::Shared_spin_lock::Guard::Guard(
    lock::Shared_spin_lock &target,
    lock::Shared_spin_lock::enum_lock_acquisition acquisition,
    bool try_and_fail)
    : m_target(target), m_acquisition(Shared_spin_lock::SL_NO_ACQUISITION)
{
  if (acquisition != Shared_spin_lock::SL_NO_ACQUISITION)
  {
    this->acquire(acquisition, try_and_fail);
  }
}

lock::Shared_spin_lock::Guard::~Guard() { this->release(); }

lock::Shared_spin_lock *lock::Shared_spin_lock::Guard::operator->()
{
  return &this->m_target;
}

lock::Shared_spin_lock &lock::Shared_spin_lock::Guard::operator*()
{
  return this->m_target;
}

lock::Shared_spin_lock::Guard &lock::Shared_spin_lock::Guard::acquire(
    enum_lock_acquisition acquisition, bool try_and_fail)
{
  assert(this->m_acquisition == Shared_spin_lock::SL_NO_ACQUISITION);
  assert(acquisition == Shared_spin_lock::SL_SHARED ||
         acquisition == Shared_spin_lock::SL_EXCLUSIVE);

  this->m_acquisition= acquisition;

  switch (this->m_acquisition)
  {
    case Shared_spin_lock::SL_SHARED:
    {
      if (try_and_fail)
      {
        this->m_target.try_shared();
        if (!this->m_target.is_shared_acquisition())
        {
          this->m_acquisition= Shared_spin_lock::SL_NO_ACQUISITION;
        }
      }
      else
      {
        this->m_target.acquire_shared();
      }
      break;
    }
    case Shared_spin_lock::SL_EXCLUSIVE:
    {
      if (try_and_fail)
      {
        this->m_target.try_exclusive();
        if (!this->m_target.is_exclusive_acquisition())
        {
          this->m_acquisition= Shared_spin_lock::SL_NO_ACQUISITION;
        }
      }
      else
      {
        this->m_target.acquire_exclusive();
      }
      break;
    }
    default:
      break;
  }
  return (*this);
}

lock::Shared_spin_lock::Guard &lock::Shared_spin_lock::Guard::release()
{
  if (this->m_acquisition == Shared_spin_lock::SL_NO_ACQUISITION)
  {
    return (*this);
  }
  switch (this->m_acquisition)
  {
    case Shared_spin_lock::SL_SHARED:
    {
      this->m_target.release_shared();
      this->m_acquisition= Shared_spin_lock::SL_NO_ACQUISITION;
      break;
    }
    case Shared_spin_lock::SL_EXCLUSIVE:
    {
      this->m_target.release_exclusive();
      this->m_acquisition= Shared_spin_lock::SL_NO_ACQUISITION;
      break;
    }
    default:
      break;
  }
  return (*this);
}

lock::Shared_spin_lock::Guard::Guard(const Shared_spin_lock::Guard &rhs)
    : m_target(rhs.m_target)
{
}

lock::Shared_spin_lock::Guard &lock::Shared_spin_lock::Guard::operator=(
    Shared_spin_lock::Guard const &)
{
  return (*this);
}

lock::Shared_spin_lock::Shared_spin_lock()
    : m_shared_access(0), m_exclusive_access(0), m_exclusive_owner(0)
{
}

lock::Shared_spin_lock::~Shared_spin_lock() {}

lock::Shared_spin_lock &lock::Shared_spin_lock::acquire_shared()
{
  return this->try_or_spin_shared_lock(false);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::acquire_exclusive()
{
  return this->try_or_spin_exclusive_lock(false);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_shared()
{
  return this->try_or_spin_shared_lock(true);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_exclusive()
{
  return this->try_or_spin_exclusive_lock(true);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::release_shared()
{
  assert(my_atomic_load32(&this->m_shared_access) > 0);
  my_atomic_add32(&this->m_shared_access, -1);
  return (*this);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::release_exclusive()
{
  my_thread_t self= my_thread_self();
  my_thread_t owner= (my_thread_t)(my_atomic_load64(&this->m_exclusive_owner));
  assert(self != 0);
  assert(my_thread_equal(owner, self));
  if (!my_thread_equal(owner, self)) return (*this);

  if (my_atomic_load32(&this->m_exclusive_access) == 1)
    my_atomic_store64(&this->m_exclusive_owner, 0);

  assert(my_atomic_load32(&this->m_exclusive_access) > 0);
  my_atomic_add32(&this->m_exclusive_access, -1);
  return (*this);
}

bool lock::Shared_spin_lock::is_shared_acquisition()
{
  return my_atomic_load32(&this->m_shared_access) != 0;
}

bool lock::Shared_spin_lock::is_exclusive_acquisition()
{
  if (my_atomic_load32(&this->m_exclusive_access) != 0)
  {
    my_thread_t self= my_thread_self();
    my_thread_t owner=
        (my_thread_t)(my_atomic_load64(&this->m_exclusive_owner));
    return my_thread_equal(owner, self);
  }
  return false;
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_or_spin_shared_lock(
    bool try_and_fail)
{
  if (try_and_fail)
  {
    this->try_shared_lock();
  }
  else
  {
    this->spin_shared_lock();
  }
  return (*this);
}

lock::Shared_spin_lock &lock::Shared_spin_lock::try_or_spin_exclusive_lock(
    bool try_and_fail)
{
  my_thread_t self= my_thread_self();
  my_thread_t owner= (my_thread_t)(my_atomic_load64(&this->m_exclusive_owner));
  if (owner != 0 && my_thread_equal(owner, self))
  {
    my_atomic_add32(&this->m_exclusive_access, 1);
    return (*this);
  }

  if (try_and_fail)
  {
    if (!this->try_exclusive_lock())
    {
      return (*this);
    }
  }
  else
  {
    this->spin_exclusive_lock();
  }
#if defined(__APPLE__)
  my_atomic_store64(&this->m_exclusive_owner, reinterpret_cast<int64>(self));
#else
  my_atomic_store64(&this->m_exclusive_owner, self);
#endif
  return (*this);
}

bool lock::Shared_spin_lock::try_shared_lock()
{
  if (my_atomic_load32(&this->m_exclusive_access) != 0)
  {
    return false;
  }

  my_atomic_add32(&this->m_shared_access, 1);

  if (my_atomic_load32(&this->m_exclusive_access) != 0)
  {
    my_atomic_add32(&this->m_shared_access, -1);
    return false;
  }
  return true;
}

bool lock::Shared_spin_lock::try_exclusive_lock()
{
  int32 expected= 0;
  if (!my_atomic_cas32(&this->m_exclusive_access, &expected, 1))
  {
    return false;
  }
  if (my_atomic_load32(&this->m_shared_access) != 0)
  {
    my_atomic_store32(&this->m_exclusive_access, 0);
    return false;
  }
  return true;
}

void lock::Shared_spin_lock::spin_shared_lock()
{
  do
  {
    if (my_atomic_load32(&this->m_exclusive_access) != 0)
    {
      this->yield();
      continue;
    }

    my_atomic_add32(&this->m_shared_access, 1);

    if (my_atomic_load32(&this->m_exclusive_access) != 0)
    {
      my_atomic_add32(&this->m_shared_access, -1);
      this->yield();
      continue;
    }

    break;
  } while (true);
}

void lock::Shared_spin_lock::spin_exclusive_lock()
{
  bool success= false;
  do
  {
    int32 expected= 0;
    success= my_atomic_cas32(&this->m_exclusive_access, &expected, 1);
    if (!success) this->yield();
  } while (!success);

  while (my_atomic_load32(&this->m_shared_access) != 0)
  {
    this->yield();
  }
}

lock::Shared_spin_lock &lock::Shared_spin_lock::yield()
{
  my_thread_yield();
  return (*this);
}
