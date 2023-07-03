/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "ngs/protocol_authentication.h"
#include "ngs/protocol_encoder.h"

bool ngs::Authentication_handler::extract_null_terminated_element(const std::string &message, std::size_t &element_position, std::size_t element_size,
                                                                  char *output)
{
  output[0] = 0;

  if (std::string::npos == element_position)
    return false;

  std::size_t last_character_of_element = message.find('\0', element_position);

  std::string element = message.substr(element_position, last_character_of_element);

  if (element.size() >= element_size)
    return false;

  strncpy(output, element.c_str(), element_size);

  element_position = last_character_of_element;
  if (element_position != std::string::npos)
    ++element_position;

  return true;
}


#include "sha1.h" // for SHA1_HASH_SIZE

std::string ngs::Authentication_handler::compute_password_hash(const std::string &password)
{
  std::string hash;
  hash.resize(2*SHA1_HASH_SIZE + 2);
  make_scrambled_password(&hash[0], password.c_str());
  hash.resize(2*SHA1_HASH_SIZE + 1); // strip the \0
  return hash;
}
