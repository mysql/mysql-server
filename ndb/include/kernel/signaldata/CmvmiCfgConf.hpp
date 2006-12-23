/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CMVMI_CFGCONF_H
#define CMVMI_CFGCONF_H

#include "SignalData.hpp"

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

#endif
