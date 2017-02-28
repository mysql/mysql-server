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

#define MYSQL_SERVER  "We need security context"

#include <sql_class.h>                      /* THD, Security context */
#include <sql_acl.h>                        /* SUPER_ACL */

#include "security_context_wrapper.h"

namespace connection_control
{
  /**
    Security_context_wrapper constructor.

    @param [in] thd    Handle to THD

    Get security context from thd.
  */
  Security_context_wrapper::Security_context_wrapper(MYSQL_THD thd)
  {
    m_valid= thd->security_ctx ? true : false;
    m_thd= thd;
  }


  /**
    Get value for given property from security context

    @param [in] property    Property to be checked
    @param [out] value      Value of the property

    @returns status of property check
      @retval true Error fetching property value
      @retval false value contains valid value for given property
  */

  bool
  Security_context_wrapper::get_property(const char *property, LEX_CSTRING *value)
  {
    value->length=0;
    value->str= 0;

    if (!m_valid || !property)
      return true;
    else
    {
      if (!strcmp(property, "priv_user"))
      {
        if (m_thd->security_ctx->priv_user)
        {
          value->str= m_thd->security_ctx->priv_user;
          value->length= strlen(value->str);
        }
      }
      else if (!strcmp(property, "priv_host"))
      {
        if (m_thd->security_ctx->priv_host)
        {
          value->str= m_thd->security_ctx->priv_host;
          value->length= strlen(value->str);
        }
      }
      else if (!strcmp(property, "user"))
      {
        if (m_thd->security_ctx->user)
        {
          value->str= m_thd->security_ctx->user;
          value->length= strlen(value->str);
        }
      }
      else if (!strcmp(property, "proxy_user"))
      {
        if (m_thd->security_ctx->proxy_user)
        {
          value->str= m_thd->security_ctx->proxy_user;
          value->length= strlen(value->str);
        }
      }
      else if (!strcmp(property, "host"))
      {
        if (m_thd->security_ctx->get_host()->length())
        {
          value->str= m_thd->security_ctx->get_host()->c_ptr();
          value->length= strlen(value->str);
        }
      }
      else if (!strcmp(property, "ip"))
      {
        if (m_thd->security_ctx->get_ip()->length())
        {
          value->str= m_thd->security_ctx->get_ip()->c_ptr();
          value->length= strlen(value->str);
        }
      }
      else
      {
        return true;
      }
    }
    return false;
  }


  /**  Get proxy user information from security context */

  const char *
    Security_context_wrapper::get_proxy_user()
  {
    LEX_CSTRING proxy_user;
    if (get_property("proxy_user", &proxy_user))
      return 0;
    return proxy_user.str;
  }


  /** Get priv user information from security context */

  const char *
    Security_context_wrapper::get_priv_user()
  {
    LEX_CSTRING priv_user;
    if (get_property("priv_user", &priv_user))
      return 0;
    return priv_user.str;
  }


  /** Get priv host information from security context */

  const char *
    Security_context_wrapper::get_priv_host()
  {
    LEX_CSTRING priv_host;
    if (get_property("priv_host", &priv_host))
      return 0;
    return priv_host.str;
  }


  /** Get connected user information from security context */

  const char *
    Security_context_wrapper::get_user()
  {
    LEX_CSTRING user;
    if (get_property("user", &user))
      return 0;
    return user.str;
  }


  /** Get connected host information from security context */

  const char *
    Security_context_wrapper::get_host()
  {
    /*
      We can't use thd->security_ctx->priv_host_name()
      because it returns "%" if hostname is empty.
      However, thd->security_ctx->proxy_user won't have
      "%" if hostname was empty.

      To be consistent, we will always use
      'user'@'host'/''@'host'/''@'' type of representation.
    */
    LEX_CSTRING host;
    if (get_property("host", &host))
      return 0;
    return host.str;
  }


  /** Get connected ip information from security context */

  const char *
    Security_context_wrapper::get_ip()
  {
    LEX_CSTRING ip;
    if (get_property("ip", &ip))
      return 0;
    return ip.str;
  }


  /** Check if valid security context exists for give THD or not */

  bool
    Security_context_wrapper::security_context_exists()
  {
    return m_valid;
  }


  /** Check whether user has requried privilege or not */

  bool
    Security_context_wrapper::is_super_user()
  {
    if (!m_valid)
      return false;

    bool has_super= ((m_thd->security_ctx->master_access & SUPER_ACL) == SUPER_ACL);

    return has_super;
  }
}
