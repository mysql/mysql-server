/*  Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "m_ctype.h"  /* my_charset_utf8_bin */
#include "my_dbug.h"   /* DBUG_ASSERT */
#include <mysqld_error.h> /* To get ER_NOT_VALID_PASSWORD */
#include <mysql/plugin_validate_password.h> /* validate_password plugin */

#include "strfunc.h"
#include "sql_string.h"
#include "sql_plugin.h"


LEX_CSTRING validate_password_plugin= {
  C_STRING_WITH_LEN("validate_password")
};

/**
  Validate the input password based on defined policies.

  @param password        password which needs to be validated against the
                         defined policies
  @param password_len    length of password

  @retval 0 ok
  @retval 1 ERROR;
*/

int my_validate_password_policy(const char *password, unsigned int password_len)
{
  plugin_ref plugin;
  String password_str;

  if (password)
  {
    String tmp_str(password, password_len, &my_charset_utf8_bin);
    password_str= tmp_str;
  }
  plugin= my_plugin_lock_by_name(0, validate_password_plugin,
                                 MYSQL_VALIDATE_PASSWORD_PLUGIN);
  if (plugin)
  {
    st_mysql_validate_password *password_validate=
                      (st_mysql_validate_password *) plugin_decl(plugin)->info;

    if (!password_validate->validate_password(&password_str))
    {
      my_error(ER_NOT_VALID_PASSWORD, MYF(0));
      plugin_unlock(0, plugin);
      return (1);
    }
    plugin_unlock(0, plugin);
  }
  return (0);
}


/* called when new user is created or exsisting password is changed */
int my_calculate_password_strength(const char *password, unsigned int password_len)
{
  int res= 0;
  DBUG_ASSERT(password != NULL);

  String password_str;
  if (password)
    password_str.set(password, password_len, &my_charset_utf8_bin);
  plugin_ref plugin= my_plugin_lock_by_name(0, validate_password_plugin,
                                            MYSQL_VALIDATE_PASSWORD_PLUGIN);
  if (plugin)
  {
    st_mysql_validate_password *password_strength=
                      (st_mysql_validate_password *) plugin_decl(plugin)->info;

    res= password_strength->get_password_strength(&password_str);
    plugin_unlock(0, plugin);
  }
  return(res);
}
