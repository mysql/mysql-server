/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef STRFUNC_INCLUDED
#define STRFUNC_INCLUDED

#include "my_global.h"                          /* ulonglong, uint */

typedef struct st_typelib TYPELIB;

ulonglong find_set(TYPELIB *lib, const char *x, uint length, CHARSET_INFO *cs,
		   char **err_pos, uint *err_len, bool *set_warning);
ulonglong find_set_from_flags(TYPELIB *lib, uint default_name,
                              ulonglong cur_set, ulonglong default_set,
                              const char *str, uint length, CHARSET_INFO *cs,
                              char **err_pos, uint *err_len, bool *set_warning);
uint find_type(const TYPELIB *lib, const char *find, uint length,
               bool part_match);
uint find_type2(const TYPELIB *lib, const char *find, uint length,
                CHARSET_INFO *cs);
void unhex_type2(TYPELIB *lib);
uint check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word);
int find_string_in_array(LEX_STRING * const haystack, LEX_STRING * const needle,
                         CHARSET_INFO * const cs);
char *flagset_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                        const char *lib[]);
char *set_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                    const char *lib[]);

/*
  These functions were protected by INNODB_COMPATIBILITY_HOOKS
 */
uint strconvert(CHARSET_INFO *from_cs, const char *from,
                CHARSET_INFO *to_cs, char *to, uint to_length, uint *errors);

#endif /* STRFUNC_INCLUDED */
