/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"

/*
  Include all compiled character sets into the client
  If a client don't want to use all of them, he can define his own
  init_compiled_charsets() that only adds those that he wants
*/

my_bool init_compiled_charsets(myf flags __attribute__((unused)))
{
  CHARSET_INFO *cs;

  add_compiled_collation(&my_charset_bin);
  
  add_compiled_collation(&my_charset_latin1);
  add_compiled_collation(&my_charset_latin1_bin);
  add_compiled_collation(&my_charset_latin1_german2_ci);

#ifdef HAVE_CHARSET_big5
  add_compiled_collation(&my_charset_big5_chinese_ci);
  add_compiled_collation(&my_charset_big5_bin);
#endif

#ifdef HAVE_CHARSET_cp1250
  add_compiled_collation(&my_charset_cp1250_czech_ci);
#endif

#ifdef HAVE_CHARSET_latin2
  add_compiled_collation(&my_charset_latin2_czech_ci);
#endif

#ifdef HAVE_CHARSET_euckr
  add_compiled_collation(&my_charset_euckr_korean_ci);
  add_compiled_collation(&my_charset_euckr_bin);
#endif

#ifdef HAVE_CHARSET_gb2312
  add_compiled_collation(&my_charset_gb2312_chinese_ci);
  add_compiled_collation(&my_charset_gb2312_bin);
#endif

#ifdef HAVE_CHARSET_gbk
  add_compiled_collation(&my_charset_gbk_chinese_ci);
  add_compiled_collation(&my_charset_gbk_bin);
#endif

#ifdef HAVE_CHARSET_sjis
  add_compiled_collation(&my_charset_sjis_japanese_ci);
  add_compiled_collation(&my_charset_sjis_bin);
#endif

#ifdef HAVE_CHARSET_tis620
  add_compiled_collation(&my_charset_tis620_thai_ci);
  add_compiled_collation(&my_charset_tis620_bin);
#endif

#ifdef HAVE_CHARSET_ucs2
  add_compiled_collation(&my_charset_ucs2_general_ci);
  add_compiled_collation(&my_charset_ucs2_bin);
#endif

#ifdef HAVE_CHARSET_ujis
  add_compiled_collation(&my_charset_ujis_japanese_ci);
  add_compiled_collation(&my_charset_ujis_bin);
#endif

#ifdef HAVE_CHARSET_utf8
  add_compiled_collation(&my_charset_utf8_general_ci);
  add_compiled_collation(&my_charset_utf8_bin);
#endif

  /* Copy compiled charsets */
  for (cs=compiled_charsets; cs->name; cs++)
    add_compiled_collation(cs);
  
  return FALSE;
}
