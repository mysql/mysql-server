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

#define MY_CS_NAME_SIZE			32
#define MY_CS_CTYPE_TABLE_SIZE		257
#define MY_CS_TO_LOWER_TABLE_SIZE	256
#define MY_CS_TO_UPPER_TABLE_SIZE	256
#define MY_CS_SORT_ORDER_TABLE_SIZE	256
#define MY_CS_TO_UNI_TABLE_SIZE		256

#define CHARSET_DIR	"charsets/"

#define my_wc_t ulong

typedef struct unicase_info_st
{
  uint16 toupper;
  uint16 tolower;
  uint16 sort;
} MY_UNICASE_INFO;

#define MY_CS_ILSEQ	0
#define MY_CS_ILUNI	0
#define MY_CS_TOOSMALL	-1
#define MY_CS_TOOFEW(n)	(-1-(n))

#define MY_SEQ_INTTAIL	1
#define MY_SEQ_SPACES	2

        /* My charsets_list flags */
#define MY_NO_SETS       0
#define MY_CS_COMPILED  1      /* compiled-in sets               */
#define MY_CS_CONFIG    2      /* sets that have a *.conf file   */
#define MY_CS_INDEX     4      /* sets listed in the Index file  */
#define MY_CS_LOADED    8      /* sets that are currently loaded */
#define MY_CS_BINSORT	16     /* if binary sort order           */
#define MY_CS_PRIMARY	32     /* if primary collation           */
#define MY_CS_STRNXFRM	64     /* if strnxfrm is used for sort   */

#define MY_CHARSET_UNDEFINED 0
#define MY_CHARSET_CURRENT (default_charset_info->number)


typedef struct my_uni_idx_st
{
  uint16 from;
  uint16 to;
  uchar  *tab;
} MY_UNI_IDX;


enum my_lex_states
{
  MY_LEX_START, MY_LEX_CHAR, MY_LEX_IDENT, 
  MY_LEX_IDENT_SEP, MY_LEX_IDENT_START,
  MY_LEX_FOUND_IDENT, MY_LEX_SIGNED_NUMBER, MY_LEX_REAL, MY_LEX_HEX_NUMBER,
  MY_LEX_CMP_OP, MY_LEX_LONG_CMP_OP, MY_LEX_STRING, MY_LEX_COMMENT, MY_LEX_END,
  MY_LEX_OPERATOR_OR_IDENT, MY_LEX_NUMBER_IDENT, MY_LEX_INT_OR_REAL,
  MY_LEX_REAL_OR_POINT, MY_LEX_BOOL, MY_LEX_EOL, MY_LEX_ESCAPE, 
  MY_LEX_LONG_COMMENT, MY_LEX_END_LONG_COMMENT, MY_LEX_COLON, 
  MY_LEX_SET_VAR, MY_LEX_USER_END, MY_LEX_HOSTNAME, MY_LEX_SKIP, 
  MY_LEX_USER_VARIABLE_DELIMITER, MY_LEX_SYSTEM_VAR,
  MY_LEX_IDENT_OR_KEYWORD, MY_LEX_IDENT_OR_HEX, MY_LEX_IDENT_OR_BIN,
  MY_LEX_STRING_OR_DELIMITER
};


typedef struct charset_info_st
{
  uint      number;
  uint      primary_number;
  uint      binary_number;
  uint      state;
  const char *csname;
  const char *name;
  const char *comment;
  uchar    *ctype;
  uchar    *to_lower;
  uchar    *to_upper;
  uchar    *sort_order;
  uint16      *tab_to_uni;
  MY_UNI_IDX  *tab_from_uni;
  uchar state_map[256];
  uchar ident_map[256];
  
  /* Collation routines */
  uint      strxfrm_multiply;
  int     (*strnncoll)(struct charset_info_st *,
		       const uchar *, uint, const uchar *, uint);
  int     (*strnncollsp)(struct charset_info_st *,
		       const uchar *, uint, const uchar *, uint);
  int     (*strnxfrm)(struct charset_info_st *,
		      uchar *, uint, const uchar *, uint);
  my_bool (*like_range)(struct charset_info_st *,
			const char *s, uint s_length,
			int w_prefix, int w_one, int w_many, 
			uint res_length,
			char *min_str, char *max_str,
			uint *min_len, uint *max_len);
  int     (*wildcmp)(struct charset_info_st *,
  		     const char *str,const char *str_end,
                     const char *wildstr,const char *wildend,
                     int escape,int w_one, int w_many);
  
  /* Multibyte routines */
  uint      mbmaxlen;
  int     (*ismbchar)(struct charset_info_st *, const char *, const char *);
  my_bool (*ismbhead)(struct charset_info_st *, uint);
  int     (*mbcharlen)(struct charset_info_st *, uint);
  uint    (*numchars)(struct charset_info_st *, const char *b, const char *e);
  uint    (*charpos)(struct charset_info_st *, const char *b, const char *e, uint pos);
  
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
  
  /* Charset dependant snprintf() */
  int  (*snprintf)(struct charset_info_st *, char *to, uint n, const char *fmt,
		   ...);
  int  (*long10_to_str)(struct charset_info_st *, char *to, uint n, int radix,
			long int val);
  int (*longlong10_to_str)(struct charset_info_st *, char *to, uint n,
			   int radix, longlong val);
  
  void (*fill)(struct charset_info_st *, char *to, uint len, int fill);
  
  /* String-to-number convertion routines */
  long        (*strntol)(struct charset_info_st *, const char *s, uint l,
			 int base, char **e, int *err);
  ulong      (*strntoul)(struct charset_info_st *, const char *s, uint l,
			 int base, char **e, int *err);
  longlong   (*strntoll)(struct charset_info_st *, const char *s, uint l,
			 int base, char **e, int *err);
  ulonglong (*strntoull)(struct charset_info_st *, const char *s, uint l,
			 int base, char **e, int *err);
  double      (*strntod)(struct charset_info_st *, char *s, uint l, char **e,
			 int *err);
  
  ulong        (*scan)(struct charset_info_st *, const char *b, const char *e,
		       int sq);
  
} CHARSET_INFO;


extern CHARSET_INFO my_charset_bin;
extern CHARSET_INFO my_charset_latin1;
extern CHARSET_INFO my_charset_big5;
extern CHARSET_INFO my_charset_czech;
extern CHARSET_INFO my_charset_euc_kr;
extern CHARSET_INFO my_charset_gb2312;
extern CHARSET_INFO my_charset_gbk;
extern CHARSET_INFO my_charset_latin1_de;
extern CHARSET_INFO my_charset_sjis;
extern CHARSET_INFO my_charset_tis620;
extern CHARSET_INFO my_charset_ucs2;
extern CHARSET_INFO my_charset_ucse;
extern CHARSET_INFO my_charset_ujis;
extern CHARSET_INFO my_charset_utf8;
extern CHARSET_INFO my_charset_win1250ch;


extern my_bool my_parse_charset_xml(const char *bug, uint len,
				    int (*add)(CHARSET_INFO *cs));

/* declarations for simple charsets */
extern int  my_strnxfrm_simple(CHARSET_INFO *, uchar *, uint, const uchar *,
			       uint); 
extern int  my_strnncoll_simple(CHARSET_INFO *, const uchar *, uint,
				const uchar *, uint);

extern int  my_strnncollsp_simple(CHARSET_INFO *, const uchar *, uint,
				const uchar *, uint);

extern uint my_hash_caseup_simple(CHARSET_INFO *cs,
				  const byte *key, uint len);
				  
extern void my_hash_sort_simple(CHARSET_INFO *cs,
				const uchar *key, uint len,
				ulong *nr1, ulong *nr2); 


/* Functions for 8bit */
extern void my_caseup_str_8bit(CHARSET_INFO *, char *);
extern void my_casedn_str_8bit(CHARSET_INFO *, char *);
extern void my_caseup_8bit(CHARSET_INFO *, char *, uint);
extern void my_casedn_8bit(CHARSET_INFO *, char *, uint);
extern void my_tosort_8bit(CHARSET_INFO *, char *, uint);

extern int my_strcasecmp_8bit(CHARSET_INFO * cs, const char *, const char *);
extern int my_strncasecmp_8bit(CHARSET_INFO * cs, const char *, const char *,
			       uint);

int my_mb_wc_8bit(CHARSET_INFO *cs,my_wc_t *wc, const uchar *s,const uchar *e);
int my_wc_mb_8bit(CHARSET_INFO *cs,my_wc_t wc, uchar *s, uchar *e);

ulong my_scan_8bit(CHARSET_INFO *cs, const char *b, const char *e, int sq);

int my_snprintf_8bit(struct charset_info_st *, char *to, uint n,
		     const char *fmt, ...);

long        my_strntol_8bit(CHARSET_INFO *, const char *s, uint l, int base,
			    char **e, int *err);
ulong      my_strntoul_8bit(CHARSET_INFO *, const char *s, uint l, int base,
			    char **e, int *err);
longlong   my_strntoll_8bit(CHARSET_INFO *, const char *s, uint l, int base,
			    char **e, int *err);
ulonglong my_strntoull_8bit(CHARSET_INFO *, const char *s, uint l, int base,
			    char **e, int *err);
double      my_strntod_8bit(CHARSET_INFO *, char *s, uint l,char **e,
			    int *err);

int  my_long10_to_str_8bit(CHARSET_INFO *, char *to, uint l, int radix,
			   long int val);
int my_longlong10_to_str_8bit(CHARSET_INFO *, char *to, uint l, int radix,
			      longlong val);

void my_fill_8bit(CHARSET_INFO *cs, char* to, uint l, int fill);

my_bool  my_like_range_simple(CHARSET_INFO *cs,
			      const char *ptr, uint ptr_length,
			      int escape, int w_one, int w_many,
			      uint res_length,
			      char *min_str, char *max_str,
			      uint *min_length, uint *max_length);


int my_wildcmp_8bit(CHARSET_INFO *,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many);

uint my_numchars_8bit(CHARSET_INFO *, const char *b, const char *e);
uint my_charpos_8bit(CHARSET_INFO *, const char *b, const char *e, uint pos);


#ifdef USE_MB
/* Functions for multibyte charsets */
extern void my_caseup_str_mb(CHARSET_INFO *, char *);
extern void my_casedn_str_mb(CHARSET_INFO *, char *);
extern void my_caseup_mb(CHARSET_INFO *, char *, uint);
extern void my_casedn_mb(CHARSET_INFO *, char *, uint);
extern int my_strcasecmp_mb(CHARSET_INFO * cs,const char *, const char *);
extern int my_strncasecmp_mb(CHARSET_INFO * cs,const char *, const char *t,
			     uint);
int my_wildcmp_mb(CHARSET_INFO *,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many);
uint my_numchars_mb(CHARSET_INFO *, const char *b, const char *e);
uint my_charpos_mb(CHARSET_INFO *, const char *b, const char *e, uint pos);

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

#define use_strnxfrm(s)               ((s)->state  & MY_CS_STRNXFRM)
#define my_strnxfrm(s, a, b, c, d)    ((s)->strnxfrm((s), (a), (b), (c), (d)))
#define my_strnncoll(s, a, b, c, d)   ((s)->strnncoll((s), (a), (b), (c), (d)))
#define my_like_range(s, a, b, c, d, e, f, g, h, i, j) \
                ((s)->like_range((s), (a), (b), (c), (d), (e), (f), (g), (h), (i), (j)))
#define my_wildcmp(cs,s,se,w,we,e,o,m)	((cs)->wildcmp((cs),(s),(se),(w),(we),(e),(o),(m)))

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

#define my_strntol(s, a, b, c, d, e)  ((s)->strntol((s),(a),(b),(c),(d),(e)))
#define my_strntoul(s, a, b, c, d, e) ((s)->strntoul((s),(a),(b),(c),(d),(e)))
#define my_strntoll(s, a, b, c, d, e) ((s)->strntoll((s),(a),(b),(c),(d),(e)))
#define my_strntoull(s, a, b, c,d, e) ((s)->strntoull((s),(a),(b),(c),(d),(e)))
#define my_strntod(s, a, b, c, d)     ((s)->strntod((s),(a),(b),(c),(d)))


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
