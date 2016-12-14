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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef _MYSQLX_ERROR_H_
#define _MYSQLX_ERROR_H_

#include <stdexcept>
#include <string>


namespace mysqlx
{
  class Error
  {
  public:
    Error(int err = 0, const std::string &message = "")
    :_message(message), _error(err)
    { }

    int error() const { return _error; }

    operator bool () const
    {
      return 0 != _error;
    }

    const char *what() const
    {
      return _message.c_str();
    }

  private:
    std::string _message;
    int _error;
  };
}

#endif
