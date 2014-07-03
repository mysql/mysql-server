/*
   Copyright (C) 2003-2006 MySQL AB
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

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include <ndb_global.h>
#include <time.h>

#include "ndb_schema.hpp"
#include "ndb_error.hpp"
#include "userInterface.h"
#include <NdbMutex.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NdbApi.hpp>
#include <NdbOut.hpp>

/***************************************************************
* L O C A L   C O N S T A N T S                                *
***************************************************************/

/***************************************************************
* L O C A L   D A T A   S T R U C T U R E S                    *
***************************************************************/

/***************************************************************
* L O C A L   F U N C T I O N S                                *
***************************************************************/

#ifndef NDB_WIN32
#include <unistd.h>
#endif


static NdbMutex* startupMutex = NdbMutex_Create();

Ndb*
asyncDbConnect(int parallellism){
  NdbMutex_Lock(startupMutex);
  Ndb * pNDB = new Ndb("");
  
  pNDB->init(parallellism + 1);
  
  while(pNDB->waitUntilReady() != 0){
  }
  
  NdbMutex_Unlock(startupMutex);

  return pNDB;
}

void 
asyncDbDisconnect(Ndb* pNDB)
{
  delete pNDB;
}

static NDB_TICKS initTicks;

double
userGetTime(void)
{
  double timeValue = 0;

  if ( !NdbTick_IsValid(initTicks)) {
    initTicks = NdbTick_getCurrentTicks();
    timeValue = 0.0;
  } else {
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 elapsedMicro =
      NdbTick_Elapsed(initTicks,now).microSec();

    timeValue = ((double)elapsedMicro) / 1000000.0;
  }
  return timeValue;
}

void showTime()
{
  char buf[128];
  struct tm* tm_now;
  time_t now;
  now = ::time((time_t*)NULL);
  tm_now = ::gmtime(&now);

  BaseString::snprintf(buf, 128,
	     "%d-%.2d-%.2d %.2d:%.2d:%.2d", 
	     tm_now->tm_year + 1900, 
	     tm_now->tm_mon, 
	     tm_now->tm_mday,
	     tm_now->tm_hour,
	     tm_now->tm_min,
	     tm_now->tm_sec);

  ndbout_c("Time: %s", buf);
}

