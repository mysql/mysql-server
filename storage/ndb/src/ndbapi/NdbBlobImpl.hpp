/*
   Copyright (C) 2004-2006 MySQL AB
    Use is subject to license terms.

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

#ifndef NdbBlobImpl_H
#define NdbBlobImpl_H

class NdbBlobImpl {
public:
  STATIC_CONST( BlobTableNameSize = 40 );
  // "Invalid blob attributes or invalid blob parts table"
  STATIC_CONST( ErrTable = 4263 );
  // "Invalid usage of blob attribute" 
  STATIC_CONST( ErrUsage = 4264 );
  // "The blob method is not valid in current blob state"
  STATIC_CONST( ErrState = 4265 );
  // "Invalid blob seek position"
  STATIC_CONST( ErrSeek = 4266 );
  // "Corrupted blob value"
  STATIC_CONST( ErrCorrupt = 4267 );
  // "Error in blob head update forced rollback of transaction"
  STATIC_CONST( ErrAbort = 4268 );
  // "Unknown blob error"
  STATIC_CONST( ErrUnknown = 4270 );
  // "Corrupted main table PK in blob operation"
  STATIC_CONST( ErrCorruptPK = 4274 );
  // "The blob method is incompatible with operation type or lock mode"
  STATIC_CONST( ErrCompat = 4275 );
};

#endif
