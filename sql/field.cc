/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/*****************************************************************************
** This file implements classes defined in field.h
*****************************************************************************/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include <m_ctype.h>
#include <errno.h>
#ifdef HAVE_FCONVERT
#include <floatingpoint.h>
#endif

// Maximum allowed exponent value for converting string to decimal
#define MAX_EXPONENT 1024

/*****************************************************************************
  Instansiate templates and static variables
*****************************************************************************/

#ifdef __GNUC__
template class List<create_field>;
template class List_iterator<create_field>;
#endif

uchar Field_null::null[1]={1};
const char field_separator=',';

/*****************************************************************************
  Static help functions
*****************************************************************************/

void Field_num::prepend_zeros(String *value)
{
  int diff;
  if ((diff= (int) (field_length - value->length())) > 0)
  {
    bmove_upp((char*) value->ptr()+field_length,value->ptr()+value->length(),
	      value->length());
    bfill((char*) value->ptr(),diff,'0');
    value->length(field_length);
    (void) value->c_ptr_quick();		// Avoid warnings in purify
  }
}

/*
  Test if given number is a int (or a fixed format float with .000)

  SYNOPSIS
    test_if_int()
    str		String to test
    end		Pointer to char after last used digit
    cs		Character set

  NOTES
    This is called after one has called my_strntol() or similar function.
    This is only used to give warnings in ALTER TABLE or LOAD DATA...

  TODO
    Make this multi-byte-character safe

  RETURN
    0	ok
    1	error
*/

bool test_if_int(const char *str, int length, const char *int_end,
		 CHARSET_INFO *cs)
{
  if (str == int_end)
    return 0;					// Empty string
  const char *end=str+length;
  if ((str= int_end) == end)
    return 1;					// All digits was used

  /* Allow end .0000 */
  if (*str == '.')
  {
    for (str++ ; str != end && *str == '0'; str++) ;
  }
  /* Allow end space */
  for (str++ ; str != end ; str++)
  {
    if (!my_isspace(cs,*str))
      return 0;
  }
  return 1;
}


static bool test_if_real(const char *str,int length, CHARSET_INFO *cs)
{
  cs= system_charset_info; // QQ move test_if_real into CHARSET_INFO struct

  while (length && my_isspace(cs,*str))
  {						// Allow start space
    length--; str++;
  }
  if (!length)
    return 0;
  if (*str == '+' || *str == '-')
  {
    length--; str++;
    if (!length || !(my_isdigit(cs,*str) || *str == '.'))
      return 0;
  }
  while (length && my_isdigit(cs,*str))
  {
    length--; str++;
  }
  if (!length)
    return 1;
  if (*str == '.')
  {
    length--; str++;
    while (length && my_isdigit(cs,*str))
    {
      length--; str++;
    }
  }
  if (!length)
    return 1;
  if (*str == 'E' || *str == 'e')
  {
    if (length < 3 || (str[1] != '+' && str[1] != '-') || 
        !my_isdigit(cs,str[2]))
      return 0;
    length-=3;
    str+=3;
    while (length && my_isdigit(cs,*str))
    {
      length--; str++;
    }
  }
  for (; length ; length--, str++)
  {						// Allow end space
    if (!my_isspace(cs,*str))
      return 0;
  }
  return 1;
}


static inline uint field_length_without_space(const char *ptr, uint length)
{
  const char *end= ptr+length;
  while (end > ptr && end[-1] == ' ')
    end--;
  return (uint) (end-ptr);
}

/*
 Tables of filed type compatibility.

 There are tables for every type, table consist of list of types in which
 given type can be converted without data lost, list should be ended with
 FIELD_CAST_STOP
*/
static Field::field_cast_enum field_cast_decimal[]=
{Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_tiny[]=
{Field::FIELD_CAST_SHORT, Field::FIELD_CAST_MEDIUM, Field::FIELD_CAST_LONG,
 Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_FLOAT, Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_short[]=
{Field::FIELD_CAST_MEDIUM, Field::FIELD_CAST_LONG, Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_FLOAT, Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_medium[]=
{Field::FIELD_CAST_LONG, Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_long[]=
{Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_longlong[]=
{Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_float[]=
{Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_double[]=
{Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_null[]=
{Field::FIELD_CAST_DECIMAL, Field::FIELD_CAST_TINY, Field::FIELD_CAST_SHORT,
 Field::FIELD_CAST_MEDIUM, Field::FIELD_CAST_LONG, Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_FLOAT, Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_TIMESTAMP, Field::FIELD_CAST_YEAR,
 Field::FIELD_CAST_DATE, Field::FIELD_CAST_NEWDATE,
 Field::FIELD_CAST_TIME, Field::FIELD_CAST_DATETIME,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB,
 Field::FIELD_CAST_GEOM, Field::FIELD_CAST_ENUM, Field::FIELD_CAST_SET,
 Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_timestamp[]=
{Field::FIELD_CAST_DATETIME,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_year[]=
{Field::FIELD_CAST_SHORT, Field::FIELD_CAST_MEDIUM, Field::FIELD_CAST_LONG,
 Field::FIELD_CAST_LONGLONG,
 Field::FIELD_CAST_FLOAT, Field::FIELD_CAST_DOUBLE,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_date[]=
{Field::FIELD_CAST_DATETIME,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_newdate[]=
{Field::FIELD_CAST_DATETIME,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_time[]=
{Field::FIELD_CAST_DATETIME,
 Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_datetime[]=
{Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_string[]=
{Field::FIELD_CAST_VARSTRING, Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_varstring[]=
{Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_blob[]=
{Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_geom[]=
{Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_enum[]=
{Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
static Field::field_cast_enum field_cast_set[]=
{Field::FIELD_CAST_STRING, Field::FIELD_CAST_VARSTRING,
 Field::FIELD_CAST_BLOB, Field::FIELD_CAST_STOP};
// Array of pointers on conversion table for all fields types casting
static Field::field_cast_enum *field_cast_array[]=
{0, //FIELD_CAST_STOP
 field_cast_decimal, field_cast_tiny, field_cast_short,
 field_cast_medium, field_cast_long, field_cast_longlong,
 field_cast_float, field_cast_double,
 field_cast_null,
 field_cast_timestamp, field_cast_year, field_cast_date, field_cast_newdate,
 field_cast_time, field_cast_datetime,
 field_cast_string, field_cast_varstring, field_cast_blob,
 field_cast_geom, field_cast_enum, field_cast_set
};


bool Field::field_cast_compatible(Field::field_cast_enum type)
{
  DBUG_ASSERT(type != FIELD_CAST_STOP);
  Field::field_cast_enum *array= field_cast_array[field_cast_type()];
  uint i= 0;
  Field::field_cast_enum tp;
  do
  {
    tp=  array[i++];
    if (tp == type)
      return 1;
  } while (tp != FIELD_CAST_STOP);
  return 0;
}


/****************************************************************************
** Functions for the base classes
** This is an unpacked number.
****************************************************************************/

Field::Field(char *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
  :ptr(ptr_arg),null_ptr(null_ptr_arg),
   table(table_arg),table_name(table_arg ? table_arg->table_name : 0),
   field_name(field_name_arg),
   query_id(0), key_start(0), part_of_key(0), part_of_sortkey(0),
   unireg_check(unireg_check_arg),
   field_length(length_arg),null_bit(null_bit_arg),abs_offset(0)
{
  flags=null_ptr ? 0: NOT_NULL_FLAG;
  comment.str= (char*) "";
  comment.length=0;
}

uint Field::offset()
{
  return (uint) (ptr - (char*) table->record[0]);
}


void Field::copy_from_tmp(int row_offset)
{
  memcpy(ptr,ptr+row_offset,pack_length());
  if (null_ptr)
  {
    *null_ptr= (uchar) ((null_ptr[0] & (uchar) ~(uint) null_bit) |
			null_ptr[row_offset] & (uchar) null_bit);
  }
}


bool Field::send_binary(Protocol *protocol)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),charset());
  val_str(&tmp,&tmp);
  return protocol->store(tmp.ptr(), tmp.length(), tmp.charset());
}


void Field_num::add_zerofill_and_unsigned(String &res) const
{
  if (unsigned_flag)
    res.append(" unsigned");
  if (zerofill)
    res.append(" zerofill");
}

void Field_num::make_field(Send_field *field)
{
  /* table_cache_key is not set for temp tables */
  field->db_name=table->table_cache_key ? table->table_cache_key : ""; 
  field->org_table_name=table->real_name;
  field->table_name=table_name;
  field->col_name=field->org_col_name=field_name;
  field->charsetnr= charset()->number;
  field->length=field_length;
  field->type=type();
  field->flags=table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals=dec;
}


void Field_str::make_field(Send_field *field)
{
  /* table_cache_key is not set for temp tables */
  field->db_name=table->table_cache_key ? table->table_cache_key : ""; 
  field->org_table_name=table->real_name;
  field->table_name=table_name;
  field->col_name=field->org_col_name=field_name;
  field->charsetnr= charset()->number;
  field->length=field_length;
  field->type=type();
  field->flags=table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals=0;
}


uint Field::fill_cache_field(CACHE_FIELD *copy)
{
  copy->str=ptr;
  copy->length=pack_length();
  copy->blob_field=0;
  if (flags & BLOB_FLAG)
  {
    copy->blob_field=(Field_blob*) this;
    copy->strip=0;
    copy->length-=table->blob_ptr_size;
    return copy->length;
  }
  else if (!zero_pack() && (type() == FIELD_TYPE_STRING && copy->length > 4 ||
			    type() == FIELD_TYPE_VAR_STRING))
    copy->strip=1;				/* Remove end space */
  else
    copy->strip=0;
  return copy->length+(int) copy->strip;
}


bool Field::get_date(TIME *ltime,uint fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),tmp2,*res;
  if (!(res=val_str(&tmp,&tmp2)) ||
      str_to_TIME(res->ptr(),res->length(),ltime,fuzzydate) <=
      TIMESTAMP_DATETIME_ERROR)
    return 1;
  return 0;
}

bool Field::get_time(TIME *ltime)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),tmp2,*res;
  if (!(res=val_str(&tmp,&tmp2)) ||
      str_to_time(res->ptr(),res->length(),ltime))
    return 1;
  return 0;
}

/*
  This is called when storing a date in a string

  NOTES
    Needs to be changed if/when we want to support different time formats
*/

void Field::store_time(TIME *ltime,timestamp_type type)
{
  char buff[25];
  switch (type)  {
  case TIMESTAMP_NONE:
  case TIMESTAMP_DATETIME_ERROR:
    store("",0,&my_charset_bin);	// Probably an error
    break;
  case TIMESTAMP_DATE:
    sprintf(buff,"%04d-%02d-%02d", ltime->year,ltime->month,ltime->day);
    store(buff,10,&my_charset_bin);
    break;
  case TIMESTAMP_DATETIME:
    sprintf(buff,"%04d-%02d-%02d %02d:%02d:%02d",
	    ltime->year,ltime->month,ltime->day,
	    ltime->hour,ltime->minute,ltime->second);
    store(buff,19,&my_charset_bin);
    break;
  case TIMESTAMP_TIME:
  {
    ulong length= my_sprintf(buff, (buff, "%02d:%02d:%02d",
				    ltime->hour,ltime->minute,ltime->second));
    store(buff,(uint) length, &my_charset_bin);
    break;
  }
  }
}


bool Field::optimize_range(uint idx)
{
  return !test(table->file->index_flags(idx) & HA_WRONG_ASCII_ORDER);
}

/****************************************************************************
  Field_null, a field that always return NULL
****************************************************************************/

void Field_null::sql_type(String &res) const
{
  res.set_ascii("null", 4);
}


/****************************************************************************
  Functions for the Field_decimal class
  This is an number stored as a pre-space (or pre-zero) string
****************************************************************************/

void
Field_decimal::reset(void)
{
  Field_decimal::store("0",1,&my_charset_bin);
}

void Field_decimal::overflow(bool negative)
{
  uint len=field_length;
  char *to=ptr, filler= '9';

  set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
  if (negative)
  {
    if (!unsigned_flag)
    {
      /* Put - sign as a first digit so we'll have -999..999 or 999..999 */
      *to++ = '-';
      len--;
    }
    else
    {
      filler= '0';				// Fill up with 0
      if (!zerofill)
      {
	/*
	  Handle unsigned integer without zerofill, in which case
	  the number should be of format '   0' or '   0.000'
	*/
	uint whole_part=field_length- (dec ? dec+2 : 1);
	// Fill with spaces up to the first digit
	bfill(to, whole_part, ' ');
	to+=  whole_part;
	len-= whole_part;
	// The main code will also handle the 0 before the decimal point
      }
    }
  }
  bfill(to, len, filler);
  if (dec)
    ptr[field_length-dec-1]='.';
  return;
}


int Field_decimal::store(const char *from, uint len, CHARSET_INFO *cs)
{
  char buff[80];
  String tmp(buff,sizeof(buff), &my_charset_bin);

  /* Convert character set if the old one is multi byte */
  if (cs->mbmaxlen > 1)
  { 
    tmp.copy(from, len, cs, &my_charset_bin);
    from= tmp.ptr();
    len=  tmp.length();
  }

  const char *end= from+len;
  /* The pointer where the field value starts (i.e., "where to write") */
  char *to=ptr;
  uint tmp_dec, tmp_uint;
  /*
    The sign of the number : will be 0 (means positive but sign not
    specified), '+' or '-'
  */
  char sign_char=0;
  /* The pointers where prezeros start and stop */
  const char *pre_zeros_from, *pre_zeros_end;
  /* The pointers where digits at the left of '.' start and stop */
  const char *int_digits_from, *int_digits_end;
  /* The pointers where digits at the right of '.' start and stop */
  const char *frac_digits_from, *frac_digits_end;
  /* The sign of the exponent : will be 0 (means no exponent), '+' or '-' */
  char expo_sign_char=0;
  uint exponent=0;                                // value of the exponent
  /*
    Pointers used when digits move from the left of the '.' to the
    right of the '.' (explained below)
  */
  const char *int_digits_tail_from;
  /* Number of 0 that need to be added at the left of the '.' (1E3: 3 zeros) */
  uint int_digits_added_zeros;
  /*
    Pointer used when digits move from the right of the '.' to the left
    of the '.'
  */
  const char *frac_digits_head_end;
  /* Number of 0 that need to be added at the right of the '.' (for 1E-3) */
  uint frac_digits_added_zeros;
  char *pos,*tmp_left_pos,*tmp_right_pos;
  /* Pointers that are used as limits (begin and end of the field buffer) */
  char *left_wall,*right_wall;
  char tmp_char;
  /*
    To remember if current_thd->cuted_fields has already been incremented,
    to do that only once
  */
  bool is_cuted_fields_incr=0;

  LINT_INIT(int_digits_tail_from);
  LINT_INIT(int_digits_added_zeros);
  LINT_INIT(frac_digits_head_end);
  LINT_INIT(frac_digits_added_zeros);

  /*
    There are three steps in this function :
    - parse the input string
    - modify the position of digits around the decimal dot '.' 
      according to the exponent value (if specified)
    - write the formatted number
  */

  if ((tmp_dec=dec))
    tmp_dec++;

  /* skip pre-space */
  while (from != end && my_isspace(&my_charset_bin,*from))
    from++;
  if (from == end)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    is_cuted_fields_incr=1;
  }
  else if (*from == '+' || *from == '-')	// Found some sign ?
  {
    sign_char= *from++;
    /*
      We allow "+" for unsigned decimal unless defined different
      Both options allowed as one may wish not to have "+" for unsigned numbers
      because of data processing issues
    */ 
    if (unsigned_flag)  
    { 
      if (sign_char=='-')
      {
        Field_decimal::overflow(1);
        return 1;
      }
      /* 
	 Defining this will not store "+" for unsigned decimal type even if
	 it is passed in numeric string. This will make some tests to fail
      */	 
#ifdef DONT_ALLOW_UNSIGNED_PLUS      
      else 
        sign_char=0;
#endif 	
    }
  }

  pre_zeros_from= from;
  for (; from!=end && *from == '0'; from++) ;	// Read prezeros
  pre_zeros_end=int_digits_from=from;      
  /* Read non zero digits at the left of '.'*/
  for (; from != end && my_isdigit(&my_charset_bin, *from) ; from++) ;
  int_digits_end=from;
  if (from!=end && *from == '.')		// Some '.' ?
    from++;
  frac_digits_from= from;
  /* Read digits at the right of '.' */
  for (;from!=end && my_isdigit(&my_charset_bin, *from); from++) ;
  frac_digits_end=from;
  // Some exponentiation symbol ?
  if (from != end && (*from == 'e' || *from == 'E'))
  {   
    from++;
    if (from != end && (*from == '+' || *from == '-'))  // Some exponent sign ?
      expo_sign_char= *from++;
    else
      expo_sign_char= '+';
    /*
      Read digits of the exponent and compute its value.  We must care about
      'exponent' overflow, because as unsigned arithmetic is "modulo", big 
      exponents will become small (e.g. 1e4294967296 will become 1e0, and the 
      field will finally contain 1 instead of its max possible value).
    */
    for (;from!=end && my_isdigit(&my_charset_bin, *from); from++)
    {
      exponent=10*exponent+(*from-'0');
      if (exponent>MAX_EXPONENT)
        break;
    }
  }
  
  /*
    We only have to generate warnings if count_cuted_fields is set.
    This is to avoid extra checks of the number when they are not needed.
    Even if this flag is not set, it's ok to increment warnings, if
    it makes the code easer to read.
  */

  if (current_thd->count_cuted_fields)
  {
    // Skip end spaces
    for (;from != end && my_isspace(&my_charset_bin, *from); from++) ;
    if (from != end)                     // If still something left, warn
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED); 
      is_cuted_fields_incr=1;
    }
  }
  
  /*
    Now "move" digits around the decimal dot according to the exponent value,
    and add necessary zeros.
    Examples :
    - 1E+3 : needs 3 more zeros at the left of '.' (int_digits_added_zeros=3)
    - 1E-3 : '1' moves at the right of '.', and 2 more zeros are needed
    between '.' and '1'
    - 1234.5E-3 : '234' moves at the right of '.'
    These moves are implemented with pointers which point at the begin
    and end of each moved segment. Examples :
    - 1234.5E-3 : before the code below is executed, the int_digits part is
    from '1' to '4' and the frac_digits part from '5' to '5'. After the code
    below, the int_digits part is from '1' to '1', the frac_digits_head
    part is from '2' to '4', and the frac_digits part from '5' to '5'.
    - 1234.5E3 : before the code below is executed, the int_digits part is
    from '1' to '4' and the frac_digits part from '5' to '5'. After the code
    below, the int_digits part is from '1' to '4', the int_digits_tail
    part is from '5' to '5', the frac_digits part is empty, and
    int_digits_added_zeros=2 (to make 1234500).
  */
  
  /* 
     Below tmp_uint cannot overflow with small enough MAX_EXPONENT setting,
     as int_digits_added_zeros<=exponent<4G and 
     (int_digits_end-int_digits_from)<=max_allowed_packet<=2G and
     (frac_digits_from-int_digits_tail_from)<=max_allowed_packet<=2G
  */

  if (!expo_sign_char)
    tmp_uint=tmp_dec+(uint)(int_digits_end-int_digits_from);
  else if (expo_sign_char == '-') 
  {
    tmp_uint=min(exponent,(uint)(int_digits_end-int_digits_from));
    frac_digits_added_zeros=exponent-tmp_uint;
    int_digits_end -= tmp_uint;
    frac_digits_head_end=int_digits_end+tmp_uint;
    tmp_uint=tmp_dec+(uint)(int_digits_end-int_digits_from);     
  }
  else // (expo_sign_char=='+') 
  {
    tmp_uint=min(exponent,(uint)(frac_digits_end-frac_digits_from));
    int_digits_added_zeros=exponent-tmp_uint;
    int_digits_tail_from=frac_digits_from;
    frac_digits_from=frac_digits_from+tmp_uint;
    /*
      We "eat" the heading zeros of the 
      int_digits.int_digits_tail.int_digits_added_zeros concatenation
      (for example 0.003e3 must become 3 and not 0003)
    */
    if (int_digits_from == int_digits_end) 
    {
      /*
	There was nothing in the int_digits part, so continue
	eating int_digits_tail zeros
      */
      for (; int_digits_tail_from != frac_digits_from &&
	     *int_digits_tail_from == '0'; int_digits_tail_from++) ;
      if (int_digits_tail_from == frac_digits_from) 
      {
	// there were only zeros in int_digits_tail too
	int_digits_added_zeros=0;
      }
    }
    tmp_uint= (tmp_dec+(int_digits_end-int_digits_from)+
               (uint)(frac_digits_from-int_digits_tail_from)+
               int_digits_added_zeros);
  }
  
  /*
    Now write the formated number
    
    First the digits of the int_% parts.
    Do we have enough room to write these digits ?
    If the sign is defined and '-', we need one position for it
  */

  if (field_length < tmp_uint + (int) (sign_char == '-')) 
  {
    // too big number, change to max or min number
    Field_decimal::overflow(sign_char == '-');
    return 1;
  }
 
  /*
    Tmp_left_pos is the position where the leftmost digit of
    the int_% parts will be written
  */
  tmp_left_pos=pos=to+(uint)(field_length-tmp_uint);
  
  // Write all digits of the int_% parts
  while (int_digits_from != int_digits_end)
    *pos++ = *int_digits_from++ ;

  if (expo_sign_char == '+')
  {    
    while (int_digits_tail_from != frac_digits_from)
      *pos++= *int_digits_tail_from++;
    while (int_digits_added_zeros-- >0)
      *pos++= '0';  
  }
  /*
    Note the position where the rightmost digit of the int_% parts has been
    written (this is to later check if the int_% parts contained nothing,
    meaning an extra 0 is needed).
  */
  tmp_right_pos=pos;

  /*
    Step back to the position of the leftmost digit of the int_% parts,
    to write sign and fill with zeros or blanks or prezeros.
  */
  pos=tmp_left_pos-1;
  if (zerofill)
  {
    left_wall=to-1;
    while (pos != left_wall)			// Fill with zeros
      *pos--='0';
  }
  else
  {
    left_wall=to+(sign_char != 0)-1;
    if (!expo_sign_char)	// If exponent was specified, ignore prezeros
    {
      for (;pos != left_wall && pre_zeros_from !=pre_zeros_end;
	   pre_zeros_from++)
	*pos--= '0';
    }
    if (pos == tmp_right_pos-1)
      *pos--= '0';		// no 0 has ever been written, so write one
    left_wall= to-1;
    if (sign_char && pos != left_wall)
    {
      /* Write sign if possible (it is if sign is '-') */
      *pos--= sign_char;
    }
    while (pos != left_wall)
      *pos--=' ';  //fill with blanks
  }
  
  /*
    Write digits of the frac_% parts ;
    Depending on current_thd->count_cutted_fields, we may also want
    to know if some non-zero tail of these parts will
    be truncated (for example, 0.002->0.00 will generate a warning,
    while 0.000->0.00 will not)
    (and 0E1000000000 will not, while 1E-1000000000 will)
  */
      
  pos=to+(uint)(field_length-tmp_dec);	// Calculate post to '.'
  right_wall=to+field_length;
  if (pos != right_wall) 
    *pos++='.';

  if (expo_sign_char == '-')
  {
    while (frac_digits_added_zeros-- > 0)
    {
      if (pos == right_wall) 
      {
        if (current_thd->count_cuted_fields && !is_cuted_fields_incr) 
          break; // Go on below to see if we lose non zero digits
        return 0;
      }
      *pos++='0';
    }
    while (int_digits_end != frac_digits_head_end)
    {
      tmp_char= *int_digits_end++;
      if (pos == right_wall)
      {
        if (tmp_char != '0')			// Losing a non zero digit ?
        {
          if (!is_cuted_fields_incr)
            set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
          return 0;
        }
        continue;
      }
      *pos++= tmp_char;
    }
  }

  for (;frac_digits_from!=frac_digits_end;) 
  {
    tmp_char= *frac_digits_from++;
    if (pos == right_wall)
    {
      if (tmp_char != '0')			// Losing a non zero digit ?
      {
        if (!is_cuted_fields_incr)
	  set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
        return 0;
      }
      continue;
    }
    *pos++= tmp_char;
  }
      
  while (pos != right_wall)
   *pos++='0';			// Fill with zeros at right of '.'
  return 0;
}


int Field_decimal::store(double nr)
{
  if (unsigned_flag && nr < 0)
  {
    overflow(1);
    return 1;
  }
  
#ifdef HAVE_FINITE
  if (!finite(nr)) // Handle infinity as special case
  {
    overflow(nr < 0.0);
    return 1;
  }
#endif

  reg4 uint i,length;
  char fyllchar,*to;
  char buff[320];

  fyllchar = zerofill ? (char) '0' : (char) ' ';
#ifdef HAVE_SNPRINTF
  buff[sizeof(buff)-1]=0;			// Safety
  snprintf(buff,sizeof(buff)-1, "%.*f",(int) dec,nr);
  length=(uint) strlen(buff);
#else
  length=(uint) my_sprintf(buff,(buff,"%.*f",dec,nr));
#endif

  if (length > field_length)
  {
    overflow(nr < 0.0);
    return 1;
  }
  else
  {
    to=ptr;
    for (i=field_length-length ; i-- > 0 ;)
      *to++ = fyllchar;
    memcpy(to,buff,length);
    return 0;
  }
}


int Field_decimal::store(longlong nr)
{
  if (unsigned_flag && nr < 0)
  {
    overflow(1);
    return 1;
  }
  char buff[22];
  uint length=(uint) (longlong10_to_str(nr,buff,-10)-buff);
  uint int_part=field_length- (dec  ? dec+1 : 0);

  if (length > int_part)
  {
    overflow(test(nr < 0L));			/* purecov: inspected */
    return 1;
  }
  else
  {
    char fyllchar = zerofill ? (char) '0' : (char) ' ';
    char *to=ptr;
    for (uint i=int_part-length ; i-- > 0 ;)
      *to++ = fyllchar;
    memcpy(to,buff,length);
    if (dec)
    {
      to[length]='.';
      bfill(to+length+1,dec,'0');
    }
    return 0;
  }
}


double Field_decimal::val_real(void)
{
  int not_used;
  return my_strntod(&my_charset_bin, ptr, field_length, NULL, &not_used);
}

longlong Field_decimal::val_int(void)
{
  int not_used;
  if (unsigned_flag)
    return my_strntoull(&my_charset_bin, ptr, field_length, 10, NULL,
			&not_used);
  else
    return my_strntoll(&my_charset_bin, ptr, field_length, 10, NULL,
			&not_used);
}


String *Field_decimal::val_str(String *val_buffer __attribute__((unused)),
			       String *val_ptr)
{
  char *str;
  for (str=ptr ; *str == ' ' ; str++) ;
  uint tmp_length=(uint) (str-ptr);
  val_ptr->set_charset(&my_charset_bin);
  if (field_length < tmp_length)		// Error in data
    val_ptr->length(0);
  else
    val_ptr->set_ascii((const char*) str, field_length-tmp_length);
  return val_ptr;
}

/*
** Should be able to handle at least the following fixed decimal formats:
** 5.00 , -1.0,  05,  -05, +5 with optional pre/end space
*/

int Field_decimal::cmp(const char *a_ptr,const char *b_ptr)
{
  const char *end;
  int swap=0;
  /* First remove prefixes '0', ' ', and '-' */
  for (end=a_ptr+field_length;
       a_ptr != end &&
	 (*a_ptr == *b_ptr ||
	  ((my_isspace(&my_charset_bin,*a_ptr)  || *a_ptr == '+' || 
            *a_ptr == '0') &&
	   (my_isspace(&my_charset_bin,*b_ptr) || *b_ptr == '+' || 
            *b_ptr == '0')));
       a_ptr++,b_ptr++)
  {
    if (*a_ptr == '-')				// If both numbers are negative
      swap= -1 ^ 1;				// Swap result      
  }
  if (a_ptr == end)
    return 0;
  if (*a_ptr == '-')
    return -1;
  else if (*b_ptr == '-')
    return 1;

  while (a_ptr != end)
  {
    if (*a_ptr++ != *b_ptr++)
      return swap ^ (a_ptr[-1] < b_ptr[-1] ? -1 : 1); // compare digits
  }
  return 0;
}


void Field_decimal::sort_string(char *to,uint length)
{
  char *str,*end;
  for (str=ptr,end=ptr+length;
       str != end &&
	 ((my_isspace(&my_charset_bin,*str) || *str == '+' ||
	   *str == '0')) ;
       str++)
    *to++=' ';
  if (str == end)
    return;					/* purecov: inspected */

  if (*str == '-')
  {
    *to++=1;					// Smaller than any number
    str++;
    while (str != end)
      if (my_isdigit(&my_charset_bin,*str))
	*to++= (char) ('9' - *str++);
      else
	*to++= *str++;
  }
  else memcpy(to,str,(uint) (end-str));
}


void Field_decimal::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  uint tmp=field_length;
  if (!unsigned_flag)
    tmp--;
  if (dec)
    tmp--;
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "decimal(%d,%d)",tmp,dec));
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
** tiny int
****************************************************************************/

int Field_tiny::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int not_used;				// We can ignore result from str2int
  char *end;
  long tmp= my_strntol(cs, from, len, 10, &end, &not_used);
  int error= 0;

  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0; /* purecov: inspected */
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);      
      error= 1;
    }
    else if (tmp > 255)
    {
      tmp= 255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }
  else
  {
    if (tmp < -128)
    {
      tmp= -128;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (tmp >= 128)
    {
      tmp= 127;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }
  ptr[0]= (char) tmp;
  return error;
}


int Field_tiny::store(double nr)
{
  int error= 0;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0.0)
    {
      *ptr=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > 255.0)
    {
      *ptr=(char) 255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < -128.0)
    {
      *ptr= (char) -128;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > 127.0)
    {
      *ptr=127;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  return error;
}

int Field_tiny::store(longlong nr)
{
  int error= 0;
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      *ptr=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > 255L)
    {
      *ptr= (char) 255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < -128L)
    {
      *ptr= (char) -128;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > 127L)
    {
      *ptr=127;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  return error;
}


double Field_tiny::val_real(void)
{
  int tmp= unsigned_flag ? (int) ((uchar*) ptr)[0] :
    (int) ((signed char*) ptr)[0];
  return (double) tmp;
}

longlong Field_tiny::val_int(void)
{
  int tmp= unsigned_flag ? (int) ((uchar*) ptr)[0] :
    (int) ((signed char*) ptr)[0];
  return (longlong) tmp;
}

String *Field_tiny::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,5*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();

  if (unsigned_flag)
    length= (uint) cs->cset->long10_to_str(cs,to,mlength, 10,
					   (long) *((uchar*) ptr));
  else
    length= (uint) cs->cset->long10_to_str(cs,to,mlength,-10,
					   (long) *((signed char*) ptr));
  
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}

bool Field_tiny::send_binary(Protocol *protocol)
{
  return protocol->store_tiny((longlong) (int8) ptr[0]);
}

int Field_tiny::cmp(const char *a_ptr, const char *b_ptr)
{
  signed char a,b;
  a=(signed char) a_ptr[0]; b= (signed char) b_ptr[0];
  if (unsigned_flag)
    return ((uchar) a < (uchar) b) ? -1 : ((uchar) a > (uchar) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_tiny::sort_string(char *to,uint length __attribute__((unused)))
{
  if (unsigned_flag)
    *to= *ptr;
  else
    to[0] = (char) ((uchar) ptr[0] ^ (uchar) 128);	/* Revers signbit */
}

void Field_tiny::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "tinyint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
 Field type short int (2 byte)
****************************************************************************/

int Field_short::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int not_used;				// We can ignore result from str2int
  char *end;
  long tmp= my_strntol(cs, from, len, 10, &end, &not_used);
  int error= 0;

  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (tmp > (uint16) ~0)
    {
      tmp=(uint16) ~0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }
  else
  {
    if (tmp < INT_MIN16)
    {
      tmp= INT_MIN16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (tmp > INT_MAX16)
    {
      tmp=INT_MAX16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int2store(ptr,tmp);
  }
  else
#endif
    shortstore(ptr,(short) tmp);
  return error;
}


int Field_short::store(double nr)
{
  int error= 0;
  int16 res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (double) (uint16) ~0)
    {
      res=(int16) (uint16) ~0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int16) (uint16) nr;
  }
  else
  {
    if (nr < (double) INT_MIN16)
    {
      res=INT_MIN16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (double) INT_MAX16)
    {
      res=INT_MAX16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int16) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}

int Field_short::store(longlong nr)
{
  int error= 0;
  int16 res;
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (longlong) (uint16) ~0)
    {
      res=(int16) (uint16) ~0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int16) (uint16) nr;
  }
  else
  {
    if (nr < INT_MIN16)
    {
      res=INT_MIN16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > INT_MAX16)
    {
      res=INT_MAX16;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int16) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}


double Field_short::val_real(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (double) (unsigned short) j : (double) j;
}

longlong Field_short::val_int(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (longlong) (unsigned short) j : (longlong) j;
}


String *Field_short::val_str(String *val_buffer,
			     String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,7*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);

  if (unsigned_flag)
    length=(uint) cs->cset->long10_to_str(cs, to, mlength, 10, 
					  (long) (uint16) j);
  else
    length=(uint) cs->cset->long10_to_str(cs, to, mlength,-10, (long) j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_short::send_binary(Protocol *protocol)
{
  return protocol->store_short(Field_short::val_int());
}


int Field_short::cmp(const char *a_ptr, const char *b_ptr)
{
  short a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint2korr(a_ptr);
    b=sint2korr(b_ptr);
  }
  else
#endif
  {
    shortget(a,a_ptr);
    shortget(b,b_ptr);
  }

  if (unsigned_flag)
    return ((unsigned short) a < (unsigned short) b) ? -1 :
    ((unsigned short) a > (unsigned short) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_short::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[1];
    else
      to[0] = (char) (ptr[1] ^ 128);		/* Revers signbit */
    to[1]   = ptr[0];
  }
}

void Field_short::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "smallint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
  Field type medium int (3 byte)
****************************************************************************/

int Field_medium::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int not_used;				// We can ignore result from str2int
  char *end;
  long tmp= my_strntol(cs, from, len, 10, &end, &not_used);
  int error= 0;

  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (tmp >= (long) (1L << 24))
    {
      tmp=(long) (1L << 24)-1L;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }
  else
  {
    if (tmp < INT_MIN24)
    {
      tmp= INT_MIN24;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (tmp > INT_MAX24)
    {
      tmp=INT_MAX24;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      error= 1;
    }
  }

  int3store(ptr,tmp);
  return error;
}


int Field_medium::store(double nr)
{
  int error= 0;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      int3store(ptr,0);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr >= (double) (long) (1L << 24))
    {
      uint32 tmp=(uint32) (1L << 24)-1L;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      int3store(ptr,(uint32) nr);
  }
  else
  {
    if (nr < (double) INT_MIN24)
    {
      long tmp=(long) INT_MIN24;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (double) INT_MAX24)
    {
      long tmp=(long) INT_MAX24;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      int3store(ptr,(long) nr);
  }
  return error;
}

int Field_medium::store(longlong nr)
{
  int error= 0;
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      int3store(ptr,0);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr >= (longlong) (long) (1L << 24))
    {
      long tmp=(long) (1L << 24)-1L;;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      int3store(ptr,(uint32) nr);
  }
  else
  {
    if (nr < (longlong) INT_MIN24)
    {
      long tmp=(long) INT_MIN24;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (longlong) INT_MAX24)
    {
      long tmp=(long) INT_MAX24;
      int3store(ptr,tmp);
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      int3store(ptr,(long) nr);
  }
  return error;
}


double Field_medium::val_real(void)
{
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);
  return (double) j;
}


longlong Field_medium::val_int(void)
{
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);
  return (longlong) j;
}


String *Field_medium::val_str(String *val_buffer,
			      String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,10*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);

  length=(uint) cs->cset->long10_to_str(cs,to,mlength,-10,j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer); /* purecov: inspected */
  return val_buffer;
}


bool Field_medium::send_binary(Protocol *protocol)
{
  return protocol->store_long(Field_medium::val_int());
}


int Field_medium::cmp(const char *a_ptr, const char *b_ptr)
{
  long a,b;
  if (unsigned_flag)
  {
    a=uint3korr(a_ptr);
    b=uint3korr(b_ptr);
  }
  else
  {
    a=sint3korr(a_ptr);
    b=sint3korr(b_ptr);
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_medium::sort_string(char *to,uint length __attribute__((unused)))
{
  if (unsigned_flag)
    to[0] = ptr[2];
  else
    to[0] = (uchar) (ptr[2] ^ 128);		/* Revers signbit */
  to[1] = ptr[1];
  to[2] = ptr[0];
}


void Field_medium::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(), 
			  "mediumint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** long int
****************************************************************************/


int Field_long::store(const char *from,uint len,CHARSET_INFO *cs)
{
  long tmp;
  int error= 0;
  char *end;
  
  tmp= cs->cset->scan(cs, from, from+len, MY_SEQ_SPACES);
  len-= tmp;
  from+= tmp;
  my_errno=0;
  if (unsigned_flag)
  {
    if (!len || *from == '-')
    {
      tmp=0;					// Set negative to 0
      my_errno=ERANGE;
      error= 1;
    }
    else
      tmp=(long) my_strntoul(cs,from,len,10,&end,&error);
  }
  else
    tmp=my_strntol(cs,from,len,10,&end,&error);
  if (error ||
      (from+len != end && current_thd->count_cuted_fields &&
       !test_if_int(from,len,end,cs)))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    error= 1;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
  return error;
}


int Field_long::store(double nr)
{
  int error= 0;
  int32 res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (double) (ulong) ~0L)
    {
      res=(int32) (uint32) ~0L;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int32) (ulong) nr;
  }
  else
  {
    if (nr < (double) INT_MIN32)
    {
      res=(int32) INT_MIN32;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (double) INT_MAX32)
    {
      res=(int32) INT_MAX32;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int32) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,res);
  }
  else
#endif
    longstore(ptr,res);
  return error;
}


int Field_long::store(longlong nr)
{
  int error= 0;
  int32 res;
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr >= (LL(1) << 32))
    {
      res=(int32) (uint32) ~0L;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int32) (uint32) nr;
  }
  else
  {
    if (nr < (longlong) INT_MIN32)
    {
      res=(int32) INT_MIN32;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr > (longlong) INT_MAX32)
    {
      res=(int32) INT_MAX32;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(int32) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,res);
  }
  else
#endif
    longstore(ptr,res);
  return error;
}


double Field_long::val_real(void)
{
  int32 j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return unsigned_flag ? (double) (uint32) j : (double) j;
}

longlong Field_long::val_int(void)
{
  int32 j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return unsigned_flag ? (longlong) (uint32) j : (longlong) j;
}

String *Field_long::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,12*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  int32 j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);

  if (unsigned_flag)
    length=cs->cset->long10_to_str(cs,to,mlength, 10,(long) (uint32)j);
  else
    length=cs->cset->long10_to_str(cs,to,mlength,-10,(long) j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_long::send_binary(Protocol *protocol)
{
  return protocol->store_long(Field_long::val_int());
}

int Field_long::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint4korr(a_ptr);
    b=sint4korr(b_ptr);
  }
  else
#endif
  {
    longget(a,a_ptr);
    longget(b,b_ptr);
  }
  if (unsigned_flag)
    return ((uint32) a < (uint32) b) ? -1 : ((uint32) a > (uint32) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_long::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
    to[2]   = ptr[2];
    to[3]   = ptr[3];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[3];
    else
      to[0] = (char) (ptr[3] ^ 128);		/* Revers signbit */
    to[1]   = ptr[2];
    to[2]   = ptr[1];
    to[3]   = ptr[0];
  }
}


void Field_long::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "int(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
 Field type longlong int (8 bytes)
****************************************************************************/

int Field_longlong::store(const char *from,uint len,CHARSET_INFO *cs)
{
  longlong tmp;
  int error= 0;
  char *end;
  
  tmp= cs->cset->scan(cs, from, from+len, MY_SEQ_SPACES);
  len-= (uint)tmp;
  from+= tmp;
  my_errno=0;
  if (unsigned_flag)
  {
    if (!len || *from == '-')
    {
      tmp=0;					// Set negative to 0
      my_errno= ERANGE;
      error= 1;
    }
    else
      tmp=(longlong) my_strntoull(cs,from,len,10,&end,&error);
  }
  else
    tmp=my_strntoll(cs,from,len,10,&end,&error);
  if (error ||
      (from+len != end && current_thd->count_cuted_fields &&
       !test_if_int(from,len,end,cs)))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    error= 1;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    longlongstore(ptr,tmp);
  return error;
}


int Field_longlong::store(double nr)
{
  int error= 0;
  longlong res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr >= (double) ~ (ulonglong) 0)
    {
      res= ~(longlong) 0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(longlong) (ulonglong) nr;
  }
  else
  {
    if (nr <= (double) LONGLONG_MIN)
    {
      res=(longlong) LONGLONG_MIN;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else if (nr >= (double) LONGLONG_MAX)
    {
      res=(longlong) LONGLONG_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      res=(longlong) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,res);
  }
  else
#endif
    longlongstore(ptr,res);
  return error;
}


int Field_longlong::store(longlong nr)
{
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,nr);
  }
  else
#endif
    longlongstore(ptr,nr);
  return 0;
}


double Field_longlong::val_real(void)
{
  longlong j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    j=sint8korr(ptr);
  }
  else
#endif
    longlongget(j,ptr);
  /* The following is open coded to avoid a bug in gcc 3.3 */
  if (unsigned_flag)
  {
    ulonglong tmp= (ulonglong) j;
    return ulonglong2double(tmp);
  }
  return (double) j;
}


longlong Field_longlong::val_int(void)
{
  longlong j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    longlongget(j,ptr);
  return j;
}


String *Field_longlong::val_str(String *val_buffer,
				String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,22*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  longlong j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    longlongget(j,ptr);

  length=(uint) (cs->cset->longlong10_to_str)(cs,to,mlength,
					unsigned_flag ? 10 : -10, j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_longlong::send_binary(Protocol *protocol)
{
  return protocol->store_longlong(Field_longlong::val_int(), unsigned_flag);
}


int Field_longlong::cmp(const char *a_ptr, const char *b_ptr)
{
  longlong a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint8korr(a_ptr);
    b=sint8korr(b_ptr);
  }
  else
#endif
  {
    longlongget(a,a_ptr);
    longlongget(b,b_ptr);
  }
  if (unsigned_flag)
    return ((ulonglong) a < (ulonglong) b) ? -1 :
    ((ulonglong) a > (ulonglong) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_longlong::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
    to[2]   = ptr[2];
    to[3]   = ptr[3];
    to[4]   = ptr[4];
    to[5]   = ptr[5];
    to[6]   = ptr[6];
    to[7]   = ptr[7];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[7];
    else
      to[0] = (char) (ptr[7] ^ 128);		/* Revers signbit */
    to[1]   = ptr[6];
    to[2]   = ptr[5];
    to[3]   = ptr[4];
    to[4]   = ptr[3];
    to[5]   = ptr[2];
    to[6]   = ptr[1];
    to[7]   = ptr[0];
  }
}


void Field_longlong::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "bigint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** single precision float
****************************************************************************/

int Field_float::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int err;
  Field_float::store(my_strntod(cs,(char*) from,len,(char**)NULL,&err));
  if (err || current_thd->count_cuted_fields && !test_if_real(from,len,cs))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    return 1;
  }
  return (err) ? 1 : 0;
}


int Field_float::store(double nr)
{
  float j;
  int error= 0;
  if (dec < NOT_FIXED_DEC)
    nr=floor(nr*log_10[dec]+0.5)/log_10[dec]; // To fixed point
  if (unsigned_flag && nr < 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    nr=0;
    error= 1;
  }
  if (nr < -FLT_MAX)
  {
    j= -FLT_MAX;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else if (nr > FLT_MAX)
  {
    j=FLT_MAX;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
    j= (float) nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4store(ptr,j);
  }
  else
#endif
    memcpy_fixed(ptr,(byte*) &j,sizeof(j));
  return error;
}


int Field_float::store(longlong nr)
{
  int error= 0;
  float j= (float) nr;
  if (unsigned_flag && j < 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    j=0;
    error= 1;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4store(ptr,j);
  }
  else
#endif
    memcpy_fixed(ptr,(byte*) &j,sizeof(j));
  return error;
}


double Field_float::val_real(void)
{
  float j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4get(j,ptr);
  }
  else
#endif
    memcpy_fixed((byte*) &j,ptr,sizeof(j));
  return ((double) j);
}

longlong Field_float::val_int(void)
{
  float j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4get(j,ptr);
  }
  else
#endif
    memcpy_fixed((byte*) &j,ptr,sizeof(j));
  return ((longlong) j);
}


String *Field_float::val_str(String *val_buffer,
			     String *val_ptr __attribute__((unused)))
{
  float nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4get(nr,ptr);
  }
  else
#endif
    memcpy_fixed((byte*) &nr,ptr,sizeof(nr));

  uint to_length=max(field_length,70);
  val_buffer->alloc(to_length);
  char *to=(char*) val_buffer->ptr();

  if (dec >= NOT_FIXED_DEC)
  {
    sprintf(to,"%-*.*g",(int) field_length,FLT_DIG,nr);
    to=strcend(to,' ');
    *to=0;
  }
  else
  {
#ifdef HAVE_FCONVERT
    char buff[70],*pos=buff;
    int decpt,sign,tmp_dec=dec;

    VOID(sfconvert(&nr,tmp_dec,&decpt,&sign,buff));
    if (sign)
    {
      *to++='-';
    }
    if (decpt < 0)
    {					/* val_buffer is < 0 */
      *to++='0';
      if (!tmp_dec)
	goto end;
      *to++='.';
      if (-decpt > tmp_dec)
	decpt= - (int) tmp_dec;
      tmp_dec=(uint) ((int) tmp_dec+decpt);
      while (decpt++ < 0)
	*to++='0';
    }
    else if (decpt == 0)
    {
      *to++= '0';
      if (!tmp_dec)
	goto end;
      *to++='.';
    }
    else
    {
      while (decpt-- > 0)
	*to++= *pos++;
      if (!tmp_dec)
	goto end;
      *to++='.';
    }
    while (tmp_dec--)
      *to++= *pos++;
#else
#ifdef HAVE_SNPRINTF
    to[to_length-1]=0;			// Safety
    snprintf(to,to_length-1,"%.*f",dec,nr);
    to=strend(to);
#else
    to+= my_sprintf(to,(to,"%.*f",dec,nr));
#endif
#endif
  }
#ifdef HAVE_FCONVERT
 end:
#endif
  val_buffer->length((uint) (to-val_buffer->ptr()));
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_float::cmp(const char *a_ptr, const char *b_ptr)
{
  float a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4get(a,a_ptr);
    float4get(b,b_ptr);
  }
  else
#endif
  {
    memcpy_fixed(&a,a_ptr,sizeof(float));
    memcpy_fixed(&b,b_ptr,sizeof(float));
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

#define FLT_EXP_DIG (sizeof(float)*8-FLT_MANT_DIG)

void Field_float::sort_string(char *to,uint length __attribute__((unused)))
{
  float nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float4get(nr,ptr);
  }
  else
#endif
    memcpy_fixed(&nr,ptr,sizeof(float));

  uchar *tmp= (uchar*) to;
  if (nr == (float) 0.0)
  {						/* Change to zero string */
    tmp[0]=(uchar) 128;
    bzero((char*) tmp+1,sizeof(nr)-1);
  }
  else
  {
#ifdef WORDS_BIGENDIAN
    memcpy_fixed(tmp,&nr,sizeof(nr));
#else
    tmp[0]= ptr[3]; tmp[1]=ptr[2]; tmp[2]= ptr[1]; tmp[3]=ptr[0];
#endif
    if (tmp[0] & 128)				/* Negative */
    {						/* make complement */
      uint i;
      for (i=0 ; i < sizeof(nr); i++)
	tmp[i]= (uchar) (tmp[i] ^ (uchar) 255);
    }
    else
    {
      ushort exp_part=(((ushort) tmp[0] << 8) | (ushort) tmp[1] |
		       (ushort) 32768);
      exp_part+= (ushort) 1 << (16-1-FLT_EXP_DIG);
      tmp[0]= (uchar) (exp_part >> 8);
      tmp[1]= (uchar) exp_part;
    }
  }
}


bool Field_float::send_binary(Protocol *protocol)
{
  return protocol->store((float) Field_float::val_real(), dec, (String*) 0);
}


void Field_float::sql_type(String &res) const
{
  if (dec == NOT_FIXED_DEC)
  {
    res.set_ascii("float", 5);
  }
  else
  {
    CHARSET_INFO *cs= res.charset();
    res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			    "float(%d,%d)",(int) field_length,dec));
  }
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** double precision floating point numbers
****************************************************************************/

int Field_double::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int err;
  double j= my_strntod(cs,(char*) from,len,(char**)0,&err);
  if (err || current_thd->count_cuted_fields && !test_if_real(from,len,cs))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    err= 1;
  }
  if (unsigned_flag && j < 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    j=0;
    err= 1;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8store(ptr,j);
  }
  else
#endif
    doublestore(ptr,j);
  return err;
}


int Field_double::store(double nr)
{
  int error= 0;
  if (dec < NOT_FIXED_DEC)
    nr=floor(nr*log_10[dec]+0.5)/log_10[dec]; // To fixed point
  if (unsigned_flag && nr < 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    nr=0;
    error= 1;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8store(ptr,nr);
  }
  else
#endif
    doublestore(ptr,nr);
  return error;
}


int Field_double::store(longlong nr)
{
  double j= (double) nr;
  int error= 0;
  if (unsigned_flag && j < 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
    j=0;
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8store(ptr,j);
  }
  else
#endif
    doublestore(ptr,j);
  return error;
}


double Field_double::val_real(void)
{
  double j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  return j;
}

longlong Field_double::val_int(void)
{
  double j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  return ((longlong) j);
}


String *Field_double::val_str(String *val_buffer,
			      String *val_ptr __attribute__((unused)))
{
  double nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
    doubleget(nr,ptr);

  uint to_length=max(field_length,320);
  val_buffer->alloc(to_length);
  char *to=(char*) val_buffer->ptr();

  if (dec >= NOT_FIXED_DEC)
  {
    sprintf(to,"%-*.*g",(int) field_length,DBL_DIG,nr);
    to=strcend(to,' ');
  }
  else
  {
#ifdef HAVE_FCONVERT
    char buff[320],*pos=buff;
    int decpt,sign,tmp_dec=dec;

    VOID(fconvert(nr,tmp_dec,&decpt,&sign,buff));
    if (sign)
    {
      *to++='-';
    }
    if (decpt < 0)
    {					/* val_buffer is < 0 */
      *to++='0';
      if (!tmp_dec)
	goto end;
      *to++='.';
      if (-decpt > tmp_dec)
	decpt= - (int) tmp_dec;
      tmp_dec=(uint) ((int) tmp_dec+decpt);
      while (decpt++ < 0)
	*to++='0';
    }
    else if (decpt == 0)
    {
      *to++= '0';
      if (!tmp_dec)
	goto end;
      *to++='.';
    }
    else
    {
      while (decpt-- > 0)
	*to++= *pos++;
      if (!tmp_dec)
	goto end;
      *to++='.';
    }
    while (tmp_dec--)
      *to++= *pos++;
#else
#ifdef HAVE_SNPRINTF
    to[to_length-1]=0;			// Safety
    snprintf(to,to_length-1,"%.*f",dec,nr);
    to=strend(to);
#else
    to+= my_sprintf(to,(to,"%.*f",dec,nr));
#endif
#endif
  }
#ifdef HAVE_FCONVERT
 end:
#endif

  val_buffer->length((uint) (to-val_buffer->ptr()));
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}

bool Field_double::send_binary(Protocol *protocol)
{
  return protocol->store((double) Field_double::val_real(), dec, (String*) 0);
}


int Field_double::cmp(const char *a_ptr, const char *b_ptr)
{
  double a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8get(a,a_ptr);
    float8get(b,b_ptr);
  }
  else
#endif
  {
/* could this ALWAYS be 2 calls to doubleget() ?? */
#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
    doubleget(a, a_ptr);
    doubleget(b, b_ptr);
#else
    memcpy_fixed(&a,a_ptr,sizeof(double));
    memcpy_fixed(&b,b_ptr,sizeof(double));
#endif
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

/* The following should work for IEEE */

void Field_double::sort_string(char *to,uint length __attribute__((unused)))
{
  double nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
/* could this ALWAYS be 2 calls to doubleget() ?? */
#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
    doubleget(nr,ptr);
#else
    memcpy_fixed(&nr,ptr,sizeof(nr));
#endif
  change_double_for_sort(nr, (byte*) to);
}


void Field_double::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  if (dec == NOT_FIXED_DEC)
  {
    res.set_ascii("double",6);
  }
  else
  {
    res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			    "double(%d,%d)",(int) field_length,dec));
  }
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
** timestamp
** The first timestamp in the table is automaticly updated
** by handler.cc.  The form->timestamp points at the automatic timestamp.
****************************************************************************/

Field_timestamp::Field_timestamp(char *ptr_arg, uint32 len_arg,
				 enum utype unireg_check_arg,
				 const char *field_name_arg,
				 struct st_table *table_arg,
				 CHARSET_INFO *cs)
  :Field_str(ptr_arg, 19, (uchar*) 0,0,
	     unireg_check_arg, field_name_arg, table_arg, cs)
{
  flags|=ZEROFILL_FLAG; /* 4.0 MYD compatibility */
  if (table && !table->timestamp_field)
  {
    table->timestamp_field= this;		// Automatic timestamp
    table->time_stamp=(ulong) (ptr_arg - (char*) table->record[0])+1;
    flags|=TIMESTAMP_FLAG;
  }
}


int Field_timestamp::store(const char *from,uint len,CHARSET_INFO *cs)
{
  long tmp=(long) str_to_timestamp(from,len);
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
  return 0;
}

int Field_timestamp::store(double nr)
{
  int error= 0;
  if (nr < 0 || nr > 99991231235959.0)
  {
    nr= 0;					// Avoid overflow on buff
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  error|= Field_timestamp::store((longlong) rint(nr));
  return error;
}


/*
  Convert a datetime of formats YYMMDD, YYYYMMDD or YYMMDDHHMSS to
  YYYYMMDDHHMMSS.  The high date '99991231235959' is checked before this
  function.
*/

static longlong fix_datetime(longlong nr)
{
  current_thd->last_cuted_field= 0;
  if (nr == LL(0) || nr >= LL(10000101000000))
    return nr;					// Normal datetime >= Year 1000
  if (nr < 101)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*10000L+1231L)
    return (nr+20000000L)*1000000L;		// YYMMDD, year: 2000-2069
  if (nr < (YY_PART_YEAR)*10000L+101L)
    goto err;
  if (nr <= 991231L)
    return (nr+19000000L)*1000000L;		// YYMMDD, year: 1970-1999
  if (nr < 10000101L)
    goto err;
  if (nr <= 99991231L)
    return nr*1000000L;
  if (nr < 101000000L)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*LL(10000000000)+LL(1231235959))
    return nr+LL(20000000000000);		// YYMMDDHHMMSS, 2000-2069
  if (nr <  YY_PART_YEAR*LL(10000000000)+ LL(101000000))
    goto err;
  if (nr <= LL(991231235959))
    return nr+LL(19000000000000);		// YYMMDDHHMMSS, 1970-1999

 err:
    current_thd->last_cuted_field= 1;
  return LL(0);
}


int Field_timestamp::store(longlong nr)
{
  TIME l_time;
  time_t timestamp;
  long part1,part2;

  if ((nr=fix_datetime(nr)))
  {
    long not_used;
    part1=(long) (nr/LL(1000000));
    part2=(long) (nr - (longlong) part1*LL(1000000));
    l_time.year=  (int) (part1/10000L);  part1%=10000L;
    l_time.month= (int) part1 / 100;
    l_time.day=	  (int) part1 % 100; 
    l_time.hour=  (int) (part2/10000L);  part2%=10000L;
    l_time.minute=(int) part2 / 100;
    l_time.second=(int) part2 % 100; 
    timestamp=my_gmt_sec(&l_time, &not_used);
  }
  else
    timestamp=0;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,timestamp);
  }
  else
#endif
    longstore(ptr,(uint32) timestamp);
  if (current_thd->last_cuted_field)    
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  return 0;
}


double Field_timestamp::val_real(void)
{
  return (double) Field_timestamp::val_int();
}

longlong Field_timestamp::val_int(void)
{
  int part_time;
  uint32 temp;
  time_t time_arg;
  struct tm *l_time;
  longlong res;
  struct tm tm_tmp;

#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);

  if (temp == 0L)				// No time
    return(0);					/* purecov: inspected */
  time_arg=(time_t) temp;
  localtime_r(&time_arg,&tm_tmp);
  l_time=&tm_tmp;

  part_time= l_time->tm_year % 100;
  res= ((longlong) (part_time+ ((part_time < YY_PART_YEAR) ? 2000 : 1900))*
	LL(10000000000));
  part_time= l_time->tm_mon+1;
  res+= (longlong) part_time * LL(100000000);
  part_time=l_time->tm_mday;
  res+= (longlong) ((long) part_time * 1000000L);
  part_time=l_time->tm_hour;
  res+= (longlong) (part_time * 10000L);
  part_time=l_time->tm_min;
  res+= (longlong) (part_time * 100);
  part_time=l_time->tm_sec;
  return res+part_time;
}


String *Field_timestamp::val_str(String *val_buffer,
				 String *val_ptr __attribute__((unused)))
{
  uint32 temp, temp2;
  time_t time_arg;
  struct tm *l_time;
  struct tm tm_tmp;

  val_buffer->alloc(field_length+1);
  char *to= (char*) val_buffer->ptr();
  val_buffer->length(field_length);

#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);

  if (temp == 0L)
  {				      /* Zero time is "000000" */
    val_ptr->set("0000-00-00 00:00:00", 19, &my_charset_bin);
    return val_ptr;
  }
  val_buffer->set_charset(&my_charset_bin);	// Safety
  time_arg=(time_t) temp;
  localtime_r(&time_arg,&tm_tmp);
  l_time=&tm_tmp;

  temp= l_time->tm_year % 100;
  if (temp < YY_PART_YEAR)
  {
    *to++= '2';
    *to++= '0';
  }
  else
  {
    *to++= '1';
    *to++= '9';
  }
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= '-';
  temp=l_time->tm_mon+1;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= '-';
  temp=l_time->tm_mday;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ' ';
  temp=l_time->tm_hour;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ':';
  temp=l_time->tm_min;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ':';
  temp=l_time->tm_sec;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to= 0;
  return val_buffer;
}


bool Field_timestamp::get_date(TIME *ltime, uint fuzzydate)
{
  long temp;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);
  if (temp == 0L)
  {				      /* Zero time is "000000" */
    if (!fuzzydate)
      return 1;
    bzero((char*) ltime,sizeof(*ltime));
  }
  else
  {
    struct tm tm_tmp;
    time_t time_arg= (time_t) temp;
    localtime_r(&time_arg,&tm_tmp);
    struct tm *start= &tm_tmp;
    ltime->year=	start->tm_year+1900;
    ltime->month=	start->tm_mon+1;
    ltime->day=		start->tm_mday;
    ltime->hour=	start->tm_hour;
    ltime->minute=	start->tm_min;
    ltime->second=	start->tm_sec;
    ltime->second_part=	0;
    ltime->neg=		0;
    ltime->time_type=TIMESTAMP_DATETIME;
  }
  return 0;
}

bool Field_timestamp::get_time(TIME *ltime)
{
  return Field_timestamp::get_date(ltime,0);
}


bool Field_timestamp::send_binary(Protocol *protocol)
{
  TIME tm;
  Field_timestamp::get_date(&tm, TIME_FUZZY_DATE);
  return protocol->store(&tm);
}


int Field_timestamp::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint4korr(a_ptr);
    b=sint4korr(b_ptr);
  }
  else
#endif
  {
  longget(a,a_ptr);
  longget(b,b_ptr);
  }
  return ((uint32) a < (uint32) b) ? -1 : ((uint32) a > (uint32) b) ? 1 : 0;
}


void Field_timestamp::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    to[0] = ptr[0];
    to[1] = ptr[1];
    to[2] = ptr[2];
    to[3] = ptr[3];
  }
  else
#endif
  {
    to[0] = ptr[3];
    to[1] = ptr[2];
    to[2] = ptr[1];
    to[3] = ptr[0];
  }
}


void Field_timestamp::sql_type(String &res) const
{
  res.set_ascii("timestamp", 9);
}


void Field_timestamp::set_time()
{
  long tmp= (long) current_thd->query_start();
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
}

/****************************************************************************
** time type
** In string context: HH:MM:SS
** In number context: HHMMSS
** Stored as a 3 byte unsigned int
****************************************************************************/

int Field_time::store(const char *from,uint len,CHARSET_INFO *cs)
{
  TIME ltime;
  long tmp;
  int error= 0;
  if (str_to_time(from,len,&ltime))
  {
    tmp=0L;
    error= 1;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
  else
  {
    if (ltime.month)
      ltime.day=0;
    tmp=(ltime.day*24L+ltime.hour)*10000L+(ltime.minute*100+ltime.second);
    if (tmp > 8385959)
    {
      tmp=8385959;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
  }
  if (ltime.neg)
    tmp= -tmp;
  error |= Field_time::store((longlong) tmp);
  return error;
}


int Field_time::store(double nr)
{
  long tmp;
  int error= 0;
  if (nr > 8385959.0)
  {
    tmp=8385959L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else if (nr < -8385959.0)
  {
    tmp= -8385959L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
  {
    tmp=(long) floor(fabs(nr));			// Remove fractions
    if (nr < 0)
      tmp= -tmp;
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
  }
  int3store(ptr,tmp);
  return error;
}


int Field_time::store(longlong nr)
{
  long tmp;
  int error= 0;
  if (nr > (longlong) 8385959L)
  {
    tmp=8385959L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else if (nr < (longlong) -8385959L)
  {
    tmp= -8385959L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
  {
    tmp=(long) nr;
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
  }
  int3store(ptr,tmp);
  return error;
}


double Field_time::val_real(void)
{
  uint32 j= (uint32) uint3korr(ptr);
  return (double) j;
}

longlong Field_time::val_int(void)
{
  return (longlong) sint3korr(ptr);
}


/*
  This function is multi-byte safe as the result string is always of type
  my_charset_bin
*/

String *Field_time::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  TIME ltime;
  val_buffer->alloc(19);
  long tmp=(long) sint3korr(ptr);
  ltime.neg= 0;
  if (tmp < 0)
  {
    tmp= -tmp;
    ltime.neg= 1;
  }
  ltime.day= (uint) 0;
  ltime.hour= (uint) (tmp/10000);
  ltime.minute= (uint) (tmp/100 % 100);
  ltime.second= (uint) (tmp % 100);
  make_time((DATE_TIME_FORMAT*) 0, &ltime, val_buffer);
  return val_buffer;
}


bool Field_time::get_time(TIME *ltime)
{
  long tmp=(long) sint3korr(ptr);
  ltime->neg=0;
  if (tmp < 0)
  {
    ltime->neg= 1;
    tmp=-tmp;
  }
  ltime->day= 0;
  ltime->hour=   (int) (tmp/10000);
  tmp-=ltime->hour*10000;
  ltime->minute= (int) tmp/100;
  ltime->second= (int) tmp % 100;
  ltime->second_part=0;
  ltime->time_type= TIMESTAMP_TIME;
  return 0;
}


bool Field_time::send_binary(Protocol *protocol)
{
  TIME tm;
  Field_time::get_time(&tm);
  tm.day= tm.hour/3600;				// Move hours to days
  tm.hour-= tm.day*3600;
  return protocol->store_time(&tm);
}


int Field_time::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
  a=(int32) sint3korr(a_ptr);
  b=(int32) sint3korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_time::sort_string(char *to,uint length __attribute__((unused)))
{
  to[0] = (uchar) (ptr[2] ^ 128);
  to[1] = ptr[1];
  to[2] = ptr[0];
}

void Field_time::sql_type(String &res) const
{
  res.set_ascii("time", 4);
}

/****************************************************************************
** year type
** Save in a byte the year 0, 1901->2155
** Can handle 2 byte or 4 byte years!
****************************************************************************/

int Field_year::store(const char *from, uint len,CHARSET_INFO *cs)
{
  int not_used;				// We can ignore result from str2int
  char *end;
  long nr= my_strntol(cs, from, len, 10, &end, &not_used);

  if (nr < 0 || nr >= 100 && nr <= 1900 || nr > 2155)
  {
    *ptr=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    return 1;
  }
  else if (current_thd->count_cuted_fields && !test_if_int(from,len,end,cs))
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  if (nr != 0 || len != 4)
  {
    if (nr < YY_PART_YEAR)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (unsigned char) nr;
  return 0;
}

int Field_year::store(double nr)
{
  if (nr < 0.0 || nr >= 2155.0)
  {
    (void) Field_year::store((longlong) -1);
    return 1;
  }
  else
    return Field_year::store((longlong) nr);
}

int Field_year::store(longlong nr)
{
  if (nr < 0 || nr >= 100 && nr <= 1900 || nr > 2155)
  {
    *ptr=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    return 1;
  }
  if (nr != 0 || field_length != 4)		// 0000 -> 0; 00 -> 2000
  {
    if (nr < YY_PART_YEAR)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (unsigned char) nr;
  return 0;
}

bool Field_year::send_binary(Protocol *protocol)
{
  ulonglong tmp= Field_year::val_int();
  return protocol->store_short(tmp);
}

double Field_year::val_real(void)
{
  return (double) Field_year::val_int();
}

longlong Field_year::val_int(void)
{
  int tmp= (int) ((uchar*) ptr)[0];
  if (field_length != 4)
    tmp%=100;					// Return last 2 char
  else if (tmp)
    tmp+=1900;
  return (longlong) tmp;
}

String *Field_year::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  val_buffer->alloc(5);
  val_buffer->length(field_length);
  char *to=(char*) val_buffer->ptr();
  sprintf(to,field_length == 2 ? "%02d" : "%04d",(int) Field_year::val_int());
  return val_buffer;
}

void Field_year::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*)res.ptr(),res.alloced_length(),
			  "year(%d)",(int) field_length));
}


/****************************************************************************
** date type
** In string context: YYYY-MM-DD
** In number context: YYYYMMDD
** Stored as a 4 byte unsigned int
****************************************************************************/

int Field_date::store(const char *from, uint len,CHARSET_INFO *cs)
{
  TIME l_time;
  uint32 tmp;
  int error= 0;
  if (str_to_TIME(from,len,&l_time,1) <= TIMESTAMP_DATETIME_ERROR)
  {
    tmp=0;
    error= 1;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
  else
    tmp=(uint32) l_time.year*10000L + (uint32) (l_time.month*100+l_time.day);
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
  return error;
}


int Field_date::store(double nr)
{
  long tmp;
  int error= 0;
  if (nr >= 19000000000000.0 && nr <= 99991231235959.0)
    nr=floor(nr/1000000.0);			// Timestamp to date
  if (nr < 0.0 || nr > 99991231.0)
  {
    tmp=0L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
    tmp=(long) rint(nr);
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
  return error;
}


int Field_date::store(longlong nr)
{
  long tmp;
  int error= 0;
  if (nr >= LL(19000000000000) && nr < LL(99991231235959))
    nr=nr/LL(1000000);			// Timestamp to date
  if (nr < 0 || nr > LL(99991231))
  {
    tmp=0L;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
    tmp=(long) nr;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,tmp);
  }
  else
#endif
    longstore(ptr,tmp);
  return error;
}


bool Field_date::send_binary(Protocol *protocol)
{
  longlong tmp= Field_date::val_int();
  TIME tm;
  tm.year= (uint32) tmp/10000L % 10000;
  tm.month= (uint32) tmp/100 % 100;
  tm.day= (uint32) tmp % 100;
  return protocol->store_date(&tm);
}


double Field_date::val_real(void)
{
  int32 j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return (double) (uint32) j;
}

longlong Field_date::val_int(void)
{
  int32 j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return (longlong) (uint32) j;
}

String *Field_date::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  TIME ltime;
  val_buffer->alloc(field_length);
  int32 tmp;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    tmp=sint4korr(ptr);
  else
#endif
    longget(tmp,ptr);
  ltime.neg= 0;
  ltime.year= (int) ((uint32) tmp/10000L % 10000);
  ltime.month= (int) ((uint32) tmp/100 % 100);
  ltime.day= (int) ((uint32) tmp % 100);
  make_date((DATE_TIME_FORMAT *) 0, &ltime, val_buffer);
  return val_buffer;
}


int Field_date::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint4korr(a_ptr);
    b=sint4korr(b_ptr);
  }
  else
#endif
  {
    longget(a,a_ptr);
    longget(b,b_ptr);
  }
  return ((uint32) a < (uint32) b) ? -1 : ((uint32) a > (uint32) b) ? 1 : 0;
}


void Field_date::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    to[0] = ptr[0];
    to[1] = ptr[1];
    to[2] = ptr[2];
    to[3] = ptr[3];
  }
  else
#endif
  {
    to[0] = ptr[3];
    to[1] = ptr[2];
    to[2] = ptr[1];
    to[3] = ptr[0];
  }
}

void Field_date::sql_type(String &res) const
{
  res.set_ascii("date", 4);
}

/****************************************************************************
** The new date type
** This is identical to the old date type, but stored on 3 bytes instead of 4
** In number context: YYYYMMDD
****************************************************************************/

int Field_newdate::store(const char *from,uint len,CHARSET_INFO *cs)
{
  TIME l_time;
  long tmp;
  int error= 0;
  if (str_to_TIME(from,len,&l_time,1) <= TIMESTAMP_DATETIME_ERROR)
  {
    tmp=0L;
    error= 1;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
  else
    tmp= l_time.day + l_time.month*32 + l_time.year*16*32;
  int3store(ptr,tmp);
  return error;
}

int Field_newdate::store(double nr)
{
  if (nr < 0.0 || nr > 99991231235959.0)
  {
    (void) Field_newdate::store((longlong) -1);
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    return 1;
  }
  else
    return Field_newdate::store((longlong) rint(nr));
}


int Field_newdate::store(longlong nr)
{
  int32 tmp;
  int error= 0;
  if (nr >= LL(100000000) && nr <= LL(99991231235959))
    nr=nr/LL(1000000);			// Timestamp to date
  if (nr < 0L || nr > 99991231L)
  {
    tmp=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
  {
    tmp=(int32) nr;
    if (tmp)
    {
      if (tmp < YY_PART_YEAR*10000L)			// Fix short dates
	tmp+= (uint32) 20000000L;
      else if (tmp < 999999L)
	tmp+= (uint32) 19000000L;
    }
    uint month= (uint) ((tmp/100) % 100);
    uint day=   (uint) (tmp%100);
    if (month > 12 || day > 31)
    {
      tmp=0L;					// Don't allow date to change
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
      error= 1;
    }
    else
      tmp= day + month*32 + (tmp/10000)*16*32;
  }
  int3store(ptr,(int32) tmp);
  return error;
}

void Field_newdate::store_time(TIME *ltime,timestamp_type type)
{
  long tmp;
  if (type == TIMESTAMP_DATE || type == TIMESTAMP_DATETIME)
    tmp=ltime->year*16*32+ltime->month*32+ltime->day;
  else
  {
    tmp=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
  int3store(ptr,tmp);
}

bool Field_newdate::send_binary(Protocol *protocol)
{
  TIME tm;
  Field_newdate::get_date(&tm,0);
  return protocol->store_date(&tm);
}

double Field_newdate::val_real(void)
{
  return (double) Field_newdate::val_int();
}

longlong Field_newdate::val_int(void)
{
  ulong j= uint3korr(ptr);
  j= (j % 32L)+(j / 32L % 16L)*100L + (j/(16L*32L))*10000L;
  return (longlong) j;
}

String *Field_newdate::val_str(String *val_buffer,
			       String *val_ptr __attribute__((unused)))
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  uint32 tmp=(uint32) uint3korr(ptr);
  int part;
  char *pos=(char*) val_buffer->ptr()+10;

  /* Open coded to get more speed */
  *pos--=0;					// End NULL
  part=(int) (tmp & 31);
  *pos--= (char) ('0'+part%10);
  *pos--= (char) ('0'+part/10);
  *pos--= '-';
  part=(int) (tmp >> 5 & 15);
  *pos--= (char) ('0'+part%10);
  *pos--= (char) ('0'+part/10);
  *pos--= '-';
  part=(int) (tmp >> 9);
  *pos--= (char) ('0'+part%10); part/=10;
  *pos--= (char) ('0'+part%10); part/=10;
  *pos--= (char) ('0'+part%10); part/=10;
  *pos=   (char) ('0'+part);
  return val_buffer;
}

bool Field_newdate::get_date(TIME *ltime,uint fuzzydate)
{
  if (is_null())
    return 1;
  uint32 tmp=(uint32) uint3korr(ptr);
  ltime->day=   tmp & 31;
  ltime->month= (tmp >> 5) & 15;
  ltime->year=  (tmp >> 9);
  ltime->time_type=TIMESTAMP_DATE;
  ltime->hour= ltime->minute= ltime->second= ltime->second_part= ltime->neg= 0;
  return (!fuzzydate && (!ltime->month || !ltime->day)) ? 1 : 0;
}

bool Field_newdate::get_time(TIME *ltime)
{
  return Field_newdate::get_date(ltime,0);
}

int Field_newdate::cmp(const char *a_ptr, const char *b_ptr)
{
  uint32 a,b;
  a=(uint32) uint3korr(a_ptr);
  b=(uint32) uint3korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_newdate::sort_string(char *to,uint length __attribute__((unused)))
{
  to[0] = ptr[2];
  to[1] = ptr[1];
  to[2] = ptr[0];
}

void Field_newdate::sql_type(String &res) const
{
  res.set_ascii("date", 4);
}


/****************************************************************************
** datetime type
** In string context: YYYY-MM-DD HH:MM:DD
** In number context: YYYYMMDDHHMMDD
** Stored as a 8 byte unsigned int. Should sometimes be change to a 6 byte int.
****************************************************************************/

int Field_datetime::store(const char *from,uint len,CHARSET_INFO *cs)
{
  longlong tmp=str_to_datetime(from,len,1);
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    longlongstore(ptr,tmp);
  return 0;
}


int Field_datetime::store(double nr)
{
  int error= 0;
  if (nr < 0.0 || nr > 99991231235959.0)
  {
    nr=0.0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  error |= Field_datetime::store((longlong) rint(nr));
  return error;
}


int Field_datetime::store(longlong nr)
{
  int error= 0;
  if (nr < 0 || nr > LL(99991231235959))
  {
    nr=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE);
    error= 1;
  }
  else
    nr=fix_datetime(nr);
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,nr);
  }
  else
#endif
    longlongstore(ptr,nr);
  if (current_thd->last_cuted_field)    
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  return error;
}

void Field_datetime::store_time(TIME *ltime,timestamp_type type)
{
  longlong tmp;
  if (type == TIMESTAMP_DATE || type == TIMESTAMP_DATETIME)
    tmp=((ltime->year*10000L+ltime->month*100+ltime->day)*LL(1000000)+
	 (ltime->hour*10000L+ltime->minute*100+ltime->second));
  else
  {
    tmp=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    longlongstore(ptr,tmp);
}

bool Field_datetime::send_binary(Protocol *protocol)
{
  TIME tm;
  Field_datetime::get_date(&tm, TIME_FUZZY_DATE);
  return protocol->store(&tm);
}


double Field_datetime::val_real(void)
{
  return (double) Field_datetime::val_int();
}

longlong Field_datetime::val_int(void)
{
  longlong j;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    longlongget(j,ptr);
  return j;
}


String *Field_datetime::val_str(String *val_buffer,
				String *val_ptr __attribute__((unused)))
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  ulonglong tmp;
  long part1,part2;
  char *pos;
  int part3;

#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
    tmp=sint8korr(ptr);
  else
#endif
    longlongget(tmp,ptr);

  /*
    Avoid problem with slow longlong aritmetic and sprintf
  */

  part1=(long) (tmp/LL(1000000));
  part2=(long) (tmp - (ulonglong) part1*LL(1000000));

  pos=(char*) val_buffer->ptr()+19;
  *pos--=0;
  *pos--= (char) ('0'+(char) (part2%10)); part2/=10;
  *pos--= (char) ('0'+(char) (part2%10)); part3= (int) (part2 / 10);
  *pos--= ':';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= ':';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) part3);
  *pos--= ' ';
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= '-';
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= (char) ('0'+(char) (part1%10)); part3= (int) (part1/10);
  *pos--= '-';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos=(char) ('0'+(char) part3);
  return val_buffer;
}

bool Field_datetime::get_date(TIME *ltime, uint fuzzydate)
{
  longlong tmp=Field_datetime::val_int();
  uint32 part1,part2;
  part1=(uint32) (tmp/LL(1000000));
  part2=(uint32) (tmp - (ulonglong) part1*LL(1000000));

  ltime->time_type=	TIMESTAMP_DATETIME;
  ltime->neg=		0;
  ltime->second_part=	0;
  ltime->second=	(int) (part2%100);
  ltime->minute=	(int) (part2/100%100);
  ltime->hour=		(int) (part2/10000);
  ltime->day=		(int) (part1%100);
  ltime->month= 	(int) (part1/100%100);
  ltime->year= 		(int) (part1/10000);
  return (!fuzzydate && (!ltime->month || !ltime->day)) ? 1 : 0;
}

bool Field_datetime::get_time(TIME *ltime)
{
  return Field_datetime::get_date(ltime,0);
}

int Field_datetime::cmp(const char *a_ptr, const char *b_ptr)
{
  longlong a,b;
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    a=sint8korr(a_ptr);
    b=sint8korr(b_ptr);
  }
  else
#endif
  {
    longlongget(a,a_ptr);
    longlongget(b,b_ptr);
  }
  return ((ulonglong) a < (ulonglong) b) ? -1 :
    ((ulonglong) a > (ulonglong) b) ? 1 : 0;
}

void Field_datetime::sort_string(char *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->db_low_byte_first)
  {
    to[0] = ptr[0];
    to[1] = ptr[1];
    to[2] = ptr[2];
    to[3] = ptr[3];
    to[4] = ptr[4];
    to[5] = ptr[5];
    to[6] = ptr[6];
    to[7] = ptr[7];
  }
  else
#endif
  {
    to[0] = ptr[7];
    to[1] = ptr[6];
    to[2] = ptr[5];
    to[3] = ptr[4];
    to[4] = ptr[3];
    to[5] = ptr[2];
    to[6] = ptr[1];
    to[7] = ptr[0];
  }
}


void Field_datetime::sql_type(String &res) const
{
  res.set_ascii("datetime", 8);
}

/****************************************************************************
** string type
** A string may be varchar or binary
****************************************************************************/

	/* Copy a string and fill with space */

static bool use_conversion(CHARSET_INFO *cs1, CHARSET_INFO *cs2)
{
  return (cs1 != &my_charset_bin) && (cs2 != &my_charset_bin) && (cs1!=cs2);
}

int Field_string::store(const char *from,uint length,CHARSET_INFO *cs)
{
  int error= 0;
  char buff[80];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);
  /* Convert character set if nesessary */
  if (use_conversion(cs, field_charset))
  { 
    tmpstr.copy(from, length, cs, field_charset);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }
  if (length <= field_length)
  {
    memcpy(ptr,from,length);
    if (length < field_length)
      field_charset->cset->fill(field_charset,ptr+length,field_length-length,
				' ');
  }
  else
  {
    memcpy(ptr,from,field_length);
    if (current_thd->count_cuted_fields)
    {						// Check if we loosed some info
      const char *end=from+length;
      from+= field_length;
      from+= field_charset->cset->scan(field_charset, from, end,
				       MY_SEQ_SPACES);
      if (from != end)
      {
        set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
	error=1;
      }
    }
  }
  return error;
}


int Field_string::store(double nr)
{
  char buff[MAX_FIELD_WIDTH],*end;
  int width=min(field_length,DBL_DIG+5);
  sprintf(buff,"%-*.*g",width,max(width-5,0),nr);
  end=strcend(buff,' ');
  return Field_string::store(buff,(uint) (end - buff), &my_charset_bin);
}


int Field_string::store(longlong nr)
{
  char buff[64];
  int  l;
  CHARSET_INFO *cs=charset();
  l= (cs->cset->longlong10_to_str)(cs,buff,sizeof(buff),-10,nr);
  return Field_string::store(buff,(uint)l,cs);
}


double Field_string::val_real(void)
{
  int not_used;
  CHARSET_INFO *cs=charset();
  return my_strntod(cs,ptr,field_length,(char**)0,&not_used);
}


longlong Field_string::val_int(void)
{
  int not_used;
  CHARSET_INFO *cs=charset();
  return my_strntoll(cs,ptr,field_length,10,NULL,&not_used);
}


String *Field_string::val_str(String *val_buffer __attribute__((unused)),
			      String *val_ptr)
{
  uint length= field_charset->cset->lengthsp(field_charset, ptr, field_length);
  val_ptr->set((const char*) ptr, length, field_charset);
  return val_ptr;
}


int Field_string::cmp(const char *a_ptr, const char *b_ptr)
{
  if (field_charset->strxfrm_multiply > 1)
  {
    /*
      We have to remove end space to be able to compare multi-byte-characters
      like in latin_de 'ae' and 0xe4
    */
    return field_charset->coll->strnncollsp(field_charset,
				      (const uchar*) a_ptr, field_length,
				      (const uchar*) b_ptr, field_length);
  }
  return field_charset->coll->strnncoll(field_charset,
				  (const uchar*) a_ptr, field_length,
				  (const uchar*) b_ptr, field_length);
}

void Field_string::sort_string(char *to,uint length)
{
  uint tmp=my_strnxfrm(field_charset,
                          (unsigned char *)to, length,
                          (unsigned char *) ptr, field_length);
  if (tmp < length)
    bzero(to + tmp, length - tmp);
}


void Field_string::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  ulong length= cs->cset->snprintf(cs,(char*) res.ptr(),
			    res.alloced_length(), "%s(%d)",
			    (field_length > 3 &&
			     (table->db_options_in_use &
			      HA_OPTION_PACK_RECORD) ?
			     "varchar" : "char"),
			    (int) field_length / charset()->mbmaxlen);
  res.length(length);
}


char *Field_string::pack(char *to, const char *from, uint max_length)
{
  const char *end=from+min(field_length,max_length);
  uchar length;
  while (end > from && end[-1] == ' ')
    end--;
  *to= length=(uchar) (end-from);
  memcpy(to+1, from, (int) length);
  return to+1+length;
}


const char *Field_string::unpack(char *to, const char *from)
{
  uint length= (uint) (uchar) *from++;
  memcpy(to, from, (int) length);
  bfill(to+length, field_length - length, ' ');
  return from+length;
}


int Field_string::pack_cmp(const char *a, const char *b, uint length)
{
  uint a_length= (uint) (uchar) *a++;
  uint b_length= (uint) (uchar) *b++;
  return my_strnncoll(field_charset,
		      (const uchar*)a,a_length,
		      (const uchar*)b,b_length);
}


int Field_string::pack_cmp(const char *b, uint length)
{
  uint b_length= (uint) (uchar) *b++;
  char *end= ptr + field_length;
  while (end > ptr && end[-1] == ' ')
    end--;
  uint a_length = (uint) (end - ptr);
  return my_strnncoll(field_charset,
		     (const uchar*)ptr,a_length,
		     (const uchar*)b, b_length);
}


uint Field_string::packed_col_length(const char *data_ptr, uint length)
{
  if (length > 255)
    return uint2korr(data_ptr)+2;
  else
    return (uint) ((uchar) *data_ptr)+1;
}

uint Field_string::max_packed_col_length(uint max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}


/****************************************************************************
** VARCHAR type  (Not available for the end user yet)
****************************************************************************/


int Field_varstring::store(const char *from,uint length,CHARSET_INFO *cs)
{
  int error= 0;
  char buff[80];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);
  /* Convert character set if nesessary */
  if (use_conversion(cs, field_charset))
  { 
    tmpstr.copy(from, length, cs, field_charset);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }
  if (length > field_length)
  {
    length=field_length;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    error= 1;
  }
  memcpy(ptr+2,from,length);
  int2store(ptr, length);
  return error;
}


int Field_varstring::store(double nr)
{
  char buff[MAX_FIELD_WIDTH],*end;
  int width=min(field_length,DBL_DIG+5);
  sprintf(buff,"%-*.*g",width,max(width-5,0),nr);
  end=strcend(buff,' ');
  return Field_varstring::store(buff,(uint) (end - buff), &my_charset_bin);
}


int Field_varstring::store(longlong nr)
{
  char buff[64];
  int  l;
  CHARSET_INFO *cs=charset();
  l= (cs->cset->longlong10_to_str)(cs,buff,sizeof(buff),-10,nr);
  return Field_varstring::store(buff,(uint)l,cs);
}


double Field_varstring::val_real(void)
{
  int not_used;
  uint length=uint2korr(ptr)+2;
  CHARSET_INFO *cs=charset();
  return my_strntod(cs,ptr+2,length,(char**)0, &not_used);
}


longlong Field_varstring::val_int(void)
{
  int not_used;
  uint length=uint2korr(ptr)+2;
  CHARSET_INFO *cs=charset();
  return my_strntoll(cs,ptr+2,length,10,NULL, &not_used);
}


String *Field_varstring::val_str(String *val_buffer __attribute__((unused)),
				 String *val_ptr)
{
  uint length=uint2korr(ptr);
  val_ptr->set((const char*) ptr+2,length,field_charset);
  return val_ptr;
}


int Field_varstring::cmp(const char *a_ptr, const char *b_ptr)
{
  uint a_length=uint2korr(a_ptr);
  uint b_length=uint2korr(b_ptr);
  int diff;
  diff=my_strnncoll(field_charset,
		      (const uchar*)a_ptr+2,min(a_length,b_length),
		      (const uchar*)b_ptr+2,min(a_length,b_length));
  return diff ? diff : (int) (a_length - b_length);
}

void Field_varstring::sort_string(char *to,uint length)
{
  uint tot_length=uint2korr(ptr);
  tot_length=my_strnxfrm(field_charset,
                             (unsigned char *) to, length,
                             (unsigned char *)ptr+2, tot_length);
  if (tot_length < length)
    bzero(to+tot_length,length-tot_length);
}


void Field_varstring::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  ulong length= cs->cset->snprintf(cs,(char*) res.ptr(),
			     res.alloced_length(),"varchar(%u)",
			     field_length / charset()->mbmaxlen);
  res.length(length);
}

char *Field_varstring::pack(char *to, const char *from, uint max_length)
{
  uint length=uint2korr(from);
  if (length > max_length)
    length=max_length;
  *to++= (char) (length & 255);
  if (max_length > 255)
    *to++= (char) (length >> 8);
  if (length)
    memcpy(to, from+2, length);
  return to+length;
}


const char *Field_varstring::unpack(char *to, const char *from)
{
  uint length;
  if (field_length > 255)
  {
    length= (uint) (uchar) (*to= *from++);
    to[1]=0;
  }
  else
  {
    length=uint2korr(from);
    to[0] = *from++;
    to[1] = *from++;
  }
  if (length)
    memcpy(to+2, from, length);
  return from+length;
}


int Field_varstring::pack_cmp(const char *a, const char *b, uint key_length)
{
  uint a_length;
  uint b_length;
  if (key_length > 255)
  {
    a_length=uint2korr(a); a+=2;
    b_length=uint2korr(b); b+=2;
  }
  else
  {
    a_length= (uint) (uchar) *a++;
    b_length= (uint) (uchar) *b++;
  }
  return my_strnncoll(field_charset,
		     (const uchar *)a,a_length,
		     (const uchar *)b,b_length);
}

int Field_varstring::pack_cmp(const char *b, uint key_length)
{
  char *a=ptr+2;
  uint a_length=uint2korr(ptr);
  uint b_length;
  if (key_length > 255)
  {
    b_length=uint2korr(b); b+=2;
  }
  else
  {
    b_length= (uint) (uchar) *b++;
  }
  return my_strnncoll(field_charset,
		     (const uchar *)a,a_length,
		     (const uchar *)b,b_length);
}

uint Field_varstring::packed_col_length(const char *data_ptr, uint length)
{
  if (length > 255)
    return uint2korr(data_ptr)+2;
  else
    return (uint) ((uchar) *data_ptr)+1;
}

uint Field_varstring::max_packed_col_length(uint max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}

void Field_varstring::get_key_image(char *buff, uint length, CHARSET_INFO *cs,
				    imagetype type)
{
  length-= HA_KEY_BLOB_LENGTH;
  uint f_length=uint2korr(ptr);
  if (f_length > length)
    f_length= length;
  int2store(buff,length);
  memcpy(buff+2,ptr+2,length);
#ifdef HAVE_purify
  if (f_length < length)
    bzero(buff+2+f_length, (length-f_length));
#endif
}

void Field_varstring::set_key_image(char *buff,uint length, CHARSET_INFO *cs)
{
  length=uint2korr(buff);			// Real length is here
  (void) Field_varstring::store(buff+2, length, cs);
}



/****************************************************************************
** blob type
** A blob is saved as a length and a pointer. The length is stored in the
** packlength slot and may be from 1-4.
****************************************************************************/

Field_blob::Field_blob(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
		       enum utype unireg_check_arg, const char *field_name_arg,
		       struct st_table *table_arg,uint blob_pack_length,
		       CHARSET_INFO *cs)
  :Field_str(ptr_arg, (1L << min(blob_pack_length,3)*8)-1L,
	     null_ptr_arg, null_bit_arg, unireg_check_arg, field_name_arg,
	     table_arg, cs),
   geom_flag(true), packlength(blob_pack_length)
{
  flags|= BLOB_FLAG;
  if (table)
    table->blob_fields++;
}


void Field_blob::store_length(uint32 number)
{
  switch (packlength) {
  case 1:
    if (number > 255)
    {
      number=255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    }
    ptr[0]= (uchar) number;
    break;
  case 2:
    if (number > (uint16) ~0)
    {
      number= (uint16) ~0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    }
#ifdef WORDS_BIGENDIAN
    if (table->db_low_byte_first)
    {
      int2store(ptr,(unsigned short) number);
    }
    else
#endif
      shortstore(ptr,(unsigned short) number);
    break;
  case 3:
    if (number > (uint32) (1L << 24))
    {
      number= (uint32) (1L << 24)-1L;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    }
    int3store(ptr,number);
    break;
  case 4:
#ifdef WORDS_BIGENDIAN
    if (table->db_low_byte_first)
    {
      int4store(ptr,number);
    }
    else
#endif
      longstore(ptr,number);
  }
}


uint32 Field_blob::get_length(const char *pos)
{
  switch (packlength) {
  case 1:
    return (uint32) (uchar) pos[0];
  case 2:
    {
      uint16 tmp;
#ifdef WORDS_BIGENDIAN
      if (table->db_low_byte_first)
	tmp=sint2korr(pos);
      else
#endif
	shortget(tmp,pos);
      return (uint32) tmp;
    }
  case 3:
    return (uint32) uint3korr(pos);
  case 4:
    {
      uint32 tmp;
#ifdef WORDS_BIGENDIAN
      if (table->db_low_byte_first)
	tmp=uint4korr(pos);
      else
#endif
	longget(tmp,pos);
      return (uint32) tmp;
    }
  }
  return 0;					// Impossible
}


int Field_blob::store(const char *from,uint length,CHARSET_INFO *cs)
{
  if (!length)
  {
    bzero(ptr,Field_blob::pack_length());
  }
  else
  {
    char buff[80];
    String tmpstr(buff,sizeof(buff), &my_charset_bin);
    /* Convert character set if nesessary */
    if (use_conversion(cs, field_charset))
    { 
      tmpstr.copy(from, length, cs, field_charset);
      from= tmpstr.ptr();
      length=  tmpstr.length();
    }
    Field_blob::store_length(length);
    if (table->copy_blobs || length <= MAX_FIELD_WIDTH)
    {						// Must make a copy
      if (from != value.ptr())			// For valgrind
      {
	value.copy(from,length,charset());
	from=value.ptr();
      }
    }
    bmove(ptr+packlength,(char*) &from,sizeof(char*));
  }
  return 0;
}


int Field_blob::store(double nr)
{
  CHARSET_INFO *cs=charset();
  value.set(nr, 2, cs);
  return Field_blob::store(value.ptr(),(uint) value.length(), cs);
}


int Field_blob::store(longlong nr)
{
  CHARSET_INFO *cs=charset();
  value.set(nr, cs);
  return Field_blob::store(value.ptr(), (uint) value.length(), cs);
}


double Field_blob::val_real(void)
{
  int not_used;
  char *blob;
  memcpy_fixed(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    return 0.0;
  uint32 length=get_length(ptr);
  CHARSET_INFO *cs=charset();
  return my_strntod(cs,blob,length,(char**)0, &not_used);
}


longlong Field_blob::val_int(void)
{
  int not_used;
  char *blob;
  memcpy_fixed(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    return 0;
  uint32 length=get_length(ptr);
  return my_strntoll(charset(),blob,length,10,NULL,&not_used);
}


String *Field_blob::val_str(String *val_buffer __attribute__((unused)),
			    String *val_ptr)
{
  char *blob;
  memcpy_fixed(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    val_ptr->set("",0,charset());	// A bit safer than ->length(0)
  else
    val_ptr->set((const char*) blob,get_length(ptr),charset());
  return val_ptr;
}


int Field_blob::cmp(const char *a,uint32 a_length, const char *b,
		    uint32 b_length)
{
  int diff=my_strnncoll(field_charset,
			(const uchar*)a,min(a_length,b_length),
			(const uchar*)b,min(a_length,b_length));
  return diff ? diff : (int) (a_length - b_length);
}


int Field_blob::cmp(const char *a_ptr, const char *b_ptr)
{
  char *blob1,*blob2;
  memcpy_fixed(&blob1,a_ptr+packlength,sizeof(char*));
  memcpy_fixed(&blob2,b_ptr+packlength,sizeof(char*));
  return Field_blob::cmp(blob1,get_length(a_ptr),
			 blob2,get_length(b_ptr));
}


int Field_blob::cmp_offset(uint row_offset)
{
  return Field_blob::cmp(ptr,ptr+row_offset);
}


int Field_blob::cmp_binary_offset(uint row_offset)
{
  return cmp_binary(ptr, ptr+row_offset);
}


int Field_blob::cmp_binary(const char *a_ptr, const char *b_ptr,
			   uint32 max_length)
{
  char *a,*b;
  uint diff;
  uint32 a_length,b_length;
  memcpy_fixed(&a,a_ptr+packlength,sizeof(char*));
  memcpy_fixed(&b,b_ptr+packlength,sizeof(char*));
  a_length=get_length(a_ptr);
  if (a_length > max_length)
    a_length=max_length;
  b_length=get_length(b_ptr);
  if (b_length > max_length)
    b_length=max_length;
  diff=memcmp(a,b,min(a_length,b_length));
  return diff ? diff : (int) (a_length - b_length);
}


/* The following is used only when comparing a key */

void Field_blob::get_key_image(char *buff,uint length,
			       CHARSET_INFO *cs, imagetype type)
{
  length-= HA_KEY_BLOB_LENGTH;
  uint32 blob_length= get_length(ptr);
  char *blob;

  if (type == itMBR)
  {
    if (!blob_length)
      return;
    get_ptr(&blob);

    MBR mbr;
    Geometry gobj;
    gobj.create_from_wkb(blob + SRID_SIZE, blob_length - SRID_SIZE);
    gobj.get_mbr(&mbr);
    float8store(buff,    mbr.xmin);
    float8store(buff+8,  mbr.xmax);
    float8store(buff+16, mbr.ymin);
    float8store(buff+24, mbr.ymax);
    return;
  }

  if ((uint32) length > blob_length)
  {
    /*
      Must clear this as we do a memcmp in opt_range.cc to detect
      identical keys
    */
    bzero(buff+2+blob_length, (length-blob_length));
    length=(uint) blob_length;
  }
  int2store(buff,length);
  get_ptr(&blob);
  memcpy(buff+2,blob,length);
}

void Field_blob::set_key_image(char *buff,uint length, CHARSET_INFO *cs)
{
  length=uint2korr(buff);
  (void) Field_blob::store(buff+2,length,cs);
}


int Field_blob::key_cmp(const byte *key_ptr, uint max_key_length)
{
  char *blob1;
  uint blob_length=get_length(ptr);
  max_key_length-=2;
  memcpy_fixed(&blob1,ptr+packlength,sizeof(char*));
  return Field_blob::cmp(blob1,min(blob_length, max_key_length),
			 (char*) key_ptr+2,uint2korr(key_ptr));
}

int Field_blob::key_cmp(const byte *a,const byte *b)
{
  return Field_blob::cmp((char*) a+2,uint2korr(a),
			 (char*) b+2,uint2korr(b));
}


void Field_blob::sort_string(char *to,uint length)
{
  char *blob;
  uint blob_length=get_length();

  if (!blob_length)
    bzero(to,length);
  else
  {
    memcpy_fixed(&blob,ptr+packlength,sizeof(char*));
    
    blob_length=my_strnxfrm(field_charset,
                            (unsigned char *)to, length, 
                            (unsigned char *)blob, blob_length);
    if (blob_length < length)
      bzero(to+blob_length, length-blob_length);
  }
}


void Field_blob::sql_type(String &res) const
{
  const char *str;
  uint length;
  switch (packlength) {
  default: str="tiny"; length=4; break;
  case 2:  str="";     length=0; break;
  case 3:  str="medium"; length= 6; break;
  case 4:  str="long";  length=4; break;
  }
  res.set_ascii(str,length);
  if (charset() == &my_charset_bin)
    res.append("blob");
  else
  {
    res.append("text");
  }
}


char *Field_blob::pack(char *to, const char *from, uint max_length)
{
  char *save=ptr;
  ptr=(char*) from;
  uint32 length=get_length();			// Length of from string
  if (length > max_length)
  {
    ptr=to;
    length=max_length;
    store_length(length);			// Store max length
    ptr=(char*) from;
  }
  else
    memcpy(to,from,packlength);			// Copy length
  if (length)
  {
    get_ptr((char**) &from);
    memcpy(to+packlength, from,length);
  }
  ptr=save;					// Restore org row pointer
  return to+packlength+length;
}


const char *Field_blob::unpack(char *to, const char *from)
{
  memcpy(to,from,packlength);
  uint32 length=get_length(from);
  from+=packlength;
  if (length)
    memcpy_fixed(to+packlength, &from, sizeof(from));
  else
    bzero(to+packlength,sizeof(from));
  return from+length;
}

/* Keys for blobs are like keys on varchars */

int Field_blob::pack_cmp(const char *a, const char *b, uint key_length)
{
  uint a_length;
  uint b_length;
  if (key_length > 255)
  {
    a_length=uint2korr(a); a+=2;
    b_length=uint2korr(b); b+=2;
  }
  else
  {
    a_length= (uint) (uchar) *a++;
    b_length= (uint) (uchar) *b++;
  }
  return my_strnncoll(field_charset,
		     (const uchar *)a,a_length,
		     (const uchar *)b,b_length);
}


int Field_blob::pack_cmp(const char *b, uint key_length)
{
  char *a;
  memcpy_fixed(&a,ptr+packlength,sizeof(char*));
  if (!a)
    return key_length > 0 ? -1 : 0;
  uint a_length=get_length(ptr);
  uint b_length;

  if (key_length > 255)
  {
    b_length=uint2korr(b); b+=2;
  }
  else
  {
    b_length= (uint) (uchar) *b++;
  }
  return my_strnncoll(field_charset,
		     (const uchar *)a,a_length,
		     (const uchar *)b,b_length);
}

/* Create a packed key that will be used for storage from a MySQL row */

char *Field_blob::pack_key(char *to, const char *from, uint max_length)
{
  char *save=ptr;
  ptr=(char*) from;
  uint32 length=get_length();			// Length of from string
  if (length > max_length)
    length=max_length;
  *to++= (uchar) length;
  if (max_length > 255)				// 2 byte length
    *to++= (uchar) (length >> 8);
  if (length)
  {
    get_ptr((char**) &from);
    memcpy(to, from, length);
  }
  ptr=save;					// Restore org row pointer
  return to+length;
}

/* Create a packed key that will be used for storage from a MySQL key */

char *Field_blob::pack_key_from_key_image(char *to, const char *from,
					  uint max_length)
{
  uint length=uint2korr(from);
  if (length > max_length)
    length=max_length;
  *to++= (char) (length & 255);
  if (max_length > 255)
    *to++= (char) (length >> 8);
  if (length)
    memcpy(to, from+2, length);
  return to+length;
}

uint Field_blob::packed_col_length(const char *data_ptr, uint length)
{
  if (length > 255)
    return uint2korr(data_ptr)+2;
  else
    return (uint) ((uchar) *data_ptr)+1;
}

uint Field_blob::max_packed_col_length(uint max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}


void Field_geom::get_key_image(char *buff, uint length, CHARSET_INFO *cs,
			       imagetype type)
{
  length-= HA_KEY_BLOB_LENGTH;
  ulong blob_length= get_length(ptr);
  char *blob;
  get_ptr(&blob);

  MBR mbr;
  Geometry gobj;
  gobj.create_from_wkb(blob + SRID_SIZE, blob_length - SRID_SIZE);
  gobj.get_mbr(&mbr);
  float8store(buff, mbr.xmin);
  float8store(buff + 8, mbr.xmax);
  float8store(buff + 16, mbr.ymin);
  float8store(buff + 24, mbr.ymax);
  return;
}


void Field_geom::set_key_image(char *buff, uint length, CHARSET_INFO *cs)
{
  Field_blob::set_key_image(buff, length, cs);
}

void Field_geom::sql_type(String &res) const
{
  CHARSET_INFO *cs= &my_charset_latin1;
  switch (geom_type)
  {
    case GEOM_POINT:
     res.set("point", 5, cs);
     break;
    case GEOM_LINESTRING:
     res.set("linestring", 10, cs);
     break;
    case GEOM_POLYGON:
     res.set("polygon", 7, cs);
     break;
    case GEOM_MULTIPOINT:
     res.set("multipoint", 10, cs);
     break;
    case GEOM_MULTILINESTRING:
     res.set("multilinestring", 15, cs);
     break;
    case GEOM_MULTIPOLYGON:
     res.set("multipolygon", 12, cs);
     break;
    case GEOM_GEOMETRYCOLLECTION:
     res.set("geometrycollection", 18, cs);
     break;
    default:
     res.set("geometry", 8, cs);
  }
}


int Field_geom::store(const char *from, uint length, CHARSET_INFO *cs)
{
  if (!length)
  {
    bzero(ptr, Field_blob::pack_length());
  }
  else
  {
    // Should check given WKB
    if (length < 4 + 1 + 4 + 8 + 8)		// SRID + WKB_HEADER + X + Y
      return 1;
    uint32 wkb_type= uint4korr(from + 5);
    if (wkb_type < 1 || wkb_type > 7)
      return 1;
    Field_blob::store_length(length);
    if (table->copy_blobs || length <= MAX_FIELD_WIDTH)
    {						// Must make a copy
      value.copy(from, length, cs);
      from= value.ptr();
    }
    bmove(ptr + packlength, (char*) &from, sizeof(char*));
  }
  return 0;
}


/****************************************************************************
** enum type.
** This is a string which only can have a selection of different values.
** If one uses this string in a number context one gets the type number.
****************************************************************************/

enum ha_base_keytype Field_enum::key_type() const
{
  switch (packlength) {
  default: return HA_KEYTYPE_BINARY;
  case 2: return HA_KEYTYPE_USHORT_INT;
  case 3: return HA_KEYTYPE_UINT24;
  case 4: return HA_KEYTYPE_ULONG_INT;
  case 8: return HA_KEYTYPE_ULONGLONG;
  }
}

void Field_enum::store_type(ulonglong value)
{
  switch (packlength) {
  case 1: ptr[0]= (uchar) value;  break;
  case 2:
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int2store(ptr,(unsigned short) value);
  }
  else
#endif
    shortstore(ptr,(unsigned short) value);
  break;
  case 3: int3store(ptr,(long) value); break;
  case 4:
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(ptr,value);
  }
  else
#endif
    longstore(ptr,(long) value);
  break;
  case 8:
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int8store(ptr,value);
  }
  else
#endif
    longlongstore(ptr,value); break;
  }
}


/*
** Note. Storing a empty string in a enum field gives a warning
** (if there isn't a empty value in the enum)
*/

int Field_enum::store(const char *from,uint length,CHARSET_INFO *cs)
{
  int err= 0;
  char buff[80];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);
  /* Convert character set if nesessary */
  if (use_conversion(cs, field_charset))
  { 
    tmpstr.copy(from, length, cs, field_charset);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }

  /* Remove end space */
  while (length > 0 && my_isspace(system_charset_info,from[length-1]))
    length--;
  uint tmp=find_type(typelib, from, length, 0);
  if (!tmp)
  {
    if (length < 6)			// Can't be more than 99999 enums
    {
      /* This is for reading numbers with LOAD DATA INFILE */
      char *end;
      tmp=(uint) my_strntoul(cs,from,length,10,&end,&err);
      if (err || end != from+length || tmp > typelib->count)
      {
	tmp=0;
	set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
      }
    }
    else
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
  }
  store_type((ulonglong) tmp);
  return err;
}


int Field_enum::store(double nr)
{
  return Field_enum::store((longlong) nr);
}


int Field_enum::store(longlong nr)
{
  int error= 0;
  if ((uint) nr > typelib->count || nr == 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    nr=0;
    error=1;
  }
  store_type((ulonglong) (uint) nr);
  return error;
}


double Field_enum::val_real(void)
{
  return (double) Field_enum::val_int();
}


longlong Field_enum::val_int(void)
{
  switch (packlength) {
  case 1:
    return (longlong) (uchar) ptr[0];
  case 2:
    {
      uint16 tmp;
#ifdef WORDS_BIGENDIAN
      if (table->db_low_byte_first)
	tmp=sint2korr(ptr);
      else
#endif
	shortget(tmp,ptr);
      return (longlong) tmp;
    }
  case 3:
    return (longlong) uint3korr(ptr);
  case 4:
    {
      uint32 tmp;
#ifdef WORDS_BIGENDIAN
      if (table->db_low_byte_first)
	tmp=uint4korr(ptr);
      else
#endif
	longget(tmp,ptr);
      return (longlong) tmp;
    }
  case 8:
    {
      longlong tmp;
#ifdef WORDS_BIGENDIAN
      if (table->db_low_byte_first)
	tmp=sint8korr(ptr);
      else
#endif
	longlongget(tmp,ptr);
      return tmp;
    }
  }
  return 0;					// impossible
}


String *Field_enum::val_str(String *val_buffer __attribute__((unused)),
			    String *val_ptr)
{
  uint tmp=(uint) Field_enum::val_int();
  if (!tmp || tmp > typelib->count)
    val_ptr->length(0);
  else
    val_ptr->set((const char*) typelib->type_names[tmp-1],
		 (uint) strlen(typelib->type_names[tmp-1]),
		 field_charset);
  return val_ptr;
}

int Field_enum::cmp(const char *a_ptr, const char *b_ptr)
{
  char *old=ptr;
  ptr=(char*) a_ptr;
  ulonglong a=Field_enum::val_int();
  ptr=(char*) b_ptr;
  ulonglong b=Field_enum::val_int();
  ptr=old;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_enum::sort_string(char *to,uint length __attribute__((unused)))
{
  ulonglong value=Field_enum::val_int();
  to+=packlength-1;
  for (uint i=0 ; i < packlength ; i++)
  {
    *to-- = (uchar) (value & 255);
    value>>=8;
  }
}


void Field_enum::sql_type(String &res) const
{
  res.length(0);
  res.append("enum(");

  bool flag=0;
  for (const char **pos=typelib->type_names; *pos ; pos++)
  {
    if (flag)
      res.append(',');
    append_unescaped(&res, *pos, strlen(*pos));
    flag=1;
  }
  res.append(')');
}


/*
   set type.
   This is a string which can have a collection of different values.
   Each string value is separated with a ','.
   For example "One,two,five"
   If one uses this string in a number context one gets the bits as a longlong
   number.
*/


int Field_set::store(const char *from,uint length,CHARSET_INFO *cs)
{
  bool set_warning= 0;
  int err= 0;
  char *not_used;
  uint not_used2;
  char buff[80];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);
  /* Convert character set if nesessary */
  if (use_conversion(cs, field_charset))
  { 
    tmpstr.copy(from, length, cs, field_charset);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }
  ulonglong tmp= find_set(typelib, from, length, &not_used, &not_used2,
			  &set_warning);
  if (!tmp && length && length < 22)
  {
    /* This is for reading numbers with LOAD DATA INFILE */
    char *end;
    tmp=my_strntoull(cs,from,length,10,&end,&err);
    if (err || end != from+length ||
	tmp > (ulonglong) (((longlong) 1 << typelib->count) - (longlong) 1))
    {
      tmp=0;      
      THD *thd= current_thd;
      if (thd->count_cuted_fields)
      {
	thd->cuted_fields++;
	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
			    ER_WARN_DATA_TRUNCATED, ER(ER_WARN_DATA_TRUNCATED),
			    field_name, 0);
      }
    }
  }
  store_type(tmp);
  return err;
}


int Field_set::store(longlong nr)
{
  int error= 0;
  if ((ulonglong) nr > (ulonglong) (((longlong) 1 << typelib->count) -
				    (longlong) 1))
  {
    nr&= (longlong) (((longlong) 1 << typelib->count) - (longlong) 1);    
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED);
    error=1;
  }
  store_type((ulonglong) nr);
  return error;
}


String *Field_set::val_str(String *val_buffer,
			   String *val_ptr __attribute__((unused)))
{
  ulonglong tmp=(ulonglong) Field_enum::val_int();
  uint bitnr=0;

  val_buffer->length(0);
  while (tmp && bitnr < (uint) typelib->count)
  {
    if (tmp & 1)
    {
      if (val_buffer->length())
	val_buffer->append(field_separator);
      String str(typelib->type_names[bitnr],
		 (uint) strlen(typelib->type_names[bitnr]),
		 field_charset);
      val_buffer->append(str);
    }
    tmp>>=1;
    bitnr++;
  }
  return val_buffer;
}


void Field_set::sql_type(String &res) const
{
  res.length(0);
  res.append("set(");

  bool flag=0;
  for (const char **pos=typelib->type_names; *pos ; pos++)
  {
    if (flag)
      res.append(',');
    append_unescaped(&res, *pos, strlen(*pos));
    flag=1;
  }
  res.append(')');
}

/* returns 1 if the fields are equally defined */

bool Field::eq_def(Field *field)
{
  if (real_type() != field->real_type() || charset() != field->charset() ||
      pack_length() != field->pack_length())
    return 0;
  return 1;
}

bool Field_enum::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;
  TYPELIB *from_lib=((Field_enum*) field)->typelib;

  if (typelib->count < from_lib->count)
    return 0;
  for (uint i=0 ; i < from_lib->count ; i++)
    if (my_strcasecmp(field_charset,
                      typelib->type_names[i],from_lib->type_names[i]))
      return 0;
  return 1;
}

bool Field_num::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;
  Field_num *from_num= (Field_num*) field;

  if (unsigned_flag != from_num->unsigned_flag ||
      zerofill && !from_num->zerofill && !zero_pack() ||
      dec != from_num->dec)
    return 0;
  return 1;
}


/*****************************************************************************
** Handling of field and create_field
*****************************************************************************/

void create_field::create_length_to_internal_length(void)
{
  switch (sql_type)
  {
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      length*= charset->mbmaxlen;
      pack_length= calc_pack_length(sql_type == FIELD_TYPE_VAR_STRING ?
				    FIELD_TYPE_STRING : sql_type, length);
      break;
    default:
      /* do nothing */
      break;
  }
}

/*
  Make a field from the .frm file info
*/

uint32 calc_pack_length(enum_field_types type,uint32 length)
{
  switch (type) {
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_DECIMAL: return (length);
  case FIELD_TYPE_VAR_STRING: return (length+2);
  case FIELD_TYPE_YEAR:
  case FIELD_TYPE_TINY	: return 1;
  case FIELD_TYPE_SHORT : return 2;
  case FIELD_TYPE_INT24:
  case FIELD_TYPE_NEWDATE:
  case FIELD_TYPE_TIME:   return 3;
  case FIELD_TYPE_TIMESTAMP:
  case FIELD_TYPE_DATE:
  case FIELD_TYPE_LONG	: return 4;
  case FIELD_TYPE_FLOAT : return sizeof(float);
  case FIELD_TYPE_DOUBLE: return sizeof(double);
  case FIELD_TYPE_DATETIME:
  case FIELD_TYPE_LONGLONG: return 8;	/* Don't crash if no longlong */
  case FIELD_TYPE_NULL	: return 0;
  case FIELD_TYPE_TINY_BLOB:	return 1+portable_sizeof_char_ptr;
  case FIELD_TYPE_BLOB:		return 2+portable_sizeof_char_ptr;
  case FIELD_TYPE_MEDIUM_BLOB:	return 3+portable_sizeof_char_ptr;
  case FIELD_TYPE_LONG_BLOB:	return 4+portable_sizeof_char_ptr;
  case FIELD_TYPE_GEOMETRY:	return 4+portable_sizeof_char_ptr;
  case FIELD_TYPE_SET:
  case FIELD_TYPE_ENUM: abort(); return 0;	// This shouldn't happen
  default: return 0;
  }
  return 0;					// This shouldn't happen
}


uint pack_length_to_packflag(uint type)
{
  switch (type) {
    case 1: return f_settype((uint) FIELD_TYPE_TINY);
    case 2: return f_settype((uint) FIELD_TYPE_SHORT);
    case 3: return f_settype((uint) FIELD_TYPE_INT24);
    case 4: return f_settype((uint) FIELD_TYPE_LONG);
    case 8: return f_settype((uint) FIELD_TYPE_LONGLONG);
  }
  return 0;					// This shouldn't happen
}


Field *make_field(char *ptr, uint32 field_length,
		  uchar *null_pos, uchar null_bit,
		  uint pack_flag,
		  enum_field_types field_type,
		  CHARSET_INFO *field_charset,
		  Field::geometry_type geom_type,
		  Field::utype unireg_check,
		  TYPELIB *interval,
		  const char *field_name,
		  struct st_table *table)
{
  if (!f_maybe_null(pack_flag))
  {
    null_pos=0;
    null_bit=0;
  }
  if (f_is_alpha(pack_flag))
  {
    if (!f_is_packed(pack_flag))
      return new Field_string(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name, table, field_charset);

    uint pack_length=calc_pack_length((enum_field_types)
				      f_packtype(pack_flag),
				      field_length);

    if (f_is_geom(pack_flag))
      return new Field_geom(ptr,null_pos,null_bit,
			    unireg_check, field_name, table,
			    pack_length, geom_type);
    if (f_is_blob(pack_flag))
      return new Field_blob(ptr,null_pos,null_bit,
			    unireg_check, field_name, table,
			    pack_length, field_charset);
    if (interval)
    {
      if (f_is_enum(pack_flag))
	return new Field_enum(ptr,field_length,null_pos,null_bit,
				  unireg_check, field_name, table,
				  pack_length, interval, field_charset);
      else
	return new Field_set(ptr,field_length,null_pos,null_bit,
			     unireg_check, field_name, table,
			     pack_length, interval, field_charset);
    }
  }

  switch (field_type) {
  case FIELD_TYPE_DECIMAL:
    return new Field_decimal(ptr,field_length,null_pos,null_bit,
			     unireg_check, field_name, table,
			     f_decimals(pack_flag),
			     f_is_zerofill(pack_flag) != 0,
			     f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_FLOAT:
    return new Field_float(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_decimals(pack_flag),
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag)== 0);
  case FIELD_TYPE_DOUBLE:
    return new Field_double(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name, table,
			    f_decimals(pack_flag),
			    f_is_zerofill(pack_flag) != 0,
			    f_is_dec(pack_flag)== 0);
  case FIELD_TYPE_TINY:
    return new Field_tiny(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name, table,
			  f_is_zerofill(pack_flag) != 0,
			  f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_SHORT:
    return new Field_short(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_INT24:
    return new Field_medium(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name, table,
			    f_is_zerofill(pack_flag) != 0,
			    f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_LONG:
    return new Field_long(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_LONGLONG:
    return new Field_longlong(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name, table,
			      f_is_zerofill(pack_flag) != 0,
			      f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_TIMESTAMP:
    return new Field_timestamp(ptr,field_length,
			       unireg_check, field_name, table, field_charset);
  case FIELD_TYPE_YEAR:
    return new Field_year(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name, table);
  case FIELD_TYPE_DATE:
    return new Field_date(ptr,null_pos,null_bit,
			  unireg_check, field_name, table, field_charset);
  case FIELD_TYPE_NEWDATE:
    return new Field_newdate(ptr,null_pos,null_bit,
			     unireg_check, field_name, table, field_charset);
  case FIELD_TYPE_TIME:
    return new Field_time(ptr,null_pos,null_bit,
			  unireg_check, field_name, table, field_charset);
  case FIELD_TYPE_DATETIME:
    return new Field_datetime(ptr,null_pos,null_bit,
			      unireg_check, field_name, table, field_charset);
  case FIELD_TYPE_NULL:
    return new Field_null(ptr,field_length,unireg_check,field_name,table, field_charset);
  default:					// Impossible (Wrong version)
    break;
  }
  return 0;
}


/* Create a field suitable for create of table */

create_field::create_field(Field *old_field,Field *orig_field)
{
  field=      old_field;
  field_name=change=old_field->field_name;
  length=     old_field->field_length;
  flags=      old_field->flags;
  unireg_check=old_field->unireg_check;
  pack_length=old_field->pack_length();
  sql_type=   old_field->real_type();
  charset=    old_field->charset();		// May be NULL ptr
  comment=    old_field->comment;

  /* Fix if the original table had 4 byte pointer blobs */
  if (flags & BLOB_FLAG)
    pack_length= (pack_length- old_field->table->blob_ptr_size +
		  portable_sizeof_char_ptr);

  switch (sql_type)
  {
    case FIELD_TYPE_BLOB:
      switch (pack_length - portable_sizeof_char_ptr)
      {
        case  1: sql_type= FIELD_TYPE_TINY_BLOB; break;
        case  2: sql_type= FIELD_TYPE_BLOB; break;
        case  3: sql_type= FIELD_TYPE_MEDIUM_BLOB; break;
        default: sql_type= FIELD_TYPE_LONG_BLOB; break;
      }
      length /= charset->mbmaxlen;		// QQ: Probably not needed
      break;
    case FIELD_TYPE_STRING:
    case FIELD_TYPE_VAR_STRING:
      length /= charset->mbmaxlen;
      break;
    default:
      break;
  }
  
  decimals= old_field->decimals();
  if (sql_type == FIELD_TYPE_STRING)
  {
    /* Change CHAR -> VARCHAR if dynamic record length */
    sql_type=old_field->type();
    decimals=0;
  }
  if (flags & (ENUM_FLAG | SET_FLAG))
    interval= ((Field_enum*) old_field)->typelib;
  else
    interval=0;
  def=0;
  if (!old_field->is_real_null() && ! (flags & BLOB_FLAG) &&
      old_field->type() != FIELD_TYPE_TIMESTAMP && old_field->ptr &&
      orig_field)
  {
    char buff[MAX_FIELD_WIDTH],*pos;
    String tmp(buff,sizeof(buff), charset);

    /* Get the value from default_values */
    my_ptrdiff_t diff= (my_ptrdiff_t) (orig_field->table->rec_buff_length*2);
    orig_field->move_field(diff);		// Points now at default_values
    bool is_null=orig_field->is_real_null();
    orig_field->val_str(&tmp,&tmp);
    orig_field->move_field(-diff);		// Back to record[0]
    if (!is_null)
    {
      pos= (char*) sql_memdup(tmp.ptr(),tmp.length()+1);
      pos[tmp.length()]=0;
      def=new Item_string(pos,tmp.length(), charset);
    }
  }
  if (sql_type == FIELD_TYPE_GEOMETRY)
  {
    geom_type= ((Field_geom*)old_field)->geom_type;
  }
}


/* Warning handling */
void Field::set_warning(const uint level, const uint code)
{
  THD *thd= current_thd;
  if (thd->count_cuted_fields)
  {
    thd->cuted_fields++;
    push_warning_printf(thd, (MYSQL_ERROR::enum_warning_level) level, 
			code, ER(code), field_name, thd->row_count);
  }
}
