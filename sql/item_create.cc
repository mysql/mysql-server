/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Functions to create an item. Used by lex.h */

#include "mysql_priv.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Item *create_func_abs(Item* a)
{
  return new Item_func_abs(a);
}

Item *create_func_acos(Item* a)
{
  return new Item_func_acos(a);
}

Item *create_func_ascii(Item* a)
{
  return new Item_func_ascii(a);
}

Item *create_func_ord(Item* a)
{
  return new Item_func_ord(a);
}

Item *create_func_asin(Item* a)
{
  return new Item_func_asin(a);
}

Item *create_func_bin(Item* a)
{
  return new Item_func_conv(a,new Item_int((int32) 10,2),
			    new Item_int((int32) 2,1));
}

Item *create_func_bit_count(Item* a)
{
  return new Item_func_bit_count(a);
}

Item *create_func_ceiling(Item* a)
{
  return new Item_func_ceiling(a);
}

Item *create_func_connection_id(void)
{
  return new Item_int("CONNECTION_ID()",(longlong) current_thd->thread_id,10);
}

Item *create_func_conv(Item* a, Item *b, Item *c)
{
  return new Item_func_conv(a,b,c);
}

Item *create_func_cos(Item* a)
{
  return new Item_func_cos(a);
}

Item *create_func_cot(Item* a)
{
  return new Item_func_div(new Item_int((char*) "1",1,1),
			   new Item_func_tan(a));
}

Item *create_func_date_format(Item* a,Item *b)
{
  return new Item_func_date_format(a,b,0);
}

Item *create_func_dayofmonth(Item* a)
{
  return new Item_func_dayofmonth(a);
}

Item *create_func_dayofweek(Item* a)
{
  return new Item_func_weekday(new Item_func_to_days(a),1);
}

Item *create_func_dayofyear(Item* a)
{
  return new Item_func_dayofyear(a);
}

Item *create_func_dayname(Item* a)
{
  return new Item_func_dayname(new Item_func_to_days(a));
}

Item *create_func_degrees(Item *a)
{
  return new Item_func_units((char*) "degrees",a,180/M_PI,0.0);
}

Item *create_func_exp(Item* a)
{
  return new Item_func_exp(a);
}

Item *create_func_find_in_set(Item* a, Item *b)
{
  return new Item_func_find_in_set(a, b);
}

Item *create_func_floor(Item* a)
{
  return new Item_func_floor(a);
}

Item *create_func_from_days(Item* a)
{
  return new Item_func_from_days(a);
}

Item *create_func_get_lock(Item* a, Item *b)
{
  return new Item_func_get_lock(a, b);
}

Item *create_func_hex(Item *a)
{
  return new Item_func_conv(a,new Item_int((int32) 10,2),
			    new Item_int((int32) 16,2));
}

Item *create_func_inet_ntoa(Item* a)
{
  return new Item_func_inet_ntoa(a);
}

Item *create_func_inet_aton(Item* a)
{
  return new Item_func_inet_aton(a);
}


Item *create_func_ifnull(Item* a, Item *b)
{
  return new Item_func_ifnull(a,b);
}

Item *create_func_nullif(Item* a, Item *b)
{
  return new Item_func_nullif(a,b);
}

Item *create_func_locate(Item* a, Item *b)
{
  return new Item_func_locate(b,a);
}

Item *create_func_instr(Item* a, Item *b)
{
  return new Item_func_locate(a,b);
}

Item *create_func_isnull(Item* a)
{
  return new Item_func_isnull(a);
}

Item *create_func_lcase(Item* a)
{
  return new Item_func_lcase(a);
}

Item *create_func_length(Item* a)
{
  return new Item_func_length(a);
}

Item *create_func_char_length(Item* a)
{
  return new Item_func_char_length(a);
}

Item *create_func_log(Item* a)
{
  return new Item_func_log(a);
}

Item *create_func_log10(Item* a)
{
  return new Item_func_log10(a);
}

Item *create_func_lpad(Item* a, Item *b, Item *c)
{
  return new Item_func_lpad(a,b,c);
}

Item *create_func_ltrim(Item* a)
{
  return new Item_func_ltrim(a,new Item_string(" ",1));
}

Item *create_func_md5(Item* a)
{
  return new Item_func_md5(a);
}

Item *create_func_mod(Item* a, Item *b)
{
  return new Item_func_mod(a,b);
}

Item *create_func_monthname(Item* a)
{
  return new Item_func_monthname(a);
}

Item *create_func_month(Item* a)
{
  return new Item_func_month(a);
}

Item *create_func_oct(Item *a)
{
  return new Item_func_conv(a,new Item_int((int32) 10,2),
			    new Item_int((int32) 8,1));
}

Item *create_func_period_add(Item* a, Item *b)
{
  return new Item_func_period_add(a,b);
}

Item *create_func_period_diff(Item* a, Item *b)
{
  return new Item_func_period_diff(a,b);
}

Item *create_func_pi(void)
{
  return new Item_real("PI()",M_PI,6,8);
}

Item *create_func_pow(Item* a, Item *b)
{
  return new Item_func_pow(a,b);
}

Item *create_func_quarter(Item* a)
{
  return new Item_func_quarter(a);
}

Item *create_func_radians(Item *a)
{
  return new Item_func_units((char*) "radians",a,M_PI/180,0.0);
}

Item *create_func_release_lock(Item* a)
{
  return new Item_func_release_lock(a);
}

Item *create_func_repeat(Item* a, Item *b)
{
  return new Item_func_repeat(a,b);
}

Item *create_func_reverse(Item* a)
{
  return new Item_func_reverse(a);
}

Item *create_func_rpad(Item* a, Item *b, Item *c)
{
  return new Item_func_rpad(a,b,c);
}

Item *create_func_rtrim(Item* a)
{
  return new Item_func_rtrim(a,new Item_string(" ",1));
}

Item *create_func_sec_to_time(Item* a)
{
  return new Item_func_sec_to_time(a);
}

Item *create_func_sign(Item* a)
{
  return new Item_func_sign(a);
}

Item *create_func_sin(Item* a)
{
  return new Item_func_sin(a);
}

Item *create_func_space(Item *a)
{
  return new Item_func_repeat(new Item_string(" ",1),a);
}

Item *create_func_soundex(Item* a)
{
  return new Item_func_soundex(a);
}

Item *create_func_sqrt(Item* a)
{
  return new Item_func_sqrt(a);
}

Item *create_func_strcmp(Item* a, Item *b)
{
  return new Item_func_strcmp(a,b);
}

Item *create_func_tan(Item* a)
{
  return new Item_func_tan(a);
}

Item *create_func_time_format(Item *a, Item *b)
{
  return new Item_func_date_format(a,b,1);
}

Item *create_func_time_to_sec(Item* a)
{
  return new Item_func_time_to_sec(a);
}

Item *create_func_to_days(Item* a)
{
  return new Item_func_to_days(a);
}

Item *create_func_truncate (Item *a, Item *b)
{
  return new Item_func_round(a,b,1);
}

Item *create_func_ucase(Item* a)
{
  return new Item_func_ucase(a);
}

Item *create_func_version(void)
{
  return new Item_string(NullS,server_version, (uint) strlen(server_version));
}

Item *create_func_weekday(Item* a)
{
  return new Item_func_weekday(new Item_func_to_days(a),0);
}

Item *create_func_year(Item* a)
{
  return new Item_func_year(a);
}

Item *create_load_file(Item* a)
{
  return new Item_load_file(a);
}
