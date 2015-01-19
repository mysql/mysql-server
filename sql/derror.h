/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef DERROR_INCLUDED
#define DERROR_INCLUDED

#include "my_global.h"                          /* uint */

class THD;

class MY_LOCALE_ERRMSGS
{
  const char *language;
  const char **errmsgs;

public:
  MY_LOCALE_ERRMSGS(const char *lang_par)
    : language(lang_par), errmsgs(NULL)
  {}

  /** Return error message string for a given error number. */
  const char *lookup(int mysql_errno);

  /** Has the error message file been sucessfully loaded? */
  bool is_loaded() const { return errmsgs != NULL; }

  /** Deallocate error message strings. */
  void destroy();

  /** Read the error message file and initialize strings. */
  bool read_texts();

  const char *get_language() const { return language; }
};

const char* ER_DEFAULT(int mysql_errno);
const char* ER_THD(const THD *thd, int mysql_errno);

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
