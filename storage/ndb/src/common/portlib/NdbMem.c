/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbMem.h>


int NdbMem_MemLockAll(int i){
  if (i == 1)
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && defined (MCL_FUTURE)
    return mlockall(MCL_CURRENT | MCL_FUTURE);
#else
    return -1;
#endif
  }
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return mlockall(MCL_CURRENT);
#else
  return -1;
#endif
}

int NdbMem_MemUnlockAll(){
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return munlockall();
#else
  return -1;
#endif
}

int NdbMem_MemLock(const void * ptr, size_t len)
{
#if defined(HAVE_MLOCK)
  return mlock(ptr, len);
#else
  return -1;
#endif
}
