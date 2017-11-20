#ifndef OPTIONS_PARSER_H_INCLUDED
#define OPTIONS_PARSER_H_INCLUDED

/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/

#include <map>
#include <string>

class String;

namespace options_parser {


/**
  Parses options string into a map of keys and values, raises
  an error if parsing is unsuccessful.

  @param str unparsed options string
  @param map map to fill with keys and values
  @param func_name function name used in error reporting

  @retval false parsing was successful
  @retval true parsing was successful
*/
bool parse_string(String *str, std::map<std::string, std::string> *map,
                  const char *func_name);

} // options_parser

#endif // OPTIONS_PARSER_H_INCLUDED
