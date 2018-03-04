/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DERROR_INCLUDED
#define DERROR_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "my_macros.h"

#ifdef EXTRA_CODE_FOR_UNIT_TESTING
#include "mysqld_error.h"
#endif


/**
  A record describing an error message.
*/
struct
{
  const char *name;         ///< MySQL error symbol ("ER_STARTUP")
  uint        mysql_errno;  ///< MySQL error code (consecutive within sections)
  const char *text;         ///< MySQL error message
  const char *odbc_state;   ///< SQL state
  const char *jdbc_state;   ///< JBDC state
  uint        error_index;  ///< consecutive. 0 for obsolete.
} typedef server_error;

class THD;
struct TABLE;

typedef struct charset_info_st CHARSET_INFO;

/**
  Character set of the buildin error messages loaded from errmsg.sys.
*/
extern CHARSET_INFO *error_message_charset_info;

class MY_LOCALE_ERRMSGS
{
  const char                      *language;
  const char                     **errmsgs;

public:
  MY_LOCALE_ERRMSGS(const char *lang_par)
    : language(lang_par), errmsgs(nullptr)
  {}

  /** Return error message string for a given error number. */
  const char *lookup(int mysql_errno);

#ifdef EXTRA_CODE_FOR_UNIT_TESTING
  bool replace_msg(int mysql_errno, const char *new_msg)
  {
    int offset= 0; // Position where the current section starts in the array.
    int num_sections= sizeof(errmsg_section_start) /
      sizeof(errmsg_section_start[0]);
    for (int i= 0; i < num_sections; i++)
    {
      if (mysql_errno < (errmsg_section_start[i] + errmsg_section_size[i]))
      {
        errmsgs[mysql_errno - errmsg_section_start[i] + offset]= new_msg;
        return false;
      }
      offset+= errmsg_section_size[i];
    }
    return true;
  }
#endif

  /** Has the error message file been sucessfully loaded? */
  bool is_loaded() const { return errmsgs != NULL; }

  /** Deallocate error message strings. */
  void destroy();

  /** Read the error message file and initialize strings. */
  bool read_texts();

  /** What language is this error message set for? */
  const char *get_language() const { return language; }
};

const char* ER_DEFAULT(int mysql_errno);
const char* ER_THD(const THD *thd, int mysql_errno);

C_MODE_START
const char *get_server_errmsgs(int mysql_errno);
C_MODE_END

const char *mysql_errno_to_symbol(int mysql_errno);
int         mysql_symbol_to_errno(const char *error_symbol);

int         errmsgs_reload(THD *thd);

/**
  Read the error message file, initialize and register error messages
  for all languages.

  @retval true if initialization failed, false otherwise.
*/
bool init_errmessage();

/**
  Unregister error messages for all languages.
*/
void deinit_errmessage();

#endif /* DERROR_INCLUDED */
