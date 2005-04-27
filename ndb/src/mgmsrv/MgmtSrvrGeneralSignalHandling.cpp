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

//******************************************************************************
// General signal handling methods
// All implementations stolen from the Ndb class.
// Some kind of reuse should be preferred.
//******************************************************************************

#include "MgmtSrvr.hpp"
#include <NdbApiSignal.hpp>
#include <NdbTick.h>


NdbApiSignal*
MgmtSrvr::getSignal()
{
  NdbApiSignal* tSignal;
  tSignal = theSignalIdleList;
  if (tSignal != NULL){
    NdbApiSignal* tSignalNext = tSignal->next();
    tSignal->next(NULL);
    theSignalIdleList = tSignalNext;
    return tSignal;
  } else
  {
    tSignal = new NdbApiSignal(_ownReference);
    if (tSignal != NULL)
      tSignal->next(NULL);
  }
  return tSignal;
}


void
MgmtSrvr::releaseSignal(NdbApiSignal* aSignal)
{
  aSignal->next(theSignalIdleList);
  theSignalIdleList = aSignal;
}


void
MgmtSrvr::freeSignal()
{
  NdbApiSignal* tSignal = theSignalIdleList;
  theSignalIdleList = tSignal->next();
  delete tSignal;
}


int
MgmtSrvr::sendSignal(Uint16 aNodeId,
		     WaitSignalType aWaitState,
		     NdbApiSignal* aSignal,
		     bool force)
{
  int tReturnCode;
  theFacade->lock_mutex();
  if(force){
    tReturnCode = theFacade->sendSignalUnCond(aSignal, 
								  aNodeId);
  } else {
    tReturnCode = theFacade->sendSignal(aSignal, 
							    aNodeId);
  }
  releaseSignal(aSignal);
  if (tReturnCode == -1) {
    theFacade->unlock_mutex();
    return -1;
  }
  theWaitState = aWaitState;
  theFacade->unlock_mutex();
  return 0;
}


int
MgmtSrvr::sendRecSignal(Uint16 aNodeId,
			WaitSignalType aWaitState,
			NdbApiSignal* aSignal,
			bool force,
			int waitTime)
{
  int tReturnCode;
  theFacade->lock_mutex();
  if(force){
    tReturnCode = theFacade->sendSignalUnCond(aSignal, aNodeId);
  } else {
    tReturnCode = theFacade->sendSignalUnCond(aSignal, aNodeId);
  }
  releaseSignal(aSignal);
  if (tReturnCode == -1) {
    theFacade->unlock_mutex();
    return -1;
  }
  theWaitState = aWaitState;
  theWaitNode = aNodeId;
  return receiveOptimisedResponse(waitTime);
}


int	
MgmtSrvr::receiveOptimisedResponse(int waitTime)
{
  int tResultCode;
  theFacade->checkForceSend(_blockNumber);
  NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + waitTime;
  
  while (theWaitState != NO_WAIT && theWaitState != WAIT_NODEFAILURE
	 && waitTime > 0) {
    NdbCondition_WaitTimeout(theMgmtWaitForResponseCondPtr, 
			     theFacade->theMutexPtr,
			     waitTime);
    if(theWaitState == NO_WAIT || theWaitState == WAIT_NODEFAILURE)
      break;
    waitTime = (maxTime - NdbTick_CurrentMillisecond());
  }//while
  
  if(theWaitState == NO_WAIT) {
    tResultCode = 0;
  } else {
    tResultCode = -1;
  }
  theFacade->unlock_mutex();
  return tResultCode;
}


