/* Copyright (C) 2005 MySQL AB

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

#include <my_user.h>
#include <m_string.h>
#include <mysql_com.h>

/*
  Parse user value to user name and host name parts.

  SYNOPSIS
    user_id_str     [IN]  User value string (the source).
    user_id_len     [IN]  Length of the user value.
    user_name_str   [OUT] Buffer to store user name part.
                          Must be not less than USERNAME_LENGTH + 1.
    user_name_len   [OUT] A place to store length of the user name part.
    host_name_str   [OUT] Buffer to store host name part.
                          Must be not less than HOSTNAME_LENGTH + 1.
    host_name_len   [OUT] A place to store length of the host name part.
*/

void parse_user(const char *user_id_str, size_t user_id_len,
                char *user_name_str, size_t *user_name_len,
                char *host_name_str, size_t *host_name_len)
{
  char *p= strrchr(user_id_str, '@');

  if (!p)
  {
    *user_name_len= 0;
    *host_name_len= 0;
  }
  else
  {
    *user_name_len= p - user_id_str;
    *host_name_len= user_id_len - *user_name_len - 1;

    if (*user_name_len > USERNAME_LENGTH)
      *user_name_len= USERNAME_LENGTH;

    if (*host_name_len > HOSTNAME_LENGTH)
      *host_name_len= HOSTNAME_LENGTH;

    memcpy(user_name_str, user_id_str, *user_name_len);
    memcpy(host_name_str, p + 1, *host_name_len);
  }

  user_name_str[*user_name_len]= 0;
  host_name_str[*host_name_len]= 0;
}
