/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef STRINGS_M_CTYPE_INTERNALS_H_
#define STRINGS_M_CTYPE_INTERNALS_H_

#include "mysql/strings/m_ctype.h"

constexpr int MY_CS_CTYPE_TABLE_SIZE = 257;
constexpr int MY_CS_TO_LOWER_TABLE_SIZE = 256;
constexpr int MY_CS_TO_UPPER_TABLE_SIZE = 256;
constexpr int MY_CS_SORT_ORDER_TABLE_SIZE = 256;
constexpr int MY_CS_TO_UNI_TABLE_SIZE = 256;

constexpr my_wc_t MY_CS_REPLACEMENT_CHARACTER = 0xFFFD;

extern MY_UNICASE_INFO my_unicase_default;
extern MY_UNICASE_INFO my_unicase_turkish;
extern MY_UNICASE_INFO my_unicase_mysql500;
extern MY_UNICASE_INFO my_unicase_unicode520;

extern MY_UNI_CTYPE my_uni_ctype[256];

extern MY_COLLATION_HANDLER my_collation_mb_bin_handler;
extern MY_COLLATION_HANDLER my_collation_8bit_bin_handler;
extern MY_COLLATION_HANDLER my_collation_8bit_simple_ci_handler;
extern MY_COLLATION_HANDLER my_collation_ucs2_uca_handler;

extern MY_CHARSET_HANDLER my_charset_8bit_handler;
extern MY_CHARSET_HANDLER my_charset_ascii_handler;
extern MY_CHARSET_HANDLER my_charset_ucs2_handler;

/* declarations for simple charsets */
extern size_t my_strnxfrm_simple(const CHARSET_INFO *, uint8_t *dst,
                                 size_t dstlen, unsigned nweights,
                                 const uint8_t *src, size_t srclen,
                                 unsigned flags);
size_t my_strnxfrmlen_simple(const CHARSET_INFO *, size_t);
extern int my_strnncoll_simple(const CHARSET_INFO *, const uint8_t *, size_t,
                               const uint8_t *, size_t, bool);

extern int my_strnncollsp_simple(const CHARSET_INFO *, const uint8_t *, size_t,
                                 const uint8_t *, size_t);

extern void my_hash_sort_simple(const CHARSET_INFO *cs, const uint8_t *key,
                                size_t len, uint64_t *nr1, uint64_t *nr2);

extern size_t my_lengthsp_8bit(const CHARSET_INFO *cs, const char *ptr,
                               size_t length);

extern unsigned my_instr_simple(const CHARSET_INFO *, const char *b,
                                size_t b_length, const char *s, size_t s_length,
                                my_match_t *match, unsigned nmatch);

/* Functions for 8bit */
extern size_t my_caseup_str_8bit(const CHARSET_INFO *, char *);
extern size_t my_casedn_str_8bit(const CHARSET_INFO *, char *);
extern size_t my_caseup_8bit(const CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);
extern size_t my_casedn_8bit(const CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);

extern int my_strcasecmp_8bit(const CHARSET_INFO *cs, const char *,
                              const char *);

int my_mb_wc_8bit(const CHARSET_INFO *cs, my_wc_t *wc, const uint8_t *s,
                  const uint8_t *e);
int my_wc_mb_8bit(const CHARSET_INFO *cs, my_wc_t wc, uint8_t *s, uint8_t *e);

int my_mb_ctype_8bit(const CHARSET_INFO *, int *, const uint8_t *,
                     const uint8_t *);
int my_mb_ctype_mb(const CHARSET_INFO *, int *, const uint8_t *,
                   const uint8_t *);

size_t my_scan_8bit(const CHARSET_INFO *cs, const char *b, const char *e,
                    int sq);

size_t my_snprintf_8bit(const CHARSET_INFO *, char *to, size_t n,
                        const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 4, 5)));

long my_strntol_8bit(const CHARSET_INFO *, const char *s, size_t l, int base,
                     const char **e, int *err);
unsigned long my_strntoul_8bit(const CHARSET_INFO *, const char *nptr, size_t l,
                               int base, const char **endptr, int *err);
long long my_strntoll_8bit(const CHARSET_INFO *cs, const char *nptr, size_t l,
                           int base, const char **endptr, int *err);
unsigned long long my_strntoull_8bit(const CHARSET_INFO *cs, const char *nptr,
                                     size_t l, int base, const char **endptr,
                                     int *err);
double my_strntod_8bit(const CHARSET_INFO *, const char *s, size_t l,
                       const char **e, int *err);
size_t my_long10_to_str_8bit(const CHARSET_INFO *, char *to, size_t l,
                             int radix, long int val);
size_t my_longlong10_to_str_8bit(const CHARSET_INFO *, char *to, size_t l,
                                 int radix, long long val);

long long my_strtoll10_8bit(const CHARSET_INFO *cs, const char *nptr,
                            const char **endptr, int *error);
long long my_strtoll10_ucs2(const CHARSET_INFO *cs, const char *nptr,
                            char **endptr, int *error);

unsigned long long my_strntoull10rnd_8bit(const CHARSET_INFO *cs,
                                          const char *str, size_t length,
                                          int unsigned_fl, const char **endptr,
                                          int *error);
unsigned long long my_strntoull10rnd_ucs2(const CHARSET_INFO *cs,
                                          const char *str, size_t length,
                                          int unsigned_fl, char **endptr,
                                          int *error);

void my_fill_8bit(const CHARSET_INFO *cs, char *to, size_t l, int fill);

/* For 8-bit character set */
bool my_like_range_simple(const CHARSET_INFO *cs, const char *ptr,
                          size_t ptr_length, char escape, char w_one,
                          char w_many, size_t res_length, char *min_str,
                          char *max_str, size_t *min_length,
                          size_t *max_length);

/* For ASCII-based multi-byte character sets with mbminlen=1 */
bool my_like_range_mb(const CHARSET_INFO *cs, const char *ptr,
                      size_t ptr_length, char escape, char w_one, char w_many,
                      size_t res_length, char *min_str, char *max_str,
                      size_t *min_length, size_t *max_length);

/* For other character sets, with arbitrary mbminlen and mbmaxlen numbers */
bool my_like_range_generic(const CHARSET_INFO *cs, const char *ptr,
                           size_t ptr_length, char escape, char w_one,
                           char w_many, size_t res_length, char *min_str,
                           char *max_str, size_t *min_length,
                           size_t *max_length);

int my_wildcmp_8bit(const CHARSET_INFO *, const char *str, const char *str_end,
                    const char *wildstr, const char *wildend, int escape,
                    int w_one, int w_many);

int my_wildcmp_bin(const CHARSET_INFO *, const char *str, const char *str_end,
                   const char *wildstr, const char *wildend, int escape,
                   int w_one, int w_many);

size_t my_numchars_8bit(const CHARSET_INFO *, const char *b, const char *e);
size_t my_numcells_8bit(const CHARSET_INFO *, const char *b, const char *e);
size_t my_charpos_8bit(const CHARSET_INFO *, const char *b, const char *e,
                       size_t pos);
size_t my_well_formed_len_8bit(const CHARSET_INFO *, const char *b,
                               const char *e, size_t pos, int *error);
unsigned my_mbcharlen_8bit(const CHARSET_INFO *, unsigned c);

/* Functions for multibyte charsets */
extern size_t my_caseup_str_mb(const CHARSET_INFO *, char *);
extern size_t my_casedn_str_mb(const CHARSET_INFO *, char *);
extern size_t my_caseup_mb(const CHARSET_INFO *, char *src, size_t srclen,
                           char *dst, size_t dstlen);
extern size_t my_casedn_mb(const CHARSET_INFO *, char *src, size_t srclen,
                           char *dst, size_t dstlen);
extern size_t my_caseup_mb_varlen(const CHARSET_INFO *, char *src,
                                  size_t srclen, char *dst, size_t dstlen);
extern size_t my_casedn_mb_varlen(const CHARSET_INFO *, char *src,
                                  size_t srclen, char *dst, size_t dstlen);
extern size_t my_caseup_ujis(const CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);
extern size_t my_casedn_ujis(const CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);
extern int my_strcasecmp_mb(const CHARSET_INFO *cs, const char *, const char *);

int my_wildcmp_mb(const CHARSET_INFO *, const char *str, const char *str_end,
                  const char *wildstr, const char *wildend, int escape,
                  int w_one, int w_many);
size_t my_numchars_mb(const CHARSET_INFO *, const char *b, const char *e);
size_t my_numcells_mb(const CHARSET_INFO *, const char *b, const char *e);
size_t my_charpos_mb3(const CHARSET_INFO *, const char *b, const char *e,
                      size_t pos);
size_t my_well_formed_len_mb(const CHARSET_INFO *, const char *b, const char *e,
                             size_t pos, int *error);
unsigned my_instr_mb(const CHARSET_INFO *, const char *b, size_t b_length,
                     const char *s, size_t s_length, my_match_t *match,
                     unsigned nmatch);

int my_strnncoll_mb_bin(const CHARSET_INFO *cs, const uint8_t *s, size_t slen,
                        const uint8_t *t, size_t tlen, bool t_is_prefix);

int my_strnncollsp_mb_bin(const CHARSET_INFO *cs, const uint8_t *a,
                          size_t a_length, const uint8_t *b, size_t b_length);

int my_strcasecmp_mb_bin(const CHARSET_INFO *, const char *s, const char *t);

void my_hash_sort_mb_bin(const CHARSET_INFO *, const uint8_t *key, size_t len,
                         uint64_t *nr1, uint64_t *nr2);

size_t my_strnxfrm_mb(const CHARSET_INFO *, uint8_t *dst, size_t dstlen,
                      unsigned nweights, const uint8_t *src, size_t srclen,
                      unsigned flags);

size_t my_strnxfrm_unicode(const CHARSET_INFO *, uint8_t *dst, size_t dstlen,
                           unsigned nweights, const uint8_t *src, size_t srclen,
                           unsigned flags);

size_t my_strnxfrm_unicode_full_bin(const CHARSET_INFO *, uint8_t *dst,
                                    size_t dstlen, unsigned nweights,
                                    const uint8_t *src, size_t srclen,
                                    unsigned flags);
size_t my_strnxfrmlen_unicode_full_bin(const CHARSET_INFO *, size_t);

int my_wildcmp_unicode(const CHARSET_INFO *cs, const char *str,
                       const char *str_end, const char *wildstr,
                       const char *wildend, int escape, int w_one, int w_many,
                       const MY_UNICASE_INFO *weights);

extern bool my_parse_charset_xml(MY_CHARSET_LOADER *loader, const char *buf,
                                 size_t buflen, MY_CHARSET_ERRMSG *errmsg);
bool my_propagate_simple(const CHARSET_INFO *cs, const uint8_t *str,
                         size_t len);
bool my_propagate_complex(const CHARSET_INFO *cs, const uint8_t *str,
                          size_t len);

bool my_charset_is_8bit_pure_ascii(const CHARSET_INFO *cs);

size_t my_strxfrm_pad(const CHARSET_INFO *cs, uint8_t *str, uint8_t *frmend,
                      uint8_t *strend, unsigned nweights, unsigned flags);

bool my_charset_is_ascii_compatible(const CHARSET_INFO *cs);

#endif  // STRINGS_M_CTYPE_INTERNALS_H_
