/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

%include "globals.i"
%include "config.h"

%{

#include <my_global.h>
#include <my_sys.h>

#include <mysql.h>
#include <NdbApi.hpp>

/* These get included in mysql.h. Not sure they should... */
#undef PACKAGE
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

#include "config.h"


  long long getMicroTime()
  {
    struct timeval tTime;
    gettimeofday(&tTime, 0);
    long long microSeconds = (long long) tTime.tv_sec * 1000000 + tTime.tv_usec;
    return microSeconds;
  }


/* voidint is a way to mark a method that returns int for SWIG
   to tell it we want the wrapped function to return void, but
   we don't want to discard the return value out of hand because
   we need it at the wrapping layer to find errors */
  typedef int voidint;
  typedef Uint32 NdbTimestamp;


  typedef struct st_byte {
    char * theString;
    int theLength;
  } BYTES;

#ifdef _mysql_h
#ifndef _decimal_h

#define DECIMAL_BUFF 9

  typedef int32 decimal_digit_t;

  typedef struct st_decimal_t {
    int intg, frac, len;
    my_bool sign;
    decimal_digit_t *buf;
  } decimal_t;

#define string2decimal(A,B,C) internal_str2dec((A), (B), (C), 0)
#define decimal_string_size(dec) (((dec)->intg ? (dec)->intg : 1) +     \
                                  (dec)->frac + ((dec)->frac > 0) + 2)
  extern "C" {
    int decimal_size(int precision, int scale);
    int decimal_bin_size(int precision, int scale);
    int decimal2bin(decimal_t *from, char *to, int precision, int scale);
    int bin2decimal(char *from, decimal_t *to, int precision, int scale);
    int decimal2string(decimal_t *from, char *to, int *to_len,
                       int fixed_precision, int fixed_decimals,
                       char filler);
    int internal_str2dec(const char *from, decimal_t *to, char **end,
                         my_bool fixed);
  }
#endif
#endif

  char * decimal2bytes(decimal_t * val) {

    int theScale = val->frac;
    int thePrecision = (val->intg)+theScale;

    char * theValue = (char *) malloc(decimal_bin_size(thePrecision,
                                                       theScale));
    if (theValue == NULL) {
      return NULL;
    }
    decimal2bin(val, theValue, thePrecision, theScale);
    return theValue;
  }

  class NdbDateTime
  {
  public:
    unsigned int  year, month, day, hour, minute, second;
    NdbDateTime();
  };

  NdbDateTime::NdbDateTime() {
    year=0;
    month=0;
    day=0;
    hour=0;
    minute=0;
    second=0;
  }

/* We don't just typedef these right in the first place
   because there because that would mean NdbDictionary::Dictionary
   would get renamed to NdbDictionary at the C level, which
   wouldn't work out so well. */

  typedef NdbDictionary::Object NdbDictObject;
  typedef NdbDictionary::Table NdbDictTable;
  typedef NdbDictionary::Column NdbDictColumn;
  typedef NdbDictionary::Index NdbDictIndex;
  typedef NdbDictionary::Dictionary NdbDictDictionary;
  typedef NdbDictionary::Event NdbDictEvent;


  int getColumnId(NdbOperation * op, const char* columnName) {
    const NdbDictColumn * theColumn = op->getTable()->getColumn(columnName);
    return theColumn->getColumnNo();
  }

  char * ndbFormatString(const NdbDictColumn * theColumn,
                         const char* aString, size_t len) {

    // This method should be safe and do all the error checking.
    // There should be other methods in case you want to bypass checks.
    if (aString==0) {
      return 0;
    }
    if ((!theColumn) || (len>65535)) {
      return NULL;
    }

    switch(theColumn->getType()) {
    case NDB_TYPE_VARCHAR:
    case NDB_TYPE_VARBINARY:
      //    printf("it's a varchar\n");
      if (len>255) {
        return NULL;
      } else {
        // Need one space for the length
        char * buf = (char *)malloc((len+1));
        unsigned char lowb=len;
        buf[0]=lowb;
        memcpy(buf+1, aString, len);
        return buf;
      }
      break;
    case NDB_TYPE_LONGVARCHAR:
    case NDB_TYPE_LONGVARBINARY:
    {
      char* buff=(char *)malloc(len+2);

      short l = (short)len;
      /*
        We need to copy the length of our string into the first 2 bytes
        of the buffer.
        We take a bitwise AND of the 1st byte in the short 'l' and copy
        it int the 1st byte of our buffer.
      */

      buff[0]=(unsigned char) ((l & 0x00FF))>>0;
      /*
        We take a bitwise AND of the 2nd byte in the short 'l'
        and copy it into the 2nd byte of our buffer.
      */

      buff[1]= (unsigned char)((l & 0xFF00)>>8);
      memcpy(&buff[2],aString, l);
      return buff;
    }
    break;
    case NDB_TYPE_CHAR:
      if (len>255) {
        return NULL;
      } else {
        int colLength = theColumn->getLength();
        char * buf = (char *)malloc(colLength+1);
        memset(buf,32, colLength);
        memcpy(buf, aString, len);
        return buf;
      }
      break;
    case NDB_TYPE_BINARY:
      if (len>255) {
        return NULL;
      } else {
        int colLength = theColumn->getLength();
        char * buf = (char *)malloc(colLength+1);
        memset(buf, 0, colLength);
        memcpy(buf, aString, len);
        return buf;
      }
      break;
    default:
      return NULL;
    }
  }

  Uint64 ndbFormatDateTime(const NdbDictColumn * theColumn, NdbDateTime * tm) {

    // Returns 1 on failure. How much does that suck?

    char dt_buf[20];
    Uint64 val = 0;
    switch(theColumn->getType()) {
    case NDB_TYPE_DATETIME:
    case NDB_TYPE_TIMESTAMP:
    {
      snprintf(dt_buf, 20, "%04d%02d%02d%02d%02d%02d",
               tm->year, tm->month, tm->day, tm->hour, tm->minute, tm->second);
      val=strtoull(dt_buf, 0, 10);
    }
    break;
    case NDB_TYPE_TIME:
    {
      snprintf(dt_buf, 20, "%02d%02d%02d",
               tm->hour, tm->minute, tm->second);
      val=strtoull(dt_buf, 0, 10);
    }
    break;
    case NDB_TYPE_DATE:
    {
      val=(tm->year << 9) | (tm->month << 5) | tm->day;
      //int3store(dt_buf, val);
    }
    break;
    default:
      return 1;
    }
    return val;
  }



  %}

%rename(NdbObject) NdbDictObject;
%rename(NdbTable) NdbDictTable;
%rename(NdbColumn) NdbDictColumn;
%rename(NdbIndex) NdbDictIndex;
%rename(NdbDictionary) NdbDictDictionary;
%rename(NdbEvent) NdbDictEvent;

/* Do this here so we can override it in the Java interface */
%rename(Ndb_cluster_connection) NdbClusterConnection;

long long getMicroTime();


enum AbortOption {
  CommitIfFailFree = 0,
  TryCommit = 0,
  AbortOnError = 0,
  CommitAsMuchAsPossible = 2,
  AO_IgnoreError = 2
};

enum ExecType {
  NoExecTypeDef = -1,
  Prepare = 0,
  NoCommit = 1,
  Commit = 2,
  Rollback = 3
};

typedef void (* NdbAsynchCallback)(int, NdbTransaction*, void*);
typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);

typedef const char * NdbDatetime;
typedef const char * NdbDate;
typedef const char * NdbTime;
typedef Uint32 NdbTimestamp;

// ndbFormatString mallocs memory. Return value must be free'd by calling code
%newobject ndbformatString;
%typemap(newfree) char * "free($1);";

%ndbexception("NdbApiException") {
  $action
    if (result==NULL) {
      NDB_exception(NdbApiException,"Error Converting Argument Type!");
    }
 }
char * ndbFormatString(const NdbDictColumn * theColumn,
                       const char* aString, size_t len);

%ndbnoexception;
