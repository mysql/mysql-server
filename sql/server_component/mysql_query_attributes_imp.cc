/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql_query_attributes_imp.h"

#include <decimal.h>
#include <my_byteorder.h>
#include <my_time.h>
#include <mysql/com_data.h>
#include <mysql/strings/dtoa.h>
#include <mysql/strings/m_ctype.h>
#include <mysql_time.h>
#include <sql/current_thd.h>
#include <sql/sql_class.h>
#include <sql_string.h>
#include "sql-common/my_decimal.h"

namespace mysql_query_attributes {

/**
  Iterator over the @ref THD::bind_parameter_values collection

  Will only return the named ones and will skip past the unnamed ones.
*/
class iterator {
 private:
  THD *thd{nullptr};
  PS_PARAM *current{nullptr};
  unsigned long ofs{0};

 public:
  /**
    Create a cursor.

    The name is expected to be in UTF8mb4's primary collation.
    Sets the iterator to the first matching element (if any) or at eof.

    @param hthd the thread handle
    @param name the query attribute name to look for and set the iterator to
    @retval false : found
    @retval true : not found or error initializing
  */
  bool init(MYSQL_THD hthd, const char *name) {
    thd = hthd ? dynamic_cast<THD *>(hthd) : current_thd;
    if (!thd) return true;
    if (!thd->bind_parameter_values_count || !thd->bind_parameter_values)
      return true;
    /* set to first element*/
    ofs = 0;
    current = thd->bind_parameter_values_count > 0 ? thd->bind_parameter_values
                                                   : nullptr;

    if (name && *name) {
      uint error_ignore;
      String name_str;
      // convert the name supplied to thd->charset()
      if (name_str.copy(name, strlen(name),
                        get_charset_by_csname("utf8mb4", MY_CS_PRIMARY, MYF(0)),
                        thd->charset(), &error_ignore))
        return true;
      // try to find the parameter name
      for (/* already initialized */; ofs < thd->bind_parameter_values_count;
           ofs++, current++)
        if (current->name_length &&
            !my_strnncoll(
                thd->charset(), reinterpret_cast<uchar *>(name_str.c_ptr()),
                name_str.length(), current->name, current->name_length))
          break;
    } else {
      /** skip the unnamed parameters */
      while (ofs < thd->bind_parameter_values_count &&
             current->name_length == 0) {
        ofs++;
        current++;
      }
    }

    // return false if found
    return ofs >= thd->bind_parameter_values_count;
  }

  bool next() {
    while (ofs < thd->bind_parameter_values_count) {
      ofs++;
      current++;
      if (ofs < thd->bind_parameter_values_count) {
        if (current->name_length > 0 && current->name) break;
      }
    }
    return ofs >= thd->bind_parameter_values_count;
  }

  const PS_PARAM *get_current() const { return current; }
  THD *get_thd() const { return thd; }
};

}  // namespace mysql_query_attributes

DEFINE_BOOL_METHOD(mysql_query_attributes_imp::create,
                   (MYSQL_THD hthd, const char *name,
                    mysqlh_query_attributes_iterator *out_iterator)) {
  mysql_query_attributes::iterator *iter =
      new mysql_query_attributes::iterator();

  if (iter->init(hthd, name)) {
    delete iter;
    return true;
  }
  *out_iterator = reinterpret_cast<mysqlh_query_attributes_iterator>(iter);
  return false;
}

DEFINE_BOOL_METHOD(mysql_query_attributes_imp::get_type,
                   (mysqlh_query_attributes_iterator iter,
                    enum enum_field_types *out_type)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  assert(iter_ptr);
  assert(iter_ptr->get_current());
  *out_type = iter_ptr->get_current()->type;
  return true;
}

DEFINE_BOOL_METHOD(mysql_query_attributes_imp::next,
                   (mysqlh_query_attributes_iterator iter)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  assert(iter_ptr);
  return iter_ptr->next();
}

DEFINE_BOOL_METHOD(mysql_query_attributes_imp::get_name,
                   (mysqlh_query_attributes_iterator iter,
                    my_h_string *out_name_handle)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  assert(iter_ptr);
  assert(iter_ptr->get_current());
  assert(iter_ptr->get_current()->name);

  if (!iter_ptr->get_current()->name) return true;

  String *elt = new String[1];
  elt->set(reinterpret_cast<const char *>(iter_ptr->get_current()->name),
           iter_ptr->get_current()->name_length,
           iter_ptr->get_thd()->charset());
  *out_name_handle = reinterpret_cast<my_h_string>(elt);
  return false;
}

DEFINE_METHOD(void, mysql_query_attributes_imp::release,
              (mysqlh_query_attributes_iterator iter)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  delete iter_ptr;
  return;
}

// is null methods
DEFINE_BOOL_METHOD(mysql_query_attributes_imp::isnull_get,
                   (mysqlh_query_attributes_iterator iter, bool *out_null)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  assert(iter_ptr);
  assert(iter_ptr->get_current());
  *out_null = iter_ptr->get_current()->null_bit != 0;
  return false;
}

/** keep in sync with setup_one_conversion_function() */
static String *query_parameter_val_str(const PS_PARAM *param,
                                       const CHARSET_INFO *cs) {
  String *str = nullptr;
  switch (param->type) {
    // the expected data types listed in the manual
    case MYSQL_TYPE_TINY:
      if (param->length == 1) {
        const int8 value = (int8)*param->value;
        str = new String[1];
        str->set_int(value, param->unsigned_type != 0, cs);
      }
      break;
    case MYSQL_TYPE_SHORT:
      if (param->length == 2) {
        const int16 value = sint2korr(param->value);
        str = new String[1];
        str->set_int(value, param->unsigned_type != 0, cs);
      }
      break;
    case MYSQL_TYPE_LONG:
      if (param->length == 4) {
        const int32 value = sint4korr(param->value);
        str = new String[1];
        str->set_int(value, param->unsigned_type != 0, cs);
      }
      break;
    case MYSQL_TYPE_LONGLONG:
      if (param->length == 8) {
        const longlong value = sint8korr(param->value);
        str = new String[1];
        str->set_int(value, param->unsigned_type != 0, cs);
      }
      break;
    case MYSQL_TYPE_FLOAT:
      if (param->length == 4) {
        const float value = float4get(param->value);
        str = new String[1];
        str->set_real(value, DECIMAL_NOT_SPECIFIED, cs);
      }
      break;
    case MYSQL_TYPE_DOUBLE:
      if (param->length == 8) {
        const double value = float8get(param->value);
        str = new String[1];
        str->set_real(value, DECIMAL_NOT_SPECIFIED, cs);
      }
      break;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
      if (param->length > 0) {
        str = new String[1];
        const char *end =
            reinterpret_cast<const char *>(param->value) + param->length;
        my_decimal decimal_value;
        str2my_decimal(E_DEC_FATAL_ERROR,
                       reinterpret_cast<const char *>(param->value),
                       &decimal_value, &end);
        my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, str);
      }
      break;
    case MYSQL_TYPE_TIME: {
      MYSQL_TIME tm;
      if (param->length >= 8) {
        const uchar *to = param->value;
        uint day;

        tm.neg = (bool)to[0];
        day = (uint)sint4korr(to + 1);
        tm.hour = (uint)to[5] + day * 24;
        tm.minute = (uint)to[6];
        tm.second = (uint)to[7];
        tm.second_part = (param->length > 8) ? (ulong)sint4korr(to + 8) : 0;
        if (tm.hour > 838) {
          /* TODO: add warning 'Data truncated' here */
          tm.hour = 838;
          tm.minute = 59;
          tm.second = 59;
        }
        tm.day = tm.year = tm.month = 0;
        tm.time_type = MYSQL_TIMESTAMP_TIME;
      } else
        set_zero_time(&tm, MYSQL_TIMESTAMP_TIME);

      str = new String[1];
      if (!str->reserve(MAX_DATE_STRING_REP_LENGTH)) {
        str->length(
            my_TIME_to_str(tm, str->ptr(), uint8{DATETIME_MAX_DECIMALS}));
      } else {
        delete[] str;
        str = nullptr;
      }
      break;
    }
    case MYSQL_TYPE_DATE: {
      MYSQL_TIME tm;
      if (param->length >= 4) {
        const uchar *to = param->value;

        tm.year = (uint)sint2korr(to);
        tm.month = (uint)to[2];
        tm.day = (uint)to[3];

        tm.hour = tm.minute = tm.second = 0;
        tm.second_part = 0;
        tm.neg = false;
        tm.time_type = MYSQL_TIMESTAMP_DATE;
      } else
        set_zero_time(&tm, MYSQL_TIMESTAMP_DATE);

      str = new String[1];
      if (!str->reserve(MAX_DATE_STRING_REP_LENGTH)) {
        str->length(
            my_TIME_to_str(tm, str->ptr(), uint8{DATETIME_MAX_DECIMALS}));
      } else {
        delete[] str;
        str = nullptr;
      }
      break;
    }
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP: {
      MYSQL_TIME tm;
      assert(param->length == 0 || param->length == 4 || param->length == 7 ||
             param->length == 11 || param->length == 13);
      if (param->length >= 4) {
        const uchar *to = param->value;

        tm.neg = false;
        tm.year = (uint)sint2korr(to);
        tm.month = (uint)to[2];
        tm.day = (uint)to[3];

        if (param->length >= 7) {
          tm.hour = (uint)to[4];
          tm.minute = (uint)to[5];
          tm.second = (uint)to[6];
        } else  // len == 4
          tm.hour = tm.minute = tm.second = 0;
        tm.time_type = MYSQL_TIMESTAMP_DATETIME;
        tm.second_part = (param->length >= 11)
                             ? static_cast<std::uint64_t>(sint4korr(to + 7))
                             : 0;

        if (param->length >= 13) {
          tm.time_zone_displacement = sint2korr(to + 11) * SECS_PER_MIN;
          tm.time_type = MYSQL_TIMESTAMP_DATETIME_TZ;
        }
      } else
        set_zero_time(&tm, MYSQL_TIMESTAMP_DATETIME);

      str = new String[1];
      if (!str->reserve(MAX_DATE_STRING_REP_LENGTH)) {
        str->length(
            my_TIME_to_str(tm, str->ptr(), uint8{DATETIME_MAX_DECIMALS}));
      } else {
        delete[] str;
        str = nullptr;
      }
      break;
    }
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VECTOR: {
      str = new String[1];
      uint dummy_errors;
      if (str->copy(reinterpret_cast<const char *>(param->value), param->length,
                    &my_charset_bin, &my_charset_bin, &dummy_errors)) {
        delete[] str;
        str = nullptr;
      }
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      str = new String[1];
      uint dummy_errors;
      if (str->copy(reinterpret_cast<const char *>(param->value), param->length,
                    cs, cs, &dummy_errors)) {
        delete[] str;
        str = nullptr;
      }
      break;

    // the rest is an error
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_INVALID:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_DATETIME2:   /**< Internal to MySQL. Not used in protocol */
    case MYSQL_TYPE_TIME2:       /**< Internal to MySQL. Not used in protocol */
    case MYSQL_TYPE_TYPED_ARRAY: /**< Used for replication only */
    case MYSQL_TYPE_BOOL:        /**< Currently just a placeholder */
    case MYSQL_TYPE_NEWDATE:     /**< Internal to MySQL. Not used in protocol */
    default:
      str = nullptr;
  }
  return str;
}

// string methods
DEFINE_BOOL_METHOD(mysql_query_attributes_imp::string_get,
                   (mysqlh_query_attributes_iterator iter,
                    my_h_string *out_string_value)) {
  mysql_query_attributes::iterator *iter_ptr =
      reinterpret_cast<mysql_query_attributes::iterator *>(iter);
  assert(iter_ptr);
  const PS_PARAM *param = iter_ptr->get_current();
  assert(param);
  String *str = query_parameter_val_str(param, iter_ptr->get_thd()->charset());
  *out_string_value = reinterpret_cast<my_h_string>(str);
  return false;
}
