/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef INTRUSIVE_TAGS_HPP
#define INTRUSIVE_TAGS_HPP

#include "ndb_types.h"

#define JAM_FILE_ID 511

/**
 * Tags to be used for intrusive data types
 */

enum IntrusiveTags {
  IA_List,
  IA_Hash,
  IA_Cursor,
  IA_Stack,
  IA_Queue,
  IA_Sublist,
  IA_Dirty,
  IA_Fragment,
  IA_Page8,
  IA_ApiConnect,
  IA_TcConnect,
  IA_CacheRec,
  IA_Gcp,
  IA_GcpConnect,
  IA_Scan
};

template <IntrusiveTags tag>
struct IntrusiveAccess;

#define INTRUSIVE_ACCESS(tag)        \
  template <>                        \
  struct IntrusiveAccess<IA_##tag> { \
    template <typename T>            \
    static Uint32 &getNext(T &r) {   \
      return r.next##tag;            \
    }                                \
    template <typename T>            \
    static Uint32 &getPrev(T &r) {   \
      return r.prev##tag;            \
    }                                \
    template <typename T>            \
    static Uint32 &getFirst(T &r) {  \
      return r.first##tag;           \
    }                                \
    template <typename T>            \
    static Uint32 &getLast(T &r) {   \
      return r.last##tag;            \
    }                                \
    template <typename T>            \
    static Uint32 &getCount(T &r) {  \
      return r.count##tag;           \
    }                                \
  }

INTRUSIVE_ACCESS(List);
INTRUSIVE_ACCESS(Hash);
INTRUSIVE_ACCESS(Cursor);
INTRUSIVE_ACCESS(Stack);
INTRUSIVE_ACCESS(Queue);
INTRUSIVE_ACCESS(Sublist);
INTRUSIVE_ACCESS(Dirty);
INTRUSIVE_ACCESS(Fragment);
INTRUSIVE_ACCESS(ApiConnect);
INTRUSIVE_ACCESS(TcConnect);
INTRUSIVE_ACCESS(CacheRec);
INTRUSIVE_ACCESS(Gcp);
INTRUSIVE_ACCESS(GcpConnect);
INTRUSIVE_ACCESS(Scan);

template <>
struct IntrusiveAccess<IA_Page8> {
  template <typename T>
  static Uint32 &getNext(T &r) {
    return r.word32[r.NEXT_PAGE];
  }
  template <typename T>
  static Uint32 &getPrev(T &r) {
    return r.word32[r.PREV_PAGE];
  }
  template <typename T>
  static Uint32 &getFirst(T &r);
  template <typename T>
  static Uint32 &getLast(T &r);
  template <typename T>
  static Uint32 &getCount(T &r);
};
#undef JAM_FILE_ID

#endif
