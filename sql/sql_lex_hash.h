/*
   Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/*
  Hash accessor function for lexical scanners:
  * sql_lex.h, sql_lex.cc,
  * sql_lex_hints.h, sql_lex_hints.cc.
*/

#ifndef SQL_LEX_HASH_INCLUDED
#define SQL_LEX_HASH_INCLUDED

struct SYMBOL;

class Lex_hash
{
private:
  const unsigned char *hash_map;
  const unsigned int entry_max_len;

public:
  Lex_hash(const unsigned char *hash_map_arg, unsigned int entry_max_len_arg)
  : hash_map(hash_map_arg), entry_max_len(entry_max_len_arg)
  {}

  const struct SYMBOL *get_hash_symbol(const char *s, unsigned int len) const;

  static const Lex_hash sql_keywords;
  static const Lex_hash sql_keywords_and_funcs;

  static const Lex_hash hint_keywords;
};


#endif /* SQL_LEX_HASH_INCLUDED */
