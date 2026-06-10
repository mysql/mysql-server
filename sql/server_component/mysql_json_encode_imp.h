/* Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_JSON_ENCODE_SERVICE_H
#define MYSQL_SERVER_JSON_ENCODE_SERVICE_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_json_encode.h>

class mysql_json_encode_imp {
 public:
  static DEFINE_METHOD(const unsigned char *, encode,
                       (const unsigned char *src, const unsigned char *src_end,
                        const unsigned char *src_data_end, unsigned char *dst,
                        unsigned char *dst_end, const CHARSET_INFO_h charset,
                        unsigned char **dst_out));
};

#endif /* MYSQL_SERVER_JSON_ENCODE_SERVICE_H */
