/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysqld_error.h>
#include <new>
#include <stdexcept>  // std::exception subclasses

/**
  Checks if last thrown exception is any kind of standard exceptions, i.e. the
  exceptions inheriting from std::exception. If so, reports an error message
  that states exception type and message. On any other thrown value it just
  reports general error.

  @param funcname Name of the function where the error occurs.
*/
void mysql_components_handle_std_exception(const char *funcname) {
  try {
    throw;
  } catch (const std::bad_alloc &e) {
    mysql_error_service_printf(ER_STD_BAD_ALLOC_ERROR, MYF(0), e.what(),
                               funcname);
  } catch (const std::domain_error &e) {
    mysql_error_service_printf(ER_STD_DOMAIN_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::length_error &e) {
    mysql_error_service_printf(ER_STD_LENGTH_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::invalid_argument &e) {
    mysql_error_service_printf(ER_STD_INVALID_ARGUMENT, MYF(0), e.what(),
                               funcname);
  } catch (const std::out_of_range &e) {
    mysql_error_service_printf(ER_STD_OUT_OF_RANGE_ERROR, MYF(0), e.what(),
                               funcname);
  } catch (const std::overflow_error &e) {
    mysql_error_service_printf(ER_STD_OVERFLOW_ERROR, MYF(0), e.what(),
                               funcname);
  } catch (const std::range_error &e) {
    mysql_error_service_printf(ER_STD_RANGE_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::underflow_error &e) {
    mysql_error_service_printf(ER_STD_UNDERFLOW_ERROR, MYF(0), e.what(),
                               funcname);
  } catch (const std::logic_error &e) {
    mysql_error_service_printf(ER_STD_LOGIC_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::runtime_error &e) {
    mysql_error_service_printf(ER_STD_RUNTIME_ERROR, MYF(0), e.what(),
                               funcname);
  } catch (const std::exception &e) {
    mysql_error_service_printf(ER_STD_UNKNOWN_EXCEPTION, MYF(0), e.what(),
                               funcname);
  } catch (...) {
    mysql_error_service_printf(ER_UNKNOWN_ERROR, MYF(0));
  }
}
