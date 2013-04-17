/* Copyright (C) 2013 Sergei Golubchik and Monty Program Ab

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

/* old plugin api structures, used for backward compatibility */

#define upgrade_var(X) latest->X= X
#define upgrade_str(X) strmake_buf(latest->X, X)
#define downgrade_var(X) X= latest->X
#define downgrade_str(X) strmake_buf(X, latest->X)

/**************************************************************/
/* Authentication API, version 0x0100 *************************/
#define MIN_AUTHENTICATION_INTERFACE_VERSION 0x0100

struct MYSQL_SERVER_AUTH_INFO_0x0100 {
  char *user_name;
  unsigned int user_name_length;
  const char *auth_string;
  unsigned long auth_string_length;
  char authenticated_as[49]; 
  char external_user[512];
  int  password_used;
  const char *host_or_ip;
  unsigned int host_or_ip_length;

  void upgrade(MYSQL_SERVER_AUTH_INFO *latest)
  {
    upgrade_var(user_name);
    upgrade_var(user_name_length);
    upgrade_var(auth_string);
    upgrade_var(auth_string_length);
    upgrade_str(authenticated_as);
    upgrade_str(external_user);
    upgrade_var(password_used);
    upgrade_var(host_or_ip);
    upgrade_var(host_or_ip_length);
  }
  void downgrade(MYSQL_SERVER_AUTH_INFO *latest)
  {
    downgrade_var(user_name);
    downgrade_var(user_name_length);
    downgrade_var(auth_string);
    downgrade_var(auth_string_length);
    downgrade_str(authenticated_as);
    downgrade_str(external_user);
    downgrade_var(password_used);
    downgrade_var(host_or_ip);
    downgrade_var(host_or_ip_length);
  }
};

/**************************************************************/

