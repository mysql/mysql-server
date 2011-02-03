/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
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

#ifndef LCP_SIGNAL_DATA_HPP
#define LCP_SIGNAL_DATA_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

struct StartLcpReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printSTART_LCP_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:

  STATIC_CONST( SignalLength = 2 + 2 * NdbNodeBitmask::Size );
  Uint32 senderRef;
  Uint32 lcpId;
  
  NdbNodeBitmask participatingDIH;
  NdbNodeBitmask participatingLQH;
};

class StartLcpConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printSTART_LCP_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 senderRef;
  Uint32 lcpId;
};

/**
 * This signals is sent by Dbdih to Dblqh
 * to order checkpointing of a certain
 * fragment.
 */
class LcpFragOrd {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  friend class Lgman;
  friend class Pgman;
  friend class Dbtup;

  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  friend class PgmanProxy;

  friend bool printLCP_FRAG_ORD(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 6 );
private:
  
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpNo;
  Uint32 lcpId;
  Uint32 lastFragmentFlag;
  Uint32 keepGci;
};


struct LcpFragRep {
  /**
   * Sender(s) and receiver(s)
   */
  friend class Dbdih;

  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printLCP_FRAG_REP(FILE *, const Uint32 *, Uint32, Uint16);  

  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( BROADCAST_REQ = 0 );

  Uint32 nodeId;
  Uint32 lcpId;
  Uint32 lcpNo;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 maxGciCompleted;
  Uint32 maxGciStarted;
};

class LcpCompleteRep {
  /**
   * Sender(s) and receiver(s)
   */
  friend class Dbdih;
  
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printLCP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );
  
private:
  Uint32 nodeId;
  Uint32 blockNo;
  Uint32 lcpId;
};

struct LcpPrepareReq 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lcpNo;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpId;
  Uint32 backupPtr;
  Uint32 backupId;

  STATIC_CONST( SignalLength = 8 );
};

struct LcpPrepareRef
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 errorCode;
  
  STATIC_CONST( SignalLength = 5 );
};

struct LcpPrepareConf 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;
  
  STATIC_CONST( SignalLength = 4 );
};

struct EndLcpReq 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 backupPtr;
  Uint32 backupId;
  // extra word for LQH worker to proxy
  Uint32 proxyBlockNo;

  STATIC_CONST( SignalLength = 4 );
};

struct EndLcpRef 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  
  STATIC_CONST( SignalLength = 3 );
};

struct EndLcpConf
{
  Uint32 senderData;
  Uint32 senderRef;
  
  STATIC_CONST( SignalLength = 2 );
};

#endif
