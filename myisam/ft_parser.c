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

#include "ftdefs.h"

#ifdef EVAL_RUN
#ifdef PIVOT_STAT
ulong collstat=0;
#endif
#endif /* EVAL_RUN */

typedef struct st_ft_docstat {
  FT_WORD *list;
  uint uniq;
  double sum;
#ifdef EVAL_RUN
  uint words, totlen;
  double max, nsum, nsum2;
#endif /* EVAL_RUN */

  MI_INFO *info;
  uint keynr;
  byte *keybuf;
} FT_DOCSTAT;

static int FT_WORD_cmp(FT_WORD *w1, FT_WORD *w2)
{
  return _mi_compare_text(default_charset_info,
			  (uchar*) w1->pos,w1->len,
			  (uchar*) w2->pos, w2->len,0);
}

static int walk_and_copy(FT_WORD *word,uint32 count,FT_DOCSTAT *docstat)
{
    if(is_stopword(word->pos, word->len))
      return 0;

    word->weight=LWS_IN_USE;

#ifdef EVAL_RUN
    word->cnt= (uchar) count;
    if(docstat->max < word->weight) docstat->max=word->weight;
    docstat->words+=count;
    docstat->totlen+=word->len;
#endif /* EVAL_RUN */
    docstat->sum+=word->weight;

    memcpy_fixed((docstat->list)++,word,sizeof(FT_WORD));
    return 0;
}

/* transforms tree of words into the array, applying normalization */

FT_WORD * ft_linearize(MI_INFO *info, uint keynr, byte *keybuf, TREE *wtree)
{
  FT_WORD *wlist,*p;
  FT_DOCSTAT docstat;

  if ((wlist=(FT_WORD *) my_malloc(sizeof(FT_WORD)*
				   (1+wtree->elements_in_tree),MYF(0))))
  {
    docstat.info=info;
    docstat.keynr=keynr;
    docstat.keybuf=keybuf;
    docstat.list=wlist;
    docstat.uniq=wtree->elements_in_tree;
#ifdef EVAL_RUN
    docstat.nsum=docstat.nsum2=docstat.max=docstat.words=docstat.totlen=
#endif /* EVAL_RUN */
    docstat.sum=0;
    tree_walk(wtree,(tree_walk_action)&walk_and_copy,&docstat,left_root_right);
  }
  delete_tree(wtree);
  free(wtree);
  if(wlist==NULL)
    return NULL;

  docstat.list->pos=NULL;

  for(p=wlist;p->pos;p++)
  {
    p->weight=PRENORM_IN_USE;
#ifdef EVAL_RUN
    docstat.nsum+=p->weight;
    docstat.nsum2+=p->weight*p->weight;
#endif /* EVAL_RUN */
  }

#ifdef EVAL_RUN
#ifdef PIVOT_STAT
  collstat+=PIVOT_STAT;
#endif
#endif /* EVAL_RUN */

  for(p=wlist;p->pos;p++)
  {
    p->weight/=NORM_IN_USE;
  }

  return wlist;
}

#ifdef HYPHEN_IS_DELIM
#define word_char(X)	(isalnum(X) || (X)=='_' || (X)=='\'')
#else
#define word_char(X)	(isalnum(X) || (X)=='_' || (X)=='\'' || (X)=='-')
#endif

/* this is rather dumb first version of the parser */
TREE * ft_parse(TREE *wtree, byte *doc, int doclen)
{
  byte *end=doc+doclen;
  FT_WORD w;

  if(!wtree)
  {
    if(!(wtree=(TREE *)my_malloc(sizeof(TREE),MYF(0)))) return NULL;
    init_tree(wtree,0,sizeof(FT_WORD),(qsort_cmp)&FT_WORD_cmp,0,NULL);
  }

  w.weight=0;
  while(doc<end)
  {
    for(;doc<end;doc++)
      if(word_char(*doc)) break;
    for(w.pos=doc; doc<end; doc++)
      if(!word_char(*doc)) break;
    if((w.len= (uint) (doc-w.pos)) < MIN_WORD_LEN) continue;
    if(!tree_insert(wtree, &w, 0))
    {
      delete_tree(wtree);
      free(wtree);
      return NULL;
    }
  }
  return wtree;
}
