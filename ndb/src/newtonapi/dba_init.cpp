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


#include "dba_internal.hpp"
#include "dba_process.hpp"
#include <NdbOut.hpp>


#ifdef NDB_WIN32
static NdbMutex & DBA__InitMutex = * NdbMutex_Create();
#else
static NdbMutex DBA__InitMutex = NDB_MUTEX_INITIALIZER;
#endif

Ndb      * DBA__TheNdb           = 0;
NdbMutex * DBA__TheNewtonMutex   = 0;
unsigned DBA__SentTransactions   = 0;
unsigned DBA__RecvTransactions   = 0;
NewtonBatchProcess * DBA__TheNBP = 0;

extern "C"
DBA_Error_t
DBA_Open( ) {
  NdbMutex_Lock(&DBA__InitMutex);
  
  if(DBA__TheNdb != 0){
    NdbMutex_Unlock(&DBA__InitMutex);
    return DBA_NO_ERROR;
  }
  
  DBA__TheNdb = new Ndb("Newton");
  DBA__TheNdb->init(1024);
  if(DBA__TheNdb->waitUntilReady() != 0){
    delete DBA__TheNdb; DBA__TheNdb = 0;
    NdbMutex_Unlock(&DBA__InitMutex);
    return DBA_NDB_ERROR;
  }
  DBA__TheNewtonMutex = NdbMutex_Create();
  DBA__TheNBP = new NewtonBatchProcess(* DBA__TheNdb, * DBA__TheNewtonMutex);
  DBA__TheNBP->doStart();
  NdbMutex_Unlock(&DBA__InitMutex);
  return DBA_NO_ERROR;
}


/**
 * Closes the database.
 * 
 * @return Error status
 */
extern "C"
DBA_Error_t
DBA_Close(void){

  NdbMutex_Lock(&DBA__InitMutex);

  if(DBA__TheNBP != 0)
    DBA__TheNBP->doStop(true);
  delete DBA__TheNBP; 
  DBA__TheNBP = 0;
  
  if(DBA__TheNdb != 0)
    delete DBA__TheNdb;
  DBA__TheNdb = 0;
  
  if(DBA__TheNewtonMutex != 0)
    NdbMutex_Destroy(DBA__TheNewtonMutex);
  DBA__TheNewtonMutex = 0;

  NdbMutex_Unlock(&DBA__InitMutex);
  return DBA_NO_ERROR;
}
