#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_OUTPUT_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_OUTPUT_H
/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>

enum enum_option_type
{
  GET_VALUE = 1,
  GET_LINE
};

bool parse_output_and_get_value(const char *command,
                                const char *option_name_str,
                                uint option_name_length,
                                char *option_value_buf,
                                size_t option_value_buf_size,
                                enum_option_type option_type);

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_OUTPUT_H */
