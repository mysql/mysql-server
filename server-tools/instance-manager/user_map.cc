/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef __GNUC__
#pragma interface 
#endif

#include "user_map.h"

#include <mysql_com.h>
#include <m_string.h>

#include "log.h"

struct User
{
  char user[USERNAME_LENGTH + 1];
  uint8 user_length;
  uint8 salt[SCRAMBLE_LENGTH];
  int init(const char *line);
};


int User::init(const char *line)
{
  const char *name_begin, *name_end, *password;

  if (line[0] == '\'' || line[0] == '"')
  {
    name_begin= line + 1;
    name_end= strchr(name_begin, line[0]);
    if (name_end == 0 || name_end[1] != ':')
      goto err;
    password= name_end + 2;
  }
  else
  {
    name_begin= line;
    name_end= strchr(name_begin, ':');
    if (name_end == 0)
      goto err;
    password= name_end + 1;
  }
  user_length= name_end - name_begin;
  if (user_length > USERNAME_LENGTH)
    goto err;

  /* assume that newline characater is present */
  if (strlen(password) != SCRAMBLED_PASSWORD_CHAR_LENGTH + 1)
    goto err;

  memcpy(user, name_begin, user_length);
  user[user_length]= 0;
  get_salt_from_password(salt, password);
  log_info("loaded user %s", user);

  return 0;
err:
  log_error("error parsing user and password at line %d", line);
  return 1;
}


C_MODE_START

static byte* get_user_key(const byte* u, uint* len,
                          my_bool __attribute__((unused)) t)
{
  const User *user= (const User *) u;
  *len= user->user_length;
  return (byte *) user->user;
}

static void delete_user(void *u)
{
  User *user= (User *) u;
  delete user;
}

C_MODE_END


int User_map::init()
{
  enum { START_HASH_SIZE = 16 };
  if (hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
      get_user_key, delete_user, 0))
    return 1;
  return 0;
}


User_map::~User_map()
{
  hash_free(&hash);
}


/*
  Load all users from the password file. Must be called once right after
  construction.
  In case of failure, puts error message to the log file and returns 1
*/

int User_map::load(const char *password_file_name)
{
  FILE *file;
  char line[USERNAME_LENGTH + SCRAMBLED_PASSWORD_CHAR_LENGTH +
            2 +                               /* for possible quotes */
            1 +                               /* for ':' */
            1 +                               /* for newline */
            1];                               /* for trailing zero */
  uint line_length;
  User *user;
  int rc= 1;

  if ((file= my_fopen(password_file_name, O_RDONLY | O_BINARY, MYF(0))) == 0)
  {
    log_error("can't open password file %s: errno=%d, %s", password_file_name,
              errno, strerror(errno));
    return 1;
  }

  while (fgets(line, sizeof(line), file))
  {
    /* skip comments and empty lines */
    if (line[0] == '#' || line[0] == '\n' &&
        (line[1] == '\0' || line[1] == '\r'))
      continue;
    if ((user= new User) == 0)
      goto done;
    if (user->init(line) || my_hash_insert(&hash, (byte *) user))
      goto err_init_user;
  }
  if (feof(file))
    rc= 0;
  goto done;
err_init_user:
  delete user;
done:
  my_fclose(file, MYF(0));
  return rc;
}


/*
    Check if user exists and password is correct
  RETURN VALUE
    0 - user found and password OK
    1 - password mismatch
    2 - user not found
*/

int User_map::authenticate(const char *user_name, uint length,
                           const char *scrambled_password,
                           const char *scramble) const
{
  const User *user= (const User *) hash_search((HASH *) &hash,
                                               (byte *) user_name, length);
  if (user)
    return check_scramble(scrambled_password, scramble, user->salt);
  return 2;
}
