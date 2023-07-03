/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <string.h>
#include <sys/types.h>

#include <mysql/components/minimal_chassis.h>
#include "m_ctype.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/mysql_string.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_string_service_imp.h"
#include "sql_string.h"

PSI_memory_key key_memory_string_service_iterator;

/**
  The string functions as a service to the mysql_server component.
  So, that by default this service is available to all the components
  register to the server.
*/

struct my_h_string_imp {};

struct my_h_string_iterator_imp {};

static const CHARSET_INFO *from_api(CHARSET_INFO_h api) {
  return reinterpret_cast<const CHARSET_INFO *>(api);
}

static CHARSET_INFO_h to_api(const CHARSET_INFO *impl) {
  return reinterpret_cast<CHARSET_INFO_h>(const_cast<CHARSET_INFO *>(impl));
}

static String *from_api(my_h_string api) {
  return reinterpret_cast<String *>(api);
}

DEFINE_METHOD(CHARSET_INFO_h, mysql_string_imp::get_charset_utf8mb4, ()) {
  return to_api(&my_charset_utf8mb4_0900_ai_ci);
}

DEFINE_METHOD(CHARSET_INFO_h, mysql_string_imp::get_charset_by_name,
              (const char *name)) {
  CHARSET_INFO *cs = ::get_charset_by_name(name, MYF(0));
  return to_api(cs);
}

DEFINE_BOOL_METHOD(mysql_string_imp::create, (my_h_string * out_string)) {
  try {
    String *res = new String[1];
    *out_string = (my_h_string)res;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::tolower,
                   (my_h_string * out_string, my_h_string in_string)) {
  try {
    String *str = reinterpret_cast<String *>(in_string);
    if (str == nullptr) return true;
    String *res = reinterpret_cast<String *>(*out_string);
    const CHARSET_INFO *cs = str->charset();
    if (cs->casedn_multiply == 1) {
      res->copy(*str);
      my_casedn_str(cs, res->c_ptr_quick());
    } else {
      size_t len = str->length() * cs->casedn_multiply;
      res->set_charset(cs);
      res->alloc(len);
      len = cs->cset->casedn(cs, str->ptr(), str->length(), res->ptr(), len);
      res->length(len);
    }
    *out_string = (my_h_string)res;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::toupper,
                   (my_h_string * out_string, my_h_string in_string)) {
  try {
    String *str = reinterpret_cast<String *>(in_string);
    if (str == nullptr) return true;
    String *res = reinterpret_cast<String *>(*out_string);
    const CHARSET_INFO *cs = str->charset();
    if (cs->caseup_multiply == 1) {
      res->copy(*str);
      my_caseup_str(cs, res->c_ptr_quick());
    } else {
      size_t len = str->length() * cs->caseup_multiply;
      res->set_charset(cs);
      res->alloc(len);
      len = cs->cset->caseup(cs, str->ptr(), str->length(), res->ptr(), len);
      res->length(len);
    }
    *out_string = (my_h_string)res;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/* mysql_string_converter service, deprecated. */

DEFINE_BOOL_METHOD(mysql_string_imp::convert_from_buffer,
                   (my_h_string * out_string, const char *in_buffer,
                    uint64 length, const char *charset_name)) {
  assert(0 != strcmp(charset_name, "utf8"));
  try {
    if (in_buffer == nullptr || length == 0 || length > strlen(in_buffer))
      return true;

    String *res = new String[1];
    CHARSET_INFO *cs =
        get_charset_by_csname(charset_name, MY_CS_PRIMARY, MYF(0));

    if (!cs || res->copy(in_buffer, length, cs)) return true;
    *out_string = (my_h_string)res;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::convert_to_buffer,
                   (my_h_string in_string, char *out_buffer, uint64 length,
                    const char *charset_name)) {
  assert(0 != strcmp(charset_name, "utf8"));
  try {
    String *str = reinterpret_cast<String *>(in_string);
    if (str == nullptr || length == 0) return true;
    if (str->length() == 0) {
      out_buffer[0] = '\0';
      return false;
    }
    uint error;
    CHARSET_INFO *cs =
        get_charset_by_csname(charset_name, MY_CS_PRIMARY, MYF(0));
    if (cs == nullptr) return true;
    size_t len = my_convert(out_buffer, length - 1, cs, str->ptr(),
                            str->length(), str->charset(), &error);
    out_buffer[len] = '\0';

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/* mysql_string_converter_v2 service. */

DEFINE_BOOL_METHOD(mysql_string_imp::convert_from_buffer_v2,
                   (my_h_string dest_string, const char *src_buffer,
                    uint64 src_length, CHARSET_INFO_h src_charset)) {
  try {
    assert(dest_string != nullptr);
    assert(src_buffer != nullptr);
    assert(src_charset != nullptr);

    const CHARSET_INFO *src_cs = from_api(src_charset);
    String *dest = from_api(dest_string);
    dest->copy(src_buffer, src_length, src_cs);

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::convert_to_buffer_v2,
                   (my_h_string src_string, char *dest_buffer,
                    uint64 dest_length, CHARSET_INFO_h dest_charset)) {
  try {
    assert(src_string != nullptr);
    assert(dest_buffer != nullptr);
    assert(dest_length != 0);
    assert(dest_charset != nullptr);

    uint error;
    const CHARSET_INFO *dest_cs = from_api(dest_charset);
    String *src = from_api(src_string);
    size_t len = my_convert(dest_buffer, dest_length - 1, dest_cs, src->ptr(),
                            src->length(), src->charset(), &error);
    dest_buffer[len] = 0;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_METHOD(void, mysql_string_imp::destroy, (my_h_string string)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr) return;
    str->mem_free();
    delete[] str;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
}

DEFINE_BOOL_METHOD(mysql_string_imp::get_char,
                   (my_h_string string, uint index, ulong *out_char)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr || index >= str->length()) return true;
    my_charset_conv_mb_wc mb_wc = (str->charset())->cset->mb_wc;
    int ret = str->charpos(index);
    if (ret < 0) return true;
    const char *ptr = (str->ptr() + ret);
    if ((*mb_wc)(str->charset(), out_char, pointer_cast<const uchar *>(ptr),
                 (const uchar *)(str->ptr() + str->length())) <= 0)
      return true;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::get_char_length,
                   (my_h_string string, uint *out_length)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr) return true;
    *out_length = str->numchars();
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::get_byte,
                   (my_h_string string, uint index, uint *out_char)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr || index >= str->length()) return true;

    const char *ptr = str->ptr();
    if (ptr == nullptr) return true;
    *out_char = ptr[index];
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::get_byte_length,
                   (my_h_string string, uint *out_length)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr) return true;
    *out_length = str->length();
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::iterator_create,
                   (my_h_string string, my_h_string_iterator *out_iterator)) {
  try {
    String *str = reinterpret_cast<String *>(string);
    if (str == nullptr) return true;
    st_string_iterator *iterator = (st_string_iterator *)my_malloc(
        key_memory_string_service_iterator, sizeof(st_string_iterator), MYF(0));
    iterator->iterator_str = str;
    iterator->iterator_ptr = str->ptr();
    iterator->ctype = 0;
    *out_iterator = (my_h_string_iterator)iterator;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::iterator_get_next,
                   (my_h_string_iterator iter, int *out_char)) {
  try {
    int char_len, tmp_len;
    st_string_iterator *iterator = (st_string_iterator *)iter;
    if (iterator == nullptr) return true;
    const String *str = iterator->iterator_str;
    const CHARSET_INFO *cs = str->charset();
    const char *end = str->ptr() + str->length();
    *out_char = 0;
    if (iterator->iterator_ptr >= end) return true;
    char_len = (cs->cset->ctype(
        cs, out_char, pointer_cast<const uchar *>(iterator->iterator_ptr),
        pointer_cast<const uchar *>(end)));
    iterator->ctype = *out_char;
    tmp_len = (char_len > 0 ? char_len : (char_len < 0 ? -char_len : 1));
    if (iterator->iterator_ptr + tmp_len > end)
      return true;
    else
      iterator->iterator_ptr += tmp_len;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_METHOD(void, mysql_string_imp::iterator_destroy,
              (my_h_string_iterator iter)) {
  try {
    if (iter == nullptr) return;
    my_free((st_string_iterator *)iter);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
}

DEFINE_BOOL_METHOD(mysql_string_imp::is_upper,
                   (my_h_string_iterator iter, bool *out)) {
  try {
    st_string_iterator *iterator = (st_string_iterator *)iter;
    if (iterator == nullptr) return true;
    *out = (iterator->ctype & _MY_U);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::is_lower,
                   (my_h_string_iterator iter, bool *out)) {
  try {
    st_string_iterator *iterator = (st_string_iterator *)iter;
    if (iterator == nullptr) return true;
    *out = (iterator->ctype & _MY_L);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::is_digit,
                   (my_h_string_iterator iter, bool *out)) {
  try {
    st_string_iterator *iterator = (st_string_iterator *)iter;
    if (iterator == nullptr) return true;
    *out = (iterator->ctype & _MY_NMR);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::reset, (my_h_string s)) {
  try {
    String *str = from_api(s);
    assert(str != nullptr);
    str->length(0);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::append, (my_h_string s1, my_h_string s2)) {
  try {
    String *str1 = from_api(s1);
    String *str2 = from_api(s2);
    assert(str1 != nullptr);
    assert(str2 != nullptr);
    str1->append(*str2);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::substr,
                   (my_h_string in_string, uint offset, uint count,
                    my_h_string *out_string)) {
  String *out_str_obj = nullptr;
  try {
    String *in_str_obj = from_api(in_string);
    assert(in_str_obj != nullptr);
    out_str_obj = new String[1];
    *out_str_obj = in_str_obj->substr(offset, count);
    *out_string = (my_h_string)out_str_obj;
    return false;
  } catch (...) {
    if (out_str_obj != nullptr) delete[] out_str_obj;
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::compare,
                   (my_h_string s1, my_h_string s2, int *cmp)) {
  try {
    String *str1 = from_api(s1);
    String *str2 = from_api(s2);
    assert(str1 != nullptr);
    assert(str1 != nullptr);
    assert(cmp != nullptr);
    const CHARSET_INFO *cs = str1->charset();
    *cmp = sortcmp(str1, str2, cs);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_string_imp::get_data,
                   (my_h_string s, const char **buffer_pointer,
                    size_t *buffer_length, CHARSET_INFO_h *buffer_charset)) {
  try {
    const String *str = from_api(s);
    assert(str != nullptr);
    assert(buffer_pointer != nullptr);
    assert(buffer_length != nullptr);
    assert(buffer_charset != nullptr);

    *buffer_pointer = str->ptr();
    *buffer_length = str->length();
    const CHARSET_INFO *cs = str->charset();
    *buffer_charset = to_api(cs);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
