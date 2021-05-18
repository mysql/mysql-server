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

#ifndef DROP_TABFILE_HPP
#define DROP_TABFILE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 14


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


#undef JAM_FILE_ID

#endif
