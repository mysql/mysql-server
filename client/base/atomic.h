/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ATOMIC_INCLUDED
#define ATOMIC_INCLUDED

#include "my_global.h"
#include "my_atomic.h"

namespace my_boost{

template<typename TType> class atomic
{
public:
  atomic()
  {}
  atomic(const TType& value)
  {
    this->store(value);
  }
  TType load() const
  {
    return (TType)my_atomic_load64((volatile int64*)&m_value);
  }
  void store(TType value)
  {
    my_atomic_store64(&m_value, value);
  }
  bool compare_exchange_strong(TType& expected, TType destination)
  {
    int64 u64_expected= expected;
    bool res= my_atomic_cas64(&m_value, &u64_expected, destination);
    expected= u64_expected;
    return res;
  }

  TType operator ++()
  {
    return *this += 1;
  }

  TType operator ++(int)
  {
    return (TType)my_atomic_add64(&m_value, 1);
  }

  TType operator --()
  {
    return *this -= 1;
  }

  TType operator --(int)
  {
    return my_atomic_add64(&m_value, -1);
  }

  TType operator +=(TType value)
  {
    return my_atomic_add64(&m_value, value) + value;
  }

  TType operator -=(TType value)
  {
    return this->fetch_sub(value) - value;
  }

  TType fetch_sub(TType value)
  {
    return (TType)my_atomic_add64(&m_value, -(signed)value);
  }

  operator TType() const
  {
    return this->load();
  }

  void join();

private:
  volatile int64 m_value;
};

typedef atomic<uint32> atomic_uint32_t;
typedef atomic<int64> atomic_int64_t;
typedef atomic<uint64> atomic_uint64_t;
typedef atomic<bool> atomic_bool;

}

#endif
