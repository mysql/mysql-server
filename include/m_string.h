/*
   Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _m_string_h
#define _m_string_h

#include "my_global.h"                          /* HAVE_* */

#include <string.h>

#define bfill please_use_memset_rather_than_bfill
#define bzero please_use_memset_rather_than_bzero
#define bmove please_use_memmove_rather_than_bmove
#define strmov please_use_my_stpcpy_or_my_stpmov_rather_than_strmov
#define strnmov please_use_my_stpncpy_or_my_stpnmov_rather_than_strnmov

#include "mysql/service_my_snprintf.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
  my_str_malloc(), my_str_realloc() and my_str_free() are assigned to
  implementations in strings/alloc.c, but can be overridden in
  the calling program.
 */
extern void *(*my_str_malloc)(size_t);
extern void *(*my_str_realloc)(void *, size_t);
extern void (*my_str_free)(void *);

/* Declared in int2str() */
extern char _dig_vec_upper[];
extern char _dig_vec_lower[];

	/* Prototypes for string functions */

extern	void bchange(uchar *dst,size_t old_len,const uchar *src,
		     size_t new_len,size_t tot_len);
extern	void strappend(char *s,size_t len,pchar fill);
extern	char *strend(const char *s);
extern  char *strcend(const char *, pchar);
extern	char *strfill(char * s,size_t len,pchar fill);
extern	char *strmake(char *dst,const char *src,size_t length);

extern	char *my_stpmov(char *dst,const char *src);
extern	char *my_stpnmov(char *dst, const char *src, size_t n);
extern	char *strcont(const char *src, const char *set);
extern	char *strxmov(char *dst, const char *src, ...);
extern	char *strxnmov(char *dst, size_t len, const char *src, ...);

/**
   Copy a string from src to dst until (and including) terminating null byte.

   @param dst   Destination
   @param src   Source

   @note src and dst cannot overlap.
         Use my_stpmov() if src and dst overlaps.

   @note Unsafe, consider using my_stpnpy() instead.

   @return pointer to terminating null byte.
*/
static inline char *my_stpcpy(char *dst, const char *src)
{
#if defined(HAVE_BUILTIN_STPCPY)
  return __builtin_stpcpy(dst, src);
#elif defined(HAVE_STPCPY)
  return stpcpy(dst, src);
#else
  /* Fallback to implementation supporting overlap. */
  return my_stpmov(dst, src);
#endif
}

/**
   Copy fixed-size string from src to dst.

   @param dst   Destination
   @param src   Source
   @param n     Maximum number of characters to copy.

   @note src and dst cannot overlap
         Use my_stpnmov() if src and dst overlaps.

   @return pointer to terminating null byte.
*/
static inline char *my_stpncpy(char *dst, const char *src, size_t n)
{
#if defined(HAVE_STPNCPY)
  return stpncpy(dst, src, n);
#else
  /* Fallback to implementation supporting overlap. */
  return my_stpnmov(dst, src, n);
#endif
}

static inline longlong my_strtoll(const char *nptr, char **endptr, int base)
{
#if defined _WIN32
  return _strtoi64(nptr, endptr, base);
#else
  return strtoll(nptr, endptr, base);
#endif
}

static inline ulonglong my_strtoull(const char *nptr, char **endptr, int base)
{
#if defined _WIN32
  return _strtoui64(nptr, endptr, base);
#else
  return strtoull(nptr, endptr, base);
#endif
}

static inline char *my_strtok_r(char *str, const char *delim, char **saveptr)
{
#if defined _WIN32
  return strtok_s(str, delim, saveptr);
#else
  return strtok_r(str, delim, saveptr);
#endif
}

/* native_ rather than my_ since my_strcasecmp already exists */
static inline int native_strcasecmp(const char *s1, const char *s2)
{
#if defined _WIN32
  return _stricmp(s1, s2);
#else
  return strcasecmp(s1, s2);
#endif
}

/* native_ rather than my_ for consistency with native_strcasecmp */
static inline int native_strncasecmp(const char *s1, const char *s2, size_t n)
{
#if defined _WIN32
  return _strnicmp(s1, s2, n);
#else
  return strncasecmp(s1, s2, n);
#endif
}

/* Prototypes of normal stringfunctions (with may ours) */
#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t n);
#endif

extern int is_prefix(const char *, const char *);

/* Conversion routines */
typedef enum {
  MY_GCVT_ARG_FLOAT,
  MY_GCVT_ARG_DOUBLE
} my_gcvt_arg_type;

double my_strtod(const char *str, char **end, int *error);
double my_atof(const char *nptr);
size_t my_fcvt(double x, int precision, char *to, my_bool *error);
size_t my_gcvt(double x, my_gcvt_arg_type type, int width, char *to,
               my_bool *error);

#define NOT_FIXED_DEC 31

/*
  The longest string my_fcvt can return is 311 + "precision" bytes.
  Here we assume that we never cal my_fcvt() with precision >= NOT_FIXED_DEC
  (+ 1 byte for the terminating '\0').
*/
#define FLOATING_POINT_BUFFER (311 + NOT_FIXED_DEC)

/*
  We want to use the 'e' format in some cases even if we have enough space
  for the 'f' one just to mimic sprintf("%.15g") behavior for large integers,
  and to improve it for numbers < 10^(-4).
  That is, for |x| < 1 we require |x| >= 10^(-15), and for |x| > 1 we require
  it to be integer and be <= 10^DBL_DIG for the 'f' format to be used.
  We don't lose precision, but make cases like "1e200" or "0.00001" look nicer.
*/
#define MAX_DECPT_FOR_F_FORMAT DBL_DIG

/*
  The maximum possible field width for my_gcvt() conversion.
  (DBL_DIG + 2) significant digits + sign + "." + ("e-NNN" or
  MAX_DECPT_FOR_F_FORMAT zeros for cases when |x|<1 and the 'f' format is used).
*/
#define MY_GCVT_MAX_FIELD_WIDTH (DBL_DIG + 4 + MY_MAX(5, MAX_DECPT_FOR_F_FORMAT)) \

extern char *llstr(longlong value,char *buff);
extern char *ullstr(longlong value,char *buff);

extern char *int2str(long val, char *dst, int radix, int upcase);
extern char *int10_to_str(long val,char *dst,int radix);
extern char *str2int(const char *src,int radix,long lower,long upper,
			 long *val);
longlong my_strtoll10(const char *nptr, char **endptr, int *error);
#if SIZEOF_LONG == SIZEOF_LONG_LONG
#define ll2str(A,B,C,D) int2str((A),(B),(C),(D))
#define longlong10_to_str(A,B,C) int10_to_str((A),(B),(C))
#undef strtoll
#define strtoll(A,B,C) strtol((A),(B),(C))
#define strtoull(A,B,C) strtoul((A),(B),(C))
#else
extern char *ll2str(longlong val,char *dst,int radix, int upcase);
extern char *longlong10_to_str(longlong val,char *dst,int radix);
#endif
#define longlong2str(A,B,C) ll2str((A),(B),(C),1)

#if defined(__cplusplus)
}
#endif

/*
  LEX_STRING -- a pair of a C-string and its length.
  (it's part of the plugin API as a MYSQL_LEX_STRING)
  Ditto LEX_CSTRING/MYSQL_LEX_CSTRING.
*/

#include <mysql/mysql_lex_string.h>
typedef struct st_mysql_lex_string LEX_STRING;
typedef struct st_mysql_const_lex_string LEX_CSTRING;

#define STRING_WITH_LEN(X) (X), ((sizeof(X) - 1))
#define USTRING_WITH_LEN(X) ((uchar*) X), ((sizeof(X) - 1))
#define C_STRING_WITH_LEN(X) ((char *) (X)), ((sizeof(X) - 1))


/**
  Skip trailing space.

  On most systems reading memory in larger chunks (ideally equal to the size of
  the chinks that the machine physically reads from memory) causes fewer memory
  access loops and hence increased performance.
  This is why the 'int' type is used : it's closest to that (according to how
  it's defined in C).
  So when we determine the amount of whitespace at the end of a string we do
  the following :
    1. We divide the string into 3 zones :
      a) from the start of the string (__start) to the first multiple
        of sizeof(int)  (__start_words)
      b) from the end of the string (__end) to the last multiple of sizeof(int)
        (__end_words)
      c) a zone that is aligned to sizeof(int) and can be safely accessed
        through an int *
    2. We start comparing backwards from (c) char-by-char. If all we find is
       space then we continue
    3. If there are elements in zone (b) we compare them as unsigned ints to a
       int mask (SPACE_INT) consisting of all spaces
    4. Finally we compare the remaining part (a) of the string char by char.
       This covers for the last non-space unsigned int from 3. (if any)

   This algorithm works well for relatively larger strings, but it will slow
   the things down for smaller strings (because of the additional calculations
   and checks compared to the naive method). Thus the barrier of length 20
   is added.

   @param     ptr   pointer to the input string
   @param     len   the length of the string
   @return          the last non-space character
*/
#if defined(__sparc) || defined(__sparcv9)
static inline const uchar *skip_trailing_space(const uchar *ptr,size_t len)
{
  /* SPACE_INT is a word that contains only spaces */
#if SIZEOF_INT == 4
  const unsigned SPACE_INT= 0x20202020U;
#elif SIZEOF_INT == 8
  const unsigned SPACE_INT= 0x2020202020202020ULL;
#else
#error define the appropriate constant for a word full of spaces
#endif

  const uchar *end= ptr + len;

  if (len > 20)
  {
    const uchar *end_words= (const uchar *)(intptr)
      (((ulonglong)(intptr)end) / SIZEOF_INT * SIZEOF_INT);
    const uchar *start_words= (const uchar *)(intptr)
       ((((ulonglong)(intptr)ptr) + SIZEOF_INT - 1) / SIZEOF_INT * SIZEOF_INT);

    DBUG_ASSERT(end_words > ptr);
    while (end > end_words && end[-1] == 0x20)
      end--;
    if (end[-1] == 0x20 && start_words < end_words)
      while (end > start_words && ((unsigned *)end)[-1] == SPACE_INT)
        end -= SIZEOF_INT;
  }
  while (end > ptr && end[-1] == 0x20)
    end--;
  return (end);
}
#else
/*
  Reads 8 bytes at a time, ignoring alignment.
  We use uint8korr, which is fast (it simply reads a *ulonglong)
  on all platforms, except sparc.
*/
static inline const uchar *skip_trailing_space(const uchar *ptr, size_t len)
{
  const uchar *end= ptr + len;
  while (end - ptr >= 8)
  {
    if (uint8korr(end-8) != 0x2020202020202020ULL)
      break;
    end-= 8;
  }
  while (end > ptr && end[-1] == 0x20)
    end--;
  return (end);
}
#endif

static inline void lex_string_set(LEX_STRING *lex_str, const char *c_str)
{
  lex_str->str= (char *) c_str;
  lex_str->length= strlen(c_str);
}

static inline void lex_cstring_set(LEX_CSTRING *lex_str, const char *c_str)
{
  lex_str->str= c_str;
  lex_str->length= strlen(c_str);
}

#endif
