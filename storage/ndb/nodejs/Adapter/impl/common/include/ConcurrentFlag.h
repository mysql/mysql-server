/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include "uv.h"

#include "compat_uv.h"

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define CONCURRENTFLAG_USE_GCC_ATOMICS
#else 
#define CONCURRENTFLAG_USE_LIBUV
#endif 
 

class ConcurrentFlag {
public:
  ConcurrentFlag();
  bool test();
  void set();
  void clear();
private:
#ifdef CONCURRENTFLAG_USE_GCC_ATOMICS
  int flag;
#else
  uv_rwlock_t lock;
  bool flag;
#endif
};

#ifdef CONCURRENTFLAG_USE_GCC_ATOMICS

inline ConcurrentFlag::ConcurrentFlag() {
  __sync_fetch_and_and(&flag, 0);
}

inline bool ConcurrentFlag::test() {
  return __sync_fetch_and_and(&flag, 0);
}

inline void ConcurrentFlag::set() {
  __sync_fetch_and_or(&flag, 1);
}

inline void ConcurrentFlag::clear() {
  __sync_fetch_and_and(&flag, 0);
}

#else 

inline ConcurrentFlag::ConcurrentFlag() {
  flag = 0;
  uv_rwlock_init(& lock);
}

inline bool ConcurrentFlag::test() { 
  bool val;
  uv_rwlock_rdlock(& lock);
  val = flag;
  uv_rwlock_rdunlock(& lock);
  return val;
}

inline void ConcurrentFlag::set() {
  uv_rwlock_wrlock(& lock);
  flag = true;
  uv_rwlock_wrunlock(& lock);
}

inline void ConcurrentFlag::clear() {
  uv_rwlock_wrlock(& lock);
  flag = false;
  uv_rwlock_wrunlock(& lock);
}

#endif



