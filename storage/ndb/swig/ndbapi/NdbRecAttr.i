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

%newobject NdbRecAttr::getString();


class NdbRecAttr {
  NdbRecAttr(Ndb*);

public:
  ~NdbRecAttr();

  int isNULL();

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename(getInt64) int64_value();
  %rename(getInt32) int32_value();
  %rename(getShort) short_value();
  %rename(getChar)  char_value();
  %rename(getUint64) u_64_value() const;
  %rename(getUint32) u_32_value();
  %rename(getUshort) u_short_value() const;
  %rename(getUchar) u_char_value() const;
  %rename(getFloat) float_value() const;
  %rename(getDouble) double_value() const;
#else
  %rename(get_int64) int64_value();
  %rename(get_int32) int32_value();
  %rename(get_short) short_value();
  %rename(get_char)  char_value();
  %rename(get_uint64) u_64_value() const;
  %rename(get_uint32) u_32_value();
  %rename(get_ushort) u_short_value() const;
  %rename(get_uchar) u_char_value() const;
  %rename(get_float) float_value() const;
  %rename(get_double) double_value() const;
  %rename(getSizeInBytes) get_size_in_bytes() const;
#endif

  %ndbexception("NdbApiException") {
    $action
      }

  Int64 int64_value();
  Int32 int32_value();
  short short_value();
  char  char_value();
  const unsigned long long u_64_value() const;
  const unsigned long u_32_value();
  const unsigned short u_short_value() const;
  const unsigned char u_char_value() const;
  float float_value() const;
  double double_value() const;

  char* aRef() const;

  %ndbnoexception;

  const NdbDictColumn * getColumn() const;
  NdbDictColumn::Type getType() const;

  Uint32 get_size_in_bytes() const;
};

%extend NdbRecAttr {

#define uint2korr(A)(short) (((short) ((unsigned char) (A)[0])) +       \
                             ((short) ((unsigned char) (A)[1]) << 8))

  int getColType() const {
    NdbDictionary::Column::Type x = self->getType();
    int y = (int)x;
    return y;
  }

  %ndbexception("NdbApiException") {
    $action
      if (result->buf == NULL) {
        NDB_exception(NdbApiException,"Error fetching data");
      }
  }

  decimal_t * getDecimal() {
    NdbDictionary::Column::Type colType = self->getType();

    // (an array of ints, not base-10 digits)
    decimal_digit_t digits[DECIMAL_BUFF];
    decimal_t * dec = (decimal_t *)malloc(sizeof(decimal_t));
    dec->intg = 0;
    dec->frac = 0;
    dec->len = DECIMAL_BUFF;
    dec->sign = 0;
    dec->buf = digits;

    if(colType == NdbDictionary::Column::Decimal) {

      int prec  = self->getColumn()->getPrecision();
      int scale = self->getColumn()->getScale();
      bin2decimal(self->aRef(), dec, prec, scale);
    }

    return dec;


  }

  %ndbexception("NdbApiException") {
    $action
      if ( (result.theString == NULL) || (result.theLength == -1) )  {
        NDB_exception(NdbApiException,"Error fetching data");
      }
  }
  BYTES getBytes() {
    NdbDictionary::Column::Type colType = self->getType();
    BYTES output;
    output.theString=NULL;
    output.theLength=-1;

    char* rec = self->aRef();

    if(colType == NdbDictionary::Column::Longvarbinary)
    {
      output.theLength=(int)uint2korr(rec);
      output.theString = &rec[2];
    }
    else if(colType == NdbDictionary::Column::Varbinary)
    {

      output.theLength = (int) (rec[0]);
      output.theString = &rec[1];
    }
    else if( colType == NdbDictionary::Column::Binary)
    {
      output.theLength = strlen(rec)-1;
      output.theString = &rec[0];
    }
    return output;
  }

  %ndbexception("NdbApiException") {
    $action
      if (result == NULL) {
        NDB_exception(NdbApiException,"Error fetching data");
      }
  }

  const char* getString() {
    NdbDictionary::Column::Type colType = self->getType();

    char* ref = self->aRef();

    if(colType == NdbDictionary::Column::Longvarchar)
    {
      int len=(int)uint2korr(ref);
      char * buff = (char *)malloc(len+1);
      memset(buff,0,len+1);
      ref+=2;
      memcpy(buff,ref,len);
      buff[len+1]='\0';
      return buff;
    }
    else if(colType == NdbDictionary::Column::Varchar)
    {
      short len = (short) (unsigned char) (ref[0]);
      char * buff = (char *)malloc(len+1);
      memset(buff,0, len+1);
      memcpy(buff,++ref,len);
      buff[len+1]='\0';
      return buff;
    }
    else if(colType == NdbDictionary::Column::Char)
    {
      int len=strlen(ref);
      int i=len-1;
      /**
       * truncate spaces...
       */
      while(ref[i] == ' ')
      {
        i--;
      }
      i++;  //length of string until space padding starts..
      char * buff = (char *)malloc(i+1); //+1 for null;
      memcpy(buff, ref, i);
      buff[i]='\0';
      return buff;
    }
    return NULL;
  }

  %ndbnoexception;
  NdbDateTime * getDatetime() {
    return  new NdbDateTime();
  }

  %ndbexception("NdbApiException") {
    $action
      }

  NdbTimestamp getTimestamp() {
    return (NdbTimestamp)(self->u_32_value());
  }

  %ndbnoexception;
};
