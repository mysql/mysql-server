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

#define my_wc_t ulong

typedef struct unicase_info_st {
  uint16 toupper;
  uint16 tolower;
  uint16 sort;
} MY_UNICASE_INFO;

#define MY_CS_ILSEQ	0
#define MY_CS_ILUNI	0
#define MY_CS_TOOSMALL	-1
#define MY_CS_TOOFEW(n)	(-1-(n))

        /* My charsets_list flags */
#define MY_NO_SETS       0
#define MY_CS_COMPILED  1      /* compiled-in sets               */
#define MY_CS_CONFIG    2      /* sets that have a *.conf file   */
#define MY_CS_INDEX     4      /* sets listed in the Index file  */
#define MY_CS_LOADED    8      /* sets that are currently loaded */

#define MY_CHARSET_UNDEFINED 0
#define MY_CHARSET_CURRENT (default_charset_info->number)


typedef struct my_uni_idx_st {
  uint16 from;
  uint16 to;
  uchar  *tab;
} MY_UNI_IDX;


typedef struct charset_info_st
{
  uint      number;
  uint      state;
  const char *name;
  const char *comment;
  uchar    *ctype;
  uchar    *to_lower;
  uchar    *to_upper;
  uchar    *sort_order;
  uint16      *tab_to_uni;
  MY_UNI_IDX  *tab_from_uni;
  
  /* Collation routines */
  uint      strxfrm_multiply;
  int     (*strnncoll)(struct charset_info_st *,
		       const uchar *, uint, const uchar *, uint);
  int     (*strnxfrm)(struct charset_info_st *,
		      uchar *, uint, const uchar *, uint);
  my_bool (*like_range)(struct charset_info_st *,
			const char *, uint, pchar, uint,
			char *, char *, uint *, uint *);
    
  /* Multibyte routines */
  uint      mbmaxlen;
  int     (*ismbchar)(struct charset_info_st *, const char *, const char *);
  my_bool (*ismbhead)(struct charset_info_st *, uint);
  int     (*mbcharlen)(struct charset_info_st *, uint);
    
  /* Unicode convertion */
  int (*mb_wc)(struct charset_info_st *cs,my_wc_t *wc,
	       const unsigned char *s,const unsigned char *e);
  int (*wc_mb)(struct charset_info_st *cs,my_wc_t wc,
	       unsigned char *s,unsigned char *e);
    
  /* Functions for case and sort convertion */
  void    (*caseup_str)(struct charset_info_st *, char *);
  void    (*casedn_str)(struct charset_info_st *, char *);
  void    (*caseup)(struct charset_info_st *, char *, uint);
  void    (*casedn)(struct charset_info_st *, char *, uint);
  void    (*tosort)(struct charset_info_st *, char *, uint);
  
  /* Functions for case comparison */
  int  (*strcasecmp)(struct charset_info_st *, const char *, const char *);
  int  (*strncasecmp)(struct charset_info_st *, const char *, const char *,
		      uint);
    
  /* Hash calculation */
  uint (*hash_caseup)(struct charset_info_st *cs, const byte *key, uint len);
  void (*hash_sort)(struct charset_info_st *cs, const uchar *key, uint len,
		    ulong *nr1, ulong *nr2); 
    
  char    max_sort_char; /* For LIKE optimization */
} CHARSET_INFO;


extern CHARSET_INFO *default_charset_info;
extern CHARSET_INFO *system_charset_info;
extern CHARSET_INFO *all_charsets[256];
extern my_bool init_compiled_charsets(myf flags);


/* declarations for simple charsets */
extern int  my_strnxfrm_simple(CHARSET_INFO *, uchar *, uint, const uchar *, uint); 
extern int  my_strnncoll_simple(CHARSET_INFO *, const uchar *, uint, const uchar *, uint);

/* Functions for 8bit */
extern void my_caseup_str_8bit(CHARSET_INFO *, char *);
extern void my_casedn_str_8bit(CHARSET_INFO *, char *);
extern void my_caseup_8bit(CHARSET_INFO *, char *, uint);
extern void my_casedn_8bit(CHARSET_INFO *, char *, uint);
extern void my_tosort_8bit(CHARSET_INFO *, char *, uint);

extern int my_strcasecmp_8bit(CHARSET_INFO * cs, const char *, const char *);
extern int my_strncasecmp_8bit(CHARSET_INFO * cs, const char *, const char *, uint);

int my_mb_wc_8bit(CHARSET_INFO *cs,my_wc_t *wc, const uchar *s,const uchar *e);
int my_wc_mb_8bit(CHARSET_INFO *cs,my_wc_t wc, uchar *s, uchar *e);


#ifdef USE_MB
/* Functions for multibyte charsets */
extern void my_caseup_str_mb(CHARSET_INFO *, char *);
extern void my_casedn_str_mb(CHARSET_INFO *, char *);
extern void my_caseup_mb(CHARSET_INFO *, char *, uint);
extern void my_casedn_mb(CHARSET_INFO *, char *, uint);
extern int my_strcasecmp_mb(CHARSET_INFO * cs,const char *, const char *);
extern int my_strncasecmp_mb(CHARSET_INFO * cs,const char *, const char *t, uint);
#endif



#define	_U	01	/* Upper case */
#define	_L	02	/* Lower case */
#define	_NMR	04	/* Numeral (digit) */
#define	_SPC	010	/* Spacing character */
#define	_PNT	020	/* Punctuation */
#define	_CTR	040	/* Control character */
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
#define	my_isdigit(s, c)  (((s)->ctype+1)[(uchar) (c)] & _NMR)
#define	my_isxdigit(s, c) (((s)->ctype+1)[(uchar) (c)] & _X)
#define	my_isalnum(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_U | _L | _NMR))
#define	my_isspace(s, c)  (((s)->ctype+1)[(uchar) (c)] & _SPC)
#define	my_ispunct(s, c)  (((s)->ctype+1)[(uchar) (c)] & _PNT)
#define	my_isprint(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_PNT | _U | _L | _NMR | _B))
#define	my_isgraph(s, c)  (((s)->ctype+1)[(uchar) (c)] & (_PNT | _U | _L | _NMR))
#define	my_iscntrl(s, c)  (((s)->ctype+1)[(uchar) (c)] & _CTR)

/* Some macros that should be cleaned up a little */
#define my_isvar(s,c)                 (my_isalnum(s,c) || (c) == '_')
#define my_isvar_start(s,c)           (my_isalpha(s,c) || (c) == '_')

#define use_strnxfrm(s)               ((s)->strnxfrm  != NULL)
#define my_strnxfrm(s, a, b, c, d)    ((s)->strnxfrm((s), (a), (b), (c), (d)))
#define my_strnncoll(s, a, b, c, d)   ((s)->strnncoll((s), (a), (b), (c), (d)))
#define my_like_range(s, a, b, c, d, e, f, g, h) \
                ((s)->like_range((s), (a), (b), (c), (d), (e), (f), (g), (h)))

#define use_mb(s)                     ((s)->ismbchar != NULL)
#define my_ismbchar(s, a, b)          ((s)->ismbchar((s), (a), (b)))
#define my_ismbhead(s, a)             ((s)->ismbhead((s), (a)))
#define my_mbcharlen(s, a)            ((s)->mbcharlen((s),(a)))

#define my_caseup(s, a, l)            ((s)->caseup((s), (a), (l)))
#define my_casedn(s, a, l)            ((s)->casedn((s), (a), (l)))
#define my_tosort(s, a, l)            ((s)->tosort((s), (a), (l)))
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
