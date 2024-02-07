/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstdint>
#include <parse_mask.hpp>

ParseThreadConfiguration::ParseThreadConfiguration(
    const char *str, const struct ParseEntries *parse_entries,
    const unsigned int num_parse_entries,
    const struct ParseParams *parse_params, const unsigned int num_parse_params,
    BaseString &err_msg)
    : m_parse_entries(parse_entries),
      m_num_parse_entries(num_parse_entries),
      m_parse_params(parse_params),
      m_num_parse_params(num_parse_params),
      m_err_msg(err_msg),
      m_first(true) {
  unsigned int str_len = strlen(str);
  m_curr_str = (char *)malloc(str_len + 1);
  memcpy(m_curr_str, str, str_len + 1);
  m_save_str = m_curr_str;
}

ParseThreadConfiguration::~ParseThreadConfiguration() {
  if (m_save_str != nullptr) {
    free(m_save_str);
  }
}

int ParseThreadConfiguration::read_params(ParamValue values[],
                                          unsigned int num_values,
                                          unsigned int *type, int *ret_code,
                                          bool allow_empty) {
  unsigned int loc_type;
  char *start, *end;
  int loc_ret_code;

  assert(m_num_parse_params == num_values);
  if (m_num_parse_params != num_values) {
    *ret_code = -1;
    goto end_return;
  }
  if (m_curr_str == nullptr) {
    if (allow_empty) {
      *ret_code = 0;
      goto end_return;
    } else {
      assert(false);
      *ret_code = -1;
      goto end_return;
    }
  }
  if (m_first) {
    skipblank();
    if (*m_curr_str == 0) {
      if (allow_empty) {
        *ret_code = 0;
        goto end_return;
      } else {
        *ret_code = -1;
        m_err_msg.assfmt("empty thread specification");
        goto end_return;
      }
      return 0;
    }
    m_first = false;
  } else {
    loc_ret_code = find_next();
    if (loc_ret_code != 1) {
      *ret_code = loc_ret_code;
      goto end_return;
    }
  }
  loc_type = find_type();
  if (loc_type == PARSE_END_ENTRIES) {
    *ret_code = -1;
    goto end_return;
  }
  loc_ret_code = find_params(&start, &end);
  if (loc_ret_code == -1) {
    *ret_code = loc_ret_code;
    goto end_return;
  }
  if (loc_ret_code == 1 && !allow_empty) {
    m_err_msg.assfmt("Thread specification is required");
    *ret_code = -1;
    goto end_return;
  }
  if (loc_ret_code == 0) {
    /* We found a non-empty specification */
    *end = 0;
    loc_ret_code = parse_params(start, values);
    if (loc_ret_code != 0) {
      *ret_code = loc_ret_code;
      goto end_return;
    }
    m_curr_str++; /* Pass } character by */
  }
  *type = loc_type;
  *ret_code = 0;
  return 0;
end_return:
  free(m_save_str);
  m_save_str = nullptr;
  m_curr_str = nullptr;
  return 1;
}

int ParseThreadConfiguration::find_next() {
  skipblank();

  if (*m_curr_str == 0) {
    return 0;
  } else if (*m_curr_str == ',') {
    m_curr_str++;
    return 1;
  }
  int len = (int)strlen(m_curr_str);
  m_err_msg.assfmt("Invalid format near: '%.*s'", (len > 10) ? 10 : len,
                   m_curr_str);
  return -1;
}

unsigned int ParseThreadConfiguration::find_type() {
  skipblank();

  char *name = m_curr_str;
  if (name[0] == 0)  // The entry after comma is empty
  {
    m_err_msg.assfmt("Missing thread name");
    return PARSE_END_ENTRIES;
  }

  char *end = name;
  while (isalpha(end[0]) || end[0] == '_') {
    end++;
  }

  char save = *end;
  end[0] = 0;
  unsigned int type = get_entry_type(name);
  if (type == PARSE_END_ENTRIES) {
    m_err_msg.assfmt("unknown thread type '%s'", name);
    return PARSE_END_ENTRIES;
  }
  end[0] = save;
  m_curr_str = end;
  return type;
}

int ParseThreadConfiguration::find_params(char **start, char **end) {
  skipblank();
  do {
    if (*m_curr_str != '=') {
      skipblank();
      if (*m_curr_str == ',' || *m_curr_str == 0)
        return 1; /* Indicate empty specification */
      break;
    }

    m_curr_str++;  // skip over =
    skipblank();

    if (*m_curr_str != '{') break;

    m_curr_str++;
    *start = m_curr_str;

    /**
     * Find end
     */
    while (*m_curr_str && (*m_curr_str) != '}') {
      m_curr_str++;
    }

    if (*m_curr_str != '}') break;
    *end = m_curr_str;
    return 0;
  } while (0);

  int len = (int)strlen(m_curr_str);
  m_err_msg.assfmt("Invalid format near: '%.*s'", (len > 10) ? 10 : len,
                   m_curr_str);
  return -1;
}

int ParseThreadConfiguration::parse_params(char *str, ParamValue values[]) {
  char *save = m_curr_str;
  m_curr_str = str;
  while (*m_curr_str) {
    skipblank();
    unsigned int str_len = get_param_len();

    unsigned idx = 0;
    for (; idx < m_num_parse_params; idx++) {
      if ((strlen(m_parse_params[idx].name) == str_len) &&
          (native_strncasecmp(m_curr_str, m_parse_params[idx].name, str_len) ==
           0)) {
        break;
      }
    }
    if (idx == m_num_parse_params) {
      m_err_msg.assfmt("Unknown param near: '%s'", m_curr_str);
      return -1;
    }
    if (values[idx].found == true) {
      m_err_msg.assfmt("Param '%s' found twice", m_parse_params[idx].name);
      return -1;
    }

    m_curr_str += str_len;
    skipblank();

    if (*m_curr_str != '=') {
      m_err_msg.assfmt("Missing '=' after %s in '%s'", m_parse_params[idx].name,
                       m_curr_str);
      return -1;
    }
    m_curr_str++;
    skipblank();

    int res = 0;
    switch (m_parse_params[idx].type) {
      case ParseParams::S_UNSIGNED:
        res = parse_unsigned(&values[idx].unsigned_val);
        break;
      case ParseParams::S_BITMASK:
        res = parse_bitmask(values[idx].mask_val);
        if (res == 0) {
          m_err_msg.assfmt("Empty bitmask isn't allowed here, param: %s",
                           m_parse_params[idx].name);
          return -1;
        }
        break;
      case ParseParams::S_STRING:
        values[idx].string_val = values[idx].buf;
        res = parse_string(values[idx].string_val);
        break;
      default:
        m_err_msg.assfmt("Internal error, unknown type for param: '%s'",
                         m_parse_params[idx].name);
        return -1;
    }
    if (res == -1) {
      m_err_msg.assfmt("Unable to parse %s=%s", m_parse_params[idx].name,
                       m_curr_str);
      return -1;
    }
    if (res == -2) {
      m_err_msg.assfmt("Bitmask too big %s, %s", m_parse_params[idx].name,
                       m_curr_str);
      return -1;
    }
    if (res == -3) {
      m_err_msg.assfmt("Bitmask contained empty parts %s, %s",
                       m_parse_params[idx].name, m_curr_str);
      return -1;
    }
    values[idx].found = true;
    skipblank();

    if (*m_curr_str == 0) break;

    if (*m_curr_str != ',') {
      m_err_msg.assfmt("Unable to parse near '%s'", m_curr_str);
      return -1;
    }
    m_curr_str++;
    skipblank();

    if (*m_curr_str == 0) {
      m_err_msg.assfmt("Missing parameter after comma");
      return -1;
    }
  }
  m_curr_str = save;
  return 0;
}

void ParseThreadConfiguration::skipblank() {
  char *str = m_curr_str;
  while (isspace(*str)) {
    str++;
  }
  m_curr_str = str;
}

unsigned int ParseThreadConfiguration::get_entry_type(const char *type) {
  unsigned int type_len = strlen(type);
  for (unsigned int i = 0; i < m_num_parse_entries; i++) {
    unsigned int name_len = strlen(m_parse_entries[i].m_name);
    if ((type_len == name_len) &&
        (native_strcasecmp(type, m_parse_entries[i].m_name) == 0)) {
      return m_parse_entries[i].m_type;
    }
  }
  return PARSE_END_ENTRIES;
}

unsigned int ParseThreadConfiguration::get_param_len() {
  unsigned int len = 0;
  char *str = m_curr_str;
  while (isalpha(*str) || ((*str) == '_')) {
    len++;
    str++;
  }
  return len;
}

int ParseThreadConfiguration::parse_string(char *dest_str) {
  Uint32 i = 0;
  skipblank();
  while (m_curr_str[0] != ',' && m_curr_str[0] != ' ' && m_curr_str[0] != 0 &&
         i < MAX_STRING_SIZE) {
    dest_str[i] = m_curr_str[0];
    i++;
    m_curr_str++;
  }
  if (i >= MAX_STRING_SIZE) {
    return -1;
  }
  dest_str[i] = 0;
  return 0;
}

int ParseThreadConfiguration::parse_unsigned(unsigned *dst) {
  skipblank();
  char *endptr = nullptr;
  errno = 0;
  long long val = my_strtoll(m_curr_str, &endptr, 0);
  if (errno == ERANGE) {
    return -1;
  }
  if (val < 0 || val > Int64(UINT32_MAX)) {
    return -1;
  }
  if (endptr == m_curr_str) {
    return -1;
  }
  m_curr_str = endptr;
  *dst = (unsigned)val;
  return 0;
}

int ParseThreadConfiguration::parse_bitmask(SparseBitmask &mask) {
  skipblank();
  size_t len = strspn(m_curr_str, "0123456789-, ");
  if (len == 0) {
    return -1;
  }

  while (isspace(m_curr_str[len - 1])) {
    len--;
  }
  if (m_curr_str[len - 1] == ',') {
    len--;
  }
  char save = m_curr_str[len];
  m_curr_str[len] = 0;
  int res = ::parse_mask(m_curr_str, mask);
  m_curr_str[len] = save;
  m_curr_str += len;
  return res;
}
