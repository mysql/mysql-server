/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef STRFUNC_INCLUDED
#define STRFUNC_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <utility>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_inttypes.h"
#include "mysql/mysql_lex_string.h"             // MYSQL_LEX_CSTRING

typedef struct charset_info_st CHARSET_INFO;
typedef struct st_typelib TYPELIB;
class THD;

ulonglong find_set(TYPELIB *lib, const char *x, size_t length,
                   const CHARSET_INFO *cs,
		   char **err_pos, uint *err_len, bool *set_warning);
uint find_type(const TYPELIB *lib, const char *find, size_t length,
               bool part_match);
uint find_type2(const TYPELIB *lib, const char *find, size_t length,
                const CHARSET_INFO *cs);
uint check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word);
char *flagset_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                        const char *lib[]);
char *set_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                    const char *lib[]);

size_t strconvert(const CHARSET_INFO *from_cs, const char *from,
                  CHARSET_INFO *to_cs, char *to, size_t to_length, uint *errors);


/**
  convert a hex digit into number.
*/

inline int hexchar_to_int(char c)
{
  if (c <= '9' && c >= '0')
    return c-'0';
  c|=32;
  if (c <= 'f' && c >= 'a')
    return c-'a'+10;
  return -1;
}


/**
  Return a LEX_CSTRING handle to a std::string like (meaning someting
  which has the c_str() and length() member functions). Note that the
  std::string-like object retains ownership of the character array,
  and consquently the returned LEX_CSTRING is only valid as long as the
  std::string-like object is valid.

  @param s std::string-like object

  @return LEX_CSTRING handle to string
*/
template <class STDSTRINGLIKE_TYPE>
MYSQL_LEX_CSTRING lex_cstring_handle(const STDSTRINGLIKE_TYPE &s)
{
  return { s.c_str(), s.length() };
}


/**
  Lowercase a string according to charset.

  @param ci pointer to charset for conversion
  @param s string to lower-case
  @retval modified argument if r-value
  @retval copy of modified argument if lvalue (meaningless, don't use)
 */
template <class STRLIKE_TYPE>
STRLIKE_TYPE casedn(const CHARSET_INFO *ci,
                    STRLIKE_TYPE &&s)
{
  s.resize(ci->casedn_multiply * s.size());
  s.resize(my_casedn_str(ci, &s.front()));
  return std::forward<STRLIKE_TYPE>(s);
}


/**
  Lowercase a string according to charset. Overload for const T& which
  copies argument and forwards to T&& overload.

  @param ci pointer to charset for conversion
  @param src string to lower-case
  @retval modified copy of argument
 */

template <class STRLIKE_TYPE>
STRLIKE_TYPE casedn(const CHARSET_INFO *ci, const STRLIKE_TYPE &src)
{
  return casedn(ci, STRLIKE_TYPE {src});
}

#endif /* STRFUNC_INCLUDED */
