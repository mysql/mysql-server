/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
  static constexpr Uint32 BlobTableNameSize = 40;
  // "Invalid blob attributes or invalid blob parts table"
  static constexpr Uint32 ErrTable = 4263;
  // "Invalid usage of blob attribute"
  static constexpr Uint32 ErrUsage = 4264;
  // "The blob method is not valid in current blob state"
  static constexpr Uint32 ErrState = 4265;
  // "Invalid blob seek position"
  static constexpr Uint32 ErrSeek = 4266;
  // "Corrupted blob value"
  static constexpr Uint32 ErrCorrupt = 4267;
  // "Error in blob head update forced rollback of transaction"
  static constexpr Uint32 ErrAbort = 4268;
  // "Unknown blob error"
  static constexpr Uint32 ErrUnknown = 4270;
  // "Corrupted main table PK in blob operation"
  static constexpr Uint32 ErrCorruptPK = 4274;
  // "The blob method is incompatible with operation type or lock mode"
  static constexpr Uint32 ErrCompat = 4275;
};

#endif
