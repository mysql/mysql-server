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

#ifndef EXT_SENDER_HPP
#define EXT_SENDER_HPP

#include <NdbSleep.h>
#include <TransporterFacade.hpp>
#include <NdbApiSignal.hpp>
#include <rep/rep_version.hpp>

/**
 * @todo Johan comment:
 *
 * ext->sendSignal should return something if send failed.
 * I.e., i think all methods sending a signal should return int
 * so that we can take care of errors. ALternatively take care of 
 * the error like this:
 * if(ext->sendSignal(..) < 0 )
 *   handleSignalError(...)
 * 
 * or a combination.... 
 *
 * Should go through all places that sends signals and check that 
 * they do correct error handling.
 */

/**
 * @class  ExtSender
 * @brief  Manages connection to a transporter facade
 */
class ExtSender {
public:
  /***************************************************************************
   * Constructor / Destructor / Init / Get / Set  (Only set once!)
   ***************************************************************************/
  ExtSender();
  ~ExtSender();
  
  void    setTransporterFacade(TransporterFacade * tf) { m_tf = tf; }
  void    setNodeId(Uint32 nodeId);
  Uint32  getOwnRef() const;
  void    setOwnRef(Uint32 ref);

  /***************************************************************************
   * Usage
   ***************************************************************************/
  int sendSignal(NdbApiSignal * s);
  int sendFragmentedSignal(NdbApiSignal * s, LinearSectionPtr ptr[3],
			    Uint32 sections);

  bool connected(Uint32 TimeOutInMilliSeconds);
  bool connected(Uint32 TimeOutInMilliSeconds, Uint32 nodeId);

  NdbApiSignal * getSignal();

private:
  TransporterFacade *  m_tf;
  Uint32               m_nodeId;
  Uint32               m_ownRef;
};

#endif
