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

#ifndef SQL_LOCALE_INCLUDED
#define SQL_LOCALE_INCLUDED

typedef struct my_locale_errmsgs
{
  const char *language;
  const char **errmsgs;
} MY_LOCALE_ERRMSGS;

#include "my_global.h"                          /* uint */

typedef struct st_typelib TYPELIB;

class MY_LOCALE
{
public:
  uint  number;
  const char *name;
  const char *description;
  const bool is_ascii;
  TYPELIB *month_names;
  TYPELIB *ab_month_names;
  TYPELIB *day_names;
  TYPELIB *ab_day_names;
  uint max_month_name_length;
  uint max_day_name_length;
  uint decimal_point;
  uint thousand_sep;
  const char *grouping;
  MY_LOCALE_ERRMSGS *errmsgs;
  MY_LOCALE(uint number_par,
            const char *name_par, const char *descr_par, bool is_ascii_par,
            TYPELIB *month_names_par, TYPELIB *ab_month_names_par,
            TYPELIB *day_names_par, TYPELIB *ab_day_names_par,
            uint max_month_name_length_par, uint max_day_name_length_par,
            uint decimal_point_par, uint thousand_sep_par,
            const char *grouping_par, MY_LOCALE_ERRMSGS *errmsgs_par) :
    number(number_par),
    name(name_par), description(descr_par), is_ascii(is_ascii_par),
    month_names(month_names_par), ab_month_names(ab_month_names_par),
    day_names(day_names_par), ab_day_names(ab_day_names_par),
    max_month_name_length(max_month_name_length_par),
    max_day_name_length(max_day_name_length_par),
    decimal_point(decimal_point_par),
    thousand_sep(thousand_sep_par),
    grouping(grouping_par),
    errmsgs(errmsgs_par)
  {}
  uint repertoire() const
  { return is_ascii ? MY_REPERTOIRE_ASCII : MY_REPERTOIRE_EXTENDED; }
};
/* Exported variables */

extern MY_LOCALE my_locale_en_US;
extern MY_LOCALE *my_locales[];
extern MY_LOCALE *my_default_lc_messages;
extern MY_LOCALE *my_default_lc_time_names;

/* Exported functions */

MY_LOCALE *my_locale_by_name(const char *name);
MY_LOCALE *my_locale_by_number(uint number);
void cleanup_errmsgs(void);

#endif /* SQL_LOCALE_INCLUDED */
