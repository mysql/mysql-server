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



#include "db2i_ileBridge.h"
#include "mysql_priv.h"
#include "db2i_charsetSupport.h"
#include "ctype.h"
#include "ha_ibmdb2i.h"
#include "db2i_errors.h"
#include "wchar.h"

const char ZERO_DATETIME_VALUE[] = "0000-00-00 00:00:00";
const char ZERO_DATETIME_VALUE_SUBST[] = "0001-01-01 00:00:00";
const char ZERO_DATE_VALUE[] = "0000-00-00";
const char ZERO_DATE_VALUE_SUBST[] = "0001-01-01";


/**
  Put a BCD digit into a BCD string.
  
  @param[out] bcdString  The BCD string to be modified
  @param pos  The position within the string to be updated.
  @param val  The value to be assigned into the string at pos.
*/
static inline void bcdAssign(char* bcdString, uint pos, uint val)
{
  bcdString[pos/2] |= val << ((pos % 2) ? 0 : 4);
}

/**
  Read a BCD digit from a BCD string.
  
  @param[out] bcdString  The BCD string to be read
  @param pos  The position within the string to be read.
  
  @return bcdGet  The value of the BCD digit at pos.
*/
static inline uint bcdGet(const char* bcdString, uint pos)
{
  return (bcdString[pos/2] >> ((pos % 2) ? 0 : 4)) & 0xf;
}

/**
  In-place convert a number in ASCII represenation to EBCDIC representation.
  
  @param string  The string of ASCII characters
  @param len  The length of string
*/
static inline void convertNumericToEbcdicFast(char* string, int len)
{
  for (int i = 0; i < len; ++i, ++string)
  {
    switch(*string)
    {
      case '-':
        *string = 0x60; break;
      case ':':
        *string = 0x7A; break;
      case '.':
        *string = 0x4B; break;
      default:
        DBUG_ASSERT(isdigit(*string));
        *string += 0xF0 - '0';
        break;
    }
  }
}


/**
  atoi()-like function for a 4-character EBCDIC string.
  
  @param string  The EBCDIC string
  @return a4toi_ebcdic  The decimal value of the EBCDIC string
*/
static inline uint16 a4toi_ebcdic(const uchar* string)
{
  return ((string[0]-0xF0) * 1000 +
          (string[1]-0xF0) * 100 +
          (string[2]-0xF0) * 10 +
          (string[3]-0xF0));
};


/**
  atoi()-like function for a 4-character EBCDIC string.

  @param string  The EBCDIC string
  @return a4toi_ebcdic  The decimal value of the EBCDIC string
*/
static inline uint8 a2toi_ebcdic(const uchar* string)
{
  return ((string[0]-0xF0) * 10 +
          (string[1]-0xF0));
};

/**
  Perform character conversion for textual field data.
*/
int ha_ibmdb2i::convertFieldChars(enum_conversionDirection direction, 
                                  uint16 fieldID, 
                                  const char* input, 
                                  char* output, 
                                  size_t ilen, 
                                  size_t olen, 
                                  size_t* outDataLen,
                                  bool tacitErrors,
                                  size_t* substChars)
{
  DBUG_PRINT("ha_ibmdb2i::convertFieldChars",("Direction: %d; length = %d", direction, ilen));
  
  if (unlikely(ilen == 0))
  {
    if (outDataLen) *outDataLen = 0;
    return (0);
  }
  
  iconv_t& conversion = db2Table->getConversionDefinition(direction, fieldID);
  
  if (unlikely(conversion == (iconv_t)(-1)))
  {
    return (DB2I_ERR_UNSUPP_CHARSET);
  }

  size_t initOLen= olen;
  size_t substitutedChars = 0;
  int rc = iconv(conversion, (char**)&input, &ilen, &output, &olen, &substitutedChars );
  if (outDataLen) *outDataLen = initOLen - olen;
  if (substChars) *substChars = substitutedChars;
  if (unlikely(rc < 0))
  {
    int er = errno;
    if (er == EILSEQ)
    {
      if (!tacitErrors) getErrTxt(DB2I_ERR_ILL_CHAR, table->field[fieldID]->field_name);
      return (DB2I_ERR_ILL_CHAR);
    }
    else
    {
      if (!tacitErrors) getErrTxt(DB2I_ERR_ICONV,er);
      return (DB2I_ERR_ICONV);
    }
  }
  if (unlikely(substitutedChars) && (!tacitErrors))
  {
    warning(ha_thd(), DB2I_ERR_SUB_CHARS, table->field[fieldID]->field_name);
  }

  return (0);
}

/**
  Append the appropriate default value clause onto a CREATE TABLE definition
  
  This was inspired by get_field_default_value in sql/sql_show.cc.
  
  @param field  The field whose value is to be obtained
  @param statement  The string to receive the DEFAULT clause
  @param quoteIt  Does the data type require single quotes around the value?
  @param ccsid  The ccsid of the field value (if a string type); 0 if no conversion needed  
*/
static void get_field_default_value(Field *field, 
                                    String &statement, 
                                    bool quoteIt, 
                                    uint32 ccsid,
                                    bool substituteZeroDates)
{
  if ((field->type() != FIELD_TYPE_BLOB &&
      !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
      field->unireg_check != Field::NEXT_NUMBER))
  {
    my_ptrdiff_t old_ptr= (my_ptrdiff_t) (field->table->s->default_values - field->table->record[0]); 
    field->move_field_offset(old_ptr);
    
    String defaultClause(64);
    defaultClause.length(0);
    defaultClause.append(" DEFAULT ");
    if (!field->is_null())
    {
      my_bitmap_map *old_map = tmp_use_all_columns(field->table, field->table->read_set);
      char tmp[MAX_FIELD_WIDTH];
      
      if (field->real_type() == MYSQL_TYPE_ENUM ||
          field->real_type() == MYSQL_TYPE_SET) 
      {
        CHARSET_INFO *cs= &my_charset_bin;
        uint len = (uint)(cs->cset->longlong10_to_str)(cs,tmp,sizeof(tmp), 10, field->val_int()); 
        tmp[len]=0; 
        defaultClause.append(tmp);
      }
      else
      {
        String type(tmp, sizeof(tmp), field->charset());
        field->val_str(&type);
        if (type.length())
        {
          if (field->type() == MYSQL_TYPE_DATE &&
               memcmp(type.ptr(), STRING_WITH_LEN(ZERO_DATE_VALUE)) == 0)
          {
            if (substituteZeroDates)
              type.set(STRING_WITH_LEN(ZERO_DATE_VALUE_SUBST), field->charset());
            else
            {
              warning(current_thd, DB2I_ERR_WARN_COL_ATTRS, field->field_name);
              return;
            }
          }
          else if ((field->type() == MYSQL_TYPE_DATETIME ||
                field->type() == MYSQL_TYPE_TIMESTAMP) &&
               memcmp(type.ptr(), STRING_WITH_LEN(ZERO_DATETIME_VALUE)) == 0)
          {
            if (substituteZeroDates)
              type.set(STRING_WITH_LEN(ZERO_DATETIME_VALUE_SUBST), field->charset());
            else
            {
              warning(current_thd, DB2I_ERR_WARN_COL_ATTRS, field->field_name);
              return;
            }
          }


          if (field->type() != MYSQL_TYPE_STRING &&
              field->type() != MYSQL_TYPE_VARCHAR &&
              field->type() != MYSQL_TYPE_BLOB &&
              field->type() != MYSQL_TYPE_BIT)
          {
            if (quoteIt)
              defaultClause.append('\'');
            defaultClause.append(type);
            if (quoteIt)
              defaultClause.append('\'');
          }
          else
          {
            int length;
            char* out;
            
            // If a ccsid is specified, we need to make sure that the DEFAULT
            // string is converted to that encoding.
            if (ccsid != 0)
            {
              iconv_t iconvD;
              if (getConversion(toDB2, field->charset(), ccsid, iconvD))
              {
                warning(current_thd, DB2I_ERR_WARN_COL_ATTRS, field->field_name);
                return;
              }

              size_t ilen = type.length();
              size_t olen = 6 * ilen;
              size_t origOlen = olen;
              const char* in = type.ptr();
              const char* tempIn = in;
              out = (char*)my_malloc(olen, MYF(MY_WME));
              char* tempOut = out;
              size_t substitutedChars;

              if (iconv(iconvD, (char**)&tempIn, &ilen, &tempOut, &olen, &substitutedChars) < 0)
              {
                warning(current_thd, DB2I_ERR_WARN_COL_ATTRS, field->field_name);
                my_free(out, MYF(0));
                return;
              }
              // Now we process the converted string to represent it as 
              // hexadecimal values.

              length = origOlen - olen;
            }
            else
            {
              length = type.length();
              out = (char*)my_malloc(length*2, MYF(MY_WME));
              memcpy(out, (char*)type.ptr(), length);
            }
            
            if (length > 16370)
            {
              warning(current_thd, DB2I_ERR_WARN_COL_ATTRS, field->field_name);
              my_free(out, MYF(0));
              return;
            }

            if (ccsid == 1200)
              defaultClause.append("ux'");
            else if (ccsid == 13488)
              defaultClause.append("gx'");
            else if (field->charset() == &my_charset_bin)
              defaultClause.append("binary(x'");              
            else
              defaultClause.append("x'");

            const char hexMap[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};            
            for (int c = length-1; c >= 0; --c)
            {
              out[c*2+1] = hexMap[out[c] & 0xF];
              out[c*2] = hexMap[out[c] >> 4];
            }

            defaultClause.append(out, length*2);
            defaultClause.append('\'');
            if (field->charset() == &my_charset_bin)
              defaultClause.append(")");              

            my_free(out, MYF(0));
          }
        }
        else
          defaultClause.length(0);
      }
      tmp_restore_column_map(field->table->read_set, old_map);
    }
    else if (field->maybe_null())
      defaultClause.append(STRING_WITH_LEN("NULL"));
    
    if (old_ptr)
      field->move_field_offset(-old_ptr);
    
    statement.append(defaultClause);
  }
}




/** 
    Convert a MySQL field definition into its corresponding DB2 type.
    
    The result will be appended to mapping as a DB2 SQL phrase.
    
    @param field  The MySQL field to be evaluated
    @param[out] mapping  The receiver for the DB2 SQL syntax
    @param timeFormat  The format to be used for mapping the TIME type
*/  
int ha_ibmdb2i::getFieldTypeMapping(Field* field, 
                                    String& mapping, 
                                    enum_TimeFormat timeFormat, 
                                    enum_BlobMapping blobMapping,
                                    enum_ZeroDate zeroDateHandling,
                                    bool propagateDefaults,
                                    enum_YearFormat yearFormat)
{
  char stringBuildBuffer[257];
  uint32 fieldLength;
  bool defaultNeedsQuotes = false;
  uint16 db2Ccsid = 0;

  CHARSET_INFO* fieldCharSet = field->charset();
  switch (field->type())
  {
    case MYSQL_TYPE_NEWDECIMAL: 
      {
        uint precision= ((Field_new_decimal*)field)->precision;
        uint scale= field->decimals();

        if (precision <= MAX_DEC_PRECISION)
        {
          sprintf(stringBuildBuffer,"DECIMAL(%d, %d)",precision,scale);
        }
        else
        {
          if (scale > precision - MAX_DEC_PRECISION)
          {
            scale = scale - (precision - MAX_DEC_PRECISION);
            precision = MAX_DEC_PRECISION;
            sprintf(stringBuildBuffer,"DECIMAL(%d, %d)",precision,scale);
          }
          else
          {
            return HA_ERR_UNSUPPORTED;
          }
          warning(ha_thd(), DB2I_ERR_PRECISION); 
        }

        mapping.append(stringBuildBuffer);
      }
      break;
    case MYSQL_TYPE_TINY: 
        mapping.append(STRING_WITH_LEN("SMALLINT"));
      break;
    case MYSQL_TYPE_SHORT: 
      if (((Field_num*)field)->unsigned_flag)
        mapping.append(STRING_WITH_LEN("INT"));
      else
        mapping.append(STRING_WITH_LEN("SMALLINT"));
      break;
    case MYSQL_TYPE_LONG: 
      if (((Field_num*)field)->unsigned_flag)
        mapping.append(STRING_WITH_LEN("BIGINT"));
      else
        mapping.append(STRING_WITH_LEN("INT"));
      break;
    case MYSQL_TYPE_FLOAT: 
        mapping.append(STRING_WITH_LEN("REAL"));
      break;
    case MYSQL_TYPE_DOUBLE: 
      mapping.append(STRING_WITH_LEN("DOUBLE"));
      break;
    case MYSQL_TYPE_LONGLONG: 
      if (((Field_num*)field)->unsigned_flag)
        mapping.append(STRING_WITH_LEN("DECIMAL(20,0)"));
      else
        mapping.append(STRING_WITH_LEN("BIGINT"));
      break;
    case MYSQL_TYPE_INT24: 
      mapping.append(STRING_WITH_LEN("INTEGER"));
      break;
    case MYSQL_TYPE_DATE: 
    case MYSQL_TYPE_NEWDATE:
      mapping.append(STRING_WITH_LEN("DATE"));
      defaultNeedsQuotes = true;
      break;
    case MYSQL_TYPE_TIME:
      if (timeFormat == TIME_OF_DAY)
      {
        mapping.append(STRING_WITH_LEN("TIME"));
        defaultNeedsQuotes = true;
      }
      else
        mapping.append(STRING_WITH_LEN("INTEGER"));
      break;
    case MYSQL_TYPE_DATETIME:
      mapping.append(STRING_WITH_LEN("TIMESTAMP"));
      defaultNeedsQuotes = true;
      break;
    case MYSQL_TYPE_TIMESTAMP: 
      mapping.append(STRING_WITH_LEN("TIMESTAMP"));

      if (table_share->timestamp_field == field && propagateDefaults)
      {
        switch (((Field_timestamp*)field)->get_auto_set_type())
        {
          case TIMESTAMP_NO_AUTO_SET:
            break;
          case TIMESTAMP_AUTO_SET_ON_INSERT:
            mapping.append(STRING_WITH_LEN(" DEFAULT CURRENT_TIMESTAMP"));
            break;
          case TIMESTAMP_AUTO_SET_ON_UPDATE:
            if (osVersion.v >= 6 &&
                !field->is_null())
            {
              mapping.append(STRING_WITH_LEN(" GENERATED BY DEFAULT FOR EACH ROW ON UPDATE AS ROW CHANGE TIMESTAMP"));
              warning(ha_thd(), DB2I_ERR_WARN_COL_ATTRS, field->field_name);
            }
            else
              warning(ha_thd(), DB2I_ERR_WARN_COL_ATTRS, field->field_name);
            break;
          case TIMESTAMP_AUTO_SET_ON_BOTH:
            if (osVersion.v >= 6 &&
                !field->is_null())
              mapping.append(STRING_WITH_LEN(" GENERATED BY DEFAULT FOR EACH ROW ON UPDATE AS ROW CHANGE TIMESTAMP"));
            else
            {
              mapping.append(STRING_WITH_LEN(" DEFAULT CURRENT_TIMESTAMP"));
              warning(ha_thd(), DB2I_ERR_WARN_COL_ATTRS, field->field_name);
            }
            break;
        }
      }
      else
        defaultNeedsQuotes = true;
      break;
    case MYSQL_TYPE_YEAR:
      if (yearFormat == CHAR4)
      {
        mapping.append(STRING_WITH_LEN("CHAR(4) CCSID 1208"));
        defaultNeedsQuotes = true;
      }
      else
      {
        mapping.append(STRING_WITH_LEN("SMALLINT"));
        defaultNeedsQuotes = false;
      }
      break;
    case MYSQL_TYPE_BIT: 
      sprintf(stringBuildBuffer, "BINARY(%d)", (field->max_display_length() / 8) + 1);
      mapping.append(stringBuildBuffer);
      break;
    case MYSQL_TYPE_BLOB: 
    case MYSQL_TYPE_VARCHAR: 
    case MYSQL_TYPE_STRING: 
      {
        if (field->real_type() == MYSQL_TYPE_ENUM ||
            field->real_type() == MYSQL_TYPE_SET) 
        {
          mapping.append(STRING_WITH_LEN("BIGINT"));
        }
        else
        {
          defaultNeedsQuotes = true;
          
          fieldLength = field->max_display_length(); // Get field byte length

          if (fieldCharSet == &my_charset_bin)
          {
            if (field->type() == MYSQL_TYPE_STRING)
            {
              sprintf(stringBuildBuffer, "BINARY(%d)", max(fieldLength, 1));
            }
            else
            {
              if (fieldLength <= MAX_VARCHAR_LENGTH)
              {
                sprintf(stringBuildBuffer, "VARBINARY(%d)", max(fieldLength, 1));
              }
              else if (blobMapping == AS_VARCHAR &&
                       (field->flags & PART_KEY_FLAG))
              {
                sprintf(stringBuildBuffer, "LONG VARBINARY ");
              }
              else 
              {
                fieldLength = min(MAX_BLOB_LENGTH, fieldLength);
                sprintf(stringBuildBuffer, "BLOB(%d)", max(fieldLength, 1));
              }
            }
            mapping.append(stringBuildBuffer);
          }
          else
          {
            if (field->type() == MYSQL_TYPE_STRING)
            {
              if (fieldLength > MAX_CHAR_LENGTH)
                return 1;
              if (fieldCharSet->mbmaxlen > 1)
              {
                if (memcmp(fieldCharSet->name, "ucs2_", sizeof("ucs2_")-1) == 0 ) // UCS2
                {
                  sprintf(stringBuildBuffer, "GRAPHIC(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                  db2Ccsid = 13488;
                }
                else if (memcmp(fieldCharSet->name, "utf8_", sizeof("utf8_")-1) == 0 &&
                         strcmp(fieldCharSet->name, "utf8_general_ci") != 0) 
                {
                  sprintf(stringBuildBuffer, "CHAR(%d)", max(fieldLength, 1)); // Number of bytes
                  db2Ccsid = 1208;
                }
                else
                {
                  sprintf(stringBuildBuffer, "GRAPHIC(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                  db2Ccsid = 1200;
                }
              }
              else
              {
                sprintf(stringBuildBuffer, "CHAR(%d)", max(fieldLength, 1));
              }
              mapping.append(stringBuildBuffer);
            }
            else
            {
              if (fieldLength <= MAX_VARCHAR_LENGTH)
              {
                if (fieldCharSet->mbmaxlen > 1)
                {
                  if (memcmp(fieldCharSet->name, "ucs2_", sizeof("ucs2_")-1) == 0 ) // UCS2
                  {
                    sprintf(stringBuildBuffer, "VARGRAPHIC(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                    db2Ccsid = 13488;
                  }
                  else if (memcmp(fieldCharSet->name, "utf8_", sizeof("utf8_")-1) == 0 &&
                           strcmp(fieldCharSet->name, "utf8_general_ci") != 0) 
                  {
                    sprintf(stringBuildBuffer, "VARCHAR(%d)", max(fieldLength, 1)); // Number of bytes
                    db2Ccsid = 1208;
                  }
                  else
                  {
                    sprintf(stringBuildBuffer, "VARGRAPHIC(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                    db2Ccsid = 1200;
                  }
                }
                else
                {
                  sprintf(stringBuildBuffer, "VARCHAR(%d)", max(fieldLength, 1));
                }
              }
              else if (blobMapping == AS_VARCHAR &&
                       (field->flags & PART_KEY_FLAG))
              {
                if (fieldCharSet->mbmaxlen > 1)
                {
                  if (memcmp(fieldCharSet->name, "ucs2_", sizeof("ucs2_")-1) == 0 ) // UCS2
                  {
                    sprintf(stringBuildBuffer, "LONG VARGRAPHIC ");
                    db2Ccsid = 13488;
                  }
                  else if (memcmp(fieldCharSet->name, "utf8_", sizeof("utf8_")-1) == 0 &&
                           strcmp(fieldCharSet->name, "utf8_general_ci") != 0) 
                  {
                    sprintf(stringBuildBuffer, "LONG VARCHAR ");
                    db2Ccsid = 1208;
                  }
                  else
                  {
                    sprintf(stringBuildBuffer, "LONG VARGRAPHIC ");
                    db2Ccsid = 1200;
                  }
                }
                else
                {
                  sprintf(stringBuildBuffer, "LONG VARCHAR ");
                }
              }
              else
              {
                fieldLength = min(MAX_BLOB_LENGTH, fieldLength);

                if (fieldCharSet->mbmaxlen > 1)
                {
                  if (memcmp(fieldCharSet->name, "ucs2_", sizeof("ucs2_")-1) == 0 ) // UCS2
                  {
                    sprintf(stringBuildBuffer, "DBCLOB(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                    db2Ccsid = 13488;
                  }
                  else if (memcmp(fieldCharSet->name, "utf8_", sizeof("utf8_")-1) == 0 &&
                           strcmp(fieldCharSet->name, "utf8_general_ci") != 0) 
                  {
                    sprintf(stringBuildBuffer, "CLOB(%d)", max(fieldLength, 1)); // Number of bytes
                    db2Ccsid = 1208;
                  }
                  else
                  {
                    sprintf(stringBuildBuffer, "DBCLOB(%d)", max(fieldLength / fieldCharSet->mbmaxlen, 1)); // Number of characters
                    db2Ccsid = 1200;
                  }
                }
                else
                {
                  sprintf(stringBuildBuffer, "CLOB(%d)", max(fieldLength, 1)); // Number of characters
                }
              }

              mapping.append(stringBuildBuffer);
            }
            if (db2Ccsid == 0) // If not overriding CCSID
            {
              int32 rtnCode = convertIANAToDb2Ccsid(fieldCharSet->csname, &db2Ccsid);
              if (rtnCode)
                return rtnCode;
            }
            
            if (db2Ccsid != 1208 &&
                db2Ccsid != 13488)
            {
              // Check whether there is a character conversion available.
              iconv_t temp;
              int32 rc = getConversion(toDB2, fieldCharSet, db2Ccsid, temp);
              if (unlikely(rc))
                return rc;
            }
            
            sprintf(stringBuildBuffer, " CCSID %d ", db2Ccsid);
            mapping.append(stringBuildBuffer);
          }
        }
      }     
      break;

  }
  
  if (propagateDefaults)
    get_field_default_value(field, 
                            mapping, 
                            defaultNeedsQuotes, 
                            db2Ccsid, 
                            (zeroDateHandling==SUBSTITUTE_0001_01_01));
  
  return 0;
} 


/**
    Convert MySQL field data into the equivalent DB2 format
    
    @param field  The MySQL field to be converted
    @param db2Field  The corresponding DB2 field definition
    @param db2Buf  The buffer to receive the converted data
    @param data  NULL if field points to the correct data; otherwise,
                 the data to be converted (for use with keys)
*/
int32 ha_ibmdb2i::convertMySQLtoDB2(Field* field, const DB2Field& db2Field, char* db2Buf, const uchar* data)
{
  enum_field_types fieldType = field->type();
  switch (fieldType)
  {
    case MYSQL_TYPE_NEWDECIMAL: 
      {
        uint precision= ((Field_new_decimal*)field)->precision;
        uint scale= field->decimals();
        uint db2Precision = min(precision, MAX_DEC_PRECISION);
        uint truncationAmount = precision - db2Precision;
        
        if (scale >= truncationAmount)
        {
          String tempString(precision+2);

          if (data == NULL)
          {
            field->val_str((String*)&tempString, (String*)(NULL));
          }
          else
          {
            field->val_str(&tempString, data);
          }
          const char* temp = tempString.ptr();
          char packed[32];
          memset(&packed, 0, sizeof(packed));

          int bcdPos = db2Precision - (db2Precision % 2 ? 1 : 0);
          bcdAssign(packed, bcdPos+1, (temp[0] == '-' ? 0xD : 0xF));
          
          int strPos=tempString.length() - 1 - truncationAmount;

          for (;strPos >= 0 && bcdPos >= 0; strPos--)
          {
            if (my_isdigit(&my_charset_latin1, temp[strPos]))
            {
              bcdAssign(packed, bcdPos, temp[strPos]-'0');
              --bcdPos;
            }
          }
          memcpy(db2Buf, &packed, (db2Precision/2)+1);
        }

      }
      break;
    case MYSQL_TYPE_TINY:
      {
         int16 temp = (data == NULL ? field->val_int() : field->val_int(data));
         memcpy(db2Buf , &temp, sizeof(temp));
      }
      break;
   case MYSQL_TYPE_SHORT: 
     {
        if (((Field_num*)field)->unsigned_flag)
        {
          memset(db2Buf, 0, 2);
          memcpy(db2Buf+2, (data == NULL ? field->ptr : data), 2);
        }
        else
        {
          memcpy(db2Buf, (data == NULL ? field->ptr : data), 2);
        }
     }
     break;
    case MYSQL_TYPE_LONG: 
      {
        if (((Field_num*)field)->unsigned_flag)
        {
          memset(db2Buf, 0, 4);
          memcpy(db2Buf+4, (data == NULL ? field->ptr : data), 4);
        }
        else
        {
          memcpy(db2Buf, (data == NULL ? field->ptr : data), 4);
        }
      }
      break;
    case MYSQL_TYPE_FLOAT:
      {
        memcpy(db2Buf, (data == NULL ? field->ptr : data), 4);
      }
      break;
    case MYSQL_TYPE_DOUBLE: 
      {
        memcpy(db2Buf, (data == NULL ? field->ptr : data), 8);
      }
      break;
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
      {
        String tempString(27);
        if (data == NULL)
        {
          field->val_str(&tempString, &tempString);
        }
        else
        {
          field->val_str(&tempString, data);
        }
        memset(db2Buf, '0', 26);
        memcpy(db2Buf, tempString.ptr(), tempString.length());
        if (strncmp(db2Buf,ZERO_DATETIME_VALUE,strlen(ZERO_DATETIME_VALUE)) == 0)
        {
          if (cachedZeroDateOption == SUBSTITUTE_0001_01_01)
            memcpy(db2Buf, ZERO_DATETIME_VALUE_SUBST, sizeof(ZERO_DATETIME_VALUE_SUBST));
          else
          {
            getErrTxt(DB2I_ERR_INVALID_COL_VALUE, field->field_name);
            return(DB2I_ERR_INVALID_COL_VALUE);
          }
        }
        (db2Buf)[10] = '-';
        (db2Buf)[13] = (db2Buf)[16] = (db2Buf)[19] = '.';
        
        convertNumericToEbcdicFast(db2Buf, 26);
      }
      break;
    case MYSQL_TYPE_LONGLONG:
      {
        if (((Field_num*)field)->unsigned_flag)
        {
          char temp[23];
          String tempString(temp, sizeof(temp), &my_charset_latin1);

          if (data == NULL)
          {
            field->val_str((String*)&tempString, (String*)(NULL));
          }
          else
          {
            field->val_str(&tempString, data);
          }
          char packed[11];
          memset(packed, 0, sizeof(packed));
          bcdAssign(packed, 21, (temp[0] == '-' ? 0xD : 0xF));
          int strPos=tempString.length()-1;
          int bcdPos=20;

          for (;strPos >= 0; strPos--)
          {
            if (my_isdigit(&my_charset_latin1, temp[strPos]))
            {
              bcdAssign(packed, bcdPos, temp[strPos]-'0');
              --bcdPos;
            }
          }
          memcpy(db2Buf, &packed, 11);
        }
        else
        {
          *(uint64*)db2Buf = *(uint64*)(data == NULL ? field->ptr : data);
        }
      }
      break;
    case MYSQL_TYPE_INT24:
      {
         int32 temp= (data == NULL ? field->val_int() : field->val_int(data));
         memcpy(db2Buf , &temp, sizeof(temp));
      }
      break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      {
        String tempString(11);
        if (data == NULL)
        {
          field->val_str(&tempString, (String*)NULL);
        }
        else
        {
          field->val_str(&tempString, data);
        }
        memcpy(db2Buf, tempString.ptr(), 10);
        if (strncmp(db2Buf,ZERO_DATE_VALUE,strlen(ZERO_DATE_VALUE)) == 0)
        {
          if (cachedZeroDateOption == SUBSTITUTE_0001_01_01)
            memcpy(db2Buf, ZERO_DATE_VALUE_SUBST, sizeof(ZERO_DATE_VALUE_SUBST));
          else
          {
            getErrTxt(DB2I_ERR_INVALID_COL_VALUE,field->field_name);
            return(DB2I_ERR_INVALID_COL_VALUE);
          }
        }

        convertNumericToEbcdicFast(db2Buf,10);
      }
      break;
    case MYSQL_TYPE_TIME:
      {
        if (db2Field.getType() == QMY_TIME)
        {
          String tempString(10); 
          if (data == NULL)
          {
            field->val_str(&tempString, (String*)NULL);
          }
          else
          {
            field->val_str(&tempString, data);
          }
          memcpy(db2Buf, tempString.ptr(), 8);
          (db2Buf)[2]=(db2Buf)[5] = '.';

          convertNumericToEbcdicFast(db2Buf, 8);
        }
        else
        {
          int32 temp = sint3korr(data == NULL ? field->ptr : data);
          memcpy(db2Buf, &temp, sizeof(temp));
        }
      }
      break;
    case MYSQL_TYPE_YEAR:
      {
        String tempString(5);
        if (db2Field.getType() == QMY_CHAR)
        {
          if (data == NULL)
          {
            field->val_str(&tempString, (String*)NULL);
          }
          else
          {
            field->val_str(&tempString, data);
          }
          memcpy(db2Buf, tempString.ptr(), 4);
        }
        else
        {
          uint8 temp = *(uint8*)(data == NULL ? field->ptr : data);
          *(uint16*)(db2Buf) = (temp ? temp + 1900 : 0);
        }
      }         
      break;
    case MYSQL_TYPE_BIT:
      {
        int bytesToCopy = db2Field.getByteLengthInRecord();

        if (data == NULL)
        {
          uint64 temp = field->val_int();
          memcpy(db2Buf, 
                 ((char*)&temp) + (sizeof(temp) - bytesToCopy), 
                 bytesToCopy);
        }
        else
        {
          memcpy(db2Buf,
                 data,
                 bytesToCopy);
        }                
      }
      break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_BLOB:
      {
        if (field->real_type() == MYSQL_TYPE_ENUM ||
            field->real_type() == MYSQL_TYPE_SET) 
        {
          int64 temp= (data == NULL ? field->val_int() : field->val_int(data));
          *(int64*)db2Buf = temp;
        }
        else
        {
          const uchar* dataToStore;
          uint32 bytesToStore;
          uint32 bytesToPad = 0;
          CHARSET_INFO* fieldCharSet = field->charset();
          uint32 maxDisplayLength = field->max_display_length();
          switch (fieldType)
          {
            case MYSQL_TYPE_STRING:
              {
                bytesToStore = maxDisplayLength;
                if (data == NULL)
                  dataToStore = field->ptr;
                else
                  dataToStore = data;
              }
              break;
            case MYSQL_TYPE_VARCHAR:
              {

                if (data == NULL)
                {
                  bytesToStore = field->data_length();
                  dataToStore = field->ptr + ((Field_varstring*)field)->length_bytes;
                }
                else
                {
                  // Key lens are stored little-endian
                  bytesToStore = *(uint8*)data + ((*(uint8*)(data+1)) << 8);
                  dataToStore = data + 2;                  
                }
                bytesToPad = maxDisplayLength - bytesToStore;
              }
              break;
            case MYSQL_TYPE_BLOB:
              {
                if (data == NULL)
                {
                  bytesToStore = ((Field_blob*)field)->get_length();                
                  bytesToPad = maxDisplayLength - bytesToStore;
                  ((Field_blob*)field)->get_ptr((uchar**)&dataToStore);
                }
                else
                {
                  // Key lens are stored little-endian
                  bytesToStore = *(uint8*)data + ((*(uint8*)(data+1)) << 8);
                  dataToStore = data + 2;                  
                }
              }
              break; 
          }

          int32 rc;
          uint16 db2FieldType = db2Field.getType();
          switch(db2FieldType)
          {
            case QMY_CHAR:
                if (maxDisplayLength == 0)
                  bytesToPad = 1;              
            case QMY_VARCHAR:
                if (db2FieldType == QMY_VARCHAR)
                {
                  db2Buf += sizeof(uint16);
                  bytesToPad = 0;
                }
                
                if (bytesToStore > db2Field.getDataLengthInRecord())
                {
                  bytesToStore = db2Field.getDataLengthInRecord();
                  field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
                }

                if (fieldCharSet == &my_charset_bin) // If binary 
                {
                  if (bytesToStore)
                    memcpy(db2Buf, dataToStore, bytesToStore);
                  if (bytesToPad)
                    memset(db2Buf + bytesToStore, 0x00, bytesToPad);
                }
                else if (db2Field.getCCSID() == 1208) // utf8
                {
                  if (bytesToStore)
                    memcpy(db2Buf, dataToStore, bytesToStore);
                  if (bytesToPad)
                    memset(db2Buf + bytesToStore, ' ', bytesToPad);
                }
                else // single-byte ASCII to EBCDIC 
                {
                  DBUG_ASSERT(fieldCharSet->mbmaxlen == 1);
                  if (bytesToStore)
                  {
                    rc = convertFieldChars(toDB2, field->field_index, (char*)dataToStore, db2Buf, bytesToStore, bytesToStore, NULL);
                    if (rc)
                      return rc;
                  }
                  if (bytesToPad)
                    memset(db2Buf + bytesToStore, 0x40, bytesToPad);
                }

                if (db2FieldType == QMY_VARCHAR)
                  *(uint16*)(db2Buf - sizeof(uint16)) = bytesToStore; 
                break;
            case QMY_VARGRAPHIC:
                db2Buf += sizeof(uint16);
                bytesToPad = 0;
            case QMY_GRAPHIC:
                if (maxDisplayLength == 0 && db2FieldType == QMY_GRAPHIC)
                  bytesToPad = 2;

                if (db2Field.getCCSID() == 13488)
                {
                  if (bytesToStore)
                    memcpy(db2Buf, dataToStore, bytesToStore);
                  if (bytesToPad)
                    memset16((db2Buf + bytesToStore), 0x0020, bytesToPad/2);
                }
                else
                {
                  size_t db2BytesToStore;
                  size_t maxDb2BytesToStore;

                  if (maxDisplayLength == 0 && db2FieldType == QMY_GRAPHIC)
                    maxDb2BytesToStore = 2;
                  else
                    maxDb2BytesToStore = min(((bytesToStore * 2) / fieldCharSet->mbminlen),
                                             ((maxDisplayLength * 2) / fieldCharSet->mbmaxlen));

                  if (bytesToStore == 0)
                    db2BytesToStore = 0;
                  else
                  {
                    rc = convertFieldChars(toDB2, field->field_index, (char*)dataToStore, db2Buf, bytesToStore, maxDb2BytesToStore, &db2BytesToStore);
                    if (rc)
                      return rc;
                    bytesToStore = db2BytesToStore;
                  }
                  if (db2BytesToStore < maxDb2BytesToStore) // If need to pad
                    memset16((db2Buf + db2BytesToStore), 0x0020, (maxDb2BytesToStore - db2BytesToStore)/2);
                }

                if (db2FieldType == QMY_VARGRAPHIC)
                  *(uint16*)(db2Buf-sizeof(uint16)) = bytesToStore/2;
              break;
            case QMY_BLOBCLOB:
            case QMY_DBCLOB:
              {
                DBUG_ASSERT(data == NULL);
                DB2LobField* lobField = (DB2LobField*)(db2Buf + db2Field.calcBlobPad());

                if ((fieldCharSet == &my_charset_bin) || // binary or
                    (db2Field.getCCSID()==13488) ||
                    (db2Field.getCCSID()==1208)) // binary UTF8 
                {
                }
                else 
                {
                  char* temp;
                  int32 rc;
                  size_t db2BytesToStore;
                  if (fieldCharSet->mbmaxlen == 1) // single-byte ASCII to EBCDIC 
                  {
                    temp = getCharacterConversionBuffer(field->field_index, bytesToStore);
                    rc = convertFieldChars(toDB2, field->field_index, (char*)dataToStore,temp,bytesToStore, bytesToStore, NULL);
                    if (rc)
                      return (rc);
                  }
                  else // Else Far East, special UTF8 or non-special UTF8/UCS2
                  {
                    size_t maxDb2BytesToStore;
                    maxDb2BytesToStore = min(((bytesToStore * 2) / fieldCharSet->mbminlen),
                                             ((maxDisplayLength * 2) / fieldCharSet->mbmaxlen));
                    temp = getCharacterConversionBuffer(field->field_index, maxDb2BytesToStore);
                    rc = convertFieldChars(toDB2, field->field_index, (char*)dataToStore,temp,bytesToStore, maxDb2BytesToStore, &db2BytesToStore);
                    if (rc)
                      return (rc);
                    bytesToStore = db2BytesToStore;
                  }
                  dataToStore = (uchar*)temp;                
                }

                uint16 blobID = db2Table->getBlobIdFromField(field->field_index);
                if (blobWriteBuffers[blobID] != (char*)dataToStore)
                  blobWriteBuffers[blobID].reassign((char*)dataToStore);
                if ((void*)blobWriteBuffers[blobID])
                  lobField->dataHandle = (ILEMemHandle)blobWriteBuffers[blobID];
                else
                  lobField->dataHandle = 0;
                lobField->length = bytesToStore / (db2FieldType == QMY_DBCLOB ? 2 : 1);
              }
              break;            
            }
        }
      }
      break;
    default: 
        DBUG_ASSERT(0);
      break;
  }

  return (ha_thd()->is_error());
}


/**
    Convert DB2 field data into the equivalent MySQL format
    
    @param db2Field  The DB2 field definition
    @param field  The MySQL field to receive the converted data
    @param buf  The DB2 data to be converted
*/
int32 ha_ibmdb2i::convertDB2toMySQL(const DB2Field& db2Field, Field* field, const char* buf)
{
  int32 storeRC = 0; // Result of the field->store() operation
  
  const char* bufPtr = buf + db2Field.getBufferOffset();
  
  switch (field->type())
  {
    case MYSQL_TYPE_NEWDECIMAL:
      {
        uint precision= ((Field_new_decimal*)field)->precision;
        uint scale= field->decimals();
        uint db2Precision = min(precision, MAX_DEC_PRECISION);
        uint decimalPlace = precision-scale+1;
        char temp[80];

        if (precision <= MAX_DEC_PRECISION ||
            scale > precision - MAX_DEC_PRECISION)
        {
          uint numNibbles = db2Precision + (db2Precision % 2 ? 0 : 1);
          
          temp[0] = (bcdGet(bufPtr, numNibbles) == 0xD ? '-' : ' ');
          int strPos=1;
          int bcdPos=(db2Precision % 2 ? 0 : 1);

          for (;bcdPos < numNibbles; bcdPos++, strPos++)
          {
            if (strPos == decimalPlace)
            {
              temp[strPos] = '.';
              strPos++;
            }
            
            temp[strPos] = bcdGet(bufPtr, bcdPos) + '0';
          }
          
          temp[strPos] = 0;
          
          storeRC = field->store(temp, strPos, &my_charset_latin1);
        }
      }
      break;
    case MYSQL_TYPE_TINY:
      {
         storeRC = field->store(*(int16*)bufPtr, ((Field_num*)field)->unsigned_flag);
      }
      break;
    case MYSQL_TYPE_SHORT: 
      {
         if (((Field_num*)field)->unsigned_flag)
         {
           storeRC = field->store(*(int32*)bufPtr, TRUE);
         }
         else
         {
           storeRC = field->store(*(int16*)bufPtr, FALSE);
         }
      }
      break;
    case MYSQL_TYPE_LONG: 
      {
         if (((Field_num*)field)->unsigned_flag)
         {
           storeRC = field->store(*(int64*)bufPtr, TRUE);
         }
         else
         {
           storeRC = field->store(*(int32*)bufPtr, FALSE);
         }
      }
      break;
    case MYSQL_TYPE_FLOAT:
      {
        storeRC = field->store(*(float*)bufPtr);
      }
      break;
    case MYSQL_TYPE_DOUBLE: 
      {
        storeRC = field->store(*(double*)bufPtr);
      }
      break;
    case MYSQL_TYPE_LONGLONG:
      {
        char temp[23];
        if (((Field_num*)field)->unsigned_flag)
        {
          temp[0] = (bcdGet(bufPtr, 21) == 0xD ? '-' : ' ');
          int strPos=1;
          int bcdPos=0;

          for (;bcdPos <= 20; bcdPos++, strPos++)
          {
            temp[strPos] = bcdGet(bufPtr, bcdPos) + '0';
          }
          
          temp[strPos] = 0;
          
          storeRC = field->store(temp, strPos, &my_charset_latin1);
        }
        else
        {
          storeRC = field->store(*(int64*)bufPtr, FALSE);
        }
      }
      break;
    case MYSQL_TYPE_INT24:
      {
        storeRC = field->store(*(int32*)bufPtr, ((Field_num*)field)->unsigned_flag);
      }
      break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE: 
      {
        longlong value= a4toi_ebcdic((uchar*)bufPtr) * 10000 +
                        a2toi_ebcdic((uchar*)bufPtr+5) * 100 +
                        a2toi_ebcdic((uchar*)bufPtr+8);

        if (cachedZeroDateOption == SUBSTITUTE_0001_01_01 &&
            value == (10000 + 100 + 1))
          value = 0;
        
        storeRC = field->store(value);
      }
      break;
    case MYSQL_TYPE_TIME: 
      {
        if (db2Field.getType() == QMY_TIME)
        {
          longlong value= a2toi_ebcdic((uchar*)bufPtr) * 10000 +
                          a2toi_ebcdic((uchar*)bufPtr+3) * 100 +
                          a2toi_ebcdic((uchar*)bufPtr+6);

          storeRC = field->store(value);            
        }
        else
          storeRC = field->store(*((int32*)bufPtr));
      }
      break;
    case MYSQL_TYPE_TIMESTAMP: 
    case MYSQL_TYPE_DATETIME: 
      {
        longlong value= (a4toi_ebcdic((uchar*)bufPtr) * 10000 +
                         a2toi_ebcdic((uchar*)bufPtr+5) * 100 +
                         a2toi_ebcdic((uchar*)bufPtr+8)) * 1000000LL +
                        (a2toi_ebcdic((uchar*)bufPtr+11) * 10000 +
                         a2toi_ebcdic((uchar*)bufPtr+14) * 100 +
                         a2toi_ebcdic((uchar*)bufPtr+17));
        
        if (cachedZeroDateOption == SUBSTITUTE_0001_01_01 &&
            value == (10000 + 100 + 1) * 1000000LL)
          value = 0;

        storeRC = field->store(value);
      }
      break;
    case MYSQL_TYPE_YEAR: 
      {
        if (db2Field.getType() == QMY_CHAR)
        {
          storeRC = field->store(bufPtr, 4, &my_charset_bin);
        }
        else
        {
          storeRC = field->store(*((uint16*)bufPtr));
        }
      }
      break;
    case MYSQL_TYPE_BIT:
      {      
        uint64 temp= 0;
        int bytesToCopy= db2Field.getByteLengthInRecord();
        memcpy(((char*)&temp) + (sizeof(temp) - bytesToCopy), bufPtr, bytesToCopy);
        storeRC = field->store(temp, TRUE);
      }
      break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_BLOB:
      {
        if (field->real_type() == MYSQL_TYPE_ENUM ||
            field->real_type() == MYSQL_TYPE_SET) 
        {
          storeRC = field->store(*(int64*)bufPtr);
        }
        else
        {

          const char* dataToStore = NULL;
          uint32 bytesToStore = 0;
          CHARSET_INFO* fieldCharSet = field->charset();
          switch(db2Field.getType())
          {
            case QMY_CHAR:
            case QMY_GRAPHIC: 
              {
                bytesToStore = db2Field.getByteLengthInRecord();
                if (bytesToStore == 0)
                  bytesToStore = 1;
                dataToStore = bufPtr;              
              }
              break;
            case QMY_VARCHAR:
               {
                bytesToStore = *(uint16*)bufPtr;
                dataToStore = bufPtr+sizeof(uint16);
              }
              break;
            case QMY_VARGRAPHIC:
               {
                /* For VARGRAPHIC, convert the number of double-byte characters
                   to the number of bytes.                                       */
                bytesToStore = (*(uint16*)bufPtr)*2;
                dataToStore = bufPtr+sizeof(uint16);
              }
              break;
            case QMY_DBCLOB: 
            case QMY_BLOBCLOB:
              {
                DB2LobField* lobField = (DB2LobField* )(bufPtr + db2Field.calcBlobPad());
                bytesToStore = lobField->length * (db2Field.getType() == QMY_DBCLOB ? 2 : 1);
                dataToStore = (char*)blobReadBuffers->getBufferPtr(field->field_index);              
              }
              break;

          }

          if ((fieldCharSet != &my_charset_bin) && // not binary &
              (db2Field.getCCSID() != 13488) && // not UCS2 &
              (db2Field.getCCSID() != 1208))
          {
            char* temp;
            size_t db2BytesToStore;
            int rc;
            if (fieldCharSet->mbmaxlen > 1)
            {
              size_t maxDb2BytesToStore = ((bytesToStore / 2) * fieldCharSet->mbmaxlen); // Worst case for number of bytes
              temp = getCharacterConversionBuffer(field->field_index, maxDb2BytesToStore);
              rc = convertFieldChars(toMySQL, field->field_index, dataToStore, temp, bytesToStore, maxDb2BytesToStore, &db2BytesToStore);
              bytesToStore = db2BytesToStore;
            }
            else // single-byte ASCII to EBCDIC 
            {
              temp = getCharacterConversionBuffer(field->field_index, bytesToStore);
              rc = convertFieldChars(toMySQL, field->field_index, dataToStore, temp, bytesToStore, bytesToStore, NULL);
            }
            if (rc)
              return (rc);
            dataToStore = temp;
          }

          if ((field)->flags & BLOB_FLAG)
            ((Field_blob*)(field))->set_ptr(bytesToStore, (uchar*)dataToStore);
          else
            storeRC = field->store(dataToStore, bytesToStore, &my_charset_bin);
        }
      }
      break;
    default:
      DBUG_ASSERT(0);
      break;

  }
  
  if (storeRC)
  {
    invalidDataFound = true;
  }

  return 0;
}
