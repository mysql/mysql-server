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

Item *create_func_abs(Item* a);
Item *create_func_acos(Item* a);
Item *create_func_ascii(Item* a);
Item *create_func_asin(Item* a);
Item *create_func_bin(Item* a);
Item *create_func_bit_count(Item* a);
Item *create_func_ceiling(Item* a);
Item *create_func_char_length(Item* a);
Item *create_func_connection_id(void);
Item *create_func_conv(Item* a, Item *b, Item *c);
Item *create_func_cos(Item* a);
Item *create_func_cot(Item* a);
Item *create_func_date_format(Item* a,Item *b);
Item *create_func_dayname(Item* a);
Item *create_func_dayofmonth(Item* a);
Item *create_func_dayofweek(Item* a);
Item *create_func_dayofyear(Item* a);
Item *create_func_degrees(Item *);
Item *create_func_exp(Item* a);
Item *create_func_find_in_set(Item* a, Item *b);
Item *create_func_floor(Item* a);
Item *create_func_found_rows(void);
Item *create_func_from_days(Item* a);
Item *create_func_get_lock(Item* a, Item *b);
Item *create_func_hex(Item *a);
Item *create_func_inet_aton(Item* a);
Item *create_func_inet_ntoa(Item* a);

Item *create_func_ifnull(Item* a, Item *b);
Item *create_func_instr(Item* a, Item *b);
Item *create_func_isnull(Item* a);
Item *create_func_lcase(Item* a);
Item *create_func_length(Item* a);
Item *create_func_locate(Item* a, Item *b);
Item *create_func_log(Item* a);
Item *create_func_log10(Item* a);
Item *create_func_lpad(Item* a, Item *b, Item *c);
Item *create_func_ltrim(Item* a);
Item *create_func_md5(Item* a);
Item *create_func_mod(Item* a, Item *b);
Item *create_func_monthname(Item* a);
Item *create_func_nullif(Item* a, Item *b);
Item *create_func_oct(Item *);
Item *create_func_ord(Item* a);
Item *create_func_period_add(Item* a, Item *b);
Item *create_func_period_diff(Item* a, Item *b);
Item *create_func_pi(void);
Item *create_func_pow(Item* a, Item *b);
Item *create_func_quarter(Item* a);
Item *create_func_radians(Item *a);
Item *create_func_release_lock(Item* a);
Item *create_func_repeat(Item* a, Item *b);
Item *create_func_reverse(Item* a);
Item *create_func_rpad(Item* a, Item *b, Item *c);
Item *create_func_rtrim(Item* a);
Item *create_func_sec_to_time(Item* a);
Item *create_func_sign(Item* a);
Item *create_func_sin(Item* a);
Item *create_func_soundex(Item* a);
Item *create_func_space(Item *);
Item *create_func_sqrt(Item* a);
Item *create_func_strcmp(Item* a, Item *b);
Item *create_func_tan(Item* a);;
Item *create_func_time_format(Item *a, Item *b);
Item *create_func_time_to_sec(Item* a);
Item *create_func_to_days(Item* a);
Item *create_func_ucase(Item* a);
Item *create_func_version(void);
Item *create_func_weekday(Item* a);
Item *create_load_file(Item* a);
Item *create_wait_for_master_pos(Item* a, Item* b);
