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

#include "ExtAPI.hpp"

GrepError::Code
ExtAPI::eventSubscriptionIdCreated(Uint32 subId, Uint32 subKey)
{
  NdbApiSignal* signal = m_repSender->getSignal();
  CreateSubscriptionIdConf * conf = 
    (CreateSubscriptionIdConf *)signal->getDataPtrSend();
  conf->subscriptionId = subId;
  conf->subscriptionKey = subKey;
  signal->set(0, SSREPBLOCKNO, GSN_GREP_CREATE_SUBID_CONF, 
	      CreateSubscriptionIdConf::SignalLength);  
  m_repSender->sendSignal(signal);
  return GrepError::NO_ERROR;
}
