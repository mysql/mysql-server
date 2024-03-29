/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef FIDO_COMMON_H_
#define FIDO_COMMON_H_

#include <cstring>
#include <iostream>
#include <sstream>

#include "my_byteorder.h"
#include "my_inttypes.h"

typedef void (*plugin_messages_callback)(const char *msg);
extern plugin_messages_callback mc;

/* Define type of message */
enum class message_type {
  INFO,  /* directed to stdout */
  ERROR, /* directed to stderr */
};

/*
  Helper method to redirect plugin specific messages to a registered callback
  method or to stdout/stderr.
*/
void get_plugin_messages(const std::string &msg, message_type type);

inline uchar *net_store_length(uchar *packet, ulonglong length) {
  if (length < (ulonglong)251LL) {
    *packet = (uchar)length;
    return packet + 1;
  }
  /* 251 is reserved for NULL */
  if (length < (ulonglong)65536LL) {
    *packet++ = 252;
    int2store(packet, (uint)length);
    return packet + 2;
  }
  if (length < (ulonglong)16777216LL) {
    *packet++ = 253;
    int3store(packet, (ulong)length);
    return packet + 3;
  }
  *packet++ = 254;
  int8store(packet, length);
  return packet + 8;
}

inline uint net_length_size(ulonglong num) {
  if (num < 251ULL) return 1;
  if (num < 65536LL) return 3;
  if (num < 16777216ULL) return 4;
  return 9;
}

/* The same as above but returns longlong */
inline uint64_t net_field_length_ll(uchar **packet) {
  const uchar *pos = *packet;
  if (*pos < 251) {
    (*packet)++;
    return (uint64_t)*pos;
  }
  if (*pos == 251) {
    (*packet)++;
    return (uint64_t)((unsigned long)~0);
  }
  if (*pos == 252) {
    (*packet) += 3;
    return (uint64_t)uint2korr(pos + 1);
  }
  if (*pos == 253) {
    (*packet) += 4;
    return (uint64_t)uint3korr(pos + 1);
  }
  (*packet) += 9; /* Must be 254 when here */
  return (uint64_t)uint8korr(pos + 1);
}

#endif  // FIDO_COMMON_H_