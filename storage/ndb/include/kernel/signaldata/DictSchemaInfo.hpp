/*
   Copyright (C) 2003-2006 MySQL AB
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

#ifndef DICT_SCHEMA_INFO_HPP
#define DICT_SCHEMA_INFO_HPP

#include "SignalData.hpp"

class DictSchemaInfo {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  
public:
  STATIC_CONST( HeaderLength = 3 );
  STATIC_CONST( DataLength = 22 );
  
private:  
  Uint32 senderRef;
  Uint32 offset; 
  Uint32 totalLen; 
  
  /**
   * Length in this = signal->length() - 3
   * Sender block ref = signal->senderBlockRef()
   */
  
  Uint32 schemaInfoData[22];
};

#endif
