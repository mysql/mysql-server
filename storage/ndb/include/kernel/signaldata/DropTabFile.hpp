/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
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

#ifndef DROP_TABFILE_HPP
#define DROP_TABFILE_HPP

#include "SignalData.hpp"

class DropTabFileReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
  friend class Dbacc;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 primaryTableId;
  Uint32 secondaryTableId;
};
class DropTabFileConf {
  /**
   * Receiver(s)
   */
  friend class Dbdict;

  /**
   * Sender(s)
   */
  friend class Dbdih;
  friend class Dbacc;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 userPtr;
  Uint32 senderRef;
  Uint32 nodeId;
};

#endif
