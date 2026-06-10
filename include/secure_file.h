/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

/**
  @file include/secure_file.h
*/

#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "nulls.h"

/**
  Test a file path to determine if the path is compatible with the secure file
  path restriction.

  @param path null terminated character string
  @param opt_secure_file_priv secure_file_priv content
  @param system_charset_info system charset
  @param files_charset_info files charset
  @param lower_case_file_system true if lower case file system, false otherwise

  @retval true The path is secure
  @retval false The path isn't secure
*/

bool is_secure_file_path(const char *path, const char *opt_secure_file_priv,
                         CHARSET_INFO *system_charset_info,
                         CHARSET_INFO *files_charset_info,
                         bool lower_case_file_system) {
  char buff1[FN_REFLEN], buff2[FN_REFLEN];
  size_t opt_secure_file_priv_len;
  /*
    All paths are secure if opt_secure_file_priv is 0
  */
  if (!opt_secure_file_priv[0]) return true;

  opt_secure_file_priv_len = strlen(opt_secure_file_priv);

  if (strlen(path) >= FN_REFLEN) return false;

  if (!my_strcasecmp(system_charset_info, opt_secure_file_priv, "NULL"))
    return false;

  if (my_realpath(buff1, path, 0)) {
    /*
      The supplied file path might have been a file and not a directory.
    */
    const int length = (int)dirname_length(path);
    if (length >= FN_REFLEN) return false;
    memcpy(buff2, path, length);
    buff2[length] = '\0';
    if (length == 0 || my_realpath(buff1, buff2, 0)) return false;
  }
  convert_dirname(buff2, buff1, NullS);
  if (!lower_case_file_system) {
    if (strncmp(opt_secure_file_priv, buff2, opt_secure_file_priv_len))
      return false;
  } else {
    assert(opt_secure_file_priv_len < FN_REFLEN);
    buff2[opt_secure_file_priv_len] = '\0';
    if (files_charset_info->coll->strcasecmp(files_charset_info, buff2,
                                             opt_secure_file_priv))
      return false;
  }
  return true;
}
