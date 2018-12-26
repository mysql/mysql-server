/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef HAVE_SECURITY_CONTEXT_WRAPPER_H
#define HAVE_SECURITY_CONTEXT_WRAPPER_H

#include <mysql/plugin.h>

namespace connection_control
{
  class Security_context_wrapper
  {
  public:
    Security_context_wrapper(MYSQL_THD thd);
    ~Security_context_wrapper()
    {}
    const char * get_proxy_user();
    const char * get_priv_user();
    const char * get_priv_host();
    const char * get_user();
    const char * get_host();
    const char * get_ip();
    bool security_context_exists();
    bool is_super_user();

  private:
    bool get_property(const char *property, LEX_CSTRING *value);
    MYSQL_SECURITY_CONTEXT m_sctx;
    bool m_valid;
  };
}
#endif // !HAVE_SECURITY_CONTEXT_WRAPPER_H
