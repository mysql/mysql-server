/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
