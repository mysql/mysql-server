/* Copyright (C) 2000-2003 MySQL AB

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

Item *create_func_abs(Item* a)
{
  return new Item_func_abs(a);
}

Item *create_func_acos(Item* a)
{
  return new Item_func_acos(a);
}

Item *create_func_aes_encrypt(Item* a, Item* b)
{
  return new Item_func_aes_encrypt(a, b);
}

Item *create_func_aes_decrypt(Item* a, Item* b)
{
  return new Item_func_aes_decrypt(a, b);
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
  THD *thd=current_thd;
  thd->lex->safe_to_cache_query= 0;
  return new Item_static_int_func("connection_id()",
                                  (longlong)
                                  ((thd->slave_thread) ?
                                   thd->variables.pseudo_thread_id :
                                   thd->thread_id),
                                  10);
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

Item *create_func_found_rows(void)
{
  THD *thd=current_thd;
  thd->lex->safe_to_cache_query= 0;
  return new Item_func_found_rows();
}

Item *create_func_from_days(Item* a)
{
  return new Item_func_from_days(a);
}

Item *create_func_get_lock(Item* a, Item *b)
{
  current_thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  return new Item_func_get_lock(a, b);
}

Item *create_func_hex(Item *a)
{
  return new Item_func_hex(a);
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

Item *create_func_bit_length(Item* a)
{
  return new Item_func_bit_length(a);
}

Item *create_func_coercibility(Item* a)
{
  return new Item_func_coercibility(a);
}

Item *create_func_char_length(Item* a)
{
  return new Item_func_char_length(a);
}

Item *create_func_ln(Item* a)
{
  return new Item_func_ln(a);
}

Item *create_func_log2(Item* a)
{
  return new Item_func_log2(a);
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
  return new Item_func_ltrim(a);
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
  return new Item_static_real_func("pi()", M_PI, 6, 8);
}

Item *create_func_pow(Item* a, Item *b)
{
  return new Item_func_pow(a,b);
}

Item *create_func_current_user()
{
  THD *thd=current_thd;
  char buff[HOSTNAME_LENGTH+USERNAME_LENGTH+2];
  uint length;

  length= (uint) (strxmov(buff, thd->priv_user, "@", thd->priv_host, NullS) -
		  buff);
  return new Item_static_string_func("current_user()",
                                     thd->memdup(buff, length), length,
                                     system_charset_info);
}

Item *create_func_radians(Item *a)
{
  return new Item_func_units((char*) "radians",a,M_PI/180,0.0);
}

Item *create_func_release_lock(Item* a)
{
  current_thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
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
  return new Item_func_rtrim(a);
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

Item *create_func_sha(Item* a)
{
  return new Item_func_sha(a);  
}
    
Item *create_func_space(Item *a)
{
  CHARSET_INFO *cs= current_thd->variables.collation_connection;
  Item *sp;
  
  if (cs->mbminlen > 1)
  {
    sp= new Item_string("",0,cs);
    if (sp)
    {
      uint dummy_errors;
      sp->str_value.copy(" ", 1, &my_charset_latin1, cs, &dummy_errors);
    }
  }
  else
  {
    sp= new Item_string(" ",1,cs);
  }
  return new Item_func_repeat(sp, a);
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

Item *create_func_ucase(Item* a)
{
  return new Item_func_ucase(a);
}

Item *create_func_unhex(Item* a)
{
  return new Item_func_unhex(a);
}

Item *create_func_uuid(void)
{
  return new Item_func_uuid();
}

Item *create_func_version(void)
{
  return new Item_static_string_func("version()", server_version,
			 (uint) strlen(server_version),
			 system_charset_info, DERIVATION_IMPLICIT);
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
  current_thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  return new Item_load_file(a);
}


Item *create_func_cast(Item *a, Cast_target cast_type, int len,
		       CHARSET_INFO *cs)
{
  Item *res;
  LINT_INIT(res);

  switch (cast_type) {
  case ITEM_CAST_BINARY: 	res= new Item_func_binary(a); break;
  case ITEM_CAST_SIGNED_INT:	res= new Item_func_signed(a); break;
  case ITEM_CAST_UNSIGNED_INT:  res= new Item_func_unsigned(a); break;
  case ITEM_CAST_DATE:		res= new Item_date_typecast(a); break;
  case ITEM_CAST_TIME:		res= new Item_time_typecast(a); break;
  case ITEM_CAST_DATETIME:	res= new Item_datetime_typecast(a); break;
  case ITEM_CAST_CHAR:
    res= new Item_char_typecast(a, len, cs ? cs : 
				current_thd->variables.collation_connection);
    break;
  }
  return res;
}

Item *create_func_is_free_lock(Item* a)
{
  current_thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  return new Item_func_is_free_lock(a);
}

Item *create_func_is_used_lock(Item* a)
{
  current_thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  return new Item_func_is_used_lock(a);
}

Item *create_func_quote(Item* a)
{
  return new Item_func_quote(a);
}

#ifdef HAVE_SPATIAL
Item *create_func_as_wkt(Item *a)
{
  return new Item_func_as_wkt(a);
}

Item *create_func_as_wkb(Item *a)
{
  return new Item_func_as_wkb(a);
}

Item *create_func_srid(Item *a)
{
  return new Item_func_srid(a);
}

Item *create_func_startpoint(Item *a)
{
  return new Item_func_spatial_decomp(a, Item_func::SP_STARTPOINT);
}

Item *create_func_endpoint(Item *a)
{
  return new Item_func_spatial_decomp(a, Item_func::SP_ENDPOINT);
}

Item *create_func_exteriorring(Item *a)
{
  return new Item_func_spatial_decomp(a, Item_func::SP_EXTERIORRING);
}

Item *create_func_pointn(Item *a, Item *b)
{
  return new Item_func_spatial_decomp_n(a, b, Item_func::SP_POINTN);
}

Item *create_func_interiorringn(Item *a, Item *b)
{
  return new Item_func_spatial_decomp_n(a, b, Item_func::SP_INTERIORRINGN);
}

Item *create_func_geometryn(Item *a, Item *b)
{
  return new Item_func_spatial_decomp_n(a, b, Item_func::SP_GEOMETRYN);
}

Item *create_func_centroid(Item *a)
{
  return new Item_func_centroid(a);
}

Item *create_func_envelope(Item *a)
{
  return new Item_func_envelope(a);
}

Item *create_func_equals(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_EQUALS_FUNC);
}

Item *create_func_disjoint(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_DISJOINT_FUNC);
}

Item *create_func_intersects(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_INTERSECTS_FUNC);
}

Item *create_func_touches(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_TOUCHES_FUNC);
}

Item *create_func_crosses(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_CROSSES_FUNC);
}

Item *create_func_within(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_WITHIN_FUNC);
}

Item *create_func_contains(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_CONTAINS_FUNC);
}

Item *create_func_overlaps(Item *a, Item *b)
{
  return new Item_func_spatial_rel(a, b, Item_func::SP_OVERLAPS_FUNC);
}

Item *create_func_isempty(Item *a)
{
  return new Item_func_isempty(a);
}

Item *create_func_issimple(Item *a)
{
  return new Item_func_issimple(a);
}

Item *create_func_isclosed(Item *a)
{
  return new Item_func_isclosed(a);
}

Item *create_func_geometry_type(Item *a)
{
  return new Item_func_geometry_type(a);
}

Item *create_func_dimension(Item *a)
{
  return new Item_func_dimension(a);
}

Item *create_func_x(Item *a)
{
  return new Item_func_x(a);
}

Item *create_func_y(Item *a)
{
  return new Item_func_y(a);
}

Item *create_func_numpoints(Item *a)
{
  return new Item_func_numpoints(a);
}

Item *create_func_numinteriorring(Item *a)
{
  return new Item_func_numinteriorring(a);
}

Item *create_func_numgeometries(Item *a)
{
  return new Item_func_numgeometries(a);
}

Item *create_func_area(Item *a)
{
  return new Item_func_area(a);
}

Item *create_func_glength(Item *a)
{
  return new Item_func_glength(a);
}

Item *create_func_point(Item *a, Item *b)
{
  return new Item_func_point(a, b);
}
#endif /*HAVE_SPATIAL*/

Item *create_func_crc32(Item* a)
{
  return new Item_func_crc32(a);
}

Item *create_func_compress(Item* a)
{
  return new Item_func_compress(a);
}

Item *create_func_uncompress(Item* a)
{
  return new Item_func_uncompress(a);
}

Item *create_func_uncompressed_length(Item* a)
{
  return new Item_func_uncompressed_length(a);
}

Item *create_func_datediff(Item *a, Item *b)
{
  return new Item_func_minus(new Item_func_to_days(a),
			     new Item_func_to_days(b));
}

Item *create_func_weekofyear(Item *a)
{
  return new Item_func_week(a, new Item_int((char*) "0", 3, 1));
}

Item *create_func_makedate(Item* a,Item* b)
{
  return new Item_func_makedate(a, b);
}

Item *create_func_addtime(Item* a,Item* b)
{
  return new Item_func_add_time(a, b, 0, 0);
}

Item *create_func_subtime(Item* a,Item* b)
{
  return new Item_func_add_time(a, b, 0, 1);
}

Item *create_func_timediff(Item* a,Item* b)
{
  return new Item_func_timediff(a, b);
}

Item *create_func_maketime(Item* a,Item* b,Item* c)
{
  return new Item_func_maketime(a, b, c);
}

Item *create_func_str_to_date(Item* a,Item* b)
{
  return new Item_func_str_to_date(a, b);
}

Item *create_func_last_day(Item *a)
{
  return new Item_func_last_day(a);
}
