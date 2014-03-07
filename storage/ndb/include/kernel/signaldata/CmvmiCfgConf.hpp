/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CMVMI_CFGCONF_H
#define CMVMI_CFGCONF_H

#include "SignalData.hpp"

#define JAM_FILE_ID 88


/**
 * This signal is used for transfering the 
 *   ISP_X Data
 *
 * I.e. Configuration data which is sent in a specific start phase
 *
 */
class CmvmiCfgConf  {
  /**
   * Sender(s)
   */
  friend class Cmvmi;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  
public:
  STATIC_CONST( NO_OF_WORDS = 16 );
  STATIC_CONST( LENGTH      = 17 );
private:
  
  Uint32 startPhase;
  Uint32 theData[NO_OF_WORDS];
};


#undef JAM_FILE_ID

#endif
