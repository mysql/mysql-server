/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  @brief
  Functions to copy data to or from fields

    This could be done with a single short function but opencoding this
    gives much more speed.
*/

#include <string.h>
#include <sys/types.h>
#include <algorithm>

#include "binary_log_types.h"
#include "m_ctype.h"
#include "my_byteorder.h"
#include "my_compare.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "nullable.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/item_timefunc.h"  // Item_func_now_local
#include "sql/my_decimal.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_time.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"  // down_cast

/**
  Check if geometry type sub is a subtype of super.

  @param sub The type to check
  @param super The supertype

  @return True if sub is a subtype of super
 */
inline static bool is_subtype_of(Field::geometry_type sub,
                                 Field::geometry_type super) {
  return (
      super == Field::GEOM_GEOMETRY ||
      (super == Field::GEOM_GEOMETRYCOLLECTION &&
       (sub == Field::GEOM_MULTIPOINT || sub == Field::GEOM_MULTILINESTRING ||
        sub == Field::GEOM_MULTIPOLYGON)));
}

static void do_field_eq(Copy_field *copy) {
  memcpy(copy->to_ptr, copy->from_ptr, copy->from_length());
}

static void do_field_1(Copy_field *copy) {
  copy->to_ptr[0] = copy->from_ptr[0];
}

static void do_field_2(Copy_field *copy) {
  copy->to_ptr[0] = copy->from_ptr[0];
  copy->to_ptr[1] = copy->from_ptr[1];
}

static void do_field_3(Copy_field *copy) {
  copy->to_ptr[0] = copy->from_ptr[0];
  copy->to_ptr[1] = copy->from_ptr[1];
  copy->to_ptr[2] = copy->from_ptr[2];
}

static void do_field_4(Copy_field *copy) {
  copy->to_ptr[0] = copy->from_ptr[0];
  copy->to_ptr[1] = copy->from_ptr[1];
  copy->to_ptr[2] = copy->from_ptr[2];
  copy->to_ptr[3] = copy->from_ptr[3];
}

static void do_field_6(Copy_field *copy) {  // For blob field
  copy->to_ptr[0] = copy->from_ptr[0];
  copy->to_ptr[1] = copy->from_ptr[1];
  copy->to_ptr[2] = copy->from_ptr[2];
  copy->to_ptr[3] = copy->from_ptr[3];
  copy->to_ptr[4] = copy->from_ptr[4];
  copy->to_ptr[5] = copy->from_ptr[5];
}

static void do_field_8(Copy_field *copy) {
  copy->to_ptr[0] = copy->from_ptr[0];
  copy->to_ptr[1] = copy->from_ptr[1];
  copy->to_ptr[2] = copy->from_ptr[2];
  copy->to_ptr[3] = copy->from_ptr[3];
  copy->to_ptr[4] = copy->from_ptr[4];
  copy->to_ptr[5] = copy->from_ptr[5];
  copy->to_ptr[6] = copy->from_ptr[6];
  copy->to_ptr[7] = copy->from_ptr[7];
}

static void do_field_to_null_str(Copy_field *copy) {
  if (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)) {
    memset(copy->to_ptr, 0, copy->from_length());
    copy->to_null_ptr[0] = 1;  // Always bit 1
  } else {
    copy->to_null_ptr[0] = 0;
    memcpy(copy->to_ptr, copy->from_ptr, copy->from_length());
  }
}

type_conversion_status set_field_to_null(Field *field) {
  if (field->real_maybe_null() || field->is_tmp_nullable()) {
    field->set_null();
    field->reset();
    return TYPE_OK;
  }

  /**
    The following piece of code is run for the case when a BLOB column
    that has value NULL is queried with GROUP BY NULL and the result
    is inserted into a some table's column declared having primitive type
    (e.g. INT) and NOT NULL.

    For example, the following test case will hit this piece of code:
    CREATE TABLE t1 (a BLOB);
    CREATE TABLE t2 (a INT NOT NULL);

    INSERT t1 VALUES (NULL);
    INSERT INTO t2(a) SELECT a FROM t1 GROUP BY NULL; <<== Hit here

    In general, when set_field_to_null() is called a Field has to be either
    declared as NULL-able or be marked as temporary NULL-able.
    But in case of INSERT SELECT from a BLOB field and when GROUP BY NULL
    is specified the Field object for a destination column doesn't set
    neither NULL-able nor temporary NULL-able (see setup_copy_fields()).
  */
  field->reset();
  switch (field->table->in_use->check_for_truncated_fields) {
    case CHECK_FIELD_WARN:
      field->set_warning(Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED, 1);
      /* fall through */
    case CHECK_FIELD_IGNORE:
      return TYPE_OK;
    case CHECK_FIELD_ERROR_FOR_NULL:
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
      return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }
  DBUG_ASSERT(false);  // impossible

  my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
  return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;  // to avoid compiler's warning
}

/**
  Set field to NULL or TIMESTAMP or to next auto_increment number.

  @param field           Field to update
  @param no_conversions  Set to 1 if we should return 1 if field can't
                         take null values.
                         If set to 0 we will do store the 'default value'
                         if the field is a special field. If not we will
                         give an error.

  @retval
    0    Field could take 0 or an automatic conversion was used
  @retval
    -1   Field could not take NULL and no conversion was used.
    If no_conversion was not set, an error message is printed
*/

type_conversion_status set_field_to_null_with_conversions(Field *field,
                                                          bool no_conversions) {
  if (field->real_maybe_null()) {
    field->set_null();
    field->reset();
    return TYPE_OK;
  }

  if (no_conversions) return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;

  /*
    Check if this is a special type, which will get a special walue
    when set to NULL (TIMESTAMP fields which allow setting to NULL
    are handled by first check).

    From the manual:

    TIMESTAMP columns [...] assigning NULL assigns the current timestamp.

    But if explicit_defaults_for_timestamp, use standard-compliant behaviour:
    no special value.
  */
  if (field->type() == MYSQL_TYPE_TIMESTAMP &&
      !field->table->in_use->variables.explicit_defaults_for_timestamp) {
    Item_func_now_local::store_in(field);
    return TYPE_OK;  // Ok to set time to NULL
  }

  // Note: we ignore any potential failure of reset() here.
  field->reset();

  if (field == field->table->next_number_field) {
    field->table->auto_increment_field_not_null = false;
    return TYPE_OK;  // field is set in fill_record()
  }

  if (field->is_tmp_nullable()) {
    field->set_null();
    field->reset();
    return TYPE_OK;
  }

  // Conversion of NULL to empty string does not apply to geometry columns.
  if (field->type() == MYSQL_TYPE_GEOMETRY) {
    my_error(ER_BAD_NULL_ERROR_NOT_IGNORED, MYF(0), field->field_name);
    return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }

  switch (field->table->in_use->check_for_truncated_fields) {
    case CHECK_FIELD_WARN:
      field->set_warning(Sql_condition::SL_WARNING, ER_BAD_NULL_ERROR, 1);
      /* fall through */
    case CHECK_FIELD_IGNORE:
      if (field->type() == MYSQL_TYPE_BLOB) {
        /*
          BLOB/TEXT fields only store a pointer to their actual contents
          in the record. Make this a valid pointer to an empty string
          instead of nullptr.
        */
        return field->store("", 0, field->charset());
      }
      return TYPE_OK;
    case CHECK_FIELD_ERROR_FOR_NULL:
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
      return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }
  DBUG_ASSERT(false);  // impossible
  my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
  return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
}

static void do_skip(Copy_field *copy MY_ATTRIBUTE((unused))) {}

static void do_copy_null(Copy_field *copy) {
  if (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)) {
    *copy->to_null_ptr |= copy->to_bit;
    copy->to_field()->reset();
  } else {
    *copy->to_null_ptr &= ~copy->to_bit;
    copy->invoke_do_copy2(copy);
  }
}

static void do_copy_not_null(Copy_field *copy) {
  if (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)) {
    if (copy->to_field()->reset() == TYPE_ERR_NULL_CONSTRAINT_VIOLATION)
      my_error(ER_INVALID_USE_OF_NULL, MYF(0));
    else
      copy->to_field()->set_warning(Sql_condition::SL_WARNING,
                                    WARN_DATA_TRUNCATED, 1);
  } else
    copy->invoke_do_copy2(copy);
}

static void do_copy_maybe_null(Copy_field *copy) {
  /*
    In reverse copying (see bring_back_frame_row() for windowing),
    "to" is "from" and it may not have a null bit.
  */
  if (copy->to_null_ptr) *copy->to_null_ptr &= ~copy->to_bit;
  copy->invoke_do_copy2(copy);
}

/* timestamp and next_number has special handling in case of NULL values */

static void do_copy_timestamp(Copy_field *copy) {
  if (*copy->from_null_ptr & copy->from_bit) {
    /* Same as in set_field_to_null_with_conversions() */
    Item_func_now_local::store_in(copy->to_field());
  } else
    copy->invoke_do_copy2(copy);
}

static void do_copy_next_number(Copy_field *copy) {
  if (*copy->from_null_ptr & copy->from_bit) {
    /* Same as in set_field_to_null_with_conversions() */
    copy->to_field()->table->auto_increment_field_not_null = false;
    copy->to_field()->reset();
  } else
    copy->invoke_do_copy2(copy);
}

static void do_copy_blob(Copy_field *copy) {
  ulong from_length = ((Field_blob *)copy->from_field())->get_length();
  ((Field_blob *)copy->to_field())->store_length(from_length);
  memcpy(copy->to_ptr, copy->from_ptr, sizeof(char *));
  ulong to_length = ((Field_blob *)copy->to_field())->get_length();
  if (to_length < from_length) {
    if (copy->to_field()->table->in_use->is_strict_mode()) {
      copy->to_field()->set_warning(Sql_condition::SL_WARNING, ER_DATA_TOO_LONG,
                                    1);
    } else {
      copy->to_field()->set_warning(Sql_condition::SL_WARNING,
                                    WARN_DATA_TRUNCATED, 1);
    }
  }
}

static void do_conv_blob(Copy_field *copy) {
  copy->from_field()->val_str(&copy->tmp);
  ((Field_blob *)copy->to_field())
      ->store(copy->tmp.ptr(), copy->tmp.length(), copy->tmp.charset());
}

/** Save blob in copy->tmp for GROUP BY. */

static void do_save_blob(Copy_field *copy) {
  char buff[MAX_FIELD_WIDTH];
  String res(buff, sizeof(buff), copy->tmp.charset());
  copy->from_field()->val_str(&res);
  copy->tmp.copy(res);
  ((Field_blob *)copy->to_field())
      ->store(copy->tmp.ptr(), copy->tmp.length(), copy->tmp.charset());
}

/**
  Copy the contents of one Field_json into another Field_json.
*/
static void do_save_json(Copy_field *copy) {
  Field_json *from = down_cast<Field_json *>(copy->from_field());
  Field_json *to = down_cast<Field_json *>(copy->to_field());
  to->store(from);
}

static void do_field_string(Copy_field *copy) {
  char buff[MAX_FIELD_WIDTH];
  String res(buff, sizeof(buff), copy->from_field()->charset());
  res.length(0U);

  copy->from_field()->val_str(&res);
  copy->to_field()->store(res.c_ptr_quick(), res.length(), res.charset());
}

static void do_field_enum(Copy_field *copy) {
  if (copy->from_field()->val_int() == 0)
    ((Field_enum *)copy->to_field())->store_type((ulonglong)0);
  else
    do_field_string(copy);
}

static void do_field_varbinary_pre50(Copy_field *copy) {
  char buff[MAX_FIELD_WIDTH];
  copy->tmp.set_quick(buff, sizeof(buff), copy->tmp.charset());
  copy->from_field()->val_str(&copy->tmp);

  /* Use the same function as in 4.1 to trim trailing spaces */
  size_t length = my_lengthsp_8bit(&my_charset_bin, copy->tmp.c_ptr_quick(),
                                   copy->from_field()->field_length);

  copy->to_field()->store(copy->tmp.c_ptr_quick(), length, copy->tmp.charset());
}

static void do_field_int(Copy_field *copy) {
  longlong value = copy->from_field()->val_int();
  copy->to_field()->store(value, copy->from_field()->flags & UNSIGNED_FLAG);
}

static void do_field_real(Copy_field *copy) {
  double value = copy->from_field()->val_real();
  copy->to_field()->store(value);
}

static void do_field_decimal(Copy_field *copy) {
  my_decimal value;
  copy->to_field()->store_decimal(copy->from_field()->val_decimal(&value));
}

inline type_conversion_status copy_time_to_time(Field *from, Field *to) {
  MYSQL_TIME ltime;
  from->get_time(&ltime);
  return to->store_time(&ltime);
}

/**
  Convert between fields using time representation.
*/
static void do_field_time(Copy_field *copy) {
  (void)copy_time_to_time(copy->from_field(), copy->to_field());
}

/**
  string copy for single byte characters set when to string is shorter than
  from string.
*/

static void do_cut_string(Copy_field *copy) {
  const CHARSET_INFO *cs = copy->from_field()->charset();
  memcpy(copy->to_ptr, copy->from_ptr, copy->to_length());

  /* Check if we loosed any important characters */
  if (cs->cset->scan(cs, (char *)copy->from_ptr + copy->to_length(),
                     (char *)copy->from_ptr + copy->from_length(),
                     MY_SEQ_SPACES) < copy->from_length() - copy->to_length()) {
    copy->to_field()->set_warning(Sql_condition::SL_WARNING,
                                  WARN_DATA_TRUNCATED, 1);
  }
}

/**
  string copy for multi byte characters set when to string is shorter than
  from string.
*/

static void do_cut_string_complex(Copy_field *copy) {  // Shorter string field
  int well_formed_error;
  const CHARSET_INFO *cs = copy->from_field()->charset();
  const uchar *from_end = copy->from_ptr + copy->from_length();
  size_t copy_length = cs->cset->well_formed_len(
      cs, (char *)copy->from_ptr, (char *)from_end,
      copy->to_length() / cs->mbmaxlen, &well_formed_error);
  if (copy->to_length() < copy_length) copy_length = copy->to_length();
  memcpy(copy->to_ptr, copy->from_ptr, copy_length);

  /* Check if we lost any important characters */
  if (well_formed_error ||
      cs->cset->scan(cs, (char *)copy->from_ptr + copy_length, (char *)from_end,
                     MY_SEQ_SPACES) < (copy->from_length() - copy_length)) {
    copy->to_field()->set_warning(Sql_condition::SL_WARNING,
                                  WARN_DATA_TRUNCATED, 1);
  }

  if (copy_length < copy->to_length())
    cs->cset->fill(cs, (char *)copy->to_ptr + copy_length,
                   copy->to_length() - copy_length, ' ');
}

static void do_expand_binary(Copy_field *copy) {
  const CHARSET_INFO *cs = copy->from_field()->charset();
  memcpy(copy->to_ptr, copy->from_ptr, copy->from_length());
  cs->cset->fill(cs, (char *)copy->to_ptr + copy->from_length(),
                 copy->to_length() - copy->from_length(), '\0');
}

static void do_expand_string(Copy_field *copy) {
  const CHARSET_INFO *cs = copy->from_field()->charset();
  memcpy(copy->to_ptr, copy->from_ptr, copy->from_length());
  cs->cset->fill(cs, (char *)copy->to_ptr + copy->from_length(),
                 copy->to_length() - copy->from_length(), ' ');
}

/**
  Find how many bytes should be copied between Field_varstring fields
  so that only the bytes in use in the 'from' field are copied.
  Handles single and multi-byte charsets. Adds warning if not all
  bytes in 'from' will fit into 'to'.

  @param to   Variable length field we're copying to
  @param from Variable length field we're copying from

  @return Number of bytes that should be copied from 'from' to 'to'.
*/
static size_t get_varstring_copy_length(Field_varstring *to,
                                        const Field_varstring *from) {
  const CHARSET_INFO *const cs = from->charset();
  const bool is_multibyte_charset = (cs->mbmaxlen != 1);
  const uint to_byte_length = to->row_pack_length();

  size_t bytes_to_copy;
  if (from->length_bytes == 1)
    bytes_to_copy = *from->ptr;
  else
    bytes_to_copy = uint2korr(from->ptr);

  if (from->pack_length() - from->length_bytes <= to_byte_length) {
    /*
      There's room for everything in the destination buffer;
      no need to truncate.
    */
    return bytes_to_copy;
  }

  if (is_multibyte_charset) {
    int well_formed_error;
    const char *from_beg =
        reinterpret_cast<char *>(from->ptr + from->length_bytes);
    const uint to_char_length = (to_byte_length) / cs->mbmaxlen;
    const size_t from_byte_length = bytes_to_copy;
    bytes_to_copy =
        cs->cset->well_formed_len(cs, from_beg, from_beg + from_byte_length,
                                  to_char_length, &well_formed_error);
    if (bytes_to_copy < from_byte_length) {
      if (from->table->in_use->check_for_truncated_fields)
        to->set_warning(Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED, 1);
    }
  } else {
    if (bytes_to_copy > (to_byte_length)) {
      bytes_to_copy = to_byte_length;
      if (from->table->in_use->check_for_truncated_fields)
        to->set_warning(Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED, 1);
    }
  }
  return bytes_to_copy;
}

/**
  A variable length string field consists of:
   (a) 1 or 2 length bytes, depending on the VARCHAR column definition
   (b) as many relevant character bytes, as defined in the length byte(s)
   (c) unused padding up to the full length of the column

  This function only copies (a) and (b)

  Condition for using this function: to and from must use the same
  number of bytes for length, i.e: to->length_bytes==from->length_bytes

  @param to   Variable length field we're copying to
  @param from Variable length field we're copying from
*/
static void copy_field_varstring(Field_varstring *const to,
                                 const Field_varstring *const from) {
  const uint length_bytes = from->length_bytes;
  DBUG_ASSERT(length_bytes == to->length_bytes);
  DBUG_ASSERT(length_bytes == 1 || length_bytes == 2);

  const size_t bytes_to_copy = get_varstring_copy_length(to, from);
  if (length_bytes == 1)
    *to->ptr = static_cast<uchar>(bytes_to_copy);
  else
    int2store(to->ptr, bytes_to_copy);

  // memcpy should not be used for overlaping memory blocks
  DBUG_ASSERT(to->ptr != from->ptr);
  memcpy(to->ptr + length_bytes, from->ptr + length_bytes, bytes_to_copy);
}

static void do_varstring(Copy_field *copy) {
  copy_field_varstring(static_cast<Field_varstring *>(copy->to_field()),
                       static_cast<Field_varstring *>(copy->from_field()));
}

/***************************************************************************
** The different functions that fills in a Copy_field class
***************************************************************************/

void Copy_field::invoke_do_copy(Copy_field *f) {
  (*(this->m_do_copy))(f);

  f->check_and_set_temporary_null();
}

void Copy_field::invoke_do_copy2(Copy_field *f) {
  (*(this->m_do_copy2))(f);

  f->check_and_set_temporary_null();
}

/**
  copy of field to maybe null string.
  If field is null then the all bytes are set to 0.
  if field is not null then the first byte is set to 1 and the rest of the
  string is the field value.
  The 'to' buffer should have a size of field->pack_length()+1
*/

void Copy_field::set(uchar *to, Field *from) {
  from_ptr = from->ptr;
  to_ptr = to;
  m_from_length = from->pack_length();
  if (from->maybe_null()) {
    if ((from_null_ptr = from->get_null_ptr()))
      from_bit = from->null_bit;
    else {
      /*
        Field is not nullable but its table is the inner table of an outer
        join so field may be NULL. Read its NULLness information from
        TABLE::null_row.
        @note that in the code of window functions, bring_back_frame_row() may
        cause a change to *from_null_ptr, thus setting TABLE::null_row to be
        what it was when the row was buffered, which is correct.
      */
      from_null_ptr = (uchar *)&from->table->null_row;
      from_bit = 1;  // as TABLE::null_row contains 0 or 1
    }
    to_ptr[0] = 1;  // Null as default value
    to_null_ptr = to_ptr++;
    to_bit = 1;
    m_do_copy = do_field_to_null_str;
  } else {
    to_null_ptr = 0;  // For easy debugging
    m_do_copy = do_field_eq;
  }
}

/*
  To do:

  If 'save' is set to true and the 'from' is a blob field, m_do_copy is set to
  do_save_blob rather than do_conv_blob.  The only differences between them
  appears to be:

  - do_save_blob allocates and uses an intermediate buffer before calling
    Field_blob::store. Is this in order to trigger the call to
    well_formed_copy_nchars, by changing the pointer copy->tmp.ptr()?
    That call will take place anyway in all known cases.
 */
void Copy_field::set(Field *to, Field *from, bool save) {
  if (to->type() == MYSQL_TYPE_NULL) {
    to_null_ptr = 0;  // For easy debugging
    to_ptr = 0;
    m_do_copy = do_skip;
    return;
  }
  m_from_field = from;
  m_to_field = to;
  from_ptr = from->ptr;
  m_from_length = from->pack_length();
  to_ptr = to->ptr;
  m_to_length = m_to_field->pack_length();

  // set up null handling
  from_null_ptr = to_null_ptr = 0;
  if (from->maybe_null()) {
    if ((from_null_ptr = from->get_null_ptr()))
      from_bit = from->null_bit;
    else {
      from_null_ptr = (uchar *)&from->table->null_row;
      from_bit = 1;
    }
    if (m_to_field->real_maybe_null()) {
      to_null_ptr = to->get_null_ptr();
      to_bit = to->null_bit;
      m_do_copy = do_copy_null;
    } else {
      if (m_to_field->type() == MYSQL_TYPE_TIMESTAMP)
        m_do_copy = do_copy_timestamp;  // Automatic timestamp
      else if (m_to_field == m_to_field->table->next_number_field)
        m_do_copy = do_copy_next_number;
      else
        m_do_copy = do_copy_not_null;
    }
  } else if (m_to_field->real_maybe_null()) {
    to_null_ptr = to->get_null_ptr();
    to_bit = to->null_bit;
    m_do_copy = do_copy_maybe_null;
  } else
    m_do_copy = NULL;

  if ((to->flags & BLOB_FLAG) && save) {
    if (to->real_type() == MYSQL_TYPE_JSON &&
        from->real_type() == MYSQL_TYPE_JSON)
      m_do_copy2 = do_save_json;
    else
      m_do_copy2 = do_save_blob;
  } else
    m_do_copy2 = get_copy_func(to, from);

  if (!m_do_copy)  // Not null
    m_do_copy = m_do_copy2;
}

Copy_field::Copy_func *Copy_field::get_copy_func(Field *to, Field *from) {
  bool compatible_db_low_byte_first =
      (to->table->s->db_low_byte_first == from->table->s->db_low_byte_first);
  if (to->type() == MYSQL_TYPE_GEOMETRY) {
    if (from->type() != MYSQL_TYPE_GEOMETRY ||
        to->maybe_null() != from->maybe_null())
      return do_conv_blob;

    const Field_geom *to_geom = down_cast<const Field_geom *>(to);
    const Field_geom *from_geom = down_cast<const Field_geom *>(from);

    // If changing the SRID property of the field, we must do a full conversion.
    if (to_geom->get_srid() != from_geom->get_srid() &&
        to_geom->get_srid().has_value())
      return do_conv_blob;

    // to is same as or a wider type than from
    if (to_geom->get_geometry_type() == from_geom->get_geometry_type() ||
        is_subtype_of(from_geom->get_geometry_type(),
                      to_geom->get_geometry_type()))
      return do_field_eq;

    return do_conv_blob;
  } else if (to->flags & BLOB_FLAG) {
    /*
      We need to do conversion if we are copying from BLOB to
      non-BLOB, or if we are copying between BLOBs with different
      character sets, or if we are copying between JSON and non-JSON.
    */
    if (!(from->flags & BLOB_FLAG) || from->charset() != to->charset() ||
        ((to->type() == MYSQL_TYPE_JSON) != (from->type() == MYSQL_TYPE_JSON)))
      return do_conv_blob;
    if (m_from_length != m_to_length || !compatible_db_low_byte_first) {
      // Correct pointer to point at char pointer
      to_ptr += m_to_length - portable_sizeof_char_ptr;
      from_ptr += m_from_length - portable_sizeof_char_ptr;
      return do_copy_blob;
    }
  } else {
    if (to->real_type() == MYSQL_TYPE_BIT ||
        from->real_type() == MYSQL_TYPE_BIT)
      return do_field_int;
    if (to->result_type() == DECIMAL_RESULT) return do_field_decimal;
    // Check if identical fields
    if (from->result_type() == STRING_RESULT) {
      if (from->is_temporal()) {
        if (to->is_temporal()) {
          return do_field_time;
        } else {
          if (to->result_type() == INT_RESULT) return do_field_int;
          if (to->result_type() == REAL_RESULT) return do_field_real;
          /* Note: conversion from any to DECIMAL_RESULT is handled earlier */
        }
      }
      /*
        Detect copy from pre 5.0 varbinary to varbinary as of 5.0 and
        use special copy function that removes trailing spaces and thus
        repairs data.
      */
      if (from->type() == MYSQL_TYPE_VAR_STRING && !from->has_charset() &&
          to->type() == MYSQL_TYPE_VARCHAR && !to->has_charset())
        return do_field_varbinary_pre50;

      /*
        If we are copying date or datetime's we have to check the dates
        if we don't allow 'all' dates.
      */
      if (to->real_type() != from->real_type() ||
          to->decimals() != from->decimals() /* e.g. TIME vs TIME(6) */ ||
          !compatible_db_low_byte_first ||
          (((to->table->in_use->variables.sql_mode &
             (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES)) &&
            to->type() == MYSQL_TYPE_DATE) ||
           to->type() == MYSQL_TYPE_DATETIME)) {
        if (from->real_type() == MYSQL_TYPE_ENUM ||
            from->real_type() == MYSQL_TYPE_SET)
          if (to->result_type() != STRING_RESULT)
            return do_field_int;  // Convert SET to number
        return do_field_string;
      }
      if (to->real_type() == MYSQL_TYPE_ENUM ||
          to->real_type() == MYSQL_TYPE_SET) {
        if (!to->eq_def(from)) {
          if (from->real_type() == MYSQL_TYPE_ENUM &&
              to->real_type() == MYSQL_TYPE_ENUM)
            return do_field_enum;
          else
            return do_field_string;
        }
      } else if (to->charset() != from->charset())
        return do_field_string;
      else if (to->real_type() == MYSQL_TYPE_VARCHAR) {
        if (((Field_varstring *)to)->length_bytes !=
            ((Field_varstring *)from)->length_bytes)
          return do_field_string;
        else
          return do_varstring;
      } else if (m_to_length < m_from_length)
        return (from->charset()->mbmaxlen == 1 ? do_cut_string
                                               : do_cut_string_complex);
      else if (m_to_length > m_from_length) {
        if (to->charset() == &my_charset_bin)
          return do_expand_binary;
        else
          return do_expand_string;
      }

    } else if (to->real_type() != from->real_type() ||
               m_to_length != m_from_length || !compatible_db_low_byte_first) {
      if (to->real_type() == MYSQL_TYPE_DECIMAL ||
          to->result_type() == STRING_RESULT)
        return do_field_string;
      if (to->result_type() == INT_RESULT) return do_field_int;
      return do_field_real;
    } else {
      if (!to->eq_def(from) || !compatible_db_low_byte_first) {
        if (to->real_type() == MYSQL_TYPE_DECIMAL) return do_field_string;
        if (to->result_type() == INT_RESULT)
          return do_field_int;
        else
          return do_field_real;
      }
    }
  }
  /* Eq fields */
  switch (m_to_length) {
    case 1:
      return do_field_1;
    case 2:
      return do_field_2;
    case 3:
      return do_field_3;
    case 4:
      return do_field_4;
    case 6:
      return do_field_6;
    case 8:
      return do_field_8;
  }
  return do_field_eq;
}

void Copy_field::swap_direction() {
  std::swap(from_ptr, to_ptr);
  std::swap(from_null_ptr, to_null_ptr);
  std::swap(from_bit, to_bit);
  std::swap(m_from_length, m_to_length);
  std::swap(m_from_field, m_to_field);
}

static inline bool is_blob_type(Field *to) {
  return (to->type() == MYSQL_TYPE_BLOB || to->type() == MYSQL_TYPE_GEOMETRY);
}

/** Simple quick field convert that is called on insert. */

type_conversion_status field_conv(Field *to, Field *from) {
  const int from_type = from->type();
  const int to_type = to->type();

  if ((to_type == MYSQL_TYPE_JSON) && (from_type == MYSQL_TYPE_JSON)) {
    Field_json *to_json = down_cast<Field_json *>(to);
    Field_json *from_json = down_cast<Field_json *>(from);
    return to_json->store(from_json);
  }

  if (to->real_type() == from->real_type() &&
      !((is_blob_type(to)) && to->table->copy_blobs) &&
      to->charset() == from->charset() && to_type != MYSQL_TYPE_GEOMETRY) {
    if (to->real_type() == MYSQL_TYPE_VARCHAR &&
        from->real_type() == MYSQL_TYPE_VARCHAR) {
      Field_varstring *to_vc = static_cast<Field_varstring *>(to);
      const Field_varstring *from_vc = static_cast<Field_varstring *>(from);
      if (to_vc->length_bytes == from_vc->length_bytes) {
        copy_field_varstring(to_vc, from_vc);
        return TYPE_OK;
      }
    }
    if (to->pack_length() == from->pack_length() &&
        !(to->flags & UNSIGNED_FLAG && !(from->flags & UNSIGNED_FLAG)) &&
        to->real_type() != MYSQL_TYPE_ENUM &&
        to->real_type() != MYSQL_TYPE_SET &&
        to->real_type() != MYSQL_TYPE_BIT &&
        (!to->is_temporal_with_time() || to->decimals() == from->decimals()) &&
        (to->real_type() != MYSQL_TYPE_NEWDECIMAL ||
         (to->field_length == from->field_length &&
          (((Field_num *)to)->dec == ((Field_num *)from)->dec))) &&
        to->table->s->db_low_byte_first == from->table->s->db_low_byte_first &&
        (!(to->table->in_use->variables.sql_mode &
           (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES)) ||
         (to->type() != MYSQL_TYPE_DATE && to->type() != MYSQL_TYPE_DATETIME &&
          (!to->table->in_use->variables.explicit_defaults_for_timestamp ||
           to->type() != MYSQL_TYPE_TIMESTAMP))) &&
        (from->real_type() != MYSQL_TYPE_VARCHAR)) {  // Identical fields
      // to->ptr==from->ptr may happen if one does 'UPDATE ... SET x=x'
      memmove(to->ptr, from->ptr, to->pack_length());
      return TYPE_OK;
    }
  }
  if (to->type() == MYSQL_TYPE_BLOB) {  // Be sure the value is stored
    Field_blob *blob = (Field_blob *)to;
    from->val_str(&blob->value);

    /*
      Copy value if copy_blobs is set, or source is part of the table's
      writeset.
    */
    if (to->table->copy_blobs ||
        (!blob->value.is_alloced() && from->is_updatable()))
      blob->value.copy();

    return blob->store(blob->value.ptr(), blob->value.length(),
                       from->charset());
  }
  if (from->real_type() == MYSQL_TYPE_ENUM &&
      to->real_type() == MYSQL_TYPE_ENUM && from->val_int() == 0) {
    ((Field_enum *)(to))->store_type(0);
    return TYPE_OK;
  } else if (from->is_temporal() && to->result_type() == INT_RESULT) {
    MYSQL_TIME ltime;
    longlong nr;
    if (from->type() == MYSQL_TYPE_TIME) {
      from->get_time(&ltime);
      if (current_thd->is_fsp_truncate_mode())
        nr = TIME_to_ulonglong_time(&ltime);
      else
        nr = TIME_to_ulonglong_time_round(&ltime);
    } else {
      from->get_date(&ltime, TIME_FUZZY_DATE);
      if (current_thd->is_fsp_truncate_mode())
        nr = TIME_to_ulonglong_datetime(&ltime);
      else
        nr = TIME_to_ulonglong_datetime_round(&ltime);
    }
    return to->store(ltime.neg ? -nr : nr, 0);
  } else if (from->is_temporal() && (to->result_type() == REAL_RESULT ||
                                     to->result_type() == DECIMAL_RESULT ||
                                     to->result_type() == INT_RESULT)) {
    my_decimal tmp;
    /*
      We prefer DECIMAL as the safest precise type:
      double supports only 15 digits, which is not enough for DATETIME(6).
    */
    return to->store_decimal(from->val_decimal(&tmp));
  } else if (from->is_temporal() && to->is_temporal()) {
    return copy_time_to_time(from, to);
  } else if (from_type == MYSQL_TYPE_JSON &&
             (to_type == MYSQL_TYPE_TINY || to_type == MYSQL_TYPE_SHORT ||
              to_type == MYSQL_TYPE_INT24 || to_type == MYSQL_TYPE_LONG ||
              to_type == MYSQL_TYPE_LONGLONG)) {
    return to->store(from->val_int(), from->flags & UNSIGNED_FLAG);
  } else if (from_type == MYSQL_TYPE_JSON && to_type == MYSQL_TYPE_NEWDECIMAL) {
    my_decimal buff;
    return to->store_decimal(from->val_decimal(&buff));
  } else if (from_type == MYSQL_TYPE_JSON &&
             (to_type == MYSQL_TYPE_FLOAT || to_type == MYSQL_TYPE_DOUBLE)) {
    return to->store(from->val_real());
  } else if (from_type == MYSQL_TYPE_JSON && to->is_temporal()) {
    MYSQL_TIME ltime;
    if (from->get_time(&ltime)) return TYPE_ERR_BAD_VALUE;
    return to->store_time(&ltime);
  } else if ((from->result_type() == STRING_RESULT &&
              (to->result_type() == STRING_RESULT ||
               (from->real_type() != MYSQL_TYPE_ENUM &&
                from->real_type() != MYSQL_TYPE_SET))) ||
             to->type() == MYSQL_TYPE_DECIMAL) {
    char buff[MAX_FIELD_WIDTH];
    String result(buff, sizeof(buff), from->charset());
    from->val_str(&result);
    /*
      We use c_ptr_quick() here to make it easier if to is a float/double
      as the conversion routines will do a copy of the result doesn't
      end with \0. Can be replaced with .ptr() when we have our own
      string->double conversion.
    */
    return to->store(result.c_ptr_quick(), result.length(), from->charset());
  } else if (from->result_type() == REAL_RESULT)
    return to->store(from->val_real());
  else if (from->result_type() == DECIMAL_RESULT) {
    my_decimal buff;
    return to->store_decimal(from->val_decimal(&buff));
  } else
    return to->store(from->val_int(), from->flags & UNSIGNED_FLAG);
}
