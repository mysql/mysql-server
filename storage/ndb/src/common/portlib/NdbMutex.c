/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

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


#include <ndb_global.h>

#include <NdbMutex.h>
#include <NdbMem.h>

NdbMutex* NdbMutex_Create(void)
{
  NdbMutex* pNdbMutex;
  int result;

  pNdbMutex = (NdbMutex*)NdbMem_Allocate(sizeof(NdbMutex));

  if (pNdbMutex == NULL)
    return NULL;

  result = NdbMutex_Init(pNdbMutex);
  if (result == 0)
  {
    return pNdbMutex;
  }
  NdbMem_Free(pNdbMutex);
  return 0;
}

int NdbMutex_Init(NdbMutex* pNdbMutex)
{
  int result;
  DBUG_ENTER("NdbMutex_Init");
  
#if defined(VM_TRACE) && \
  defined(HAVE_PTHREAD_MUTEXATTR_INIT) && \
  defined(HAVE_PTHREAD_MUTEXATTR_SETTYPE)

  pthread_mutexattr_t t;
  pthread_mutexattr_init(&t);
  pthread_mutexattr_settype(&t, PTHREAD_MUTEX_ERRORCHECK);
  result = pthread_mutex_init(pNdbMutex, &t);
  assert(result == 0);
  pthread_mutexattr_destroy(&t);
#else
  result = pthread_mutex_init(pNdbMutex, 0);
#endif
  DBUG_RETURN(result);
}

int NdbMutex_Destroy(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

  result = pthread_mutex_destroy(p_mutex);

  NdbMem_Free(p_mutex);

  return result;
}


int NdbMutex_Lock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

  result = pthread_mutex_lock(p_mutex);
  assert(result == 0);

  return result;
}


int NdbMutex_Unlock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

  result = pthread_mutex_unlock(p_mutex);
  assert(result == 0);

  return result;
}


int NdbMutex_Trylock(NdbMutex* p_mutex)
{
  int result = -1;

  if (p_mutex != NULL) {
    result = pthread_mutex_trylock(p_mutex);
    assert(result == 0 || result == EBUSY);
  }

  return result;
}

