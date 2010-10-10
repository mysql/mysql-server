/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbMem.h>

NdbMutex* NdbMutex_Create(void)
{
  NdbMutex* pNdbMutex;
  int result;

  pNdbMutex = (NdbMutex*)NdbMem_Allocate(sizeof(NdbMutex));

  if (pNdbMutex == NULL)
    return NULL;

  result = pthread_mutex_init(pNdbMutex, NULL);
  assert(result == 0);

  return pNdbMutex;
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
  
  return result;
}


int NdbMutex_Unlock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

  result = pthread_mutex_unlock(p_mutex);
			     
  return result;
}


int NdbMutex_Trylock(NdbMutex* p_mutex)
{
  int result = -1;

  if (p_mutex != NULL) {
    result = pthread_mutex_trylock(p_mutex);
  }

  return result;
}

