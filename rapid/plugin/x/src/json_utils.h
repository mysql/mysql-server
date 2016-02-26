/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _JSON_UTILS_H_
#define _JSON_UTILS_H_

#include <string>
#include "ngs/protocol_encoder.h"

namespace xpl
{
  //bool validate_json_string(const char *s, size_t length);
  //bool validate_json_string(const std::string &s);

  //ngs::Error_code validate_json_document_path(const std::string &s);

  std::string quote_json(const std::string &s);
  std::string quote_json_if_needed(const std::string &s);
}

#endif
