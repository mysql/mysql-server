/*
   Copyright (c) 2004-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "user_map.h"
#include "exit_codes.h"
#include "log.h"
#include "portability.h"

User::User(const LEX_STRING *user_name_arg, const char *password)
{
  user_length= (uint8) (strmake(user, user_name_arg->str,
                                USERNAME_LENGTH) - user);
  set_password(password);
}

int User::init(const char *line)
{
  const char *name_begin, *name_end, *password;
  int password_length;

  if (line[0] == '\'' || line[0] == '"')
  {
    name_begin= line + 1;
    name_end= strchr(name_begin, line[0]);
    if (name_end == 0 || name_end[1] != ':')
    {
      log_error("Invalid format (unmatched quote) of user line (%s).",
                (const char *) line);
      return 1;
    }
    password= name_end + 2;
  }
  else
  {
    name_begin= line;
    name_end= strchr(name_begin, ':');
    if (name_end == 0)
    {
      log_error("Invalid format (no delimiter) of user line (%s).",
                (const char *) line);
      return 1;
    }
    password= name_end + 1;
  }

  user_length= (uint8) (name_end - name_begin);
  if (user_length > USERNAME_LENGTH)
  {
    log_error("User name is too long (%d). Max length: %d. "
              "User line: '%s'.",
              (int) user_length,
              (int) USERNAME_LENGTH,
              (const char *) line);
    return 1;
  }

  password_length= (int) strlen(password);
  if (password_length > SCRAMBLED_PASSWORD_CHAR_LENGTH)
  {
    log_error("Password is too long (%d). Max length: %d."
              "User line: '%s'.",
              (int) password_length,
              (int) SCRAMBLED_PASSWORD_CHAR_LENGTH,
              (const char *) line);
    return 1;
  }

  memcpy(user, name_begin, user_length);
  user[user_length]= 0;

  memcpy(scrambled_password, password, password_length);
  scrambled_password[password_length]= 0;

  get_salt_from_password(salt, password);

  log_info("Loaded user '%s'.", (const char *) user);

  return 0;
}


C_MODE_START

static uchar* get_user_key(const uchar* u, size_t* len,
                           my_bool __attribute__((unused)) t)
{
  const User *user= (const User *) u;
  *len= user->user_length;
  return (uchar *) user->user;
}

static void delete_user(void *u)
{
  User *user= (User *) u;
  delete user;
}

C_MODE_END


void User_map::Iterator::reset()
{
  cur_idx= 0;
}


User *User_map::Iterator::next()
{
  if (cur_idx < user_map->hash.records)
    return (User *) hash_element(&user_map->hash, cur_idx++);

  return NULL;
}


int User_map::init()
{
  enum { START_HASH_SIZE= 16 };
  if (hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
                get_user_key, delete_user, 0))
    return 1;

  initialized= TRUE;

  return 0;
}


User_map::User_map()
  :initialized(FALSE)
{
}


User_map::~User_map()
{
  if (initialized)
    hash_free(&hash);
}


/*
  Load password database.

  SYNOPSIS
    load()
    password_file_name  [IN] password file path
    err_msg             [OUT] error message

  DESCRIPTION
    Load all users from the password file. Must be called once right after
    construction. In case of failure, puts error message to the log file and
    returns specific error code.

  RETURN
    0   on success
    !0  on error
*/

int User_map::load(const char *password_file_name, const char **err_msg)
{
  static const int ERR_MSG_BUF_SIZE = 255;
  static char err_msg_buf[ERR_MSG_BUF_SIZE];

  FILE *file;
  char line[USERNAME_LENGTH + SCRAMBLED_PASSWORD_CHAR_LENGTH +
            2 +                               /* for possible quotes */
            1 +                               /* for ':' */
            2 +                               /* for newline */
            1];                               /* for trailing zero */
  User *user;

  if (my_access(password_file_name, F_OK) != 0)
  {
    if (err_msg)
    {
      snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
               "password file (%s) does not exist",
               (const char *) password_file_name);
      *err_msg= err_msg_buf;
    }

    return ERR_PASSWORD_FILE_DOES_NOT_EXIST;
  }

  if ((file= my_fopen(password_file_name, O_RDONLY | O_BINARY, MYF(0))) == 0)
  {
    if (err_msg)
    {
      snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
               "can not open password file (%s): %s",
               (const char *) password_file_name,
               (const char *) strerror(errno));
      *err_msg= err_msg_buf;
    }

    return ERR_IO_ERROR;
  }

  log_info("Loading the password database...");

  while (fgets(line, sizeof(line), file))
  {
    char *user_line= line;

    /*
      We need to skip EOL-symbols also from the beginning of the line, because
      if the previous line was ended by \n\r sequence, we get \r in our line.
    */

    while (user_line[0] == '\r' || user_line[0] == '\n')
      ++user_line;

    /* Skip EOL-symbols in the end of the line. */

    {
      char *ptr;

      if ((ptr= strchr(user_line, '\n')))
        *ptr= 0;

      if ((ptr= strchr(user_line, '\r')))
        *ptr= 0;
    }

    /* skip comments and empty lines */
    if (!user_line[0] || user_line[0] == '#')
      continue;

    if ((user= new User) == 0)
    {
      my_fclose(file, MYF(0));

      if (err_msg)
      {
        snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
                 "out of memory while parsing password file (%s)",
                 (const char *) password_file_name);
        *err_msg= err_msg_buf;
      }

      return ERR_OUT_OF_MEMORY;
    }

    if (user->init(user_line))
    {
      delete user;
      my_fclose(file, MYF(0));

      if (err_msg)
      {
        snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
                 "password file (%s) corrupted",
                 (const char *) password_file_name);
        *err_msg= err_msg_buf;
      }

      return ERR_PASSWORD_FILE_CORRUPTED;
    }

    if (my_hash_insert(&hash, (uchar *) user))
    {
      delete user;
      my_fclose(file, MYF(0));

      if (err_msg)
      {
        snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
                 "out of memory while parsing password file (%s)",
                 (const char *) password_file_name);
        *err_msg= err_msg_buf;
      }

      return ERR_OUT_OF_MEMORY;
    }
  }

  log_info("The password database loaded successfully.");

  my_fclose(file, MYF(0));

  if (err_msg)
    *err_msg= NULL;

  return ERR_OK;
}


int User_map::save(const char *password_file_name, const char **err_msg)
{
  static const int ERR_MSG_BUF_SIZE = 255;
  static char err_msg_buf[ERR_MSG_BUF_SIZE];

  FILE *file;

  if ((file= my_fopen(password_file_name, O_WRONLY | O_TRUNC | O_BINARY,
                      MYF(0))) == 0)
  {
    if (err_msg)
    {
      snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
               "can not open password file (%s) for writing: %s",
               (const char *) password_file_name,
               (const char *) strerror(errno));
      *err_msg= err_msg_buf;
    }

    return ERR_IO_ERROR;
  }

  {
    User_map::Iterator it(this);
    User *user;

    while ((user= it.next()))
    {
      if (fprintf(file, "%s:%s\n", (const char *) user->user,
                  (const char *) user->scrambled_password) < 0)
      {
        if (err_msg)
        {
          snprintf(err_msg_buf, ERR_MSG_BUF_SIZE,
                   "can not write to password file (%s): %s",
                   (const char *) password_file_name,
                   (const char *) strerror(errno));
          *err_msg= err_msg_buf;
        }

        my_fclose(file, MYF(0));

        return ERR_IO_ERROR;
      }
    }
  }

  my_fclose(file, MYF(0));

  return ERR_OK;
}


/*
    Check if user exists and password is correct
  RETURN VALUE
    0 - user found and password OK
    1 - password mismatch
    2 - user not found
*/

int User_map::authenticate(const LEX_STRING *user_name,
                           const char *scrambled_password,
                           const char *scramble) const
{
  const User *user= find_user(user_name);
  return user ? check_scramble(scrambled_password, scramble, user->salt) : 2;
}


User *User_map::find_user(const LEX_STRING *user_name)
{
  return (User*) hash_search(&hash, (uchar*) user_name->str, user_name->length);
}

const User *User_map::find_user(const LEX_STRING *user_name) const
{
  return const_cast<User_map *> (this)->find_user(user_name);
}


bool User_map::add_user(User *user)
{
  return my_hash_insert(&hash, (uchar*) user) == 0 ? FALSE : TRUE;
}


bool User_map::remove_user(User *user)
{
  return hash_delete(&hash, (uchar*) user) == 0 ? FALSE : TRUE;
}
