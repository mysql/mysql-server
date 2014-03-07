/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_EXTENT_HPP
#define NDB_EXTENT_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 86


struct AllocExtentReq {
  /**
   * Sender(s) / Reciver(s)
   */

  /**
   * For printing
   */
  
  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode {
    UnmappedExtentPageIsNotImplemented = 1,
    NoExtentAvailable = 1601,
    NoDatafile = 1602
  };
  
  union 
  {
    struct 
    {
      Uint32 tablespace_id;
      Uint32 table_id;
      Uint32 fragment_id;
    } request;
    struct 
    {
      Uint32 errorCode;
      Local_key page_id;
      Uint32 page_count;
    } reply;
  };
};

struct FreeExtentReq {
  /**
   * Sender(s) / Reciver(s)
   */

  /**
   * For printing
   */
  
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    UnmappedExtentPageIsNotImplemented = 1
  };
  
  union 
  {
    struct 
    {
      Local_key key;
      Uint32 table_id;
      Uint32 tablespace_id;
      Uint32 lsn_hi;
      Uint32 lsn_lo;
    } request;
    struct 
    {
      Uint32 errorCode;
    } reply;
  };
};

struct AllocPageReq {
  /**
   * Sender(s) / Reciver(s)
   */

  /**
   * For printing
   */
  
  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode {
    UnmappedExtentPageIsNotImplemented = 1,
    NoPageFree= 2
  };
  
  Local_key key; // in out
  Uint32 bits;   // in out
  union 
  {
    struct 
    {
      Uint32 table_id;
      Uint32 fragment_id;
      Uint32 tablespace_id;
    } request;
    struct 
    {
      Uint32 errorCode;
    } reply;
  };
};



#undef JAM_FILE_ID

#endif
