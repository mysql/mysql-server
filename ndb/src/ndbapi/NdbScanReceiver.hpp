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

#ifndef NdbScanReceiver_H
#define NdbScanReceiver_H

#include "Ndb.hpp"
#include "NdbConnection.hpp"
#include "NdbOperation.hpp"
#include "NdbApiSignal.hpp"
#include "NdbReceiver.hpp"
#include <NdbOut.hpp>


class NdbScanReceiver
{
  enum ReceiverStatus	{ Init,
			  Waiting,
			  Completed,
			  Executed,
			  Released };

  friend class Ndb;
  friend class NdbOperation;
public:
  NdbScanReceiver(Ndb *aNdb) : 
    theReceiver(aNdb),
    theNdbOp(NULL),
    theFirstTRANSID_AI_Recv(NULL), 
    theLastTRANSID_AI_Recv(NULL),
    theFirstKEYINFO20_Recv(NULL),
    theLastKEYINFO20_Recv(NULL),
    theTotalRecAI_Len(0),
    theTotalKI_Len(0xFFFFFFFF),
    theTotalRecKI_Len(0),
    theStatus(Init),
    theNextScanRec(NULL)  
  {
    theReceiver.init(NdbReceiver::NDB_SCANRECEIVER, this);
  }

  int	 checkMagicNumber();
  int	 receiveTRANSID_AI_SCAN(NdbApiSignal*);
  int	 receiveKEYINFO20(NdbApiSignal*);
  int    executeSavedSignals();
  void   prepareNextScanResult();

  NdbScanReceiver* next();
  void  next(NdbScanReceiver*);

  bool isCompleted(Uint32 aiLenToReceive);
  void setCompleted();

  void init(NdbOperation* aNdbOp, bool lockMode);

  Uint32 ptr2int() { return theReceiver.getId(); };
private:
  NdbScanReceiver();
  void release();

  NdbReceiver  theReceiver;

  NdbOperation*       theNdbOp;
  NdbApiSignal*       theFirstTRANSID_AI_Recv; 
  NdbApiSignal*       theLastTRANSID_AI_Recv;
  NdbApiSignal*       theFirstKEYINFO20_Recv; 
  NdbApiSignal*       theLastKEYINFO20_Recv;

  Uint32              theTotalRecAI_Len;
  Uint32              theTotalKI_Len;
  Uint32              theTotalRecKI_Len;
  ReceiverStatus      theStatus;
  Uint32              theMagicNumber; 
  NdbScanReceiver*    theNextScanRec;
  bool                theLockMode;

};

inline 
void 
NdbScanReceiver::init(NdbOperation* aNdbOp, bool lockMode){
    assert(theStatus == Init || theStatus == Released); 
    theNdbOp = aNdbOp;
    theMagicNumber = 0xA0B1C2D3;
    theTotalRecAI_Len = 0;

    /* If we are locking the records for take over 
     * KI_len to receive is at least 1, since we don't know yet 
     * how much KI we are expecting(this is written in the first KI signal)
     * set theTotalKI_Len to FFFFFFFF, this will make the ScanReciever wait for 
     * at least the first KI, and when that is received we will know if 
     * we are expecting another one
     */
    theLockMode = lockMode;
    if (theLockMode == true)
      theTotalKI_Len = 0xFFFFFFFF;
    else
      theTotalKI_Len = 0;
    theTotalRecKI_Len = 0;

    assert(theNextScanRec == NULL);
    theNextScanRec = NULL;
    assert(theFirstTRANSID_AI_Recv == NULL);
    theFirstTRANSID_AI_Recv = NULL;
    assert(theLastTRANSID_AI_Recv == NULL);
    theLastTRANSID_AI_Recv = NULL;
    assert(theFirstKEYINFO20_Recv == NULL);
    theFirstKEYINFO20_Recv = NULL;
    theLastKEYINFO20_Recv = NULL;
    
    theStatus = Waiting;
};


inline 
void 
NdbScanReceiver::release(){
  theStatus = Released;
  //  theNdbOp->theNdb->releaseSignalsInList(&theFirstTRANSID_AI_Recv);
  while(theFirstTRANSID_AI_Recv != NULL){
    NdbApiSignal* tmp = theFirstTRANSID_AI_Recv;
    theFirstTRANSID_AI_Recv = tmp->next();
    delete tmp;
  }
  theFirstTRANSID_AI_Recv = NULL;
  theLastTRANSID_AI_Recv = NULL;
  //  theNdbOp->theNdb->releaseSignalsInList(&theFirstKEYINFO20_Recv);
  while(theFirstKEYINFO20_Recv != NULL){
    NdbApiSignal* tmp = theFirstKEYINFO20_Recv;
    theFirstKEYINFO20_Recv = tmp->next();
    delete tmp;
  }
  theFirstKEYINFO20_Recv = NULL;
  theLastKEYINFO20_Recv = NULL;
  theNdbOp = NULL;
  theTotalRecAI_Len = 0;
  theTotalRecKI_Len = 0;
  theTotalKI_Len = 0xFFFFFFFF;
};

inline
int
NdbScanReceiver::checkMagicNumber()
{
  if (theMagicNumber != 0xA0B1C2D3)
    return -1;
  return 0;
}

inline 
NdbScanReceiver*
NdbScanReceiver::next(){
  return theNextScanRec;
}

inline 
void
NdbScanReceiver::next(NdbScanReceiver* aScanRec){
  theNextScanRec = aScanRec;
}

inline 
bool
NdbScanReceiver::isCompleted(Uint32 aiLenToReceive){
  assert(theStatus == Waiting || theStatus == Completed);
#if 0
  ndbout << "NdbScanReceiver::isCompleted"<<endl
	 << "  theStatus = " << theStatus << endl
	 << "  theTotalRecAI_Len = " << theTotalRecAI_Len << endl
	 << "  aiLenToReceive = " << aiLenToReceive << endl
	 << "  theTotalRecKI_Len = "<< theTotalRecKI_Len << endl
	 << "  theTotalKI_Len = "<< theTotalKI_Len << endl;
#endif
  // Have we already receive everything
  if(theStatus == Completed)
    return true;

  // Check that we have received AI
  if(theTotalRecAI_Len < aiLenToReceive)
    return false;

  // Check that we have recieved KI
  if (theTotalRecKI_Len < theTotalKI_Len)
    return false;

  // We should not have recieved more AI
  assert(theTotalRecAI_Len <= aiLenToReceive);
  return true;  
}

inline 
void
NdbScanReceiver::setCompleted(){
  theStatus = Completed;
}

#endif
