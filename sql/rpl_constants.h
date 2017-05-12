/* Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_CONSTANTS_H
#define RPL_CONSTANTS_H



/*
  Constants used to parse the stream of bytes sent by a slave
  when commands COM_BINLOG_DUMP or COM_BINLOG_DUMP_GTID are
  sent.
*/
const int BINLOG_POS_INFO_SIZE= 8;
const int BINLOG_DATA_SIZE_INFO_SIZE= 4;
const int BINLOG_POS_OLD_INFO_SIZE= 4;
const int BINLOG_FLAGS_INFO_SIZE= 2;
const int BINLOG_SERVER_ID_INFO_SIZE= 4;
const int BINLOG_NAME_SIZE_INFO_SIZE= 4;

const int BINLOG_DUMP_NON_BLOCK= 1<<0;

/**
   Enumeration of the reserved formats of Binlog extra row information
*/
enum ExtraRowInfoFormat {
  /** Ndb format */
  ERIF_NDB          =   0,

  /** Reserved formats  0 -> 63 inclusive */
  ERIF_LASTRESERVED =  63,

  /**
      Available / uncontrolled formats
      64 -> 254 inclusive
  */
  ERIF_OPEN1        =  64,
  ERIF_OPEN2        =  65,

  ERIF_LASTOPEN     =  254,

  /**
     Multi-payload format 255

      Length is total length, payload is sequence of
      sub-payloads with their own headers containing
      length + format.
  */
  ERIF_MULTI        =  255
};

#endif /* RPL_CONSTANTS_H */
