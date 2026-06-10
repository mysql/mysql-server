/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef NDB_TIMESTAMP_H
#define NDB_TIMESTAMP_H

#include <ctime>
#include "ndb_types.h"
#include "util/span.h"

struct NdbTimestampComponents {
  Int16 year;
  Int8 mon;
  Int8 mday;
  Int8 hour;
  Int8 min;
  Int8 sec;
  Int32 gmtoff;
  Int32 nsec;
};

enum class NdbTimestampStringFormat {
  DefaultFormat,
  LegacyFormat,       // YYYY-MM-DD HH:MM:SS
  Iso8601Utc,         // YYYY-MM-DDTHH:MM:SS.ssssssZ
  Iso8601SystemTime,  // YYYY-MM-DDTHH:MM:SS.ssssss±HH:MM
};

// Reset internal state after environment variable TZ has changed
void NdbTimestamp_Reset();

std::timespec NdbTimestamp_GetCurrentTime();
int NdbTimestamp_GetAsString(ndb::span<char> buf,
                             NdbTimestampStringFormat format,
                             const std::timespec *t = nullptr,
                             const NdbTimestampComponents *tm = nullptr);
int NdbTimestamp_SetDefaultStringFormat(NdbTimestampStringFormat format);
int NdbTimestamp_GetDefaultStringFormatLength();
int NdbTimestamp_GetUtcComponents(const std::timespec *t,
                                  NdbTimestampComponents *tm);
int NdbTimestamp_GetLocalComponents(const std::timespec *t,
                                    NdbTimestampComponents *tm);

#endif
