#ifndef SQL_EXCEPTION_HANDLER_H_INCLUDED
#define SQL_EXCEPTION_HANDLER_H_INCLUDED

/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief This file declares functions to convert exceptions to MySQL
  error messages.

  The pattern for use in other functions is:

  @code
  try
  {
    something_that_throws();
  }
  catch (...)
  {
    handle_foo_exception("function_name");
  }
  @endcode

  There are different handlers for different use cases.
*/

/**
  Handle an exception of any type.

  Code that could throw exceptions should be wrapped in try/catch, and
  the catch block should raise a corresponding MySQL error. If this
  function is called from the catch block, it will raise a specialized
  error message for many of the std::exception subclasses, or a more
  generic error message if it is not a std::exception.

  @param funcname the name of the function that caught an exception

  @see handle_gis_exception
*/
void handle_std_exception(const char *funcname);

#endif // SQL_EXCEPTION_HANDLER_H_INCLUDED
