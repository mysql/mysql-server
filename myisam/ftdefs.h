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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

#include "fulltext.h"
#include <m_ctype.h>
#include <my_tree.h>

#define MIN_WORD_LEN 4

#define HYPHEN_IS_DELIM
#define HYPHEN_IS_CONCAT     /* not used for now */

#define COMPILE_STOPWORDS_IN

/* Most of the formulae were shamelessly stolen from SMART distribution
   ftp://ftp.cs.cornell.edu/pub/smart/smart.11.0.tar.Z
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
#define LWS_LOG				 (count?(log(count)+1):0)
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

#ifdef EVAL_RUN
/*
extern ulong collstat;
#define PIVOT_STAT  (docstat.uniq)
#define PIVOT_SLOPE (0.69)
#define PIVOT_PIVOT ((double)collstat/(info->state->records+1))
#define NORM_PIVOT  ((1-PIVOT_SLOPE)*PIVOT_PIVOT+PIVOT_SLOPE*docstat.uniq)
*/
#endif /* EVAL_RUN */

#define PIVOT_VAL (0.0115)
#define NORM_PIVOT  (1+PIVOT_VAL*docstat.uniq)
/*---------------------------------------------------------------*/
#define GWS_NORM				     (1/sqrt(sum2))
#define GWS_GFIDF				      (sum/doc_cnt)
/* Mysterious, but w/o (double) GWS_IDF performs better :-o */
#define GWS_IDF		   log(aio->info->state->records/doc_cnt)
#define GWS_IDF1	   log((double)aio->info->state->records/doc_cnt)
#define GWS_PROB log(((double)(aio->info->state->records-doc_cnt))/doc_cnt)
#define GWS_FREQ					(1.0/doc_cnt)
#define GWS_SQUARED pow(log((double)aio->info->state->records/doc_cnt),2)
#define GWS_CUBIC   pow(log((double)aio->info->state->records/doc_cnt),3)
#define GWS_ENTROPY (1-(suml/sum-log(sum))/log(aio->info->state->records))
/*=================================================================*/

typedef struct st_ft_word {
  byte * pos;
  uint	 len;
  double weight;
#ifdef EVAL_RUN
  byte	 cnt;
#endif /* EVAL_RUN */
} FT_WORD;

int is_stopword(char *word, uint len);

uint _ft_make_key(MI_INFO *, uint , byte *, FT_WORD *, my_off_t);

TREE * ft_parse(TREE *, byte *, int);
FT_WORD * ft_linearize(MI_INFO *, uint, byte *, TREE *);
