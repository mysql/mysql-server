/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

#include "fulltext.h"
#include <m_ctype.h>
#include <my_tree.h>
#include <queues.h>
#include <mysql/plugin.h>

#define true_word_char(ctype, character) \
                      ((ctype) & (_MY_U | _MY_L | _MY_NMR) || \
                       (character) == '_')
#define misc_word_char(X)	0

#define FT_MAX_WORD_LEN_FOR_SORT 31

#define FTPARSER_MEMROOT_ALLOC_SIZE 65536

/* Interested readers may consult SMART
   (ftp://ftp.cs.cornell.edu/pub/smart/smart.11.0.tar.Z)
   for an excellent implementation of vector space model we use.
   It also demonstrate the usage of different weghting techniques.
   This code, though, is completely original and is not based on the
   SMART code but was in some cases inspired by it.

   NORM_PIVOT was taken from the article
   A.Singhal, C.Buckley, M.Mitra, "Pivoted Document Length Normalization",
   ACM SIGIR'96, 21-29, 1996
 */

#define LWS_FOR_QUERY					  LWS_TF
#define LWS_IN_USE					 LWS_LOG
#define PRENORM_IN_USE				     PRENORM_AVG
#define NORM_IN_USE				      NORM_PIVOT
#define GWS_IN_USE					GWS_PROB
/*==============================================================*/
#define LWS_TF						  (count)
#define LWS_BINARY					(count>0)
#define LWS_SQUARE				    (count*count)
#define LWS_LOG				 (count?(log( (double) count)+1):0)
/*--------------------------------------------------------------*/
#define PRENORM_NONE				      (p->weight)
#define PRENORM_MAX			  (p->weight/docstat.max)
#define PRENORM_AUG		  (0.4+0.6*p->weight/docstat.max)
#define PRENORM_AVG	     (p->weight/docstat.sum*docstat.uniq)
#define PRENORM_AVGLOG ((1+log(p->weight))/(1+log(docstat.sum/docstat.uniq)))
/*--------------------------------------------------------------*/
#define NORM_NONE					      (1)
#define NORM_SUM				   (docstat.nsum)
#define NORM_COS			    (sqrt(docstat.nsum2))

#define PIVOT_VAL (0.0115)
#define NORM_PIVOT  (1+PIVOT_VAL*docstat.uniq)
/*---------------------------------------------------------------*/
#define GWS_NORM				     (1/sqrt(sum2))
#define GWS_GFIDF				      (sum/doc_cnt)
/* Mysterious, but w/o (double) GWS_IDF performs better :-o */
#define GWS_IDF		   log(aio->info->state->records/doc_cnt)
#define GWS_IDF1	   log((double)aio->info->state->records/doc_cnt)
#define GWS_PROB ((aio->info->state->records > doc_cnt) ? log(((double)(aio->info->state->records-doc_cnt))/doc_cnt) : 0 )
#define GWS_FREQ					(1.0/doc_cnt)
#define GWS_SQUARED pow(log((double)aio->info->state->records/doc_cnt),2)
#define GWS_CUBIC   pow(log((double)aio->info->state->records/doc_cnt),3)
#define GWS_ENTROPY (1-(suml/sum-log(sum))/log(aio->info->state->records))
/*=================================================================*/

/* Boolean search operators */
#define FTB_YES   (ft_boolean_syntax[0])
#define FTB_EGAL  (ft_boolean_syntax[1])
#define FTB_NO    (ft_boolean_syntax[2])
#define FTB_INC   (ft_boolean_syntax[3])
#define FTB_DEC   (ft_boolean_syntax[4])
#define FTB_LBR   (ft_boolean_syntax[5])
#define FTB_RBR   (ft_boolean_syntax[6])
#define FTB_NEG   (ft_boolean_syntax[7])
#define FTB_TRUNC (ft_boolean_syntax[8])
#define FTB_LQUOT (ft_boolean_syntax[10])
#define FTB_RQUOT (ft_boolean_syntax[11])

typedef struct st_ft_word {
  uchar * pos;
  uint	 len;
  double weight;
} FT_WORD;

int is_stopword(char *word, uint len);

uint _ft_make_key(MI_INFO *, uint , uchar *, FT_WORD *, my_off_t);

uchar ft_get_word(const CHARSET_INFO *, uchar **, uchar *, FT_WORD *,
                  MYSQL_FTPARSER_BOOLEAN_INFO *);
uchar ft_simple_get_word(const CHARSET_INFO *, uchar **, const uchar *,
                         FT_WORD *, my_bool);

typedef struct _st_ft_seg_iterator {
  uint        num, len;
  HA_KEYSEG  *seg;
  const uchar *rec, *pos;
} FT_SEG_ITERATOR;

void _mi_ft_segiterator_init(MI_INFO *, uint, const uchar *, FT_SEG_ITERATOR *);
void _mi_ft_segiterator_dummy_init(const uchar *, uint, FT_SEG_ITERATOR *);
uint _mi_ft_segiterator(FT_SEG_ITERATOR *);

void ft_parse_init(TREE *, const CHARSET_INFO *);
int ft_parse(TREE *, uchar *, int, struct st_mysql_ftparser *parser,
             MYSQL_FTPARSER_PARAM *, MEM_ROOT *);
FT_WORD * ft_linearize(TREE *, MEM_ROOT *);
FT_WORD * _mi_ft_parserecord(MI_INFO *, uint, const uchar *, MEM_ROOT *);
uint _mi_ft_parse(TREE *, MI_INFO *, uint, const uchar *,
                  MYSQL_FTPARSER_PARAM *, MEM_ROOT *);

FT_INFO *ft_init_nlq_search(MI_INFO *, uint, uchar *, uint, uint, uchar *);
FT_INFO *ft_init_boolean_search(MI_INFO *, uint, uchar *, uint,
                                const CHARSET_INFO *);

extern const struct _ft_vft _ft_vft_nlq;
int ft_nlq_read_next(FT_INFO *, char *);
float ft_nlq_find_relevance(FT_INFO *, uchar *, uint);
void ft_nlq_close_search(FT_INFO *);
float ft_nlq_get_relevance(FT_INFO *);
my_off_t ft_nlq_get_docid(FT_INFO *);
void ft_nlq_reinit_search(FT_INFO *);

extern const struct _ft_vft _ft_vft_boolean;
int ft_boolean_read_next(FT_INFO *, char *);
float ft_boolean_find_relevance(FT_INFO *, uchar *, uint);
void ft_boolean_close_search(FT_INFO *);
float ft_boolean_get_relevance(FT_INFO *);
my_off_t ft_boolean_get_docid(FT_INFO *);
void ft_boolean_reinit_search(FT_INFO *);
MYSQL_FTPARSER_PARAM* ftparser_alloc_param(MI_INFO *info);
extern MYSQL_FTPARSER_PARAM *ftparser_call_initializer(MI_INFO *info,
                                                       uint keynr,
                                                       uint paramnr);
extern void ftparser_call_deinitializer(MI_INFO *info);
