/* Copyright (C) 2004-2006 MySQL AB

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

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H

#include <my_global.h>
#include <my_sys.h>
#include <mysql_com.h>
#include <m_string.h>
#include <hash.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

struct User
{
  User()
  {}

  User(const LEX_STRING *user_name_arg, const char *password);

  int init(const char *line);

  inline void set_password(const char *password)
  {
    make_scrambled_password(scrambled_password, password);
  }

  char user[USERNAME_LENGTH + 1];
  char scrambled_password[SCRAMBLED_PASSWORD_CHAR_LENGTH + 1];
  uint8 user_length;
  uint8 salt[SCRAMBLE_LENGTH];
};

/*
  User_map -- all users and passwords
*/

class User_map
{
public:
  /* User_map iterator */

  class Iterator
  {
  public:
    Iterator(User_map *user_map_arg) :
      user_map(user_map_arg), cur_idx(0)
    { }

  public:
    void reset();

    User *next();

  private:
    User_map *user_map;
    uint cur_idx;
  };

public:
  User_map();
  ~User_map();

  int init();
  int load(const char *password_file_name, const char **err_msg);
  int save(const char *password_file_name, const char **err_msg);
  int authenticate(const LEX_STRING *user_name,
                   const char *scrambled_password,
                   const char *scramble) const;

  const User *find_user(const LEX_STRING *user_name) const;
  User *find_user(const LEX_STRING *user_name);

  bool add_user(User *user);
  bool remove_user(User *user);

private:
  User_map(const User_map &);
  User_map &operator =(const User_map &);

private:
  HASH hash;
  bool initialized;

  friend class Iterator;
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H
