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
Item *create_func_aes_encrypt(Item* a, Item* b);
Item *create_func_aes_decrypt(Item* a, Item* b);
Item *create_func_ascii(Item* a);
Item *create_func_asin(Item* a);
Item *create_func_bin(Item* a);
Item *create_func_bit_count(Item* a);
Item *create_func_bit_length(Item* a);
Item *create_func_coercibility(Item* a);
Item *create_func_ceiling(Item* a);
Item *create_func_char_length(Item* a);
Item *create_func_cast(Item *a, Cast_target cast_type, int len, int dec,
                       CHARSET_INFO *cs);
Item *create_func_connection_id(void);
Item *create_func_conv(Item* a, Item *b, Item *c);
Item *create_func_cos(Item* a);
Item *create_func_cot(Item* a);
Item *create_func_crc32(Item* a);
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
Item *create_func_ln(Item* a);
Item *create_func_locate(Item* a, Item *b);
Item *create_func_log2(Item* a);
Item *create_func_log10(Item* a);
Item *create_func_lpad(Item* a, Item *b, Item *c);
Item *create_func_ltrim(Item* a);
Item *create_func_md5(Item* a);
Item *create_func_mod(Item* a, Item *b);
Item *create_func_monthname(Item* a);
Item *create_func_name_const(Item *a, Item *b);
Item *create_func_nullif(Item* a, Item *b);
Item *create_func_oct(Item *);
Item *create_func_ord(Item* a);
Item *create_func_period_add(Item* a, Item *b);
Item *create_func_period_diff(Item* a, Item *b);
Item *create_func_pi(void);
Item *create_func_pow(Item* a, Item *b);
Item *create_func_current_user(void);
Item *create_func_radians(Item *a);
Item *create_func_release_lock(Item* a);
Item *create_func_repeat(Item* a, Item *b);
Item *create_func_reverse(Item* a);
Item *create_func_rpad(Item* a, Item *b, Item *c);
Item *create_func_rtrim(Item* a);
Item *create_func_sec_to_time(Item* a);
Item *create_func_sign(Item* a);
Item *create_func_sin(Item* a);
Item *create_func_sha(Item* a);
Item *create_func_sleep(Item* a);
Item *create_func_soundex(Item* a);
Item *create_func_space(Item *);
Item *create_func_sqrt(Item* a);
Item *create_func_strcmp(Item* a, Item *b);
Item *create_func_tan(Item* a);
Item *create_func_time_format(Item *a, Item *b);
Item *create_func_time_to_sec(Item* a);
Item *create_func_to_days(Item* a);
Item *create_func_ucase(Item* a);
Item *create_func_unhex(Item* a);
Item *create_func_uuid(void);
Item *create_func_version(void);
Item *create_func_weekday(Item* a);
Item *create_load_file(Item* a);
Item *create_func_is_free_lock(Item* a);
Item *create_func_is_used_lock(Item* a);
Item *create_func_quote(Item* a);

#ifdef HAVE_SPATIAL

Item *create_func_geometry_from_text(Item *a);
Item *create_func_as_wkt(Item *a);
Item *create_func_as_wkb(Item *a);
Item *create_func_srid(Item *a);
Item *create_func_startpoint(Item *a);
Item *create_func_endpoint(Item *a);
Item *create_func_exteriorring(Item *a);
Item *create_func_centroid(Item *a);
Item *create_func_envelope(Item *a);
Item *create_func_pointn(Item *a, Item *b);
Item *create_func_interiorringn(Item *a, Item *b);
Item *create_func_geometryn(Item *a, Item *b);

Item *create_func_equals(Item *a, Item *b);
Item *create_func_disjoint(Item *a, Item *b);
Item *create_func_intersects(Item *a, Item *b);
Item *create_func_touches(Item *a, Item *b);
Item *create_func_crosses(Item *a, Item *b);
Item *create_func_within(Item *a, Item *b);
Item *create_func_contains(Item *a, Item *b);
Item *create_func_overlaps(Item *a, Item *b);

Item *create_func_isempty(Item *a);
Item *create_func_issimple(Item *a);
Item *create_func_isclosed(Item *a);

Item *create_func_geometry_type(Item *a);
Item *create_func_dimension(Item *a);
Item *create_func_x(Item *a);
Item *create_func_y(Item *a);
Item *create_func_area(Item *a);
Item *create_func_glength(Item *a);

Item *create_func_numpoints(Item *a);
Item *create_func_numinteriorring(Item *a);
Item *create_func_numgeometries(Item *a);

Item *create_func_point(Item *a, Item *b);

#endif /*HAVE_SPATIAL*/

Item *create_func_compress(Item *a);
Item *create_func_uncompress(Item *a);
Item *create_func_uncompressed_length(Item *a);

Item *create_func_datediff(Item *a, Item *b);
Item *create_func_weekofyear(Item *a);
Item *create_func_makedate(Item* a,Item* b);
Item *create_func_addtime(Item* a,Item* b);
Item *create_func_subtime(Item* a,Item* b);
Item *create_func_timediff(Item* a,Item* b);
Item *create_func_maketime(Item* a,Item* b,Item* c);
Item *create_func_str_to_date(Item* a,Item* b);
Item *create_func_last_day(Item *a);
