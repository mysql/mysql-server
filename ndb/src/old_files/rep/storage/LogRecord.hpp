/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LOG_RECORD_HPP
#define LOG_RECORD_HPP

#include <ndb_global.h>
#include <portlib/NdbMem.h>

/**
 * @class Record
 * @brief
 */
class Record {
public:
  enum RecordType { META = 1, LOG = 2 }; 
  RecordType  recordType;
  Uint32      recordLen;  ///< Size in bytes of entire log record, incl payload
};


/**
 * @class LogRecord
 * @brief
 */
class LogRecord : public Record {
public:
  ~LogRecord() {
    NdbMem_Free(attributeHeader); 
    NdbMem_Free(attributeData);
  }

public:
  Uint32 gci; //0
  Uint32 operation; //4
  Uint32 tableId; //8
  
  Uint32 attributeHeaderWSize; //12
  Uint32 attributeDataWSize; //16
  Uint32 * attributeHeader; //20
  Uint32 * attributeData; //24

  /**
   * Next pointer
   */
};


/**
 * @class MetaRecord
 * @brief
 */
class MetaRecord : public Record {
public:
  ~MetaRecord() {
    NdbMem_Free(data);
  }
  
public:
  Uint32 gci;
  Uint32 tableId;
  Uint32 dataLen; //in words of the data (below)
  Uint32 *data;
};


#endif

