/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/


#ifndef DB2I_ERRORS_H
#define DB2I_ERRORS_H

#include "qmyse.h"
class THD;

/**
  @enum DB2I_errors
  
  @brief These are the errors that can be returned by the storage engine proper 
  and that are specific to the engine. Refer to db2i_errors.cc for text
  descriptions of the errors.
*/

enum DB2I_errors
{
  DB2I_FIRST_ERR = 2500,
  DB2I_ERR_ICONV_OPEN,
  DB2I_ERR_INVALID_NAME,
  DB2I_ERR_RENAME_MOVE,
  DB2I_ERR_UNSUPP_CHARSET,
  DB2I_ERR_PART_AUTOINC,
  DB2I_ERR_UNKNOWN_ENCODING,
  DB2I_ERR_RESERVED,
  DB2I_ERR_TABLE_NOT_FOUND,
  DB2I_ERR_RESOLVE_OBJ,
  DB2I_ERR_PGMCALL,
  DB2I_ERR_ILECALL,
  DB2I_ERR_ICONV,
  DB2I_ERR_QTQGESP,
  DB2I_ERR_QTQGRDC,
  DB2I_ERR_INVALID_COL_VALUE,
  DB2I_ERR_TOO_LONG_SCHEMA,
  DB2I_ERR_MIXED_COLLATIONS,
  DB2I_ERR_SRTSEQ,
  DB2I_ERR_SUB_CHARS,
  DB2I_ERR_PRECISION,
  DB2I_ERR_INVALID_DATA,
  DB2I_ERR_RESERVED2,
  DB2I_ERR_ILL_CHAR,
  DB2I_ERR_BAD_RDB_NAME,
  DB2I_ERR_UNKNOWN_IDX,
  DB2I_ERR_DISCOVERY_MISMATCH,
  DB2I_ERR_WARN_CREATE_DISCOVER,
  DB2I_ERR_WARN_COL_ATTRS,
  DB2I_LAST_ERR = DB2I_ERR_WARN_COL_ATTRS
};

void getErrTxt(int errcode, ...);
void reportSystemAPIError(int errCode, const Qmy_Error_output *errInfo);
void warning(THD *thd, int errCode, ...);

const char* DB2I_SQL0350 = "\xE2\xD8\xD3\xF0\xF3\xF5\xF0"; // SQL0350 in EBCDIC
const char* DB2I_CPF503A = "\xC3\xD7\xC6\xF5\xF0\xF3\xC1"; // CPF503A in EBCDIC
const char* DB2I_SQL0538 = "\xE2\xD8\xD3\xF0\xF5\xF3\xF8"; // SQL0538 in EBCDIC

#endif
