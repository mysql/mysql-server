#ifndef FIELD_INCLUDED
#define FIELD_INCLUDED

/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "my_base.h"                            // ha_storage_media
#include "my_compare.h"                         // portable_sizeof_char_ptr
#include "my_time.h"                            // MYSQL_TIME_NOTE_TRUNCATED
#include "binary_log_funcs.h"                   // my_time_binary_length
#include "handler.h"                            // column_format_type
#include "mysqld.h"                             // system_charset_info
#include "mysqld_error.h"                       // ER_*
#include "sql_error.h"                          // Sql_condition
#include "sql_string.h"                         // String
#include "table.h"                              // TABLE
#include "mysql_version.h"                      // FRM_VER

class Create_field;
class Json_dom;
class Json_wrapper;
class Protocol;
class Relay_log_info;
class Send_field;

/*

Field class hierarchy


Field (abstract)
|
+--Field_bit
|  +--Field_bit_as_char
|  
+--Field_num (abstract)
|  |  +--Field_real (asbstract)
|  |     +--Field_decimal
|  |     +--Field_float
|  |     +--Field_double
|  |
|  +--Field_new_decimal
|  +--Field_short
|  +--Field_medium
|  +--Field_long
|  +--Field_longlong
|  +--Field_tiny
|     +--Field_year
|
+--Field_str (abstract)
|  +--Field_longstr
|  |  +--Field_string
|  |  +--Field_varstring
|  |  +--Field_blob
|  |     +--Field_geom
|  |     +--Field_json
|  |
|  +--Field_null
|  +--Field_enum
|     +--Field_set
|
+--Field_temporal (abstract)
   +--Field_time_common (abstract)
   |  +--Field_time
   |  +--Field_timef
   |
   +--Field_temporal_with_date (abstract)
      +--Field_newdate
      +--Field_temporal_with_date_and_time (abstract)
         +--Field_timestamp
         +--Field_datetime
         +--Field_temporal_with_date_and_timef (abstract)
            +--Field_timestampf
            +--Field_datetimef
*/


enum enum_check_fields
{
  CHECK_FIELD_IGNORE,
  CHECK_FIELD_WARN,
  CHECK_FIELD_ERROR_FOR_NULL
};


enum Derivation
{
  DERIVATION_IGNORABLE= 6,
  DERIVATION_NUMERIC= 5,
  DERIVATION_COERCIBLE= 4,
  DERIVATION_SYSCONST= 3,
  DERIVATION_IMPLICIT= 2,
  DERIVATION_NONE= 1,
  DERIVATION_EXPLICIT= 0
};

/**
  Status when storing a value in a field or converting from one
  datatype to another. The values should be listed in order of
  increasing seriousness so that if two type_conversion_status
  variables are compared, the bigger one is most serious.
*/
enum type_conversion_status
{
  /// Storage/conversion went fine.
  TYPE_OK= 0,
  /**
    A minor problem when converting between temporal values, e.g.
    if datetime is converted to date the time information is lost.
  */
  TYPE_NOTE_TIME_TRUNCATED,
  /**
    Value outside min/max limit of datatype. The min/max value is
    stored by Field::store() instead (if applicable)
  */
  TYPE_WARN_OUT_OF_RANGE,
  /**
    Value was stored, but something was cut. What was cut is
    considered insignificant enough to only issue a note. Example:
    trying to store a number with 5 decimal places into a field that
    can only store 3 decimals. The number rounded to 3 decimal places
    should be stored. Another example: storing the string "foo " into
    a VARCHAR(3). The string "foo" is stored in this case, so only
    whitespace is cut.
  */
  TYPE_NOTE_TRUNCATED,
  /**
    Value was stored, but something was cut. What was cut is
    considered significant enough to issue a warning. Example: storing
    the string "foo" into a VARCHAR(2). The string "fo" is stored in
    this case. Another example: storing the string "2010-01-01foo"
    into a DATE. The garbage in the end of the string is cut in this
    case.
  */
  TYPE_WARN_TRUNCATED,
  /**
    Value has invalid string data. When present in a predicate with
    equality operator, range optimizer returns an impossible where.
  */
  TYPE_WARN_INVALID_STRING,
  /// Trying to store NULL in a NOT NULL field.
  TYPE_ERR_NULL_CONSTRAINT_VIOLATION,
  /**
    Store/convert incompatible values, like converting "foo" to a
    date.
  */
  TYPE_ERR_BAD_VALUE,
  /// Out of memory
  TYPE_ERR_OOM
};

/*
  Some defines for exit codes for ::is_equal class functions.
*/
#define IS_EQUAL_NO 0
#define IS_EQUAL_YES 1
#define IS_EQUAL_PACK_LENGTH 2

#define STORAGE_TYPE_MASK 7
#define COLUMN_FORMAT_MASK 7
#define COLUMN_FORMAT_SHIFT 3

#define my_charset_numeric      my_charset_latin1
#define MY_REPERTOIRE_NUMERIC   MY_REPERTOIRE_ASCII

#define FRM_VER_TRUE_VARCHAR (FRM_VER+4) /* 10 */

struct st_cache_field;
type_conversion_status field_conv(Field *to,Field *from);

inline uint get_enum_pack_length(int elements)
{
  return elements < 256 ? 1 : 2;
}

inline uint get_set_pack_length(int elements)
{
  uint len= (elements + 7) / 8;
  return len > 4 ? 8 : len;
}

inline type_conversion_status
decimal_err_to_type_conv_status(int dec_error)
{
  if (dec_error & E_DEC_OOM)
    return TYPE_ERR_OOM;

  if (dec_error & (E_DEC_DIV_ZERO | E_DEC_BAD_NUM))
    return TYPE_ERR_BAD_VALUE;

  if (dec_error & E_DEC_TRUNCATED)
    return TYPE_NOTE_TRUNCATED;

  if (dec_error & E_DEC_OVERFLOW)
    return TYPE_WARN_OUT_OF_RANGE;

  if (dec_error == E_DEC_OK)
    return TYPE_OK;

  // impossible
  DBUG_ASSERT(false);
  return TYPE_ERR_BAD_VALUE;
}

/**
  Convert warnings returned from str_to_time() and str_to_datetime()
  to their corresponding type_conversion_status codes.
*/
inline type_conversion_status
time_warning_to_type_conversion_status(const int warn)
{
  if (warn & MYSQL_TIME_NOTE_TRUNCATED)
    return TYPE_NOTE_TIME_TRUNCATED;

  if (warn & MYSQL_TIME_WARN_OUT_OF_RANGE)
    return TYPE_WARN_OUT_OF_RANGE;

  if (warn & MYSQL_TIME_WARN_TRUNCATED)
    return TYPE_NOTE_TRUNCATED;

  if (warn & (MYSQL_TIME_WARN_ZERO_DATE | MYSQL_TIME_WARN_ZERO_IN_DATE))
    return TYPE_ERR_BAD_VALUE;

  if (warn & MYSQL_TIME_WARN_INVALID_TIMESTAMP)
    // date was fine but pointed to daylight saving time switch gap
    return TYPE_OK;

  DBUG_ASSERT(!warn);
  return TYPE_OK;
}

#define ASSERT_COLUMN_MARKED_FOR_READ \
DBUG_ASSERT(!table || (!table->read_set || \
                       bitmap_is_set(table->read_set, field_index)))
#define ASSERT_COLUMN_MARKED_FOR_WRITE \
DBUG_ASSERT(!table || (!table->write_set || \
                       bitmap_is_set(table->write_set, field_index)))


/**
  Tests if field type is temporal, i.e. represents
  DATE, TIME, DATETIME or TIMESTAMP types in SQL.
     
  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal
  @retval false  If field type is not temporal
*/
inline bool is_temporal_type(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
    return true;
  default:
    return false;
  }
}


/**
  Tests if field real type is temporal, i.e. represents
  all existing implementations of
  DATE, TIME, DATETIME or TIMESTAMP types in SQL.
     
  @param type    Field real type, as returned by field->real_type()
  @retval true   If field real type is temporal
  @retval false  If field real type is not temporal
*/
inline bool is_temporal_real_type(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME2:
  case MYSQL_TYPE_TIMESTAMP2:
  case MYSQL_TYPE_DATETIME2:
    return true;
  default:
    return is_temporal_type(type);
  }
}


/**
  Tests if field type is temporal and has time part,
  i.e. represents TIME, DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with time part.
  @retval false  If field type is not temporal type with time part.
*/
inline bool is_temporal_type_with_time(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return true;
  default:
    return false;
  }
}


/**
  Tests if field type is temporal and has date part,
  i.e. represents DATE, DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with date part.
  @retval false  If field type is not temporal type with date part.
*/
inline bool is_temporal_type_with_date(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return true;
  default:
    return false;
  }
}


/**
  Tests if field type is temporal and has date and time parts,
  i.e. represents DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with date and time parts.
  @retval false  If field type is not temporal type with date and time parts.
*/
inline bool is_temporal_type_with_date_and_time(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return true;
  default:
    return false;
  }
}


/**
  Tests if field real type can have "DEFAULT CURRENT_TIMESTAMP",
  i.e. represents TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->real_type().
  @retval true   If field real type can have "DEFAULT CURRENT_TIMESTAMP".
  @retval false  If field real type can not have "DEFAULT CURRENT_TIMESTAMP".
*/
inline bool real_type_with_now_as_default(enum_field_types type)
{
  return type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_TIMESTAMP2 ||
    type == MYSQL_TYPE_DATETIME || type == MYSQL_TYPE_DATETIME2;
}


/**
  Tests if field real type can have "ON UPDATE CURRENT_TIMESTAMP",
  i.e. represents TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->real_type().
  @retval true   If field real type can have "ON UPDATE CURRENT_TIMESTAMP".
  @retval false  If field real type can not have "ON UPDATE CURRENT_TIMESTAMP".
*/
inline bool real_type_with_now_on_update(enum_field_types type)
{
  return type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_TIMESTAMP2 ||
    type == MYSQL_TYPE_DATETIME || type == MYSQL_TYPE_DATETIME2;
}


/**
   Recognizer for concrete data type (called real_type for some reason),
   returning true if it is one of the TIMESTAMP types.
*/
inline bool is_timestamp_type(enum_field_types type)
{
  return type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_TIMESTAMP2;
}


/**
  Convert temporal real types as retuned by field->real_type()
  to field type as returned by field->type().
  
  @param real_type  Real type.
  @retval           Field type.
*/
inline enum_field_types real_type_to_type(enum_field_types real_type)
{
  switch (real_type)
  {
  case MYSQL_TYPE_TIME2:
    return MYSQL_TYPE_TIME;
  case MYSQL_TYPE_DATETIME2:
    return MYSQL_TYPE_DATETIME;
  case MYSQL_TYPE_TIMESTAMP2:
    return MYSQL_TYPE_TIMESTAMP;
  case MYSQL_TYPE_NEWDATE:
    return MYSQL_TYPE_DATE;
  /* Note: NEWDECIMAL is a type, not only a real_type */
  default: return real_type;
  }
}


/**
   Copies an integer value to a format comparable with memcmp(). The
   format is characterized by the following:

   - The sign bit goes first and is unset for negative values.
   - The representation is big endian.

   The function template can be instantiated to copy from little or
   big endian values.

   @tparam Is_big_endian True if the source integer is big endian.

   @param to          Where to write the integer.
   @param to_length   Size in bytes of the destination buffer.
   @param from        Where to read the integer.
   @param from_length Size in bytes of the source integer
   @param is_unsigned True if the source integer is an unsigned value.
*/
template<bool Is_big_endian>
void copy_integer(uchar *to, size_t to_length,
                  const uchar* from, size_t from_length,
                  bool is_unsigned)
{
  if (Is_big_endian)
  {
    if (is_unsigned)
      to[0]= from[0];
    else
      to[0]= (char)(from[0] ^ 128); // Reverse the sign bit.
    memcpy(to + 1, from + 1, to_length - 1);
  }
  else
  {
    const int sign_byte= from[from_length - 1];
    if (is_unsigned)
      to[0]= sign_byte;
    else
      to[0]= static_cast<char>(sign_byte ^ 128); // Reverse the sign bit.
    for (size_t i= 1, j= from_length - 2; i < to_length; ++i, --j)
      to[i]= from[j];
  }
}


/**
  This class is used for recording the information of
  generated column. It will be created during define a
  generated column or the table is opened.

  If one field contains such an object, it means the field
  is a genereated one.
*/
class Generated_column: public Sql_alloc
{
public:
  Item *expr_item;
  LEX_STRING expr_str;
  /* It's used to free the items created in parsing generated expression */
  Item *item_free_list;
  /// Bitmap records base columns which a generated column depends on.
  MY_BITMAP base_columns_map;

  Generated_column()
    : expr_item(0), item_free_list(0),
    field_type(MYSQL_TYPE_LONG),
    stored_in_db(false), num_non_virtual_base_cols(0),
    m_expr_str_mem_root(NULL), permanent_changes_completed(false)
  {
    expr_str.str= NULL;
    expr_str.length= 0;
  };
  ~Generated_column() {}
  enum_field_types get_real_type() const
  {
    return field_type;
  }

  void set_field_type(enum_field_types fld_type)
  {
    field_type= fld_type;
  }

  bool get_field_stored() const
  {
    return stored_in_db;
  }
  void set_field_stored(bool stored)
  {
    stored_in_db= stored;
  }
  bool register_base_columns(TABLE *table);
  /**
    Get the number of non virtual base columns that this generated
    column needs.

    @return number of non virtual base columns
  */
  uint non_virtual_base_columns() const { return num_non_virtual_base_cols; }

  /**
     Duplicates a string into expr_str.

     @param root MEM_ROOT to use for allocation; if NULL, use the remembered
     one; if non-NULL, remember it.
     @param src  source string
     @param len  length of 'src' in bytes
  */
  void dup_expr_str(MEM_ROOT *root, const char *src, size_t len);

private:
  /*
    The following data is only updated by the parser and read
    when a Create_field object is created/initialized.
  */
  enum_field_types field_type;   /* Real field type*/
  bool stored_in_db;             /* Indication that the field is 
                                    phisically stored in the database*/
  /// How many non-virtual base columns in base_columns_map
  uint num_non_virtual_base_cols;

  /// MEM_ROOT which provides memory storage for expr_str.str
  MEM_ROOT *m_expr_str_mem_root;

public:
  /**
     Used to make sure permanent changes to the item tree of expr_item are
     made only once.
  */
  bool permanent_changes_completed;
};

class Proto_field
{
public:
  virtual bool send_binary(Protocol *protocol)= 0;
  virtual bool send_text(Protocol *protocol)= 0;
};

class Field: public Proto_field
{
  Field(const Item &);				/* Prevent use of these */
  void operator=(Field &);
public:

  bool has_insert_default_function() const
  {
    return unireg_check == TIMESTAMP_DN_FIELD ||
      unireg_check == TIMESTAMP_DNUN_FIELD;
  }

  bool has_update_default_function() const
  {
    return unireg_check == TIMESTAMP_UN_FIELD ||
      unireg_check == TIMESTAMP_DNUN_FIELD;
  }

  /* To do: inherit Sql_alloc and get these for free */
  static void *operator new(size_t size) throw ()
  { return sql_alloc(size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw () {
    return alloc_root(mem_root, size);
  }
  static void operator delete(void *ptr, MEM_ROOT *mem_root)
  { DBUG_ASSERT(false); /* never called */ }

  static void operator delete(void *ptr_arg, size_t size) throw()
  { TRASH(ptr_arg, size); }

  uchar		*ptr;			// Position to field in record

private:
  /**
     Byte where the @c NULL bit is stored inside a record. If this Field is a
     @c NOT @c NULL field, this member is @c NULL.
  */
  uchar *m_null_ptr;

  /**
    Flag: if the NOT-NULL field can be temporary NULL.
  */
  bool m_is_tmp_nullable;

  /**
    This is a flag with the following semantics:
      - it can be changed only when m_is_tmp_nullable is true;
      - it specifies if this field in the first current record
        (TABLE::record[0]) was set to NULL (temporary NULL).

    This flag is used for trigger handling.
  */
  bool m_is_tmp_null;

  /**
    The value of THD::count_cuted_fields at the moment of setting
    m_is_tmp_null attribute.
  */
  enum_check_fields m_count_cuted_fields_saved;

protected:
  const uchar *get_null_ptr() const
  { return m_null_ptr; }

  uchar *get_null_ptr() 
  { return m_null_ptr; }

public:
  /*
    Note that you can use table->in_use as replacement for current_thd member 
    only inside of val_*() and store() members (e.g. you can't use it in cons)
  */
  TABLE *table;                                 // Pointer for table
  TABLE *orig_table;                            // Pointer to original table
  const char	**table_name, *field_name;
  LEX_STRING	comment;
  /* Field is part of the following keys */
  key_map key_start;                /* Keys that starts with this field */
  /// Indexes which contain this field entirely (not only a prefix)
  key_map part_of_key;
  key_map part_of_sortkey;          /* ^ but only keys usable for sorting */
  /**
    All keys that include this field, but not extended by the storage engine to
    include primary key columns.
  */
  key_map part_of_key_not_extended;

  /* 
    We use three additional unireg types for TIMESTAMP to overcome limitation 
    of current binary format of .frm file. We'd like to be able to support 
    NOW() as default and on update value for such fields but unable to hold 
    this info anywhere except unireg_check field. This issue will be resolved
    in more clean way with transition to new text based .frm format.
    See also comment for Field_timestamp::Field_timestamp().
  */
  enum utype  { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
		CHECK,EMPTY,UNKNOWN_FIELD,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,
                BIT_FIELD, TIMESTAMP_OLD_FIELD, CAPITALIZE, BLOB_FIELD,
                TIMESTAMP_DN_FIELD, TIMESTAMP_UN_FIELD, TIMESTAMP_DNUN_FIELD,
                GENERATED_FIELD= 128 };
  enum geometry_type
  {
    GEOM_GEOMETRY = 0, GEOM_POINT = 1, GEOM_LINESTRING = 2, GEOM_POLYGON = 3,
    GEOM_MULTIPOINT = 4, GEOM_MULTILINESTRING = 5, GEOM_MULTIPOLYGON = 6,
    GEOM_GEOMETRYCOLLECTION = 7
  };
  enum imagetype { itRAW, itMBR};

  utype		unireg_check;
  uint32	field_length;		// Length of field
  uint32	flags;
  uint16        field_index;            // field number in fields array
  uchar		null_bit;		// Bit used to test null bit
  /**
     If true, this field was created in create_tmp_field_from_item from a NULL
     value. This means that the type of the field is just a guess, and the type
     may be freely coerced to another type.

     @see create_tmp_field_from_item
     @see Item_type_holder::get_real_type

   */
  bool is_created_from_null_item;
  /**
     True if this field belongs to some index (unlike part_of_key, the index
     might have only a prefix).
  */
  bool m_indexed;
private:
  enum enum_pushed_warnings
  {
    BAD_NULL_ERROR_PUSHED= 1,
    NO_DEFAULT_FOR_FIELD_PUSHED= 2,
    NO_DEFAULT_FOR_VIEW_FIELD_PUSHED= 4
  };

  /*
    Bitmask specifying which warnings have been already pushed in order
    not to repeat the same warning for the collmn multiple times.
    Uses values of enum_pushed_warnings to control pushed warnings.
  */
  unsigned int m_warnings_pushed;

public:
  /* Generated column data */
  Generated_column *gcol_info;
  /*
    Indication that the field is phycically stored in tables 
    rather than just generated on SQL queries.
    As of now, FALSE can only be set for virtual generated columns.
  */
  bool stored_in_db;
  bool is_gcol() const { return gcol_info; }
  bool is_virtual_gcol() const { return gcol_info && !stored_in_db; }

  Field(uchar *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
        uchar null_bit_arg, utype unireg_check_arg,
        const char *field_name_arg);

  virtual ~Field()
  { }

  void reset_warnings()
  { m_warnings_pushed= 0; }

  /**
    Turn on temporary nullability for the field.
  */
  void set_tmp_nullable()
  {
    m_is_tmp_nullable= true;
  }

  /**
    Turn off temporary nullability for the field.
  */
  void reset_tmp_nullable()
  {
    m_is_tmp_nullable= false;
  }

  /**
    Reset temporary NULL value for field
  */
  void reset_tmp_null()
  {
    m_is_tmp_null= false;
  }

  void set_tmp_null();

  /**
    @return temporary NULL-ability flag.
    @retval true if NULL can be assigned temporary to the Field.
    @retval false if NULL can not be assigned even temporary to the Field.
  */
  bool is_tmp_nullable() const
  { return m_is_tmp_nullable; }

  /**
    @return whether Field has temporary value NULL.
    @retval true if the Field has temporary value NULL.
    @retval false if the Field's value is NOT NULL, or if the temporary
    NULL-ability flag is reset.
  */
  bool is_tmp_null() const
  { return is_tmp_nullable() && m_is_tmp_null; }

  /* Store functions returns 1 on overflow and -1 on fatal error */
  virtual type_conversion_status store(const char *to, size_t length,
                                       const CHARSET_INFO *cs)=0;
  virtual type_conversion_status store(double nr)=0;
  virtual type_conversion_status store(longlong nr, bool unsigned_val)=0;
  /**
    Store a temporal value in packed longlong format into a field.
    The packed value is compatible with TIME_to_longlong_time_packed(),
    TIME_to_longlong_date_packed() or TIME_to_longlong_datetime_packed().
    Note, the value must be properly rounded or truncated according
    according to field->decimals().

    @param  nr  temporal value in packed longlong format.
    @retval false on success
    @retval true  on error
  */
  virtual type_conversion_status store_packed(longlong nr)
  {
    return store(nr, 0);
  }
  virtual type_conversion_status store_decimal(const my_decimal *d)=0;
  /**
    Store MYSQL_TIME value with the given amount of decimal digits
    into a field.

    Note, the "dec" parameter represents number of digits of the Item
    that previously created the MYSQL_TIME value. It's needed when we
    store the value into a CHAR/VARCHAR/TEXT field to display
    the proper amount of fractional digits.
    For other field types the "dec" value does not matter and is ignored.

    @param ltime   Time, date or datetime value.
    @param dec     Number of decimals in ltime.
    @retval false  on success
    @retval true   on error
  */
  virtual type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  /**
    Store MYSQL_TYPE value into a field when the number of fractional
    digits is not important or is not know.

    @param ltime   Time, date or datetime value.
    @retval false   on success
    @retval true   on error
  */
  type_conversion_status store_time(MYSQL_TIME *ltime)
  {
    return store_time(ltime, 0);
  }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *cs,
                               enum_check_fields check_level);
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
  /**
    Returns TIME value in packed longlong format.
    This method should not be called for non-temporal types.
    Temporal field types override the default method.
  */
  virtual longlong val_time_temporal()
  {
    DBUG_ASSERT(0);
    return 0;
  }
  /**
    Returns DATE/DATETIME value in packed longlong format.
    This method should not be called for non-temporal types.
    Temporal field types override the default method.
  */
  virtual longlong val_date_temporal()
  {
    DBUG_ASSERT(0);
    return 0;
  }
  /**
    Returns "native" packed longlong representation of
    a TIME or DATE/DATETIME field depending on field type.
  */
  longlong val_temporal_by_field_type()
  {
    // Return longlong TIME or DATETIME representation, depending on field type
    if (type() == MYSQL_TYPE_TIME)
      return val_time_temporal();
    DBUG_ASSERT(is_temporal_with_date());
    return val_date_temporal();
  }
  virtual my_decimal *val_decimal(my_decimal *)= 0;
  inline String *val_str(String *str) { return val_str(str, str); }
  /*
     val_str(buf1, buf2) gets two buffers and should use them as follows:
     if it needs a temp buffer to convert result to string - use buf1
       example Field_tiny::val_str()
     if the value exists as a string already - use buf2
       example Field_string::val_str()
     consequently, buf2 may be created as 'String buf;' - no memory
     will be allocated for it. buf1 will be allocated to hold a
     value if it's too small. Using allocated buffer for buf2 may result in
     an unnecessary free (and later, may be an alloc).
     This trickery is used to decrease a number of malloc calls.
  */
  virtual String *val_str(String*,String *)=0;
  String *val_int_as_str(String *val_buffer, my_bool unsigned_flag);
  /*
   str_needs_quotes() returns TRUE if the value returned by val_str() needs
   to be quoted when used in constructing an SQL query.
  */
  virtual bool str_needs_quotes() { return FALSE; }
  virtual Item_result result_type () const=0;
  /**
    Returns Item_result type of a field when it appears
    in numeric context such as:
      SELECT time_column + 1;
      SELECT SUM(time_column);
    Examples:
    - a column of type TIME, DATETIME, TIMESTAMP act as INT.
    - a column of type TIME(1), DATETIME(1), TIMESTAMP(1)
      act as DECIMAL with 1 fractional digits.
  */
  virtual Item_result numeric_context_result_type() const
  {
    return result_type();
  }
  virtual Item_result cmp_type () const { return result_type(); }
  virtual Item_result cast_to_int_type () const { return result_type(); }
  static bool type_can_have_key_part(enum_field_types);
  static enum_field_types field_type_merge(enum_field_types, enum_field_types);
  static Item_result result_merge_type(enum_field_types);
  bool gcol_expr_is_equal(const Field *field) const;
  bool gcol_expr_is_equal(const Create_field *field) const;
  virtual bool eq(Field *field)
  {
    return (ptr == field->ptr && m_null_ptr == field->m_null_ptr &&
            null_bit == field->null_bit && field->type() == type());
  }
  virtual bool eq_def(Field *field);
  
  /*
    pack_length() returns size (in bytes) used to store field data in memory
    (i.e. it returns the maximum size of the field in a row of the table,
    which is located in RAM).
  */
  virtual uint32 pack_length() const { return (uint32) field_length; }

  /*
    pack_length_in_rec() returns size (in bytes) used to store field data on
    storage (i.e. it returns the maximal size of the field in a row of the
    table, which is located on disk).
  */
  virtual uint32 pack_length_in_rec() const { return pack_length(); }
  virtual bool compatible_field_size(uint metadata, Relay_log_info *rli,
                                     uint16 mflags, int *order);
  virtual uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_ENTER("Field::pack_length_from_metadata");
    DBUG_RETURN(field_metadata);
  }
  virtual uint row_pack_length() const { return 0; }
  virtual int save_field_metadata(uchar *first_byte)
  { return do_save_field_metadata(first_byte); }

  /*
    data_length() return the "real size" of the data in memory.
    Useful only for variable length datatypes where it's overloaded.
    By default assume the length is constant.
  */
  virtual uint32 data_length(uint row_offset= 0) { return pack_length(); }
  virtual uint32 sort_length() const { return pack_length(); }

  /**
     Get the maximum size of the data in packed format.

     @return Maximum data length of the field when packed using the
     Field::pack() function.
   */
  virtual uint32 max_data_length() const {
    return pack_length();
  };

  virtual type_conversion_status reset(void)
  {
    memset(ptr, 0, pack_length());
    return TYPE_OK;
  }
  virtual void reset_fields() {}
  /**
    Returns timestamp value in "struct timeval" format.
    This method is used in "SELECT UNIX_TIMESTAMP(field)"
    to avoid conversion from timestamp to MYSQL_TIME and back.
  */
  virtual bool get_timestamp(struct timeval *tm, int *warnings);
  /**
    Stores a timestamp value in timeval format in a field.
   
   @note 
   - store_timestamp(), get_timestamp() and store_time() do not depend on
   timezone and always work "in UTC".

   - The default implementation of this interface expects that storing the
   value will not fail. For most Field descendent classes, this is not the
   case. However, this interface is only used when the function
   CURRENT_TIMESTAMP is used as a column default expression, and currently we
   only allow TIMESTAMP and DATETIME columns to be declared with this as the
   column default. Hence it is enough that the classes implementing columns
   with these types either override this interface, or that
   store_time(MYSQL_TIME*, uint8) does not fail.

   - The column types above interpret decimals() to mean the scale of the
   fractional seconds.
   
   - We also have the limitation that the scale of a column must be the same as
   the scale of the CURRENT_TIMESTAMP. I.e. we only allow 
   
   @code
   
   [ TIMESTAMP | DATETIME ] (n) [ DEFAULT | ON UPDATE ] CURRENT_TIMESTAMP (n)

   @endcode

   Since this interface relies on the caller to truncate the value according to this
   Field's scale, it will work with all constructs that we currently allow.
  */
  virtual void store_timestamp(const timeval *tm) { DBUG_ASSERT(false); }

  /**
     Interface for legacy code. Newer code uses the store_timestamp(const
     timeval*) interface.

     @param timestamp A TIMESTAMP value in the my_time_t format.
  */
  void store_timestamp(my_time_t sec)
  {
    struct timeval tm;
    tm.tv_sec= sec;
    tm.tv_usec= 0;
    store_timestamp(&tm);
  }

  virtual void set_default()
  {
    if (has_insert_default_function())
      evaluate_insert_default_function();
    else
      copy_data(table->default_values_offset());
  }


  /**
     Evaluates the @c INSERT default function and stores the result in the
     field. If no such function exists for the column, or the function is not
     valid for the column's data type, invoking this function has no effect.
  */
  void evaluate_insert_default_function();


  /**
     Evaluates the @c UPDATE default function, if one exists, and stores the
     result in the record buffer. If no such function exists for the column,
     or the function is not valid for the column's data type, invoking this
     function has no effect.
  */
  void evaluate_update_default_function();
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32 key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  virtual enum_field_types binlog_type() const
  {
    /*
      Binlog stores field->type() as type code by default.
      This puts MYSQL_TYPE_STRING in case of CHAR, VARCHAR, SET and ENUM,
      with extra data type details put into metadata.

      We cannot store field->type() in case of temporal types with
      fractional seconds: TIME(n), DATETIME(n) and TIMESTAMP(n),
      because binlog records with MYSQL_TYPE_TIME, MYSQL_TYPE_DATETIME
      type codes do not have metadata.
      So for temporal data types with fractional seconds we'll store
      real_type() type codes instead, i.e.
      MYSQL_TYPE_TIME2, MYSQL_TYPE_DATETIME2, MYSQL_TYPE_TIMESTAMP2,
      and put precision into metatada.

      Note: perhaps binlog should eventually be modified to store
      real_type() instead of type() for all column types.
    */
    return type();
  }
  inline  int cmp(const uchar *str) { return cmp(ptr,str); }
  virtual int cmp_max(const uchar *a, const uchar *b, uint max_len)
    { return cmp(a, b); }
  virtual int cmp(const uchar *,const uchar *)=0;
  virtual int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L)
  { return memcmp(a,b,pack_length()); }
  virtual int cmp_offset(uint row_offset)
  { return cmp(ptr,ptr+row_offset); }
  virtual int cmp_binary_offset(uint row_offset)
  { return cmp_binary(ptr, ptr+row_offset); };
  virtual int key_cmp(const uchar *a,const uchar *b)
  { return cmp(a, b); }
  virtual int key_cmp(const uchar *str, uint length)
  { return cmp(ptr,str); }
  virtual uint decimals() const { return 0; }
  virtual bool is_text_key_type() const { return false; }

  /*
    Caller beware: sql_type can change str.Ptr, so check
    ptr() to see if it changed if you are using your own buffer
    in str and restore it with set() if needed
  */
  virtual void sql_type(String &str) const =0;

  bool is_temporal() const
  { return is_temporal_type(type()); }

  bool is_temporal_with_date() const
  { return is_temporal_type_with_date(type()); }

  bool is_temporal_with_time() const
  { return is_temporal_type_with_time(type()); }

  bool is_temporal_with_date_and_time() const
  { return is_temporal_type_with_date_and_time(type()); }

  /**
    Check whether the full table's row is NULL or the Field has value NULL.

    @return    true if the full table's row is NULL or the Field has value NULL
               false if neither table's row nor the Field has value NULL
  */
  bool is_null(my_ptrdiff_t row_offset= 0) const
  {
    /*
      if the field is NULLable, it returns NULLity based
      on m_null_ptr[row_offset] value. Otherwise it returns
      NULL flag depending on TABLE::has_null_row() value.

      The table may have been marked as containing only NULL values
      for all fields if it is a NULL-complemented row of an OUTER JOIN
      or if the query is an implicitly grouped query (has aggregate
      functions but no GROUP BY clause) with no qualifying rows. If
      this is the case (in which TABLE::has_null_row() is true) and the
      field is not nullable, the field is considered to be NULL.

      Do not change the order of testing. Fields may be associated
      with a TABLE object without being part of the current row.
      For NULL value check to work for these fields, they must
      have a valid m_null_ptr, and this pointer must be checked before
      TABLE::has_null_row().
    */
    if (real_maybe_null())
      return MY_TEST(m_null_ptr[row_offset] & null_bit);

    if (is_tmp_nullable())
      return m_is_tmp_null;

    return table->has_null_row();
  }

  /**
    Check whether the Field has value NULL (temporary or actual).

    @return   true if the Field has value NULL (temporary or actual)
              false if the Field has value NOT NULL.
  */
  bool is_real_null(my_ptrdiff_t row_offset= 0) const
  {
    if (real_maybe_null())
      return MY_TEST(m_null_ptr[row_offset] & null_bit);

    if (is_tmp_nullable())
      return m_is_tmp_null;

    return false;
  }

  /**
    Check if the Field has value NULL or the record specified by argument
    has value NULL for this Field.

    @return    true if the Field has value NULL or the record has value NULL
               for thois Field.
  */
  bool is_null_in_record(const uchar *record) const
  {
    if (real_maybe_null())
      return MY_TEST(record[null_offset()] & null_bit);

    return is_tmp_nullable() ? m_is_tmp_null : false;
  }

  void set_null(my_ptrdiff_t row_offset= 0);

  void set_notnull(my_ptrdiff_t row_offset= 0);

  type_conversion_status check_constraints(int mysql_errno);

  /**
    Remember the value of THD::count_cuted_fields to handle possible
    NOT-NULL constraint errors after BEFORE-trigger execution is finished.
    We should save the value of THD::count_cuted_fields before starting
    BEFORE-trigger processing since during triggers execution the
    value of THD::count_cuted_fields could be changed.
  */
  void set_count_cuted_fields(enum_check_fields count_cuted_fields)
  { m_count_cuted_fields_saved= count_cuted_fields; }

  bool maybe_null(void) const
  { return real_maybe_null() || table->is_nullable(); }

  /// @return true if this field is NULL-able, false otherwise.
  bool real_maybe_null(void) const
  { return m_null_ptr != NULL; }

  uint null_offset(const uchar *record) const
  { return (uint) (m_null_ptr - record); }

  uint null_offset() const
  { return null_offset(table->record[0]); }

  void set_null_ptr(uchar *p_null_ptr, uint p_null_bit)
  {
    m_null_ptr= p_null_ptr;
    null_bit= p_null_bit;
  }

  enum {
    LAST_NULL_BYTE_UNDEF= 0
  };

  /*
    Find the position of the last null byte for the field.

    SYNOPSIS
      last_null_byte()

    DESCRIPTION
      Return a pointer to the last byte of the null bytes where the
      field conceptually is placed.

    RETURN VALUE
      The position of the last null byte relative to the beginning of
      the record. If the field does not use any bits of the null
      bytes, the value 0 (LAST_NULL_BYTE_UNDEF) is returned.
   */
  size_t last_null_byte() const {
    size_t bytes= do_last_null_byte();
    DBUG_PRINT("debug", ("last_null_byte() ==> %ld", (long) bytes));
    DBUG_ASSERT(bytes <= table->s->null_bytes);
    return bytes;
  }

  virtual void make_field(Send_field *);

  /**
    Writes a copy of the current value in the record buffer, suitable for
    sorting using byte-by-byte comparison. Integers are always in big-endian
    regardless of hardware architecture. At most length bytes are written
    into the buffer.

    @param buff The buffer, assumed to be at least length bytes.

    @param length Number of bytes to write.
  */
  virtual void make_sort_key(uchar *buff, size_t length) = 0;
  virtual bool optimize_range(uint idx, uint part);
  /*
    This should be true for fields which, when compared with constant
    items, can be casted to longlong. In this case we will at 'fix_fields'
    stage cast the constant items to longlongs and at the execution stage
    use field->val_int() for comparison.  Used to optimize clauses like
    'a_column BETWEEN date_const, date_const'.
  */
  virtual bool can_be_compared_as_longlong() const { return false; }
  virtual void mem_free() {}
  virtual Field *new_field(MEM_ROOT *root, TABLE *new_table,
                           bool keep_type);
  virtual Field *new_key_field(MEM_ROOT *root, TABLE *new_table,
                               uchar *new_ptr, uchar *new_null_ptr,
                               uint new_null_bit);

  Field *new_key_field(MEM_ROOT *root, TABLE *new_table, uchar *new_ptr)
  { return new_key_field(root, new_table, new_ptr, m_null_ptr, null_bit); }

  /**
     Makes a shallow copy of the Field object.
     
     @note This member function must be overridden in all concrete
     subclasses. Several of the Field subclasses are concrete even though they
     are not leaf classes, so the compiler will not always catch this.

     @retval NULL If memory allocation failed.
  */ 
  virtual Field *clone() const =0;

  /**
     Makes a shallow copy of the Field object.
     
     @note This member function must be overridden in all concrete
     subclasses. Several of the Field subclasses are concrete even though they
     are not leaf classes, so the compiler will not always catch this.
     
     @param mem_root MEM_ROOT to use for memory allocation.
     @retval NULL If memory allocation failed.
   */
  virtual Field *clone(MEM_ROOT *mem_root) const = 0;

  void move_field(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg)
  {
    ptr= ptr_arg;
    m_null_ptr= null_ptr_arg;
    null_bit= null_bit_arg;
  }

  void move_field(uchar *ptr_arg)
  { ptr= ptr_arg; }

  virtual void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    ptr= ADD_TO_PTR(ptr, ptr_diff, uchar*);
    if (real_maybe_null())
      m_null_ptr= ADD_TO_PTR(m_null_ptr, ptr_diff, uchar*);
  }

  virtual void get_image(uchar *buff, size_t length, const CHARSET_INFO *cs)
  { memcpy(buff, ptr, length); }

  virtual void set_image(const uchar *buff, size_t length, const CHARSET_INFO *cs)
  { memcpy(ptr, buff, length); }


  /*
    Copy a field part into an output buffer.

    SYNOPSIS
      Field::get_key_image()
      buff   [out] output buffer
      length       output buffer size
      type         itMBR for geometry blobs, otherwise itRAW

    DESCRIPTION
      This function makes a copy of field part of size equal to or
      less than "length" parameter value.
      For fields of string types (CHAR, VARCHAR, TEXT) the rest of buffer
      is padded by zero byte.

    NOTES
      For variable length character fields (i.e. UTF-8) the "length"
      parameter means a number of output buffer bytes as if all field
      characters have maximal possible size (mbmaxlen). In the other words,
      "length" parameter is a number of characters multiplied by
      field_charset->mbmaxlen.

    RETURN
      Number of copied bytes (excluding padded zero bytes -- see above).
  */

  virtual size_t get_key_image(uchar *buff, size_t length, imagetype type)
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual void set_key_image(const uchar *buff, size_t length)
    { set_image(buff,length, &my_charset_bin); }
  inline longlong val_int_offset(uint row_offset)
    {
      ptr+=row_offset;
      longlong tmp=val_int();
      ptr-=row_offset;
      return tmp;
    }
  inline longlong val_int(const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    longlong return_value;
    ptr= (uchar*) new_ptr;
    return_value= val_int();
    ptr= old_ptr;
    return return_value;
  }
  inline String *val_str(String *str, const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    ptr= (uchar*) new_ptr;
    val_str(str);
    ptr= old_ptr;
    return str;
  }
  virtual bool send_binary(Protocol *protocol);
  virtual bool send_text(Protocol *protocol);

  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  /**
     @overload Field::pack(uchar*, const uchar*, uint, bool)
  */
  uchar *pack(uchar *to, const uchar *from)
  {
    DBUG_ENTER("Field::pack");
    uchar *result= this->pack(to, from, UINT_MAX, table->s->db_low_byte_first);
    DBUG_RETURN(result);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  /**
     @overload Field::unpack(uchar*, const uchar*, uint, bool)
  */
  const uchar *unpack(uchar* to, const uchar *from)
  {
    DBUG_ENTER("Field::unpack");
    const uchar *result= unpack(to, from, 0U, table->s->db_low_byte_first);
    DBUG_RETURN(result);
  }

  virtual uint packed_col_length(const uchar *to, uint length)
  { return length;}

  /**
    This is a wrapper around pack_length() used by filesort() to determine
    how many bytes we need for packing "addon fields".
    @returns maximum size of a row when stored in the filesort buffer.
   */
  virtual uint max_packed_col_length()
  { return pack_length(); }

  uint offset(uchar *record)
  {
    return (uint) (ptr - record);
  }

  void copy_data(my_ptrdiff_t src_record_offset);

  uint fill_cache_field(struct st_cache_field *copy);
  virtual bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  virtual bool get_time(MYSQL_TIME *ltime);
  virtual const CHARSET_INFO *charset(void) const { return &my_charset_bin; }
  virtual const CHARSET_INFO *charset_for_protocol(void) const
  { return binary() ? &my_charset_bin : charset(); }
  virtual const CHARSET_INFO *sort_charset(void) const { return charset(); }
  virtual bool has_charset(void) const { return FALSE; }
  /*
    match_collation_to_optimize_range() is to distinguish in
    range optimizer (see opt_range.cc) between real string types:
      CHAR, VARCHAR, TEXT
    and the other string-alike types with result_type() == STRING_RESULT:
      DATE, TIME, DATETIME, TIMESTAMP
    We need it to decide whether to test if collation of the operation
    matches collation of the field (needed only for real string types).
    QQ: shouldn't DATE/TIME types have their own XXX_RESULT types eventually?
  */
  virtual bool match_collation_to_optimize_range() const { return false; };
  virtual enum Derivation derivation(void) const
  { return DERIVATION_IMPLICIT; }
  virtual uint repertoire(void) const { return MY_REPERTOIRE_UNICODE30; }
  virtual void set_derivation(enum Derivation derivation_arg) { }

  /**
    Produce warning or note about data saved into field.

    @param level            - level of message (Note/Warning/Error)
    @param code             - error code of message to be produced
    @param cut_increment    - whenever we should increase cut fields count

    @note
      This function won't produce warning and increase cut fields counter
      if count_cuted_fields == CHECK_FIELD_IGNORE for current thread.

      if count_cuted_fields == CHECK_FIELD_IGNORE then we ignore notes.
      This allows us to avoid notes in optimization, like
      convert_constant_item().

    @retval
      1 if count_cuted_fields == CHECK_FIELD_IGNORE and error level is not NOTE
    @retval
      0 otherwise
  */
  bool set_warning(Sql_condition::enum_severity_level level, unsigned int code,
                   int cut_increment)
  {
    return set_warning(level, code, cut_increment, NULL, NULL);
  }

  bool set_warning(Sql_condition::enum_severity_level level, uint code,
                   int cut_increment, const char *view_db,
                   const char *view_name);

  inline bool check_overflow(int op_result)
  {
    return (op_result == E_DEC_OVERFLOW);
  }
  inline bool check_truncated(int op_result)
  {
    return (op_result == E_DEC_TRUNCATED);
  }
  bool warn_if_overflow(int op_result);
  void init(TABLE *table_arg)
  {
    orig_table= table= table_arg;
    table_name= &table_arg->alias;
  }

  /* maximum possible display length */
  virtual uint32 max_display_length()= 0;

  /**
    Whether a field being created is type-compatible with an existing one.

    Used by the ALTER TABLE code to evaluate whether the new definition
    of a table is compatible with the old definition so that it can
    determine if data needs to be copied over (table data change).
    Constraints and generation clause (default value, generation expression)
    are not checked by this function.
  */
  virtual uint is_equal(Create_field *new_field);
  /* convert decimal to longlong with overflow check */
  longlong convert_decimal2longlong(const my_decimal *val, bool unsigned_flag,
                                    bool *has_overflow);
  /* The max. number of characters */
  virtual uint32 char_length()
  {
    return field_length / charset()->mbmaxlen;
  }

  virtual geometry_type get_geometry_type() const
  {
    /* shouldn't get here. */
    DBUG_ASSERT(0);
    return GEOM_GEOMETRY;
  }
#ifndef DBUG_OFF
  /* Print field value into debug trace, in NULL-aware way. */
  void dbug_print()
  {
    if (is_real_null())
      fprintf(DBUG_FILE, "NULL");
    else
    {
      char buf[256];
      String str(buf, sizeof(buf), &my_charset_bin);
      str.length(0);
      String *pstr;
      pstr= val_str(&str);
      fprintf(DBUG_FILE, "'%s'", pstr->c_ptr_safe());
    }
  }
#endif

  ha_storage_media field_storage_type() const
  {
    return (ha_storage_media)
      ((flags >> FIELD_FLAGS_STORAGE_MEDIA) & 3);
  }

  void set_storage_type(ha_storage_media storage_type_arg)
  {
    DBUG_ASSERT(field_storage_type() == HA_SM_DEFAULT);
    flags |= (storage_type_arg << FIELD_FLAGS_STORAGE_MEDIA);
  }

  column_format_type column_format() const
  {
    return (column_format_type)
      ((flags >> FIELD_FLAGS_COLUMN_FORMAT) & 3);
  }

  void set_column_format(column_format_type column_format_arg)
  {
    DBUG_ASSERT(column_format() == COLUMN_FORMAT_TYPE_DEFAULT);
    flags |= (column_format_arg << FIELD_FLAGS_COLUMN_FORMAT);
  }

  /* Validate the value stored in a field */
  virtual type_conversion_status validate_stored_val(THD *thd)
  { return TYPE_OK; }

  /* Hash value */
  virtual void hash(ulong *nr, ulong *nr2);

  /**
    Get the upper limit of the MySQL integral and floating-point type.

    @return maximum allowed value for the field
  */
  virtual ulonglong get_max_int_value() const
  {
    DBUG_ASSERT(false);
    return 0ULL;
  }

  /* Return pointer to the actual data in memory */
  virtual void get_ptr(uchar **str) { *str= ptr; }

/**
  Checks whether a string field is part of write_set.

  @return
    FALSE  - If field is not char/varchar/....
           - If field is char/varchar/.. and is not part of write set.
    TRUE   - If field is char/varchar/.. and is part of write set.
*/
  virtual bool is_updatable() const { return FALSE; }

  /**
    Check whether field is part of the index taking the index extensions flag
    into account. Index extensions are also not applicable to UNIQUE indexes
    for loose index scans.

    @param[in]     thd             THD object
    @param[in]     cur_index       Index of the key
    @param[in]     cur_index_info  key_info object

    @retval true  Field is part of the key
    @retval false otherwise

  */
  bool is_part_of_actual_key(THD *thd, uint cur_index, KEY *cur_index_info);

  friend int cre_myisam(char * name, TABLE *form, uint options,
			ulonglong auto_increment_value);
  friend class Copy_field;
  friend class Item_avg_field;
  friend class Item_std_field;
  friend class Item_sum_num;
  friend class Item_sum_sum;
  friend class Item_sum_str;
  friend class Item_sum_count;
  friend class Item_sum_avg;
  friend class Item_sum_std;
  friend class Item_sum_min;
  friend class Item_sum_max;
  friend class Item_func_group_concat;

private:
  /*
    Primitive for implementing last_null_byte().

    SYNOPSIS
      do_last_null_byte()

    DESCRIPTION
      Primitive for the implementation of the last_null_byte()
      function. This represents the inheritance interface and can be
      overridden by subclasses.
   */
  virtual size_t do_last_null_byte() const;

/**
   Retrieve the field metadata for fields.

   This default implementation returns 0 and saves 0 in the metadata_ptr
   value.

   @param   metadata_ptr   First byte of field metadata

   @returns 0 no bytes written.
*/
  virtual int do_save_field_metadata(uchar *metadata_ptr)
  { return 0; }

protected:
  static void handle_int16(uchar *to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int16 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint2korr(from);
    else
#endif
      shortget(&val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int2store(to, val);
    else
#endif
      shortstore(to, val);
  }

  static void handle_int24(uchar *to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int32 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint3korr(from);
    else
#endif
      val= (from[0] << 16) + (from[1] << 8) + from[2];

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int2store(to, val);
    else
#endif
    {
      to[0]= 0xFF & (val >> 16);
      to[1]= 0xFF & (val >> 8);
      to[2]= 0xFF & val;
    }
  }

  /*
    Helper function to pack()/unpack() int32 values
  */
  static void handle_int32(uchar *to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int32 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint4korr(from);
    else
#endif
      longget(&val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int4store(to, val);
    else
#endif
      longstore(to, val);
  }

  /*
    Helper function to pack()/unpack() int64 values
  */
  static void handle_int64(uchar* to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int64 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint8korr(from);
    else
#endif
      longlongget(&val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int8store(to, val);
    else
#endif
      longlongstore(to, val);
  }

  uchar *pack_int16(uchar *to, const uchar *from, bool low_byte_first_to)
  {
    handle_int16(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to  + sizeof(int16);
  }

  const uchar *unpack_int16(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int16(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + sizeof(int16);
  }

  uchar *pack_int24(uchar *to, const uchar *from, bool low_byte_first_to)
  {
    handle_int24(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to + 3;
  }

  const uchar *unpack_int24(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int24(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + 3;
  }

  uchar *pack_int32(uchar *to, const uchar *from, bool low_byte_first_to)
  {
    handle_int32(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to  + sizeof(int32);
  }

  const uchar *unpack_int32(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int32(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + sizeof(int32);
  }

  uchar *pack_int64(uchar* to, const uchar *from, bool low_byte_first_to)
  {
    handle_int64(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to + sizeof(int64);
  }

  const uchar *unpack_int64(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int64(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + sizeof(int64);
  }

};


class Field_num :public Field {
public:
  const uint8 dec;
  bool zerofill,unsigned_flag;	// Purify cannot handle bit fields
  Field_num(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
            uint8 dec_arg, bool zero_arg, bool unsigned_arg);
  Item_result result_type () const { return REAL_RESULT; }
  enum Derivation derivation(void) const { return DERIVATION_NUMERIC; }
  uint repertoire(void) const { return MY_REPERTOIRE_NUMERIC; }
  const CHARSET_INFO *charset(void) const { return &my_charset_numeric; }
  void prepend_zeros(String *value);
  void add_zerofill_and_unsigned(String &res) const;
  friend class Create_field;
  uint decimals() const { return (uint) dec; }
  bool eq_def(Field *field);
  type_conversion_status store_decimal(const my_decimal *);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  uint is_equal(Create_field *new_field);
  uint row_pack_length() const { return pack_length(); }
  uint32 pack_length_from_metadata(uint field_metadata) {
    uint32 length= pack_length();
    DBUG_PRINT("result", ("pack_length_from_metadata(%d): %u",
                          field_metadata, length));
    return length;
  }
  type_conversion_status check_int(const CHARSET_INFO *cs,
                                   const char *str, size_t length,
                                   const char *int_end, int error);
  type_conversion_status get_int(const CHARSET_INFO *cs,
                                 const char *from, size_t len,
                                 longlong *rnd, ulonglong unsigned_max,
                                 longlong signed_min, longlong signed_max);
};


class Field_str :public Field {
protected:
  const CHARSET_INFO *field_charset;
  enum Derivation field_derivation;
public:
  Field_str(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg, const CHARSET_INFO *charset);
  Item_result result_type () const { return STRING_RESULT; }
  Item_result numeric_context_result_type() const
  { 
    return REAL_RESULT; 
  }
  uint decimals() const { return NOT_FIXED_DEC; }
  void make_field(Send_field *field);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val)=0;
  type_conversion_status store_decimal(const my_decimal *);
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *cs)=0;
  uint repertoire(void) const
  {
    return my_charset_repertoire(field_charset);
  }
  const CHARSET_INFO *charset(void) const { return field_charset; }
  void set_charset(const CHARSET_INFO *charset_arg)
  { field_charset= charset_arg; }
  enum Derivation derivation(void) const { return field_derivation; }
  virtual void set_derivation(enum Derivation derivation_arg)
  { field_derivation= derivation_arg; }
  bool binary() const { return field_charset == &my_charset_bin; }
  uint32 max_display_length() { return field_length; }
  friend class Create_field;
  virtual bool str_needs_quotes() { return TRUE; }
  uint is_equal(Create_field *new_field);
};


/* base class for Field_string, Field_varstring and Field_blob */

class Field_longstr :public Field_str
{
private:
  type_conversion_status report_if_important_data(const char *ptr,
                                                  const char *end,
                                                  bool count_spaces);
protected:
  type_conversion_status
    check_string_copy_error(const char *original_string,
                            const char *well_formed_error_pos,
                            const char *cannot_convert_error_pos,
                            const char *from_end_pos,
                            const char *end,
                            bool count_spaces,
                            const CHARSET_INFO *cs);
public:
  Field_longstr(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                uchar null_bit_arg, utype unireg_check_arg,
                const char *field_name_arg, const CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, charset_arg)
    {}

  type_conversion_status store_decimal(const my_decimal *d);
  uint32 max_data_length() const;
  bool is_updatable() const
  {
    DBUG_ASSERT(table && table->write_set);
    return bitmap_is_set(table->write_set, field_index);
  }
};

/* base class for float and double and decimal (old one) */
class Field_real :public Field_num {
public:
  my_bool not_fixed;

  Field_real(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg, utype unireg_check_arg,
             const char *field_name_arg,
             uint8 dec_arg, bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, dec_arg, zero_arg, unsigned_arg),
    not_fixed(dec_arg >= NOT_FIXED_DEC)
    {}
  type_conversion_status store_decimal(const my_decimal *);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool truncate(double *nr, double max_length);
  uint32 max_display_length() { return field_length; }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first);
};


class Field_decimal :public Field_real {
public:
  Field_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		uchar null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  enum_field_types type() const { return MYSQL_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
  { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  type_conversion_status reset(void);
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  void overflow(bool negative);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
  Field_decimal *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_DECIMAL);
    return new (mem_root) Field_decimal(*this);
  }
  Field_decimal *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_DECIMAL);
    return new Field_decimal(*this);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }
};


/* New decimal/numeric field which use fixed point arithmetic */
class Field_new_decimal :public Field_num {
private:
  int do_save_field_metadata(uchar *first_byte);
public:

  /* The maximum number of decimal digits can be stored */
  uint precision;
  uint bin_size;
  /*
    Constructors take max_length of the field as a parameter - not the
    precision as the number of decimal digits allowed.
    So for example we need to count length from precision handling
    CREATE TABLE ( DECIMAL(x,y)) 
  */
  Field_new_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg,
                    uint8 dec_arg, bool zero_arg, bool unsigned_arg);
  Field_new_decimal(uint32 len_arg, bool maybe_null_arg,
                    const char *field_name_arg, uint8 dec_arg,
                    bool unsigned_arg);
  enum_field_types type() const { return MYSQL_TYPE_NEWDECIMAL;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  Item_result result_type () const { return DECIMAL_RESULT; }
  type_conversion_status reset(void);
  type_conversion_status store_value(const my_decimal *decimal_value);
  void set_value_on_overflow(my_decimal *decimal_value, bool sign);
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  type_conversion_status store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  String *val_str(String*, String *);
  int cmp(const uchar *, const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return field_length; }
  uint32 pack_length() const { return (uint32) bin_size; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length() const { return pack_length(); }
  bool compatible_field_size(uint field_metadata, Relay_log_info *rli,
                             uint16 mflags, int *order_var);
  uint is_equal(Create_field *new_field);
  Field_new_decimal *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_NEWDECIMAL);
    return new (mem_root) Field_new_decimal(*this);
  }
  Field_new_decimal *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_NEWDECIMAL);
    return new Field_new_decimal(*this);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  static Field *create_from_item (Item *);
  bool send_binary(Protocol *protocol);
};


class Field_tiny :public Field_num {
public:
  Field_tiny(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void) { ptr[0]=0; return TYPE_OK; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 4; }
  Field_tiny *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_TINY);
    return new (mem_root) Field_tiny(*this);
  }
  Field_tiny *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_TINY);
    return new Field_tiny(*this);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    *to= *from;
    return to + 1;
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    *to= *from;
    return from + 1;
  }

  virtual ulonglong get_max_int_value() const
  {
    return unsigned_flag ? 0xFFULL : 0x7FULL;
  }
};


class Field_short :public Field_num {
public:
  Field_short(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_short(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	      bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, 0, 0, unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void) { ptr[0]=ptr[1]=0; return TYPE_OK; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 6; }
  Field_short *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_SHORT);
    return new (mem_root) Field_short(*this);
  }
  Field_short *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_SHORT);
    return new Field_short(*this);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    return pack_int16(to, from, low_byte_first);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    return unpack_int16(to, from, low_byte_first);
  }

  virtual ulonglong get_max_int_value() const
  {
    return unsigned_flag ? 0xFFFFULL : 0x7FFFULL;
  }
};

class Field_medium :public Field_num {
public:
  Field_medium(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_INT24;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_UINT24 : HA_KEYTYPE_INT24; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=0;
    return TYPE_OK;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 8; }
  Field_medium *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_INT24);
    return new (mem_root) Field_medium(*this);
  }
  Field_medium *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_INT24);
    return new Field_medium(*this);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }

  virtual ulonglong get_max_int_value() const
  {
    return unsigned_flag ? 0xFFFFFFULL : 0x7FFFFFULL;
  }
};


class Field_long :public Field_num {
public:

  static const int PACK_LENGTH= 4;

  Field_long(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_long(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONG_INT : HA_KEYTYPE_LONG_INT; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=0;
    return TYPE_OK;
  }
  double val_real(void);
  longlong val_int(void);
  bool send_binary(Protocol *protocol);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return PACK_LENGTH; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return MY_INT32_NUM_DECIMAL_DIGITS; }
  Field_long *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_LONG);
    return new (mem_root) Field_long(*this);
  }
  Field_long *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_LONG);
    return new Field_long(*this);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length MY_ATTRIBUTE((unused)),
                      bool low_byte_first)
  {
    return pack_int32(to, from, low_byte_first);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data MY_ATTRIBUTE((unused)),
                              bool low_byte_first)
  {
    return unpack_int32(to, from, low_byte_first);
  }

  virtual ulonglong get_max_int_value() const
  {
    return unsigned_flag ? 0xFFFFFFFFULL : 0x7FFFFFFFULL;
  }
};


class Field_longlong :public Field_num {
public:
  static const int PACK_LENGTH= 8;

  Field_longlong(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,bool maybe_null_arg,
		 const char *field_name_arg,
		  bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return TYPE_OK;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return PACK_LENGTH; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return true; }
  uint32 max_display_length() { return 20; }
  Field_longlong *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_LONGLONG);
    return new (mem_root) Field_longlong(*this);
  }
  Field_longlong *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_LONGLONG);
    return new Field_longlong(*this);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length  MY_ATTRIBUTE((unused)),
                      bool low_byte_first)
  {
    return pack_int64(to, from, low_byte_first);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data MY_ATTRIBUTE((unused)),
                              bool low_byte_first)
  {
    return unpack_int64(to, from, low_byte_first);
  }

  virtual ulonglong get_max_int_value() const
  {
    return unsigned_flag ? 0xFFFFFFFFFFFFFFFFULL : 0x7FFFFFFFFFFFFFFFULL;
  }
};


class Field_float :public Field_real {
public:
  Field_float(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
              uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_float(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	      uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  enum_field_types type() const { return MYSQL_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void)
  {
    memset(ptr, 0, sizeof(float));
    return TYPE_OK;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return sizeof(float); }
  uint row_pack_length() const { return pack_length(); }
  void sql_type(String &str) const;
  Field_float *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_FLOAT);
    return new (mem_root) Field_float(*this);
  }
  Field_float *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_FLOAT);
    return new Field_float(*this);
  }

  virtual ulonglong get_max_int_value() const
  {
    /*
      We use the maximum as per IEEE754-2008 standard, 2^24
    */
    return 0x1000000ULL;
  }

private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_double :public Field_real {
public:
  Field_double(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8 dec_arg, my_bool not_fixed_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {not_fixed= not_fixed_arg; }
  enum_field_types type() const { return MYSQL_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status reset(void)
  {
    memset(ptr, 0, sizeof(double));
    return TYPE_OK;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return sizeof(double); }
  uint row_pack_length() const { return pack_length(); }
  void sql_type(String &str) const;
  Field_double *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_DOUBLE);
    return new (mem_root) Field_double(*this);
  }
  Field_double *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_DOUBLE);
    return new Field_double(*this);
  }

  virtual ulonglong get_max_int_value() const
  {
    /*
      We use the maximum as per IEEE754-2008 standard, 2^53
    */
    return 0x20000000000000ULL;
  }

private:
  int do_save_field_metadata(uchar *first_byte);
};


/* Everything saved in this will disappear. It will always return NULL */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(uchar *ptr_arg, uint32 len_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     const CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, cs)
    {}
  enum_field_types type() const { return MYSQL_TYPE_NULL;}
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *cs)
  {
    null[0]= 1;
    return TYPE_OK;
  }
  type_conversion_status store(double nr)   { null[0]=1; return TYPE_OK; }
  type_conversion_status store(longlong nr, bool unsigned_val)
  {
    null[0]=1;
    return TYPE_OK;
  }
  type_conversion_status store_decimal(const my_decimal *d)
  {
    null[0]=1;
    return TYPE_OK;
  }
  type_conversion_status reset(void)       { return TYPE_OK; }
  double val_real(void)		{ return 0.0;}
  longlong val_int(void)	{ return 0;}
  my_decimal *val_decimal(my_decimal *) { return 0; }
  String *val_str(String *value,String *value2)
  { value2->length(0); return value2;}
  int cmp(const uchar *a, const uchar *b) { return 0;}
  void make_sort_key(uchar *buff, size_t length)  {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 4; }
  Field_null *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_NULL);
    return new (mem_root) Field_null(*this);
  }
  Field_null *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_NULL);
    return new Field_null(*this);
  }
};


/*
  Abstract class for TIME, DATE, DATETIME, TIMESTAMP
  with and without fractional part.
*/
class Field_temporal :public Field {
protected:
  uint8 dec; // Number of fractional digits

  /**
    Adjust number of decimal digits from NOT_FIXED_DEC to DATETIME_MAX_DECIMALS
  */
  uint8 normalize_dec(uint8 dec_arg)
  { return dec_arg == NOT_FIXED_DEC ? DATETIME_MAX_DECIMALS : dec_arg; }

  /**
    Low level routine to store a MYSQL_TIME value into a field.
    The value must be already properly rounded or truncated
    and checked for being a valid TIME/DATE/DATETIME value.

    @param[in]  ltime   MYSQL_TIME value.
    @param[out] error   Error flag vector, set in case of error.
    @retval     false   In case of success.
    @retval     true    In case of error.
  */
  virtual type_conversion_status store_internal(const MYSQL_TIME *ltime,
                                                int *error)= 0;

  /**
    Low level routine to store a MYSQL_TIME value into a field
    with rounding according to the field decimals() value.

    @param[in]  ltime   MYSQL_TIME value.
    @param[out] error   Error flag vector, set in case of error.
    @retval     false   In case of success.
    @retval     true    In case of error.    
  */
  virtual type_conversion_status store_internal_with_round(MYSQL_TIME *ltime,
                                                           int *warnings)= 0;

  /**
    Store a temporal value in lldiv_t into a field,
    with rounding according to the field decimals() value.

    @param[in]  lld     Temporal value.
    @param[out] warning Warning flag vector.
    @retval     false   In case of success.
    @retval     true    In case of error.    
  */
  type_conversion_status store_lldiv_t(const lldiv_t *lld, int *warning);

  /**
    Convert a string to MYSQL_TIME, according to the field type.

    @param[in]  str     String
    @param[in]  len     String length
    @param[in]  cs      String character set
    @param[out] ltime   The value is stored here
    @param[out] status  Conversion status
    @retval     false   Conversion went fine, ltime contains a valid time
    @retval     true    Conversion failed, ltime was reset and contains nothing
  */
  virtual bool convert_str_to_TIME(const char *str, size_t len,
                                   const CHARSET_INFO *cs,
                                   MYSQL_TIME *ltime, 
                                   MYSQL_TIME_STATUS *status)= 0;
  /**
    Convert a number with fractional part with nanosecond precision
    into MYSQL_TIME, according to the field type. Nanoseconds
    are rounded to milliseconds and added to ltime->second_part.

    @param[in]  nr            Number
    @param[in]  unsigned_val  SIGNED/UNSIGNED flag
    @param[in]  nanoseconds   Fractional part in nanoseconds
    @param[out] ltime         The value is stored here
    @param[out] status        Conversion status
    @retval     false         On success
    @retval     true          On error
  */
  virtual type_conversion_status convert_number_to_TIME(longlong nr,
                                                        bool unsigned_val,
                                                        int nanoseconds,
                                                        MYSQL_TIME *ltime,
                                                        int *warning)= 0;

  /**
    Convert an integer number into MYSQL_TIME, according to the field type.

    @param[in]  nr            Number
    @param[in]  unsigned_val  SIGNED/UNSIGNED flag
    @param[out] ltime         The value is stored here
    @retval     false         On success
    @retval     true          On error
  */
  longlong convert_number_to_datetime(longlong nr, bool unsigned_val,
                                      MYSQL_TIME *ltime, int *warning);

  /**
    Set a warning according to warning bit flag vector.
    Multiple warnings are possible at the same time.
    Every warning in the bit vector is set by an individual
    set_datetime_warning() call.

    @param str      Warning parameter
    @param warnings Warning bit flag
  */
  void set_warnings(ErrConvString str, int warnings);

  /**
    Flags that are passed as "flag" argument to
    check_date(), number_to_datetime(), str_to_datetime().

    Flags depend on the session sql_mode settings, such as
    MODE_NO_ZERO_DATE, MODE_NO_ZERO_IN_DATE.
    Also, Field_newdate, Field_datetime, Field_datetimef add TIME_FUZZY_DATE
    to the session sql_mode settings, to allow relaxed date format,
    while Field_timestamp, Field_timestampf do not.

    @param  thd  THD
    @retval      sql_mode flags mixed with the field type flags.
  */
  virtual my_time_flags_t date_flags(const THD *thd)
  {
    return 0;
  }
  /**
    Flags that are passed as "flag" argument to
    check_date(), number_to_datetime(), str_to_datetime().
    Similar to the above when we don't have a THD value.
  */
  my_time_flags_t date_flags()
  {
    return date_flags(table ? table->in_use : current_thd);
  }

  /**
    Set a single warning using make_truncated_value_warning().
    
    @param[in] level           Warning level (error, warning, note)
    @param[in] code            Warning code
    @param[in] str             Warning parameter
    @param[in] ts_type         Timestamp type (time, date, datetime, none)
    @param[in] cuted_inctement Incrementing of cut field counter
  */
  void set_datetime_warning(Sql_condition::enum_severity_level level, uint code,
                            ErrConvString str,
                            timestamp_type ts_type, int cuted_increment);
public:
  /**
    Constructor for Field_temporal
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param len_arg           Number of characters in the integer part.
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal(uchar *ptr_arg,
                 uchar *null_ptr_arg, uchar null_bit_arg,
                 enum utype unireg_check_arg, const char *field_name_arg,
                 uint32 len_arg, uint8 dec_arg)
    :Field(ptr_arg,
           len_arg + ((dec= normalize_dec(dec_arg)) ? normalize_dec(dec_arg) + 1 : 0),
           null_ptr_arg, null_bit_arg,
           unireg_check_arg, field_name_arg)
    { flags|= BINARY_FLAG; }
  /**
    Constructor for Field_temporal
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param len_arg           Number of characters in the integer part.
    @param dec_arg           Number of second fraction digits, 0..6
  */
  Field_temporal(bool maybe_null_arg, const char *field_name_arg,
                 uint32 len_arg, uint8 dec_arg)
    :Field((uchar *) 0, 
           len_arg + ((dec= normalize_dec(dec_arg)) ? normalize_dec(dec_arg) + 1 : 0),
           maybe_null_arg ? (uchar *) "" : 0, 0,
           NONE, field_name_arg)
    { flags|= BINARY_FLAG; }
  virtual Item_result result_type() const { return STRING_RESULT; }
  virtual uint32 max_display_length() { return field_length; }
  virtual bool str_needs_quotes() { return TRUE; }
  virtual uint is_equal(Create_field *new_field);
  Item_result numeric_context_result_type() const
  {
    return dec ? DECIMAL_RESULT : INT_RESULT;
  }
  enum Item_result cmp_type() const { return INT_RESULT; }
  enum Derivation derivation() const { return DERIVATION_NUMERIC; }
  uint repertoire() const { return MY_REPERTOIRE_NUMERIC; }
  const CHARSET_INFO *charset() const { return &my_charset_numeric; }
  bool can_be_compared_as_longlong() const { return true; }
  bool binary() const { return true; }
  type_conversion_status store(const char *str, size_t len,
                               const CHARSET_INFO *cs);
  type_conversion_status store_decimal(const my_decimal *decimal);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store(double nr);
  double val_real() // FSP-enable types redefine it.
  {
    return (double) val_int();
  }
  my_decimal *val_decimal(my_decimal *decimal_value); // FSP types redefine it
};


/**
  Abstract class for types with date
  with optional time, with or without fractional part:
  DATE, DATETIME, DATETIME(N), TIMESTAMP, TIMESTAMP(N).
*/
class Field_temporal_with_date :public Field_temporal {
protected:
  /**
    Low level function to get value into MYSQL_TIME,
    without checking for being valid.
  */
  virtual bool get_date_internal(MYSQL_TIME *ltime)= 0;

  /**
    Get value into MYSQL_TIME and check TIME_NO_ZERO_DATE flag.
    @retval   True on error: we get a zero value but flags disallow zero dates.
    @retval   False on success.
  */
  bool get_internal_check_zero(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  
  type_conversion_status convert_number_to_TIME(longlong nr, bool unsigned_val,
                                                int nanoseconds,
                                                MYSQL_TIME *ltime,
                                                int *warning);
  bool convert_str_to_TIME(const char *str, size_t len, const CHARSET_INFO *cs,
                           MYSQL_TIME *ltime, MYSQL_TIME_STATUS *status);
  type_conversion_status store_internal_with_round(MYSQL_TIME *ltime,
                                                   int *warnings);
public:
  /**
    Constructor for Field_temporal
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param len_arg           Number of characters in the integer part.
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal_with_date(uchar *ptr_arg, uchar *null_ptr_arg,
                           uchar null_bit_arg,
                           enum utype unireg_check_arg,
                           const char *field_name_arg,
                           uint8 int_length_arg, uint8 dec_arg)
    :Field_temporal(ptr_arg, null_ptr_arg, null_bit_arg,
                    unireg_check_arg, field_name_arg,
                    int_length_arg, dec_arg)
    { }
  /**
    Constructor for Field_temporal
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param len_arg           Number of characters in the integer part.
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal_with_date(bool maybe_null_arg, const char *field_name_arg,
                           uint int_length_arg, uint8 dec_arg)
    :Field_temporal((uchar*) 0, maybe_null_arg ? (uchar*) "": 0, 0,
                    NONE, field_name_arg, int_length_arg, dec_arg)
    { }
  bool send_binary(Protocol *protocol);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  String *val_str(String *, String *);
  longlong val_time_temporal();
  longlong val_date_temporal();
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_date(ltime, TIME_FUZZY_DATE);
  }
  /* Validate the value stored in a field */
  virtual type_conversion_status validate_stored_val(THD *thd);
};


/**
  Abstract class for types with date and time,
  with or without fractional part:
  DATETIME, DATETIME(N), TIMESTAMP, TIMESTAMP(N).
*/
class Field_temporal_with_date_and_time :public Field_temporal_with_date {
private:
  int do_save_field_metadata(uchar *metadata_ptr)
  {
    if (decimals())
    {
      *metadata_ptr= decimals();
      return 1;
    }
    return 0;
  }
protected:
  /**
     Initialize flags for TIMESTAMP DEFAULT CURRENT_TIMESTAMP / ON UPDATE
     CURRENT_TIMESTAMP columns.
     
     @todo get rid of TIMESTAMP_FLAG and ON_UPDATE_NOW_FLAG.
  */
  void init_timestamp_flags();
  /**
    Store "struct timeval" value into field.
    The value must be properly rounded or truncated according
    to the number of fractional second digits.
  */
  virtual void store_timestamp_internal(const struct timeval *tm)= 0;
  bool convert_TIME_to_timestamp(THD *thd, const MYSQL_TIME *ltime,
                                 struct timeval *tm, int *error);

public:
  /**
    Constructor for Field_temporal_with_date_and_time
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal_with_date_and_time(uchar *ptr_arg, uchar *null_ptr_arg,
                                    uchar null_bit_arg,
                                    enum utype unireg_check_arg,
                                    const char *field_name_arg,
                                    uint8 dec_arg)
    :Field_temporal_with_date(ptr_arg, null_ptr_arg, null_bit_arg,
                              unireg_check_arg, field_name_arg,
                              MAX_DATETIME_WIDTH, dec_arg)
    { }
  void store_timestamp(const struct timeval *tm);
};


/**
  Abstract class for types with date and time, with fractional part:
  DATETIME, DATETIME(N), TIMESTAMP, TIMESTAMP(N).
*/
class Field_temporal_with_date_and_timef :
  public Field_temporal_with_date_and_time {
private:
  int do_save_field_metadata(uchar *metadata_ptr)
  {
    *metadata_ptr= decimals();
    return 1;
  }
public:
  /**
    Constructor for Field_temporal_with_date_and_timef
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal_with_date_and_timef(uchar *ptr_arg, uchar *null_ptr_arg,
                                     uchar null_bit_arg,
                                     enum utype unireg_check_arg,
                                     const char *field_name_arg,
                                     uint8 dec_arg)
    :Field_temporal_with_date_and_time(ptr_arg, null_ptr_arg, null_bit_arg,
                                       unireg_check_arg, field_name_arg,
                                       dec_arg)
    { }
  /**
    Constructor for Field_temporal_with_date_and_timef
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_temporal_with_date_and_timef(bool maybe_null_arg,
                                     const char *field_name_arg,
                                     uint8 dec_arg)
    :Field_temporal_with_date_and_time((uchar *) 0,
                                       maybe_null_arg ? (uchar*) "" : 0, 0,
                                       NONE, field_name_arg, dec_arg)
    { }

  uint decimals() const { return dec; }
  const CHARSET_INFO *sort_charset() const { return &my_charset_bin; }
  void make_sort_key(uchar *to, size_t length) { memcpy(to, ptr, length); }
  int cmp(const uchar *a_ptr, const uchar *b_ptr)
  {
    return memcmp(a_ptr, b_ptr, pack_length());
  }
  uint row_pack_length() const { return pack_length(); }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *decimal_value);
};


/*
  Field implementing TIMESTAMP data type without fractional seconds.
  We will be removed eventually.
*/
class Field_timestamp :public Field_temporal_with_date_and_time {
protected:
  my_time_flags_t date_flags(const THD *thd);
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
  bool get_date_internal(MYSQL_TIME *ltime);
  void store_timestamp_internal(const struct timeval *tm);
public:
  static const int PACK_LENGTH= 4;
  Field_timestamp(uchar *ptr_arg, uint32 len_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg);
  Field_timestamp(bool maybe_null_arg, const char *field_name_arg);
  enum_field_types type() const { return MYSQL_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  type_conversion_status store_packed(longlong nr);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=0;
    return TYPE_OK;
  }
  longlong val_int(void);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return PACK_LENGTH; }
  void sql_type(String &str) const;
  bool zero_pack() const { return 0; }
  /* Get TIMESTAMP field value as seconds since begging of Unix Epoch */
  bool get_timestamp(struct timeval *tm, int *warnings);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  Field_timestamp *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIMESTAMP);
    return new (mem_root) Field_timestamp(*this);
  }
  Field_timestamp *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIMESTAMP);
    return new Field_timestamp(*this);
  }
  uchar *pack(uchar *to, const uchar *from,
              uint max_length MY_ATTRIBUTE((unused)), bool low_byte_first)
  {
    return pack_int32(to, from, low_byte_first);
  }
  const uchar *unpack(uchar* to, const uchar *from,
                      uint param_data MY_ATTRIBUTE((unused)),
                      bool low_byte_first)
  {
    return unpack_int32(to, from, low_byte_first);
  }
  /* Validate the value stored in a field */
  virtual type_conversion_status validate_stored_val(THD *thd);
};


/*
  Field implementing TIMESTAMP(N) data type, where N=0..6.
*/
class Field_timestampf :public Field_temporal_with_date_and_timef {
protected:
  bool get_date_internal(MYSQL_TIME *ltime);
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
  my_time_flags_t date_flags(const THD *thd);
  void store_timestamp_internal(const struct timeval *tm);
public:
  static const int PACK_LENGTH= 8;
  /**
    Field_timestampf constructor
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param share             Table share.
    @param dec_arg           Number of fractional second digits, 0..6.
  */
  Field_timestampf(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
                   enum utype unireg_check_arg, const char *field_name_arg,
                   uint8 dec_arg);
  /**
    Field_timestampf constructor
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of fractional second digits, 0..6.
  */
  Field_timestampf(bool maybe_null_arg, const char *field_name_arg,
                   uint8 dec_arg);
  Field_timestampf *clone(MEM_ROOT *mem_root) const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIMESTAMP);
    return new (mem_root) Field_timestampf(*this);
  }
  Field_timestampf *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIMESTAMP);
    return new Field_timestampf(*this);
  }

  enum_field_types type() const { return MYSQL_TYPE_TIMESTAMP; }
  enum_field_types real_type() const { return MYSQL_TYPE_TIMESTAMP2; }
  enum_field_types binlog_type() const { return MYSQL_TYPE_TIMESTAMP2; }
  bool zero_pack() const { return 0; }

  uint32 pack_length() const
  {
    return my_timestamp_binary_length(dec);
  }
  virtual uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_ENTER("Field_timestampf::pack_length_from_metadata");
    uint tmp= my_timestamp_binary_length(field_metadata);
    DBUG_RETURN(tmp);
  }

  type_conversion_status reset();
  type_conversion_status store_packed(longlong nr);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  void sql_type(String &str) const;

  bool get_timestamp(struct timeval *tm, int *warnings);
  /* Validate the value stored in a field */
  virtual type_conversion_status validate_stored_val(THD *thd);
};


class Field_year :public Field_tiny {
public:
  Field_year(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		unireg_check_arg, field_name_arg, 1, 1)
    {}
  enum_field_types type() const { return MYSQL_TYPE_YEAR;}
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return true; }
  Field_year *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_YEAR);
    return new (mem_root) Field_year(*this);
  }
  Field_year *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_YEAR);
    return new Field_year(*this);
  }
};


class Field_newdate :public Field_temporal_with_date {
protected:
  static const int PACK_LENGTH= 3;
  my_time_flags_t date_flags(const THD *thd);
  bool get_date_internal(MYSQL_TIME *ltime);
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);

public:
  Field_newdate(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
                enum utype unireg_check_arg, const char *field_name_arg)
    :Field_temporal_with_date(ptr_arg, null_ptr_arg, null_bit_arg,
                              unireg_check_arg, field_name_arg,
                              MAX_DATE_WIDTH, 0)
    { }
  Field_newdate(bool maybe_null_arg, const char *field_name_arg)
    :Field_temporal_with_date((uchar *) 0, maybe_null_arg ? (uchar *) "" : 0,
                              0, NONE, field_name_arg, MAX_DATE_WIDTH, 0)
    { }
  enum_field_types type() const { return MYSQL_TYPE_DATE;}
  enum_field_types real_type() const { return MYSQL_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=0;
    return TYPE_OK;
  }
  type_conversion_status store_packed(longlong nr);
  longlong val_int(void);
  longlong val_time_temporal();
  longlong val_date_temporal();
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return PACK_LENGTH; }
  void sql_type(String &str) const;
  bool zero_pack() const { return 1; }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  Field_newdate *clone(MEM_ROOT *mem_root) const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATE);
    DBUG_ASSERT(real_type() == MYSQL_TYPE_NEWDATE);
    return new (mem_root) Field_newdate(*this);
  }
  Field_newdate *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATE);
    DBUG_ASSERT(real_type() == MYSQL_TYPE_NEWDATE);
    return new Field_newdate(*this);
  }
};


/**
  Abstract class for TIME and TIME(N).
*/
class Field_time_common :public Field_temporal {
protected:
  bool convert_str_to_TIME(const char *str, size_t len, const CHARSET_INFO *cs,
                           MYSQL_TIME *ltime, MYSQL_TIME_STATUS *status);
  /**
    @todo: convert_number_to_TIME returns conversion status through
    two different interfaces: return value and warning. It should be
    refactored to only use return value.
   */
  type_conversion_status convert_number_to_TIME(longlong nr, bool unsigned_val,
                                                int nanoseconds,
                                                MYSQL_TIME *ltime,
                                                int *warning);
  /**
    Low-level function to store MYSQL_TIME value.
    The value must be rounded or truncated according to decimals().
  */
  virtual type_conversion_status store_internal(const MYSQL_TIME *ltime,
                                                int *error)= 0;
  /**
    Function to store time value.
    The value is rounded according to decimals().
  */
  virtual type_conversion_status store_internal_with_round(MYSQL_TIME *ltime,
                                                           int *warnings);
public:
  /**
    Constructor for Field_time_common
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_time_common(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg,
                    uint8 dec_arg)
    :Field_temporal(ptr_arg, null_ptr_arg, null_bit_arg,
                    unireg_check_arg, field_name_arg,
                    MAX_TIME_WIDTH, dec_arg)
    { }
  /**
    Constructor for Field_time_common
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_time_common(bool maybe_null_arg, const char *field_name_arg,
                    uint8 dec_arg)
    :Field_temporal((uchar *) 0, maybe_null_arg ? (uchar *) "" : 0, 0,
                    NONE, field_name_arg, MAX_TIME_WIDTH, dec_arg)
    { }
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec);
  String *val_str(String*, String *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  longlong val_date_temporal();
  bool send_binary(Protocol *protocol);
};


/*
  Field implementing TIME data type without fractional seconds.
  We will be removed eventually.
*/
class Field_time :public Field_time_common {
protected:
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
public:
  Field_time(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg)
    :Field_time_common(ptr_arg, null_ptr_arg, null_bit_arg,
                       unireg_check_arg, field_name_arg, 0)
    { }
  Field_time(bool maybe_null_arg, const char *field_name_arg)
    :Field_time_common((uchar *) 0, maybe_null_arg ? (uchar *) "" : 0, 0,
                       NONE, field_name_arg, 0)
    { }
  enum_field_types type() const { return MYSQL_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  type_conversion_status store_packed(longlong nr);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=0;
    return TYPE_OK;
  }
  longlong val_int(void);
  longlong val_time_temporal();
  bool get_time(MYSQL_TIME *ltime);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool zero_pack() const { return 1; }
  Field_time *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIME);
    return new (mem_root) Field_time(*this);
  }
  Field_time *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIME);
    return new Field_time(*this);
  }
};


/*
  Field implementing TIME(N) data type, where N=0..6.
*/
class Field_timef :public Field_time_common {
private:
  int do_save_field_metadata(uchar *metadata_ptr)
  {
    *metadata_ptr= decimals();
    return 1;
  }
protected:
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
public:
  /**
    Constructor for Field_timef
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_timef(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
              enum utype unireg_check_arg, const char *field_name_arg,
              uint8 dec_arg)
    :Field_time_common(ptr_arg, null_ptr_arg, null_bit_arg,
                       unireg_check_arg, field_name_arg, dec_arg)
  { }
  /**
    Constructor for Field_timef
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_timef(bool maybe_null_arg, const char *field_name_arg, uint8 dec_arg)
    :Field_time_common((uchar *) 0, maybe_null_arg ? (uchar *) "" : 0, 0,
                       NONE, field_name_arg, dec_arg)
  { }
  Field_timef *clone(MEM_ROOT *mem_root) const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIME);
    return new (mem_root) Field_timef(*this);
  }
  Field_timef *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_TIME);
    return new Field_timef(*this);
  }
  uint decimals() const { return dec; }
  enum_field_types type() const { return MYSQL_TYPE_TIME;}
  enum_field_types real_type() const { return MYSQL_TYPE_TIME2; }
  enum_field_types binlog_type() const { return MYSQL_TYPE_TIME2; }
  type_conversion_status store_packed(longlong nr);
  type_conversion_status reset();
  double val_real();
  longlong val_int();
  longlong val_time_temporal();
  bool get_time(MYSQL_TIME *ltime);
  my_decimal *val_decimal(my_decimal *);
  uint32 pack_length() const
  {
    return my_time_binary_length(dec);
  }
  virtual uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_ENTER("Field_timef::pack_length_from_metadata");
    uint tmp= my_time_binary_length(field_metadata);
    DBUG_RETURN(tmp);
  }
  uint row_pack_length() const { return pack_length(); }
  void sql_type(String &str) const;
  bool zero_pack() const { return 1; }
  const CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
  void make_sort_key(uchar *to, size_t length) { memcpy(to, ptr, length); }
  int cmp(const uchar *a_ptr, const uchar *b_ptr)
  {
    return memcmp(a_ptr, b_ptr, pack_length());
  }
};


/*
  Field implementing DATETIME data type without fractional seconds.
  We will be removed eventually.
*/
class Field_datetime :public Field_temporal_with_date_and_time {
protected:
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
  bool get_date_internal(MYSQL_TIME *ltime);
  my_time_flags_t date_flags(const THD *thd);
  void store_timestamp_internal(const struct timeval *tm);

public:
  static const int PACK_LENGTH= 8;

  /**
     DATETIME columns can be defined as having CURRENT_TIMESTAMP as the
     default value on inserts or updates. This constructor accepts a
     unireg_check value to initialize the column default expressions.

     The implementation of function defaults is heavily entangled with the
     binary .frm file format. The @c utype @c enum is part of the file format
     specification but is declared a member of the Field class.

     Four distinct unireg_check values are used for DATETIME columns to
     distinguish various cases of DEFAULT or ON UPDATE values. These values are:

     - TIMESTAMP_DN_FIELD - means DATETIME DEFAULT CURRENT_TIMESTAMP.

     - TIMESTAMP_UN_FIELD - means DATETIME DEFAULT <default value> ON UPDATE
     CURRENT_TIMESTAMP, where <default value> is an implicit or explicit
     expression other than CURRENT_TIMESTAMP or any synonym thereof
     (e.g. NOW().)

     - TIMESTAMP_DNUN_FIELD - means DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE
     CURRENT_TIMESTAMP.

     - NONE - means that the column has neither DEFAULT CURRENT_TIMESTAMP, nor
     ON UPDATE CURRENT_TIMESTAMP
   */
  Field_datetime(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
                 enum utype unireg_check_arg, const char *field_name_arg)
    :Field_temporal_with_date_and_time(ptr_arg, null_ptr_arg, null_bit_arg,
                                       unireg_check_arg, field_name_arg, 0)
    {}
  Field_datetime(bool maybe_null_arg, const char *field_name_arg)
    :Field_temporal_with_date_and_time((uchar *) 0,
                                       maybe_null_arg ? (uchar *) "" : 0,
                                       0, NONE, field_name_arg, 0)
    {}
  enum_field_types type() const { return MYSQL_TYPE_DATETIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
  using Field_temporal_with_date_and_time::store; // Make -Woverloaded-virtual
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_packed(longlong nr);
  type_conversion_status reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return TYPE_OK;
  }
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return PACK_LENGTH; }
  void sql_type(String &str) const;
  bool zero_pack() const { return 1; }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  Field_datetime *clone(MEM_ROOT *mem_root) const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATETIME);
    return new (mem_root) Field_datetime(*this);
  }
  Field_datetime *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATETIME);
    return new Field_datetime(*this);
  }
  uchar *pack(uchar* to, const uchar *from,
              uint max_length MY_ATTRIBUTE((unused)), bool low_byte_first)
  {
    return pack_int64(to, from, low_byte_first);
  }
  const uchar *unpack(uchar* to, const uchar *from,
                      uint param_data MY_ATTRIBUTE((unused)),
                      bool low_byte_first)
  {
    return unpack_int64(to, from, low_byte_first);
  }
};


/*
  Field implementing DATETIME(N) data type, where N=0..6.
*/
class Field_datetimef :public Field_temporal_with_date_and_timef {
protected:
  bool get_date_internal(MYSQL_TIME *ltime);
  type_conversion_status store_internal(const MYSQL_TIME *ltime, int *error);
  my_time_flags_t date_flags(const THD *thd);
  void store_timestamp_internal(const struct timeval *tm);

public:
  /**
    Constructor for Field_datetimef
    @param ptr_arg           See Field definition
    @param null_ptr_arg      See Field definition
    @param null_bit_arg      See Field definition
    @param unireg_check_arg  See Field definition
    @param field_name_arg    See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_datetimef(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
                 enum utype unireg_check_arg, const char *field_name_arg,
                 uint8 dec_arg)
    :Field_temporal_with_date_and_timef(ptr_arg, null_ptr_arg, null_bit_arg,
                                        unireg_check_arg, field_name_arg,
                                        dec_arg)
    {}
  /**
    Constructor for Field_datetimef
    @param maybe_null_arg    See Field definition
    @param field_name_arg    See Field definition
    @param len_arg           See Field definition
    @param dec_arg           Number of second fraction digits, 0..6.
  */
  Field_datetimef(bool maybe_null_arg, const char *field_name_arg,
                  uint8 dec_arg)
    :Field_temporal_with_date_and_timef((uchar *) 0,
                                        maybe_null_arg ? (uchar *) "" : 0, 0,
                                        NONE, field_name_arg, dec_arg)
    {}
  Field_datetimef *clone(MEM_ROOT *mem_root) const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATETIME);
    return new (mem_root) Field_datetimef(*this);
  }
  Field_datetimef *clone() const
  {
    DBUG_ASSERT(type() == MYSQL_TYPE_DATETIME);
    return new Field_datetimef(*this);
  }

  enum_field_types type() const { return MYSQL_TYPE_DATETIME;}
  enum_field_types real_type() const { return MYSQL_TYPE_DATETIME2; }
  enum_field_types binlog_type() const { return MYSQL_TYPE_DATETIME2; }
  uint32 pack_length() const
  {
    return my_datetime_binary_length(dec);
  }
  virtual uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_ENTER("Field_datetimef::pack_length_from_metadata");
    uint tmp= my_datetime_binary_length(field_metadata);
    DBUG_RETURN(tmp);
  }
  bool zero_pack() const { return 1; }

  type_conversion_status store_packed(longlong nr);
  type_conversion_status reset();
  longlong val_date_temporal();
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  void sql_type(String &str) const;
};


class Field_string :public Field_longstr {
public:
  bool can_alter_field_type;
  Field_string(uchar *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       const CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     can_alter_field_type(1) {};
  Field_string(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
               const CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     can_alter_field_type(1) {};

  enum_field_types type() const
  {
    return ((can_alter_field_type && table && table->s &&
             table->s->db_create_options & HA_OPTION_PACK_RECORD &&
	     field_length >= 4) &&
            table->s->frm_version < FRM_VER_TRUE_VARCHAR ?
	    MYSQL_TYPE_VAR_STRING : MYSQL_TYPE_STRING);
  }
  bool match_collation_to_optimize_range() const { return true; }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  type_conversion_status reset(void)
  {
    charset()->cset->fill(charset(),(char*) ptr, field_length,
                          (has_charset() ? ' ' : 0));
    return TYPE_OK;
  }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(longlong nr, bool unsigned_val);
  /* QQ: To be deleted */
  type_conversion_status store(double nr) { return Field_str::store(nr); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_PRINT("debug", ("field_metadata: 0x%04x", field_metadata));
    if (field_metadata == 0)
      return row_pack_length();
    return (((field_metadata >> 4) & 0x300) ^ 0x300) + (field_metadata & 0x00ff);
  }
  bool compatible_field_size(uint field_metadata, Relay_log_info *rli,
                             uint16 mflags, int *order_var);
  uint row_pack_length() const { return field_length; }
  int pack_cmp(const uchar *a,const uchar *b,uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b,uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length();
  enum_field_types real_type() const { return MYSQL_TYPE_STRING; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  Field *new_field(MEM_ROOT *root, TABLE *new_table, bool keep_type);
  Field_string *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(real_type() == MYSQL_TYPE_STRING);
    return new (mem_root) Field_string(*this);
  }
  Field_string *clone() const {
    DBUG_ASSERT(real_type() == MYSQL_TYPE_STRING);
    return new Field_string(*this);
  }
  virtual size_t get_key_image(uchar *buff, size_t length, imagetype type);
  virtual bool is_text_key_type() const { return binary() ? false : true; }
private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_varstring :public Field_longstr {
public:
  /*
    The maximum space available in a Field_varstring, in bytes. See
    length_bytes.
  */
  static const uint MAX_SIZE;
  /* Store number of bytes used to store length (1 or 2) */
  uint32 length_bytes;
  Field_varstring(uchar *ptr_arg,
                  uint32 len_arg, uint length_bytes_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  TABLE_SHARE *share, const CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     length_bytes(length_bytes_arg)
  {
    share->varchar_fields++;
  }
  Field_varstring(uint32 len_arg,bool maybe_null_arg,
                  const char *field_name_arg,
                  TABLE_SHARE *share, const CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     length_bytes(len_arg < 256 ? 1 :2)
  {
    share->varchar_fields++;
  }

  enum_field_types type() const { return MYSQL_TYPE_VARCHAR; }
  bool match_collation_to_optimize_range() const { return true; }
  enum ha_base_keytype key_type() const;
  uint row_pack_length() const { return field_length; }
  bool zero_pack() const { return 0; }
  type_conversion_status reset(void)
  {
    memset(ptr, 0, field_length+length_bytes);
    return TYPE_OK;
  }
  uint32 pack_length() const { return (uint32) field_length+length_bytes; }
  uint32 key_length() const { return (uint32) field_length; }
  uint32 sort_length() const
  {
    return (uint32) field_length + (field_charset == &my_charset_bin ?
                                    length_bytes : 0);
  }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(longlong nr, bool unsigned_val);
  /* QQ: To be deleted */
  type_conversion_status store(double nr) { return Field_str::store(nr); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
  {
    return cmp_max(a, b, ~0L);
  }
  void make_sort_key(uchar *buff, size_t length);
  size_t get_key_image(uchar *buff, size_t length, imagetype type);
  void set_key_image(const uchar *buff, size_t length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint packed_col_length(const uchar *to, uint length);

  uint32 data_length(uint row_offset= 0);
  enum_field_types real_type() const { return MYSQL_TYPE_VARCHAR; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  Field *new_field(MEM_ROOT *root, TABLE *new_table, bool keep_type);
  Field *new_key_field(MEM_ROOT *root, TABLE *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  Field_varstring *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_VARCHAR);
    DBUG_ASSERT(real_type() == MYSQL_TYPE_VARCHAR);
    return new (mem_root) Field_varstring(*this);
  }
  Field_varstring *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_VARCHAR);
    DBUG_ASSERT(real_type() == MYSQL_TYPE_VARCHAR);
    return new Field_varstring(*this);
  }
  uint is_equal(Create_field *new_field);
  void hash(ulong *nr, ulong *nr2);
  void get_ptr(uchar **str)
  {
    *str= ptr + length_bytes;
  }
  virtual bool is_text_key_type() const { return binary() ? false : true; }
private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_blob :public Field_longstr {
  virtual type_conversion_status store_internal(const char *from, size_t length,
                                                const CHARSET_INFO *cs);
  /**
    Copy value to memory storage.
  */
  type_conversion_status store_to_mem(const char *from, size_t length,
                                      const CHARSET_INFO *cs,
                                      size_t max_length,
                                      Blob_mem_storage *blob_storage);
protected:
  /**
    The number of bytes used to represent the length of the blob.
  */
  uint packlength;
  
  /**
    The 'value'-object is a cache fronting the storage engine.
  */
  String value;

private:
  /**
    In order to support update of virtual generated columns of blob type,
    we need to allocate the space blob needs on server for old_row and
    new_row respectively. This variable is used to record the
    allocated blob space for old_row.
  */
  String old_value;

  /**
    Whether we need to move the content of 'value' to 'old_value' before
    updating the BLOB stored in 'value'. This needs to be done for
    updates of BLOB columns that are virtual since the storage engine
    does not have its own copy of the old 'value'. This variable is set
    to true when we read the data into 'value'. It is reset when we move
    'value' to 'old_value'. The purpose of having this is to avoid that we
    do the move operation from 'value' to 'old_value' more than one time per
    record.
    Currently, this variable is introduced because the following call in
    sql_data_change.cc:
    \/\**
      @todo combine this call to update_generated_write_fields() with the one
      in fill_record() to avoid updating virtual generated fields twice.
    *\/
     if (table->has_gcol())
            update_generated_write_fields(table->write_set, table);
     When the @todo is done, m_keep_old_value can be deleted.
  */
  bool m_keep_old_value;

protected:
  /**
    Store ptr and length.
  */
  void store_ptr_and_length(const char *from, uint32 length)
  {
    store_length(length);
    memmove(ptr + packlength, &from, sizeof(char *));
  }
  
public:
  Field_blob(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TABLE_SHARE *share, uint blob_pack_length, const CHARSET_INFO *cs);

  Field_blob(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     const CHARSET_INFO *cs, bool set_packlength)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
    packlength(4), m_keep_old_value(false)
  {
    flags|= BLOB_FLAG;
    if (set_packlength)
    {
      packlength= len_arg <= 255 ? 1 :
                  len_arg <= 65535 ? 2 :
                  len_arg <= 16777215 ? 3 : 4;
    }
  }

  Field_blob(uint32 packlength_arg)
    :Field_longstr((uchar*) 0, 0, (uchar*) "", 0,
                   NONE, "temp", system_charset_info),
    packlength(packlength_arg), m_keep_old_value(false)
  {}

  ~Field_blob() { mem_free(); }

  /* Note that the default copy constructor is used, in clone() */
  enum_field_types type() const { return MYSQL_TYPE_BLOB;}
  bool match_collation_to_optimize_range() const { return true; }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
    { return cmp_max(a, b, ~0L); }
  int cmp(const uchar *a, uint32 a_length, const uchar *b, uint32 b_length);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint32 key_length() const { return 0; }
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const
  { return (uint32) (packlength + portable_sizeof_char_ptr); }

  /**
     Return the packed length without the pointer size added. 

     This is used to determine the size of the actual data in the row
     buffer.

     @returns The length of the raw data itself without the pointer.
  */
  uint32 pack_length_no_ptr() const
  { return (uint32) (packlength); }
  uint row_pack_length() const { return pack_length_no_ptr(); }
  uint32 sort_length() const;
  virtual uint32 max_data_length() const
  {
    return (uint32) (((ulonglong) 1 << (packlength*8)) -1);
  }
  type_conversion_status reset(void)
  {
    memset(ptr, 0, packlength+sizeof(uchar*));
    return TYPE_OK;
  }
  void reset_fields()
  { 
    value= String();
    old_value= String();
  }
  size_t get_field_buffer_size() { return value.alloced_length(); }
#ifndef WORDS_BIGENDIAN
  static
#endif
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number, bool low_byte_first);
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number)
  {
    store_length(i_ptr, i_packlength, i_number, table->s->db_low_byte_first);
  }
  inline void store_length(uint32 number)
  {
    store_length(ptr, packlength, number);
  }
  uint32 data_length(uint row_offset= 0) { return get_length(row_offset); }
  inline uint32 get_length(uint row_offset= 0)
  { return get_length(ptr+row_offset, this->packlength, table->s->db_low_byte_first); }
  uint32 get_length(const uchar *ptr, uint packlength, bool low_byte_first);
  uint32 get_length(const uchar *ptr_arg)
  { return get_length(ptr_arg, this->packlength, table->s->db_low_byte_first); }
  void put_length(uchar *pos, uint32 length);
  inline void get_ptr(uchar **str)
    {
      memcpy(str, ptr+packlength, sizeof(uchar*));
    }
  inline void get_ptr(uchar **str, uint row_offset)
    {
      memcpy(str, ptr+packlength+row_offset, sizeof(char*));
    }
  inline void set_ptr(uchar *length, uchar *data)
    {
      memcpy(ptr,length,packlength);
      memcpy(ptr+packlength, &data,sizeof(char*));
    }
  void set_ptr_offset(my_ptrdiff_t ptr_diff, uint32 length, uchar *data)
    {
      uchar *ptr_ofs= ADD_TO_PTR(ptr,ptr_diff,uchar*);
      store_length(ptr_ofs, packlength, length);
      memcpy(ptr_ofs+packlength, &data, sizeof(char*));
    }
  inline void set_ptr(uint32 length, uchar *data)
    {
      set_ptr_offset(0, length, data);
    }
  size_t get_key_image(uchar *buff, size_t length, imagetype type);
  void set_key_image(const uchar *buff, size_t length);
  void sql_type(String &str) const;
  inline bool copy()
  {
    uchar *tmp;
    get_ptr(&tmp);
    if (value.copy((char*) tmp, get_length(), charset()))
    {
      Field_blob::reset();
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), get_length());
      return 1;
    }
    tmp=(uchar*) value.ptr();
    memcpy(ptr+packlength, &tmp, sizeof(char*));
    return 0;
  }
  Field_blob *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_BLOB);
    return new (mem_root) Field_blob(*this);
  }
  Field_blob *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_BLOB);
    return new Field_blob(*this);
  }
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);
  uint packed_col_length(const uchar *col_ptr, uint length);
  uint max_packed_col_length();
  void mem_free()
  {
    // Free all allocated space
    value.mem_free();
    old_value.mem_free();
  }
  inline void clear_temporary() { value= String(); }
  friend type_conversion_status field_conv(Field *to,Field *from);
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  uint32 max_display_length();
  uint32 char_length();
  bool copy_blob_value(MEM_ROOT *mem_root);
  uint is_equal(Create_field *new_field);
  inline bool in_read_set() { return bitmap_is_set(table->read_set, field_index); }
  inline bool in_write_set() { return bitmap_is_set(table->write_set, field_index); }
  virtual bool is_text_key_type() const { return binary() ? false : true; }

  /**
    Mark that the BLOB stored in value should be copied before updating it.

    When updating virtual generated columns we need to keep the old
    'value' for BLOBs since this can be needed when the storage engine
    does the update. During read of the record the old 'value' for the
    BLOB is evaluated and stored in 'value'. This function is to be used
    to specify that we need to copy this BLOB 'value' into 'old_value'
    before we compute the new BLOB 'value'. For more information @see
    Field_blob::keep_old_value().
  */
  void set_keep_old_value(bool old_value_flag)
  {
    /*
      We should only need to keep a copy of the blob 'value' in the case
      where this is a virtual genarated column (that is indexed).
    */
    DBUG_ASSERT(is_virtual_gcol());

    /*
      If set to true, ensure that 'value' is copied to 'old_value' when
      keep_old_value() is called.
    */
    m_keep_old_value= old_value_flag;
  }

  /**
    Save the current BLOB value to avoid that it gets overwritten.

    This is used when updating virtual generated columns that are
    BLOBs. Some storage engines require that we have both the old and
    new BLOB value for virtual generated columns that are indexed in
    order for the storage engine to be able to maintain the index. This
    function will transfer the buffer storing the current BLOB value
    from 'value' to 'old_value'. This avoids that the current BLOB value
    is over-written when the new BLOB value is saved into this field.

    The reason this requires special handling when updating/deleting
    virtual columns of BLOB type is that the BLOB value is not known to
    the storage engine. For stored columns, the "old" BLOB value is read
    by the storage engine, Field_blob is made to point to the engine's
    internal buffer; Field_blob's internal buffer (Field_blob::value)
    isn't used and remains available to store the "new" value.  For
    virtual generated columns, the "old" value is written directly into
    Field_blob::value when reading the record to be
    updated/deleted. This is done in update_generated_read_fields().
    Since, in this case, the "old" value already occupies the place to
    store the "new" value, we must call this function before we write
    the "new" value into Field_blob::value object so that the "old"
    value does not get over-written. The table->record[1] buffer will
    have a pointer that points to the memory buffer inside
    old_value. The storage engine will use table->record[1] to read the
    old value for the BLOB and use table->record[0] to read the new
    value.

    This function must be called before we store the new BLOB value in
    this field object.
  */
  void keep_old_value()
  {
    /*
      We should only need to keep a copy of the blob value in the case
      where this is a virtual genarated column (that is indexed).
    */
    DBUG_ASSERT(is_virtual_gcol());

    // Transfer ownership of the current BLOB value to old_value
    if (m_keep_old_value)
    {
      old_value.takeover(value);
      m_keep_old_value= false;
    }
  }

  /**
    Use to store the blob value into an allocated space.
  */ 
  void store_in_allocated_space(const char *from, uint32 length)
  {
    store_ptr_and_length(from, length);
  }

private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_geom :public Field_blob {
  virtual type_conversion_status store_internal(const char *from, size_t length,
                                                const CHARSET_INFO *cs);
public:
  enum geometry_type geom_type;

  Field_geom(uchar *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TABLE_SHARE *share, uint blob_pack_length,
	     enum geometry_type geom_type_arg)
     :Field_blob(ptr_arg, null_ptr_arg, null_bit_arg, unireg_check_arg, 
                 field_name_arg, share, blob_pack_length, &my_charset_bin)
  { geom_type= geom_type_arg; }
  Field_geom(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     TABLE_SHARE *share, enum geometry_type geom_type_arg)
    :Field_blob(len_arg, maybe_null_arg, field_name_arg, &my_charset_bin, false)
  { geom_type= geom_type_arg; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_VARBINARY2; }
  enum_field_types type() const { return MYSQL_TYPE_GEOMETRY; }
  bool match_collation_to_optimize_range() const { return false; }
  void sql_type(String &str) const;
  using Field_blob::store;
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_decimal(const my_decimal *);
  type_conversion_status store(const char *from, size_t length,
                               const CHARSET_INFO *cs);

  /**
    Non-nullable GEOMETRY types cannot have defaults,
    but the underlying blob must still be reset.
   */
  type_conversion_status reset(void)
  {
    type_conversion_status res= Field_blob::reset();
    if (res != TYPE_OK)
      return res;
    return maybe_null() ? TYPE_OK : TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }

  geometry_type get_geometry_type() const { return geom_type; };
  Field_geom *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(type() == MYSQL_TYPE_GEOMETRY);
    return new (mem_root) Field_geom(*this);
  }
  Field_geom *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_GEOMETRY);
    return new Field_geom(*this);
  }
  uint is_equal(Create_field *new_field);
};


/// A field that stores a JSON value.
class Field_json :public Field_blob
{
  type_conversion_status unsupported_conversion();
  type_conversion_status store_binary(const char *ptr, size_t length);

  /**
    Diagnostics utility for ER_INVALID_JSON_TEXT.

    @param err        error message argument for ER_INVALID_JSON_TEXT
    @param err_offset location in text at which there is an error
  */
  void invalid_text(const char *err, size_t err_offset) const
  {
    String s;
    s.append(*table_name);
    s.append('.');
    s.append(field_name);
    my_error(ER_INVALID_JSON_TEXT, MYF(0), err, err_offset, s.c_ptr_safe());
  }
public:
  Field_json(uchar *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
             enum utype unireg_check_arg, const char *field_name_arg,
             TABLE_SHARE *share, uint blob_pack_length)
    : Field_blob(ptr_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
                 field_name_arg, share, blob_pack_length, &my_charset_bin)
  {}

  Field_json(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg)
    :Field_blob(len_arg, maybe_null_arg, field_name_arg, &my_charset_bin, false)
  {}

  enum_field_types type() const { return MYSQL_TYPE_JSON; }
  void sql_type(String &str) const;
  /**
    Return a text charset so that string functions automatically
    convert the field value to string and treat it as a non-binary
    string.
  */
  const CHARSET_INFO *charset() const { return &my_charset_utf8mb4_bin; }
  /**
    Sort should treat the field as binary and not attempt any
    conversions.
  */
  const CHARSET_INFO *sort_charset() const { return field_charset; }
  /**
    JSON columns don't have an associated charset. Returning false
    here prevents SHOW CREATE TABLE from attaching a CHARACTER SET
    clause to the column.
  */
  bool has_charset() const { return false; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_decimal(const my_decimal *);
  type_conversion_status store_json(Json_wrapper *json);
  type_conversion_status store_time(MYSQL_TIME *ltime, uint8 dec_arg);
  type_conversion_status store(Field_json *field);

  /**
    Retrieve the field's value as a JSON wrapper. It
    there is an error, wr is not modified and we return
    false, else true.

    @param[out]    wr   the JSON value
    @return true if a value is retrieved (or NULL), false if error
  */
  bool val_json(Json_wrapper *wr);

  /**
    Retrieve the JSON as an int if possible. This requires a JSON scalar
    of suitable type.

    @returns the JSON value as an int
  */
  longlong val_int();

  /**
   Retrieve the JSON as a double if possible. This requires a JSON scalar
   of suitable type.

   @returns the JSON value as a double
   */
  double val_real();

  /**
    Retrieve the JSON value stored in this field as text

    @param[in,out] buf1 string buffer for converting JSON value to string
    @param[in,out] buf2 unused
  */
  String *val_str(String *tmp, String *str);
  my_decimal *val_decimal(my_decimal *m);
  bool get_time(MYSQL_TIME *ltime);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  Field_json *clone(MEM_ROOT *mem_root) const;
  Field_json *clone() const;
  uint is_equal(Create_field *new_field);
  Item_result cast_to_int_type () const { return INT_RESULT; }
  void make_sort_key(uchar *to, size_t length);

  /**
    Make a hash key that can be used by sql_executor.cc/unique_hash
    in order to support SELECT DISTINCT

    @param[in]  hash_val  An initial hash value.
  */
  ulonglong make_hash_key(ulonglong *hash_val);
};


class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg,
             enum utype unireg_check_arg, const char *field_name_arg,
             uint packlength_arg,
             TYPELIB *typelib_arg,
             const CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, charset_arg),
    packlength(packlength_arg),typelib(typelib_arg)
  {
    flags|=ENUM_FLAG;
  }
  Field *new_field(MEM_ROOT *root, TABLE *new_table, bool keep_type);
  enum_field_types type() const { return MYSQL_TYPE_STRING; }
  bool match_collation_to_optimize_range() const { return false; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum Item_result cast_to_int_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  double val_real(void);
  my_decimal *val_decimal(my_decimal *decimal_value);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void make_sort_key(uchar *buff, size_t length);
  uint32 pack_length() const { return (uint32) packlength; }
  void store_type(ulonglong value);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return MYSQL_TYPE_ENUM; }
  uint pack_length_from_metadata(uint field_metadata)
  { return (field_metadata & 0x00ff); }
  uint row_pack_length() const { return pack_length(); }
  virtual bool zero_pack() const { return 0; }
  bool optimize_range(uint idx, uint part) { return 0; }
  bool eq_def(Field *field);
  bool has_charset(void) const { return TRUE; }
  /* enum and set are sorted as integers */
  CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
  Field_enum *clone(MEM_ROOT *mem_root) const {
    DBUG_ASSERT(real_type() == MYSQL_TYPE_ENUM);
    return new (mem_root) Field_enum(*this);
  }
  Field_enum *clone() const { 
    DBUG_ASSERT(real_type() == MYSQL_TYPE_ENUM);
    return new Field_enum(*this);
  }
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);

private:
  int do_save_field_metadata(uchar *first_byte);
  uint is_equal(Create_field *new_field);
};


class Field_set :public Field_enum {
public:
  Field_set(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
            uchar null_bit_arg,
            enum utype unireg_check_arg, const char *field_name_arg,
            uint32 packlength_arg,
            TYPELIB *typelib_arg, const CHARSET_INFO *charset_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                packlength_arg,
                typelib_arg,charset_arg),
     empty_set_string("", 0, charset_arg)
  {
    flags= (flags & ~ENUM_FLAG) | SET_FLAG;
  }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr)
  {
    return Field_set::store((longlong) nr, FALSE);
  }
  type_conversion_status store(longlong nr, bool unsigned_val);
  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return MYSQL_TYPE_SET; }
  bool has_charset(void) const { return TRUE; }
  Field_set *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(real_type() == MYSQL_TYPE_SET);
    return new (mem_root) Field_set(*this);
  }
  Field_set *clone() const {
    DBUG_ASSERT(real_type() == MYSQL_TYPE_SET);
    return new Field_set(*this);
  }
private:
  const String empty_set_string;
};


/*
  Note:
    To use Field_bit::cmp_binary() you need to copy the bits stored in
    the beginning of the record (the NULL bytes) to each memory you
    want to compare (where the arguments point).

    This is the reason:
    - Field_bit::cmp_binary() is only implemented in the base class
      (Field::cmp_binary()).
    - Field::cmp_binary() currenly use pack_length() to calculate how
      long the data is.
    - pack_length() includes size of the bits stored in the NULL bytes
      of the record.
*/
class Field_bit :public Field {
public:
  uchar *bit_ptr;     // position in record where 'uneven' bits store
  uchar bit_ofs;      // offset to 'uneven' high bits
  uint bit_len;       // number of 'uneven' high bits
  uint bytes_in_rec;
  Field_bit(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
            uchar null_bit_arg, uchar *bit_ptr_arg, uchar bit_ofs_arg,
            enum utype unireg_check_arg, const char *field_name_arg);
  enum_field_types type() const { return MYSQL_TYPE_BIT; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BIT; }
  uint32 key_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 max_data_length() const { return (field_length + 7) / 8; }
  uint32 max_display_length() { return field_length; }
  Item_result result_type () const { return INT_RESULT; }
  type_conversion_status reset(void)
  {
    memset(ptr, 0, bytes_in_rec);
    if (bit_ptr && (bit_len > 0))  // reset odd bits among null bits
      clr_rec_bits(bit_ptr, bit_ofs, bit_len);
    return TYPE_OK;
  }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr);
  type_conversion_status store(longlong nr, bool unsigned_val);
  type_conversion_status store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*, String *);
  virtual bool str_needs_quotes() { return TRUE; }
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *a, const uchar *b)
  {
    DBUG_ASSERT(ptr == a || ptr == b);
    if (ptr == a)
      return Field_bit::key_cmp(b, bytes_in_rec+MY_TEST(bit_len));
    else
      return Field_bit::key_cmp(a, bytes_in_rec+MY_TEST(bit_len)) * -1;
  }
  int cmp_binary_offset(uint row_offset)
  { return cmp_offset(row_offset); }
  int cmp_max(const uchar *a, const uchar *b, uint max_length);
  int key_cmp(const uchar *a, const uchar *b)
  { return cmp_binary((uchar *) a, (uchar *) b); }
  int key_cmp(const uchar *str, uint length);
  int cmp_offset(uint row_offset);
  void get_image(uchar *buff, size_t length, const CHARSET_INFO *cs)
  { get_key_image(buff, length, itRAW); }   
  void set_image(const uchar *buff, size_t length, const CHARSET_INFO *cs)
  { Field_bit::store((char *) buff, length, cs); }
  size_t get_key_image(uchar *buff, size_t length, imagetype type);
  void set_key_image(const uchar *buff, size_t length)
  { Field_bit::store((char*) buff, length, &my_charset_bin); }
  void make_sort_key(uchar *buff, size_t length)
  { get_key_image(buff, length, itRAW); }
  uint32 pack_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 pack_length_in_rec() const { return bytes_in_rec; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length() const
  { return (bytes_in_rec + ((bit_len > 0) ? 1 : 0)); }
  bool compatible_field_size(uint metadata, Relay_log_info *rli,
                             uint16 mflags, int *order_var);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);
  virtual void set_default();

  Field *new_key_field(MEM_ROOT *root, TABLE *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  void set_bit_ptr(uchar *bit_ptr_arg, uchar bit_ofs_arg)
  {
    bit_ptr= bit_ptr_arg;
    bit_ofs= bit_ofs_arg;
  }
  bool eq(Field *field)
  {
    return (Field::eq(field) &&
            bit_ptr == ((Field_bit *)field)->bit_ptr &&
            bit_ofs == ((Field_bit *)field)->bit_ofs);
  }
  uint is_equal(Create_field *new_field);
  void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    Field::move_field_offset(ptr_diff);
    bit_ptr= ADD_TO_PTR(bit_ptr, ptr_diff, uchar*);
  }
  void hash(ulong *nr, ulong *nr2);
  Field_bit *clone(MEM_ROOT *mem_root) const { 
    DBUG_ASSERT(type() == MYSQL_TYPE_BIT);
    return new (mem_root) Field_bit(*this);
  }
  Field_bit *clone() const {
    DBUG_ASSERT(type() == MYSQL_TYPE_BIT);
    return new Field_bit(*this);
  }
private:
  virtual size_t do_last_null_byte() const;
  int do_save_field_metadata(uchar *first_byte);
};


/**
  BIT field represented as chars for non-MyISAM tables.

  @todo The inheritance relationship is backwards since Field_bit is
  an extended version of Field_bit_as_char and not the other way
  around. Hence, we should refactor it to fix the hierarchy order.
 */
class Field_bit_as_char: public Field_bit {
public:
  Field_bit_as_char(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg);
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  type_conversion_status store(const char *to, size_t length,
                               const CHARSET_INFO *charset);
  type_conversion_status store(double nr) { return Field_bit::store(nr); }
  type_conversion_status store(longlong nr, bool unsigned_val)
  { return Field_bit::store(nr, unsigned_val); }
  void sql_type(String &str) const;
  Field_bit_as_char *clone(MEM_ROOT *mem_root) const { 
    return new (mem_root) Field_bit_as_char(*this);
  }
  Field_bit_as_char *clone() const { return new Field_bit_as_char(*this); }
};


/*
  Create field class for CREATE TABLE
*/

class Create_field :public Sql_alloc
{
public:
  const char *field_name;
  /**
    Name of column modified by ALTER TABLE's CHANGE/MODIFY COLUMN clauses,
    NULL for columns added.
  */
  const char *change;
  const char *after;			// Put column after this one
  LEX_STRING comment;			// Comment for field

  /**
     The declared default value, if any, otherwise NULL. Note that this member
     is NULL if the default is a function. If the column definition has a
     function declared as the default, the information is found in
     Create_field::unireg_check.
     
     @see Create_field::unireg_check
  */
  Item *def;
  enum	enum_field_types sql_type;
  /*
    At various stages in execution this can be length of field in bytes or
    max number of characters. 
  */
  size_t length;
  /*
    The value of `length' as set by parser: is the number of characters
    for most of the types, or of bytes for BLOBs or numeric types.
  */
  size_t char_length;
  uint  decimals, flags;
  size_t pack_length, key_length;
  Field::utype unireg_check;
  TYPELIB *interval;			// Which interval to use
  TYPELIB *save_interval;               // Temporary copy for the above
                                        // Used only for UCS2 intervals
  List<String> interval_list;
  const CHARSET_INFO *charset;
  Field::geometry_type geom_type;
  Field *field;				// For alter table

  uint8 row,col,sc_length,interval_id;	// For rea_create_table
  uint	offset,pack_flag;

  /* Generated column expression information */
  Generated_column *gcol_info;
  /*
    Indication that the field is phycically stored in tables 
    rather than just generated on SQL queries.
    As of now, FALSE can only be set for virtual generated columns.
  */
  bool stored_in_db;

  Create_field() :after(NULL) {}
  Create_field(Field *field, Field *orig_field);
  /* Used to make a clone of this object for ALTER/CREATE TABLE */
  Create_field *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Create_field(*this); }
  bool is_gcol() const { return gcol_info; }
  bool is_virtual_gcol() const
  { return gcol_info && !gcol_info->get_field_stored(); }
  void create_length_to_internal_length(void);

  /* Init for a tmp table field. To be extended if need be. */
  void init_for_tmp_table(enum_field_types sql_type_arg,
                          uint32 max_length, uint32 decimals,
                          bool maybe_null, bool is_unsigned,
                          uint pack_length = ~0U);

  bool init(THD *thd, const char *field_name, enum_field_types type,
            const char *length, const char *decimals, uint type_modifier,
            Item *default_value, Item *on_update_value, LEX_STRING *comment,
            const char *change, List<String> *interval_list,
            const CHARSET_INFO *cs, uint uint_geom_type,
            Generated_column *gcol_info= NULL);

  ha_storage_media field_storage_type() const
  {
    return (ha_storage_media)
      ((flags >> FIELD_FLAGS_STORAGE_MEDIA) & 3);
  }

  column_format_type column_format() const
  {
    return (column_format_type)
      ((flags >> FIELD_FLAGS_COLUMN_FORMAT) & 3);
  }
};


/*
  A class for sending info to the client
*/

class Send_field :public Sql_alloc {
 public:
  const char *db_name;
  const char *table_name,*org_table_name;
  const char *col_name,*org_col_name;
  ulong length;
  uint charsetnr, flags, decimals;
  enum_field_types type;
  /*
    TRUE <=> source item is an Item_field. Needed to workaround lack of
    architecture in legacy Protocol_text implementation. Needed only for
    Protocol_classic and descendants.
  */
  bool field;
  Send_field() {}
};


/*
  A class for quick copying data to fields
*/

class Copy_field :public Sql_alloc {
  /**
    Convenience definition of a copy function returned by
    get_copy_func.
  */
  typedef void Copy_func(Copy_field*);
  Copy_func *get_copy_func(Field *to, Field *from);
public:
  uchar *from_ptr,*to_ptr;
  uchar *from_null_ptr,*to_null_ptr;
  my_bool *null_row;
  uint	from_bit,to_bit;
  String tmp;					// For items

  Copy_field()
   :m_from_field(NULL),
    m_to_field(NULL)
  { }

  ~Copy_field()
  { }

  void set(Field *to, Field *from, bool save);	// Field to field
  void set(uchar *to, Field *from);		// Field to string

private:
  void (*m_do_copy)(Copy_field *);
  void (*m_do_copy2)(Copy_field *);		// Used to handle null values

  /**
    Number of bytes in the fields pointed to by 'from_ptr' and
    'to_ptr'. Usually this is the number of bytes that are copied from
    'from_ptr' to 'to_ptr'.

    For variable-length fields (VARCHAR), the first byte(s) describe
    the actual length of the text. For VARCHARs with length 
       < 256 there is 1 length byte 
       >= 256 there is 2 length bytes
    Thus, if from_field is VARCHAR(10), from_length (and in most cases
    to_length) is 11. For VARCHAR(1024), the length is 1026. @see
    Field_varstring::length_bytes

    Note that for VARCHARs, do_copy() will be do_varstring*() which
    only copies the length-bytes (1 or 2) + the actual length of the
    text instead of from/to_length bytes. @see get_copy_func()
  */
  uint m_from_length;
  uint m_to_length;
  Field *m_from_field;
  Field *m_to_field;

  void check_and_set_temporary_null()
  {
    if (m_from_field &&
        m_from_field->is_tmp_null() &&
        !m_to_field->is_tmp_null())
    {
      m_to_field->set_tmp_nullable();
      m_to_field->set_tmp_null();
    }
  }

public:
  void invoke_do_copy(Copy_field *f);
  void invoke_do_copy2(Copy_field *f);

  Field *from_field()
  { return m_from_field; }

  Field *to_field()
  { return m_to_field; }

  uint from_length() const
  { return m_from_length; }

  uint to_length() const
  { return m_to_length; }
};


Field *make_field(TABLE_SHARE *share, uchar *ptr, size_t field_length,
		  uchar *null_pos, uchar null_bit,
		  uint pack_flag, enum_field_types field_type,
		  const CHARSET_INFO *cs,
		  Field::geometry_type geom_type,
		  Field::utype unireg_check,
		  TYPELIB *interval, const char *field_name);
uint pack_length_to_packflag(uint type);
enum_field_types get_blob_type_from_length(ulong length);
size_t calc_pack_length(enum_field_types type, size_t length);
type_conversion_status set_field_to_null(Field *field);
type_conversion_status set_field_to_null_with_conversions(Field *field,
                                                          bool no_conversions);

/*
  The following are for the interface with the .frm file
*/

#define FIELDFLAG_DECIMAL		1
#define FIELDFLAG_BINARY		1	// Shares same flag
#define FIELDFLAG_NUMBER		2
#define FIELDFLAG_ZEROFILL		4
#define FIELDFLAG_PACK			120	// Bits used for packing
#define FIELDFLAG_INTERVAL		256     // mangled with decimals!
#define FIELDFLAG_BITFIELD		512	// mangled with decimals!
#define FIELDFLAG_BLOB			1024	// mangled with decimals!
#define FIELDFLAG_GEOM			2048    // mangled with decimals!
#define FIELDFLAG_JSON                  4096    /* mangled with decimals and
                                                   with bitfields! */

#define FIELDFLAG_TREAT_BIT_AS_CHAR     4096    /* use Field_bit_as_char */

#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_NO_DEFAULT		16384   /* sql */
#define FIELDFLAG_SUM			((uint) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint) 32768)// sql
#define FIELDFLAG_PACK_SHIFT		3
#define FIELDFLAG_DEC_SHIFT		8
#define FIELDFLAG_MAX_DEC		31
#define FIELDFLAG_NUM_SCREEN_TYPE	0x7F01
#define FIELDFLAG_ALFA_SCREEN_TYPE	0x7800

#define MTYP_TYPENR(type) (type & 127)	/* Remove bits from type */

inline int f_is_dec(int x)
{
  return (x & FIELDFLAG_DECIMAL);
}
inline int f_is_num(int x)
{
  return (x & FIELDFLAG_NUMBER);
}
inline int f_is_zerofill(int x)
{
  return (x & FIELDFLAG_ZEROFILL);
}
inline int f_is_packed(int x)
{
  return (x & FIELDFLAG_PACK);
}
inline int f_packtype(int x)
{
  return ((x >> FIELDFLAG_PACK_SHIFT) & 15);
}
inline uint8 f_decimals(int x)
{
  return ((uint8) ((x >> FIELDFLAG_DEC_SHIFT) & FIELDFLAG_MAX_DEC));
}
inline int f_is_alpha(int x)
{
  return (!f_is_num(x));
}
inline int f_is_binary(int x)
{
  return (x & FIELDFLAG_BINARY); // 4.0- compatibility
}
inline int f_is_enum(int x)
{
  return ((x & (FIELDFLAG_INTERVAL | FIELDFLAG_NUMBER)) == FIELDFLAG_INTERVAL);
}
inline int f_is_bitfield(int x)
{
  return ((x & (FIELDFLAG_BITFIELD | FIELDFLAG_NUMBER)) == FIELDFLAG_BITFIELD);
}
inline int f_is_blob(int x)
{
  return ((x & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB);
}
inline int f_is_geom(int x)
{
  return ((x & (FIELDFLAG_GEOM | FIELDFLAG_NUMBER)) == FIELDFLAG_GEOM);
}
inline int f_is_json(int x)
{
  return ((x & (FIELDFLAG_JSON | FIELDFLAG_NUMBER | FIELDFLAG_BITFIELD)) ==
          FIELDFLAG_JSON);
}
inline int f_is_equ(int x)
{
  return (x & (1+2+FIELDFLAG_PACK+31*256));
}
inline int f_settype(int x)
{
  return (x << FIELDFLAG_PACK_SHIFT);
}
inline int f_maybe_null(int x)
{
  return (x & FIELDFLAG_MAYBE_NULL);
}
inline int f_no_default(int x)
{
  return (x & FIELDFLAG_NO_DEFAULT);
}
inline int f_bit_as_char(int x)
{
  return (x & FIELDFLAG_TREAT_BIT_AS_CHAR);
}

#endif /* FIELD_INCLUDED */
