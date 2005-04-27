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

double
userGetTime(void)
{
  static bool initialized = false;
  static NDB_TICKS initSecs = 0;
  static Uint32 initMicros = 0;
  double timeValue = 0;

  if ( !initialized ) {
    initialized = true;
    NdbTick_CurrentMicrosecond(&initSecs, &initMicros); 
    timeValue = 0.0;
  } else {
    NDB_TICKS secs = 0;
    Uint32 micros = 0;

    NdbTick_CurrentMicrosecond(&secs, &micros);
    double s  = (double)secs  - (double)initSecs;
    double us = (double)micros - (double)initMicros;
    
    timeValue = s + (us / 1000000.0);
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

