/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "strings/collations_internal.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include "mysql/my_loglevel.h"
#include "mysql/strings/collations.h"
#include "mysql/strings/m_ctype.h"
#include "mysys_err.h"
#include "string_template_utils.h"
#include "strings/m_ctype_internals.h"
#include "strings/sql_chars.h"
#include "template_utils.h"

mysql::collation_internals::Collations *mysql::collation_internals::entry{};

int (*my_string_stack_guard)(int) = nullptr;

extern CHARSET_INFO compiled_charsets[];

/*
  These are all the hardcoded_charsets[]. All are entered into our hash maps:
  m_all_by_collation_name and m_all_by_id.

  In addition we have compiled_charsets[], which are compiled by the
  'conf_to_src' utility, and found in ctype-extra.cc
*/

extern CHARSET_INFO my_charset_latin1_bin;
extern CHARSET_INFO my_charset_latin1_german2_ci;
extern CHARSET_INFO my_charset_big5_chinese_ci;
extern CHARSET_INFO my_charset_big5_bin;
extern CHARSET_INFO my_charset_cp1250_czech_ci;
extern CHARSET_INFO my_charset_cp932_japanese_ci;
extern CHARSET_INFO my_charset_cp932_bin;
extern CHARSET_INFO my_charset_latin2_czech_ci;
extern CHARSET_INFO my_charset_eucjpms_japanese_ci;
extern CHARSET_INFO my_charset_eucjpms_bin;
extern CHARSET_INFO my_charset_euckr_korean_ci;
extern CHARSET_INFO my_charset_euckr_bin;
extern CHARSET_INFO my_charset_gb2312_chinese_ci;
extern CHARSET_INFO my_charset_gb2312_bin;
extern CHARSET_INFO my_charset_gbk_chinese_ci;
extern CHARSET_INFO my_charset_gbk_bin;
extern CHARSET_INFO my_charset_gb18030_chinese_ci;
extern CHARSET_INFO my_charset_gb18030_bin;
extern CHARSET_INFO my_charset_sjis_japanese_ci;
extern CHARSET_INFO my_charset_sjis_bin;
extern CHARSET_INFO my_charset_tis620_thai_ci;
extern CHARSET_INFO my_charset_tis620_bin;
extern CHARSET_INFO my_charset_ujis_japanese_ci;
extern CHARSET_INFO my_charset_ujis_bin;

extern CHARSET_INFO my_charset_ucs2_general_ci;
extern CHARSET_INFO my_charset_ucs2_unicode_ci;
extern CHARSET_INFO my_charset_ucs2_bin;
extern CHARSET_INFO my_charset_ucs2_general_mysql500_ci;
extern CHARSET_INFO my_charset_ucs2_german2_uca_ci;
extern CHARSET_INFO my_charset_ucs2_icelandic_uca_ci;
extern CHARSET_INFO my_charset_ucs2_latvian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_romanian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_slovenian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_polish_uca_ci;
extern CHARSET_INFO my_charset_ucs2_estonian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_spanish_uca_ci;
extern CHARSET_INFO my_charset_ucs2_swedish_uca_ci;
extern CHARSET_INFO my_charset_ucs2_turkish_uca_ci;
extern CHARSET_INFO my_charset_ucs2_czech_uca_ci;
extern CHARSET_INFO my_charset_ucs2_danish_uca_ci;
extern CHARSET_INFO my_charset_ucs2_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_slovak_uca_ci;
extern CHARSET_INFO my_charset_ucs2_spanish2_uca_ci;
extern CHARSET_INFO my_charset_ucs2_roman_uca_ci;
extern CHARSET_INFO my_charset_ucs2_persian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_esperanto_uca_ci;
extern CHARSET_INFO my_charset_ucs2_hungarian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_croatian_uca_ci;
extern CHARSET_INFO my_charset_ucs2_sinhala_uca_ci;
extern CHARSET_INFO my_charset_ucs2_unicode_520_ci;
extern CHARSET_INFO my_charset_ucs2_vietnamese_ci;

extern CHARSET_INFO my_charset_utf32_general_ci;
extern CHARSET_INFO my_charset_utf32_bin;
extern CHARSET_INFO my_charset_utf32_german2_uca_ci;
extern CHARSET_INFO my_charset_utf32_icelandic_uca_ci;
extern CHARSET_INFO my_charset_utf32_latvian_uca_ci;
extern CHARSET_INFO my_charset_utf32_romanian_uca_ci;
extern CHARSET_INFO my_charset_utf32_slovenian_uca_ci;
extern CHARSET_INFO my_charset_utf32_polish_uca_ci;
extern CHARSET_INFO my_charset_utf32_estonian_uca_ci;
extern CHARSET_INFO my_charset_utf32_spanish_uca_ci;
extern CHARSET_INFO my_charset_utf32_swedish_uca_ci;
extern CHARSET_INFO my_charset_utf32_turkish_uca_ci;
extern CHARSET_INFO my_charset_utf32_czech_uca_ci;
extern CHARSET_INFO my_charset_utf32_danish_uca_ci;
extern CHARSET_INFO my_charset_utf32_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_utf32_slovak_uca_ci;
extern CHARSET_INFO my_charset_utf32_spanish2_uca_ci;
extern CHARSET_INFO my_charset_utf32_roman_uca_ci;
extern CHARSET_INFO my_charset_utf32_persian_uca_ci;
extern CHARSET_INFO my_charset_utf32_esperanto_uca_ci;
extern CHARSET_INFO my_charset_utf32_hungarian_uca_ci;
extern CHARSET_INFO my_charset_utf32_croatian_uca_ci;
extern CHARSET_INFO my_charset_utf32_sinhala_uca_ci;
extern CHARSET_INFO my_charset_utf32_unicode_520_ci;
extern CHARSET_INFO my_charset_utf32_vietnamese_ci;

extern CHARSET_INFO my_charset_utf16_general_ci;
extern CHARSET_INFO my_charset_utf16_unicode_ci;
extern CHARSET_INFO my_charset_utf16_bin;
extern CHARSET_INFO my_charset_utf16le_general_ci;
extern CHARSET_INFO my_charset_utf16le_bin;
extern CHARSET_INFO my_charset_utf16_german2_uca_ci;
extern CHARSET_INFO my_charset_utf16_icelandic_uca_ci;
extern CHARSET_INFO my_charset_utf16_latvian_uca_ci;
extern CHARSET_INFO my_charset_utf16_romanian_uca_ci;
extern CHARSET_INFO my_charset_utf16_slovenian_uca_ci;
extern CHARSET_INFO my_charset_utf16_polish_uca_ci;
extern CHARSET_INFO my_charset_utf16_estonian_uca_ci;
extern CHARSET_INFO my_charset_utf16_spanish_uca_ci;
extern CHARSET_INFO my_charset_utf16_swedish_uca_ci;
extern CHARSET_INFO my_charset_utf16_turkish_uca_ci;
extern CHARSET_INFO my_charset_utf16_czech_uca_ci;
extern CHARSET_INFO my_charset_utf16_danish_uca_ci;
extern CHARSET_INFO my_charset_utf16_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_utf16_slovak_uca_ci;
extern CHARSET_INFO my_charset_utf16_spanish2_uca_ci;
extern CHARSET_INFO my_charset_utf16_roman_uca_ci;
extern CHARSET_INFO my_charset_utf16_persian_uca_ci;
extern CHARSET_INFO my_charset_utf16_esperanto_uca_ci;
extern CHARSET_INFO my_charset_utf16_hungarian_uca_ci;
extern CHARSET_INFO my_charset_utf16_croatian_uca_ci;
extern CHARSET_INFO my_charset_utf16_sinhala_uca_ci;
extern CHARSET_INFO my_charset_utf16_unicode_520_ci;
extern CHARSET_INFO my_charset_utf16_vietnamese_ci;

extern CHARSET_INFO my_charset_utf8mb3_tolower_ci;
extern CHARSET_INFO my_charset_utf8mb3_bin;
extern CHARSET_INFO my_charset_utf8mb3_general_mysql500_ci;
extern CHARSET_INFO my_charset_utf8mb3_german2_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_icelandic_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_latvian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_romanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_slovenian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_polish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_estonian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_spanish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_swedish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_turkish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_czech_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_danish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_slovak_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_spanish2_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_roman_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_persian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_esperanto_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_hungarian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_croatian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_sinhala_uca_ci;
extern CHARSET_INFO my_charset_utf8mb3_unicode_520_ci;
extern CHARSET_INFO my_charset_utf8mb3_vietnamese_ci;

extern CHARSET_INFO my_charset_utf8mb4_general_ci;
extern CHARSET_INFO my_charset_utf8mb4_unicode_ci;
extern CHARSET_INFO my_charset_utf8mb4_bin;
extern CHARSET_INFO my_charset_utf8mb4_german2_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_icelandic_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_latvian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_romanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_slovenian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_polish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_estonian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_spanish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_swedish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_turkish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_czech_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_danish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_slovak_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_spanish2_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_roman_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_persian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_esperanto_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_hungarian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_croatian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_sinhala_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_unicode_520_ci;
extern CHARSET_INFO my_charset_utf8mb4_vietnamese_ci;
extern CHARSET_INFO my_charset_utf8mb4_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_de_pb_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_is_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_lv_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_ro_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_sl_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_pl_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_et_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_es_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_sv_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_tr_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_cs_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_da_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_lt_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_sk_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_es_trad_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_la_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_eo_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_hu_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_hr_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_vi_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_ru_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_de_pb_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_is_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_lv_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_ro_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_sl_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_pl_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_et_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_es_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_sv_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_tr_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_cs_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_da_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_lt_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_sk_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_es_trad_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_la_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_eo_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_hu_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_hr_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_vi_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_ja_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_ja_0900_as_cs_ks;
extern CHARSET_INFO my_charset_utf8mb4_0900_as_ci;
extern CHARSET_INFO my_charset_utf8mb4_ru_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_zh_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_0900_bin;

extern CHARSET_INFO my_charset_utf8mb4_nb_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_nb_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_nn_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_nn_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_sr_latn_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_sr_latn_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_bs_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_bs_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_bg_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_bg_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_gl_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_gl_0900_as_cs;
extern CHARSET_INFO my_charset_utf8mb4_mn_cyrl_0900_ai_ci;
extern CHARSET_INFO my_charset_utf8mb4_mn_cyrl_0900_as_cs;

extern CHARSET_INFO my_charset_gb18030_unicode_520_ci;

static CHARSET_INFO *hardcoded_charsets[] = {
    &my_charset_bin,

    &my_charset_latin1,
    &my_charset_latin1_bin,
    &my_charset_latin1_german2_ci,

    &my_charset_big5_chinese_ci,
    &my_charset_big5_bin,

    &my_charset_cp1250_czech_ci,

    &my_charset_cp932_japanese_ci,
    &my_charset_cp932_bin,

    &my_charset_latin2_czech_ci,

    &my_charset_eucjpms_japanese_ci,
    &my_charset_eucjpms_bin,

    &my_charset_euckr_korean_ci,
    &my_charset_euckr_bin,

    &my_charset_gb2312_chinese_ci,
    &my_charset_gb2312_bin,

    &my_charset_gbk_chinese_ci,
    &my_charset_gbk_bin,

    &my_charset_gb18030_unicode_520_ci,
    &my_charset_gb18030_chinese_ci,
    &my_charset_gb18030_bin,

    &my_charset_sjis_japanese_ci,
    &my_charset_sjis_bin,

    &my_charset_tis620_thai_ci,
    &my_charset_tis620_bin,

    &my_charset_ucs2_general_ci,
    &my_charset_ucs2_bin,
    &my_charset_ucs2_general_mysql500_ci,
    &my_charset_ucs2_unicode_ci,
    &my_charset_ucs2_german2_uca_ci,
    &my_charset_ucs2_icelandic_uca_ci,
    &my_charset_ucs2_latvian_uca_ci,
    &my_charset_ucs2_romanian_uca_ci,
    &my_charset_ucs2_slovenian_uca_ci,
    &my_charset_ucs2_polish_uca_ci,
    &my_charset_ucs2_estonian_uca_ci,
    &my_charset_ucs2_spanish_uca_ci,
    &my_charset_ucs2_swedish_uca_ci,
    &my_charset_ucs2_turkish_uca_ci,
    &my_charset_ucs2_czech_uca_ci,
    &my_charset_ucs2_danish_uca_ci,
    &my_charset_ucs2_lithuanian_uca_ci,
    &my_charset_ucs2_slovak_uca_ci,
    &my_charset_ucs2_spanish2_uca_ci,
    &my_charset_ucs2_roman_uca_ci,
    &my_charset_ucs2_persian_uca_ci,
    &my_charset_ucs2_esperanto_uca_ci,
    &my_charset_ucs2_hungarian_uca_ci,
    &my_charset_ucs2_croatian_uca_ci,
    &my_charset_ucs2_sinhala_uca_ci,
    &my_charset_ucs2_unicode_520_ci,
    &my_charset_ucs2_vietnamese_ci,

    &my_charset_ujis_japanese_ci,
    &my_charset_ujis_bin,

    &my_charset_utf8mb3_general_ci,
    &my_charset_utf8mb3_tolower_ci,
    &my_charset_utf8mb3_bin,
    &my_charset_utf8mb3_general_mysql500_ci,
    &my_charset_utf8mb3_unicode_ci,
    &my_charset_utf8mb3_german2_uca_ci,
    &my_charset_utf8mb3_icelandic_uca_ci,
    &my_charset_utf8mb3_latvian_uca_ci,
    &my_charset_utf8mb3_romanian_uca_ci,
    &my_charset_utf8mb3_slovenian_uca_ci,
    &my_charset_utf8mb3_polish_uca_ci,
    &my_charset_utf8mb3_estonian_uca_ci,
    &my_charset_utf8mb3_spanish_uca_ci,
    &my_charset_utf8mb3_swedish_uca_ci,
    &my_charset_utf8mb3_turkish_uca_ci,
    &my_charset_utf8mb3_czech_uca_ci,
    &my_charset_utf8mb3_danish_uca_ci,
    &my_charset_utf8mb3_lithuanian_uca_ci,
    &my_charset_utf8mb3_slovak_uca_ci,
    &my_charset_utf8mb3_spanish2_uca_ci,
    &my_charset_utf8mb3_roman_uca_ci,
    &my_charset_utf8mb3_persian_uca_ci,
    &my_charset_utf8mb3_esperanto_uca_ci,
    &my_charset_utf8mb3_hungarian_uca_ci,
    &my_charset_utf8mb3_croatian_uca_ci,
    &my_charset_utf8mb3_sinhala_uca_ci,
    &my_charset_utf8mb3_unicode_520_ci,
    &my_charset_utf8mb3_vietnamese_ci,

    &my_charset_utf8mb4_0900_bin,
    &my_charset_utf8mb4_bin,

    &my_charset_utf8mb4_general_ci,
    &my_charset_utf8mb4_unicode_ci,
    &my_charset_utf8mb4_german2_uca_ci,
    &my_charset_utf8mb4_icelandic_uca_ci,
    &my_charset_utf8mb4_latvian_uca_ci,
    &my_charset_utf8mb4_romanian_uca_ci,
    &my_charset_utf8mb4_slovenian_uca_ci,
    &my_charset_utf8mb4_polish_uca_ci,
    &my_charset_utf8mb4_estonian_uca_ci,
    &my_charset_utf8mb4_spanish_uca_ci,
    &my_charset_utf8mb4_swedish_uca_ci,
    &my_charset_utf8mb4_turkish_uca_ci,
    &my_charset_utf8mb4_czech_uca_ci,
    &my_charset_utf8mb4_danish_uca_ci,
    &my_charset_utf8mb4_lithuanian_uca_ci,
    &my_charset_utf8mb4_slovak_uca_ci,
    &my_charset_utf8mb4_spanish2_uca_ci,
    &my_charset_utf8mb4_roman_uca_ci,
    &my_charset_utf8mb4_persian_uca_ci,
    &my_charset_utf8mb4_esperanto_uca_ci,
    &my_charset_utf8mb4_hungarian_uca_ci,
    &my_charset_utf8mb4_croatian_uca_ci,
    &my_charset_utf8mb4_sinhala_uca_ci,
    &my_charset_utf8mb4_unicode_520_ci,
    &my_charset_utf8mb4_vietnamese_ci,
    &my_charset_utf8mb4_0900_ai_ci,
    &my_charset_utf8mb4_de_pb_0900_ai_ci,
    &my_charset_utf8mb4_is_0900_ai_ci,
    &my_charset_utf8mb4_lv_0900_ai_ci,
    &my_charset_utf8mb4_ro_0900_ai_ci,
    &my_charset_utf8mb4_sl_0900_ai_ci,
    &my_charset_utf8mb4_pl_0900_ai_ci,
    &my_charset_utf8mb4_et_0900_ai_ci,
    &my_charset_utf8mb4_es_0900_ai_ci,
    &my_charset_utf8mb4_sv_0900_ai_ci,
    &my_charset_utf8mb4_tr_0900_ai_ci,
    &my_charset_utf8mb4_cs_0900_ai_ci,
    &my_charset_utf8mb4_da_0900_ai_ci,
    &my_charset_utf8mb4_lt_0900_ai_ci,
    &my_charset_utf8mb4_sk_0900_ai_ci,
    &my_charset_utf8mb4_es_trad_0900_ai_ci,
    &my_charset_utf8mb4_la_0900_ai_ci,
    &my_charset_utf8mb4_eo_0900_ai_ci,
    &my_charset_utf8mb4_hu_0900_ai_ci,
    &my_charset_utf8mb4_hr_0900_ai_ci,
    &my_charset_utf8mb4_vi_0900_ai_ci,
    &my_charset_utf8mb4_ru_0900_ai_ci,
    &my_charset_utf8mb4_0900_as_cs,
    &my_charset_utf8mb4_de_pb_0900_as_cs,
    &my_charset_utf8mb4_is_0900_as_cs,
    &my_charset_utf8mb4_lv_0900_as_cs,
    &my_charset_utf8mb4_ro_0900_as_cs,
    &my_charset_utf8mb4_sl_0900_as_cs,
    &my_charset_utf8mb4_pl_0900_as_cs,
    &my_charset_utf8mb4_et_0900_as_cs,
    &my_charset_utf8mb4_es_0900_as_cs,
    &my_charset_utf8mb4_sv_0900_as_cs,
    &my_charset_utf8mb4_tr_0900_as_cs,
    &my_charset_utf8mb4_cs_0900_as_cs,
    &my_charset_utf8mb4_da_0900_as_cs,
    &my_charset_utf8mb4_lt_0900_as_cs,
    &my_charset_utf8mb4_sk_0900_as_cs,
    &my_charset_utf8mb4_es_trad_0900_as_cs,
    &my_charset_utf8mb4_la_0900_as_cs,
    &my_charset_utf8mb4_eo_0900_as_cs,
    &my_charset_utf8mb4_hu_0900_as_cs,
    &my_charset_utf8mb4_hr_0900_as_cs,
    &my_charset_utf8mb4_vi_0900_as_cs,
    &my_charset_utf8mb4_ja_0900_as_cs,
    &my_charset_utf8mb4_ja_0900_as_cs_ks,
    &my_charset_utf8mb4_0900_as_ci,
    &my_charset_utf8mb4_ru_0900_as_cs,
    &my_charset_utf8mb4_zh_0900_as_cs,

    &my_charset_utf8mb4_nb_0900_ai_ci,
    &my_charset_utf8mb4_nb_0900_as_cs,
    &my_charset_utf8mb4_nn_0900_ai_ci,
    &my_charset_utf8mb4_nn_0900_as_cs,
    &my_charset_utf8mb4_sr_latn_0900_ai_ci,
    &my_charset_utf8mb4_sr_latn_0900_as_cs,
    &my_charset_utf8mb4_bs_0900_ai_ci,
    &my_charset_utf8mb4_bs_0900_as_cs,
    &my_charset_utf8mb4_bg_0900_ai_ci,
    &my_charset_utf8mb4_bg_0900_as_cs,
    &my_charset_utf8mb4_gl_0900_ai_ci,
    &my_charset_utf8mb4_gl_0900_as_cs,
    &my_charset_utf8mb4_mn_cyrl_0900_ai_ci,
    &my_charset_utf8mb4_mn_cyrl_0900_as_cs,

    &my_charset_utf16_general_ci,
    &my_charset_utf16_bin,
    &my_charset_utf16le_general_ci,
    &my_charset_utf16le_bin,
    &my_charset_utf16_unicode_ci,
    &my_charset_utf16_german2_uca_ci,
    &my_charset_utf16_icelandic_uca_ci,
    &my_charset_utf16_latvian_uca_ci,
    &my_charset_utf16_romanian_uca_ci,
    &my_charset_utf16_slovenian_uca_ci,
    &my_charset_utf16_polish_uca_ci,
    &my_charset_utf16_estonian_uca_ci,
    &my_charset_utf16_spanish_uca_ci,
    &my_charset_utf16_swedish_uca_ci,
    &my_charset_utf16_turkish_uca_ci,
    &my_charset_utf16_czech_uca_ci,
    &my_charset_utf16_danish_uca_ci,
    &my_charset_utf16_lithuanian_uca_ci,
    &my_charset_utf16_slovak_uca_ci,
    &my_charset_utf16_spanish2_uca_ci,
    &my_charset_utf16_roman_uca_ci,
    &my_charset_utf16_persian_uca_ci,
    &my_charset_utf16_esperanto_uca_ci,
    &my_charset_utf16_hungarian_uca_ci,
    &my_charset_utf16_croatian_uca_ci,
    &my_charset_utf16_sinhala_uca_ci,
    &my_charset_utf16_unicode_520_ci,
    &my_charset_utf16_vietnamese_ci,

    &my_charset_utf32_general_ci,
    &my_charset_utf32_bin,
    &my_charset_utf32_unicode_ci,
    &my_charset_utf32_german2_uca_ci,
    &my_charset_utf32_icelandic_uca_ci,
    &my_charset_utf32_latvian_uca_ci,
    &my_charset_utf32_romanian_uca_ci,
    &my_charset_utf32_slovenian_uca_ci,
    &my_charset_utf32_polish_uca_ci,
    &my_charset_utf32_estonian_uca_ci,
    &my_charset_utf32_spanish_uca_ci,
    &my_charset_utf32_swedish_uca_ci,
    &my_charset_utf32_turkish_uca_ci,
    &my_charset_utf32_czech_uca_ci,
    &my_charset_utf32_danish_uca_ci,
    &my_charset_utf32_lithuanian_uca_ci,
    &my_charset_utf32_slovak_uca_ci,
    &my_charset_utf32_spanish2_uca_ci,
    &my_charset_utf32_roman_uca_ci,
    &my_charset_utf32_persian_uca_ci,
    &my_charset_utf32_esperanto_uca_ci,
    &my_charset_utf32_hungarian_uca_ci,
    &my_charset_utf32_croatian_uca_ci,
    &my_charset_utf32_sinhala_uca_ci,
    &my_charset_utf32_unicode_520_ci,
    &my_charset_utf32_vietnamese_ci,
};

namespace {

class Charset_loader : public MY_CHARSET_LOADER {
 public:
  Charset_loader(const Charset_loader &) = delete;
  Charset_loader &operator=(const Charset_loader &) = delete;

  Charset_loader() = default;

  void reporter(loglevel, unsigned /* errcode */, ...) override {
    assert(false);
  }

  void *read_file(const char *, size_t *) override { return nullptr; }
};

template <typename Key>
using Hash = std::unordered_map<Key, CHARSET_INFO *>;

template <size_t N>
bool starts_with(std::string name, const char (&prefix)[N]) {
  size_t len = N - 1;
  return name.size() >= len && memcmp(name.data(), prefix, len) == 0;
}

std::string alternative_collation_name(std::string name) {
  // get_collation_name_alias()
  // We still need to support aliasing both ways.
  if (starts_with(name, "utf8mb3_")) {
    auto buf = name;
    buf.erase(4, 3);  // remove "mb3" from "utf8mb3_"
    return buf;
  }
  if (starts_with(name, "utf8_")) {
    auto buf = name;
    buf.insert(4, "mb3");  // insert "mb3" to get "utf8mb3_xxxx"
    return buf;
  }
  return name;
}

template <typename Key>
CHARSET_INFO *find_in_hash(const Hash<Key> &hash, Key key) {
  auto it = hash.find(key);
  return it == hash.end() ? nullptr : it->second;
}

CHARSET_INFO *find_collation_in_hash(const Hash<std::string> &hash,
                                     const std::string &key) {
  CHARSET_INFO *cs = find_in_hash(hash, key);
  if (cs != nullptr) {
    return cs;
  }
  auto alternative = alternative_collation_name(key);
  return alternative == key ? nullptr : find_in_hash(hash, alternative);
}

CHARSET_INFO *find_cs_in_hash(const Hash<std::string> &hash,
                              const mysql::collation::Name &key) {
  auto it = hash.find(key());
  return it == hash.end() ? nullptr : it->second;
}

template <typename Key>
bool add_to_hash(Hash<Key> *hash, Key key, CHARSET_INFO *cs) {
  //  return !hash->insert({key, cs}).second;
  (*hash)[key] = cs;
  return false;
}

}  // namespace

static bool my_read_charset_file(MY_CHARSET_LOADER *loader,
                                 const char *filename) {
  size_t len = 0;
  auto deleter = [](void *p) { free(p); };
  std::unique_ptr<void, decltype(deleter)> buf{
      loader->read_file(filename, &len), deleter};
  if (buf == nullptr) {
    return true;
  }

  MY_CHARSET_ERRMSG errmsg;
  if (my_parse_charset_xml(loader, pointer_cast<const char *>(buf.get()), len,
                           &errmsg)) {
    char buff[1024];
    snprintf(buff, sizeof(buff), "Error while parsing %s: %s\n", filename,
             errmsg.errarg);
    loader->reporter(ERROR_LEVEL, EE_COLLATION_PARSER_ERROR, buff);
    return true;
  }

  return false;
}

namespace mysql {
namespace collation_internals {

Collations::Collations(const char *charset_dir, MY_CHARSET_LOADER *loader)
    : m_charset_dir{charset_dir ? charset_dir : ""},
      m_owns_loader{loader == nullptr},
      m_loader{m_owns_loader ? new Charset_loader : loader} {
  for (CHARSET_INFO *cs = compiled_charsets; cs->m_coll_name; cs++) {
    if (add_internal_collation(cs)) assert(false);
    cs->state |= MY_CS_AVAILABLE;
  }
  for (CHARSET_INFO *cs : hardcoded_charsets) {
    if (add_internal_collation(cs)) assert(false);
    cs->state |= MY_CS_AVAILABLE | MY_CS_INLINE;
  }
  for (const auto &i : m_all_by_collation_name) {
    CHARSET_INFO *cs = i.second;
    if (cs->ctype && is_supported_parser_charset(cs) &&
        init_state_maps(m_loader, cs))
      throw std::bad_alloc();
  }
  mysql::collation_internals::entry = this;
  if (charset_dir) {
    my_read_charset_file(m_loader,
                         concatenate(charset_dir, MY_CHARSET_INDEX).c_str());
  }
}

Collations::~Collations() {
  for (auto p : m_all_by_id) {
    CHARSET_INFO *cs = p.second;
    if (cs->coll && cs->coll->uninit) {
      cs->coll->uninit(cs, m_loader);
    }
  }
  if (m_owns_loader) {
    delete m_loader;
  }
}

CHARSET_INFO *Collations::find_by_name(const mysql::collation::Name &name,
                                       myf flags, MY_CHARSET_ERRMSG *errmsg) {
  return safe_init_when_necessary(
      find_collation_in_hash(m_all_by_collation_name, name()), flags, errmsg);
}

CHARSET_INFO *Collations::find_by_id(unsigned id, myf flags,
                                     MY_CHARSET_ERRMSG *errmsg) {
  return safe_init_when_necessary(find_in_hash(m_all_by_id, id), flags, errmsg);
}

CHARSET_INFO *Collations::find_primary(const mysql::collation::Name &cs_name,
                                       myf flags, MY_CHARSET_ERRMSG *errmsg) {
  return safe_init_when_necessary(
      find_cs_in_hash(m_primary_by_cs_name, cs_name), flags, errmsg);
}

CHARSET_INFO *Collations::find_default_binary(
    const mysql::collation::Name &cs_name, myf flags,
    MY_CHARSET_ERRMSG *errmsg) {
  return safe_init_when_necessary(find_cs_in_hash(m_binary_by_cs_name, cs_name),
                                  flags, errmsg);
}

unsigned Collations::get_collation_id(
    const mysql::collation::Name &name) const {
  CHARSET_INFO *cs = find_collation_in_hash(m_all_by_collation_name, name());
  return cs ? cs->number : 0;
}

unsigned Collations::get_primary_collation_id(
    const mysql::collation::Name &name) const {
  CHARSET_INFO *cs = find_cs_in_hash(m_primary_by_cs_name, name);
  return cs ? cs->number : 0;
}

unsigned Collations::get_default_binary_collation_id(
    const mysql::collation::Name &name) const {
  CHARSET_INFO *cs = find_cs_in_hash(m_binary_by_cs_name, name);
  return cs ? cs->number : 0;
}

CHARSET_INFO *Collations::safe_init_when_necessary(CHARSET_INFO *cs, myf flags,
                                                   MY_CHARSET_ERRMSG *errmsg) {
  if (cs == nullptr || cs->state & MY_CS_READY) {
    return cs;
  }
  std::lock_guard<std::mutex> guard{m_mutex};
  if (cs->state & MY_CS_READY) {
    return cs;
  }
  if (errmsg != nullptr) {
    return unsafe_init(cs, flags, errmsg);
  }
  MY_CHARSET_ERRMSG dummy;
  return unsafe_init(cs, flags, &dummy);
}

CHARSET_INFO *Collations::unsafe_init(CHARSET_INFO *cs,
                                      myf flags [[maybe_unused]],
                                      MY_CHARSET_ERRMSG *errmsg) {
  // TODO(tdidriks) something is bad with init/uninit/init here
  assert(!(cs->state & MY_CS_READY));
  if (!m_charset_dir.empty() &&
      !(cs->state & (MY_CS_COMPILED | MY_CS_LOADED))) {  // CS is not in memory
    std::string filename = concatenate(m_charset_dir, cs->csname, ".xml");
    my_read_charset_file(m_loader, filename.c_str());
  }
  if (!(cs->state & MY_CS_AVAILABLE)) {
    return nullptr;
  }
  if ((cs->cset->init && cs->cset->init(cs, m_loader, errmsg)) ||
      (cs->coll->init && cs->coll->init(cs, m_loader, errmsg))) {
    // TODO(gleb): cs->state &= ~MY_CS_AVAILABLE;
    return nullptr;
  }
  cs->state |= MY_CS_READY;
  return cs;
}

bool Collations::add_internal_collation(CHARSET_INFO *cs) {
  assert(cs->number != 0);

  std::string normalized_name{mysql::collation::Name{cs->m_coll_name}()};

  if (add_to_hash(&m_all_by_collation_name, normalized_name, cs) ||
      add_to_hash(&m_all_by_id, cs->number, cs)) {
    return true;
  }
  if ((cs->state & MY_CS_PRIMARY) &&
      add_to_hash(&m_primary_by_cs_name, std::string{cs->csname}, cs))
    return true;
  // utf8mb4 is the only character set with more than two binary collations.
  // For backward compatibility, we want the deprecated BINARY type attribute
  // to use utf8mb4_bin, and not the newer utf8mb4_0900_bin collation, for the
  // utf8mb4 character set. That is, the following column definition should
  // result in a column with utf8mb4_bin collation:
  //
  //    col_name VARCHAR(10) CHARSET utf8mb4 BINARY
  //
  // Thus, we don't add utf8mb4_0900_bin to make my_charset_utf8mb4_bin the
  // preferred binary collation of utf8mb4.
  if ((cs->state & MY_CS_BINSORT) && cs != &my_charset_utf8mb4_0900_bin &&
      add_to_hash(&m_binary_by_cs_name, std::string{cs->csname}, cs))
    return true;
  return false;
}

CHARSET_INFO *Collations::find_by_name_unsafe(
    const mysql::collation::Name &name) {
  return find_collation_in_hash(m_all_by_collation_name, name());
}

}  // namespace collation_internals
}  // namespace mysql
