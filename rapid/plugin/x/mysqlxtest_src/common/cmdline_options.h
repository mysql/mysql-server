/*
 * Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _CMDLINE_OPTIONS_H_
#define _CMDLINE_OPTIONS_H_

#include <stdlib.h>
#include <iostream>

class Command_line_options
{
public:
  int exit_code;
  bool needs_password;
protected:
  Command_line_options(int argc, char **argv)
          : exit_code(0)
  {
  }

  bool check_arg(char **argv, int &argi, const char *arg, const char *larg)
  {
    if ((arg && strcmp(argv[argi], arg) == 0) || (larg && strcmp(argv[argi], larg) == 0))
      return true;
    return false;
  }

  bool is_quote_char(const char single_char)
  {
    if (single_char == '\'' || single_char == '"' || single_char == '`')
      return true;

    return false;
  }

  bool should_remove_qoutes(const char first, const char last)
  {
    if (!is_quote_char(first) || !is_quote_char(last))
      return false;

    return first == last;
  }

  bool check_arg_with_value(char **argv, int &argi, const char *arg, const char *larg, char *&value)
  {
    // --option value or -o value
    if ((arg && strcmp(argv[argi], arg) == 0) || (larg && strcmp(argv[argi], larg) == 0))
    {
      // value must be in next arg
      if (argv[argi+1] != NULL)
      {
        ++argi;
        value = argv[argi];
      }
      else
      {
        std::cerr << argv[0] << ": option " << argv[argi] << " requires an argument\n";
        exit_code = 1;
        return false;
      }
      return true;
    }
    // -ovalue
    else if (larg && strncmp(argv[argi], larg, strlen(larg)) == 0 && strlen(argv[argi]) > strlen(larg))
    {
      value = argv[argi] + strlen(larg);
      std::size_t length = strlen(value);

      if (length > 0 && should_remove_qoutes(value[0], value[length - 1]))
      {
        value[length - 1] = 0;
        value = value + 1;
      }

      return true;
    }
    // --option=value
    else if (arg && strncmp(argv[argi], arg, strlen(arg)) == 0 && argv[argi][strlen(arg)] == '=')
    {
      // value must be after =
      value = argv[argi] + strlen(arg)+1;
      std::size_t length = strlen(value);

      if (length > 0 && should_remove_qoutes(value[0], value[length - 1]))
      {
        value[length - 1] = 0;
        value = value + 1;
      }

      return true;
    }
    return false;
  }
};


#endif
