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

/*
  A better inplementation of the UNIX ctype(3) library.
  Notes:   my_global.h should be included before ctype.h
*/

#ifndef _m_ctype_h
#define _m_ctype_h

#ifdef	__cplusplus
extern "C" {
#endif


#define CHARSET_DIR	"charsets/"

typedef struct charset_info_st
{
    uint      number;
    const char *name;
    uchar    *ctype;
    uchar    *to_lower;
    uchar    *to_upper;
    uchar    *sort_order;

    uint      strxfrm_multiply;
    int     (*strnncoll)(struct charset_info_st *,
                         const uchar *, uint, const uchar *, uint);
    int     (*strnxfrm)(struct charset_info_st *,
                         uchar *, uint, const uchar *, uint);
    my_bool (*like_range)(struct charset_info_st *,
                          const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);

    uint      mbmaxlen;
    int     (*ismbchar)(const char *, const char *);
    my_bool (*ismbhead)(uint);
    int     (*mbcharlen)(uint);

    /* Functions for case convertion */
    void    (*caseup_str)(struct charset_info_st *, char *);
    void    (*casedn_str)(struct charset_info_st *, char *);
    void    (*caseup)(struct charset_info_st *, char *, uint);
    void    (*casedn)(struct charset_info_st *, char *, uint);
    
    /* Functions for case comparison */
    int  (*strcasecmp)(struct charset_info_st *, const char *, const char *);
    int  (*strncasecmp)(struct charset_info_st *, const char *, const char *, uint);
    
    char    max_sort_char; /* For LIKE otimization */
} CHARSET_INFO;

/* strings/ctype.c */
extern CHARSET_INFO *default_charset_info;
extern CHARSET_INFO *system_charset_info;
extern CHARSET_INFO *find_compiled_charset(uint cs_number);
extern CHARSET_INFO *find_compiled_charset_by_name(const char *name);
extern CHARSET_INFO  compiled_charsets[];
extern uint compiled_charset_number(const char *name);
extern const char *compiled_charset_name(uint charset_number);

#define MY_CHARSET_UNDEFINED 0
#define MY_CHARSET_CURRENT (default_charset_info->number)

/* declarations for simple charsets */
extern int  my_strnxfrm_simple(CHARSET_INFO *, char *, uint, const char *, uint); 
extern int  my_strnncoll_simple(CHARSET_INFO *, const char *, uint, const char *, uint);

/* Functions for 8bit */
extern void my_caseup_str_8bit(CHARSET_INFO *, char *);
extern void my_casedn_str_8bit(CHARSET_INFO *, char *);
extern void my_caseup_8bit(CHARSET_INFO *, char *, uint);
extern void my_casedn_8bit(CHARSET_INFO *, char *, uint);

extern int my_strcasecmp_8bit(CHARSET_INFO * cs, const char *, const char *);
extern int my_strncasecmp_8bit(CHARSET_INFO * cs, const char *, const char *, uint);

#ifdef USE_MB
/* Functions for multibyte charsets */
extern void my_caseup_str_mb(CHARSET_INFO *, char *);
extern void my_casedn_str_mb(CHARSET_INFO *, char *);
extern void my_caseup_mb(CHARSET_INFO *, char *, uint);
extern void my_casedn_mb(CHARSET_INFO *, char *, uint);
extern int my_strcasecmp_mb(CHARSET_INFO * cs,const char *, const char *);
extern int my_strncasecmp_mb(CHARSET_INFO * cs,const char *, const char *t, uint);
#endif

#ifdef HAVE_CHARSET_big5
/* declarations for the big5 character set */
extern uchar ctype_big5[], to_lower_big5[], to_upper_big5[], sort_order_big5[];
extern int     my_strnncoll_big5(CHARSET_INFO *,const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_big5(CHARSET_INFO *,uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_big5(CHARSET_INFO *,const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
extern int     ismbchar_big5(const char *, const char *);
extern my_bool ismbhead_big5(uint);
extern int     mbcharlen_big5(uint);
#endif

#ifdef HAVE_CHARSET_czech
/* declarations for the czech character set */
extern uchar ctype_czech[], to_lower_czech[], to_upper_czech[], sort_order_czech[];
extern int     my_strnncoll_czech(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_czech(CHARSET_INFO *, uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_czech(CHARSET_INFO *, 
                          const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
#endif

#ifdef HAVE_CHARSET_euc_kr
/* declarations for the euc_kr character set */
extern uchar ctype_euc_kr[], to_lower_euc_kr[], to_upper_euc_kr[], sort_order_euc_kr[];
extern int     ismbchar_euc_kr(const char *, const char *);
extern my_bool ismbhead_euc_kr(uint);
extern int     mbcharlen_euc_kr(uint);
#endif

#ifdef HAVE_CHARSET_gb2312
/* declarations for the gb2312 character set */
extern uchar ctype_gb2312[], to_lower_gb2312[], to_upper_gb2312[], sort_order_gb2312[];
extern int     ismbchar_gb2312(const char *, const char *);
extern my_bool ismbhead_gb2312(uint);
extern int     mbcharlen_gb2312(uint);
#endif

#ifdef HAVE_CHARSET_gbk
/* declarations for the gbk character set */
extern uchar ctype_gbk[], to_lower_gbk[], to_upper_gbk[], sort_order_gbk[];
extern int     my_strnncoll_gbk(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_gbk(CHARSET_INFO *, uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_gbk(CHARSET_INFO *, const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
extern int     ismbchar_gbk(const char *, const char *);
extern my_bool ismbhead_gbk(uint);
extern int     mbcharlen_gbk(uint);
#endif

#ifdef HAVE_CHARSET_latin1_de
/* declarations for the latin1_de character set */
extern uchar ctype_latin1_de[], to_lower_latin1_de[], to_upper_latin1_de[], sort_order_latin1_de[];
extern int     my_strnncoll_latin1_de(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_latin1_de(CHARSET_INFO *, uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_latin1_de(CHARSET_INFO *, const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
#endif

#ifdef HAVE_CHARSET_sjis
/* declarations for the sjis character set */
extern uchar ctype_sjis[], to_lower_sjis[], to_upper_sjis[], sort_order_sjis[];
extern int     my_strnncoll_sjis(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_sjis(CHARSET_INFO *, uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_sjis(CHARSET_INFO *, const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
extern int     ismbchar_sjis(const char *, const char *);
extern my_bool ismbhead_sjis(uint);
extern int     mbcharlen_sjis(uint);
#endif

#ifdef HAVE_CHARSET_tis620
/* declarations for the tis620 character set */
extern uchar ctype_tis620[], to_lower_tis620[], to_upper_tis620[], sort_order_tis620[];
extern int     my_strnncoll_tis620(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);
extern int     my_strnxfrm_tis620(CHARSET_INFO *, uchar *, uint, const uchar *, uint);
extern my_bool my_like_range_tis620(CHARSET_INFO *, const char *, uint, pchar, uint,
                          char *, char *, uint *, uint *);
#endif

#ifdef HAVE_CHARSET_ujis
/* declarations for the ujis character set */
extern uchar ctype_ujis[], to_lower_ujis[], to_upper_ujis[], sort_order_ujis[];
extern int     ismbchar_ujis(const char *, const char *);
extern my_bool ismbhead_ujis(uint);
extern int     mbcharlen_ujis(uint);
#endif


#define	_U	01	/* Upper case */
#define	_L	02	/* Lower case */
#define	_N	04	/* Numeral (digit) */
#define	_S	010	/* Spacing character */
#define	_P	020	/* Punctuation */
#define	_C	040	/* Control character */
#define	_B	0100	/* Blank */
#define	_X	0200	/* heXadecimal digit */


#define	my_isascii(c)	(!((c) & ~0177))
#define	my_toascii(c)	((c) & 0177)
#define my_tocntrl(c)	((c) & 31)
#define my_toprint(c)	((c) | 64)

#define my_toupper(s,c)	(char) ((s)->to_upper[(uchar) (c)])
#define my_tolower(s,c)	(char) ((s)->to_lower[(uchar) (c)])
#define	my_isalpha(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_U | _L))
#define	my_isupper(s, c)  (((s)->ctype+1)[(uchar) (c)] & _U)
#define	my_islower(s, c)  (((s)->ctype+1)[(uchar) (c)] & _L)
#define	my_isdigit(s, c)  (((s)->ctype+1)[(uchar) (c)] & _N)
#define	my_isxdigit(s, c) (((s)->ctype+1)[(uchar) (c)] & _X)
#define	my_isalnum(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_U | _L | _N))
#define	my_isspace(s, c)  (((s)->ctype+1)[(uchar) (c)] & _S)
#define	my_ispunct(s, c)  (((s)->ctype+1)[(uchar) (c)] & _P)
#define	my_isprint(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_P | _U | _L | _N | _B))
#define	my_isgraph(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_P | _U | _L | _N))
#define	my_iscntrl(s, c)  (((s)->ctype+1)[(uchar) (c)] & _C)

/* Some macros that should be cleaned up a little */
#define my_isvar(s,c)                 (my_isalnum(s,c) || (c) == '_')
#define my_isvar_start(s,c)           (my_isalpha(s,c) || (c) == '_')

#define use_strcoll(s)                ((s)->strnncoll != NULL)
#define my_strnxfrm(s, a, b, c, d)    ((s)->strnxfrm((s), (a), (b), (c), (d)))
#define my_strnncoll(s, a, b, c, d)   ((s)->strnncoll((s), (a), (b), (c), (d)))
#define my_like_range(s, a, b, c, d, e, f, g, h) \
                ((s)->like_range((s), (a), (b), (c), (d), (e), (f), (g), (h)))

#define use_mb(s)                     ((s)->ismbchar != NULL)
#define my_ismbchar(s, a, b)          ((s)->ismbchar((a), (b)))
#define my_ismbhead(s, a)             ((s)->ismbhead((a)))
#define my_mbcharlen(s, a)            ((s)->mbcharlen((a)))

#define my_caseup(s, a, l)            ((s)->caseup((s), (a), (l)))
#define my_casedn(s, a, l)            ((s)->casedn((s), (a), (l)))
#define my_caseup_str(s, a)           ((s)->caseup_str((s), (a)))
#define my_casedn_str(s, a)           ((s)->casedn_str((s), (a)))
#define my_strcasecmp(s, a, b)        ((s)->strcasecmp((s), (a), (b)))
#define my_strncasecmp(s, a, b, l)    ((s)->strncasecmp((s), (a), (b), (l)))


/* XXX: still need to take care of this one */
#ifdef MY_CHARSET_TIS620
#error The TIS620 charset is broken at the moment.  Tell tim to fix it.
#define USE_TIS620
#include "t_ctype.h"
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _m_ctype_h */
