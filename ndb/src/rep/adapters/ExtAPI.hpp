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

#ifndef EXTAPI_HPP
#define EXTAPI_HPP

#include <signaldata/RepImpl.hpp>
#include <signaldata/GrepImpl.hpp>
#include <signaldata/SumaImpl.hpp>

#include <rep/ExtSender.hpp>

/**
 * The abstract class for all extractors
 */
class ExtAPI
{
public:
  /***************************************************************************
   * Constructor / Destructor
   ***************************************************************************/
#if 0
  bool init(const char * connectString = NULL);
  
  GrepError::Code dataLogStarted(Uint32 epoch, 
					 Uint32 subId, Uint32 subKey) = 0;
  GrepError::Code metaLogStarted(Uint32 epoch, 
					 Uint32 subId, Uint32 subKey) = 0;
  GrepError::Code epochComleted() = 0;
  GrepError::Code subscriptionCreated() = 0;
  GrepError::Code subscriptionRemoved() = 0;
  GrepError::Code metaScanCompleted() = 0;
  GrepError::Code dataScanCompleted() = 0;
  GrepError::Code subscriptionRemoveFailed() = 0;
  GrepError::Code metaScanFailed() = 0;
  GrepError::Code dataScanFailed() = 0;
  GrepError::Code subscriptiodIdCreateFailed() = 0;
  GrepError::Code dataLogFailed() = 0;
  GrepError::Code metaLogFailed() = 0;
  GrepError::Code subscriptionCreateFailed() = 0;

  /**Above to be deleted*/
#endif

  virtual GrepError::Code 
  eventSubscriptionIdCreated(Uint32 subId, Uint32 subKey) ;

#if 0
  GrepError::Code 
  eventSubscriptionDeleted(Uint32 subId, Uint32 subKey);

  GrepError::Code 
  eventMetaLogStarted(NdbApiSignal*, Uint32 subId, Uint32 subKey);
  
  GrepError::Code 
  eventDataLogStarted(NdbApiSignal*, Uint32 subId, Uint32 subKey);

  GrepError::Code 
  eventMetaScanCompleted(NdbApiSignal*, Uint32 subId, Uint32 subKey,
			 Interval epochs);

  GrepError::Code 
  eventDataScanCompleted(NdbApiSignal*, Uint32 subId, Uint32 subKey, 
			 Interval epochs);

  GrepError::Code 
  eventMetaScanFailed(Uint32 subId, Uint32 subKey, GrepError::Code error);

  GrepError::Code 
  eventDataScanFailed(Uint32 subId, Uint32 subKey, GrepError::Code error);
#endif

  /***************************************************************************
   * Public Methods
   ***************************************************************************/
  void  setRepSender(ExtSender * es) { m_repSender = es; };
  //void  signalErrorHandler(NdbApiSignal * s, Uint32 nodeId);

protected:
  ExtSender * m_repSender;
};


#if 0
class TestExtAPI : public ExtAPI
{
  GrepError::Code 
  eventSubscriptionIdCreated(Uint32 subId, Uint32 subKey) {
    ndbout_c("Received subscription:%d-%d");
  };
};
#endif

#endif // EXTAPI_HPP
