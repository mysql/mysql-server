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

#include "Requestor.hpp"

#include <signaldata/GrepImpl.hpp>
#include <signaldata/SumaImpl.hpp>

#include <rep/rep_version.hpp>

/*****************************************************************************
 * Create Subscription Id
 *****************************************************************************/


/*****************************************************************************
 * Create Subscription
 *****************************************************************************/


/*****************************************************************************
 * Start Subscription
 *****************************************************************************/

/*****************************************************************************
 * Remove Subscription
 *****************************************************************************/

void
Requestor::execGREP_SUB_REMOVE_REF(NdbApiSignal* signal) 
{
#if 0
  GrepSubRemoveRef * const ref = (GrepSubRemoveRef *)signal->getDataPtr();
  Uint32 subId           = ref->subscriptionId;
  Uint32 subKey          = ref->subscriptionKey;
  Uint32 err             = ref->err;

  signal->theData[0] = EventReport::GrepSubscriptionAlert;
  signal->theData[1] = GrepEvent::GrepSS_SubRemoveRef;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  signal->theData[4] = (Uint32)err;
  sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 5, JBB);
#endif
}


