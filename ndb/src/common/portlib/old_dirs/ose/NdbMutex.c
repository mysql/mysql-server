/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "NdbMutex.h"

#include <pthread.h>
#include <stdlib.h>


NdbMutex* NdbMutex_Create(void)
{
  NdbMutex* pNdbMutex;

  pNdbMutex = create_sem(1);
  
  return pNdbMutex;
}


int NdbMutex_Destroy(NdbMutex* p_mutex)
{

  if (p_mutex == NULL)
    return -1;
  
  kill_sem(p_mutex);
			     
  return 0;

}


int NdbMutex_Lock(NdbMutex* p_mutex)
{
  if (p_mutex == NULL)
    return -1;

  wait_sem(p_mutex);

  return 0;
}


int NdbMutex_Unlock(NdbMutex* p_mutex)
{

  if (p_mutex == NULL)
    return -1;

  signal_sem(p_mutex);
			     
  return 0;
}


int NdbMutex_Trylock(NdbMutex* p_mutex)
{
  int result = -1;

  if (p_mutex != NULL) {
    OSSEMVAL semvalue = get_sem(p_mutex);
    if (semvalue > 0) {
      wait_sem(p_mutex);
      result = 0;
    }
  }

  return result;

}

