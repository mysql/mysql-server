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

} FT_DOCSTAT;

static int FT_WORD_cmp(CHARSET_INFO* cs, FT_WORD *w1, FT_WORD *w2)
{
  return _mi_compare_text(cs, (uchar*) w1->pos, w1->len,
                              (uchar*) w2->pos, w2->len, 0);
}

static int walk_and_copy(FT_WORD *word,uint32 count,FT_DOCSTAT *docstat)
{
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

FT_WORD * ft_linearize(TREE *wtree)
{
  FT_WORD *wlist,*p;
  FT_DOCSTAT docstat;
  DBUG_ENTER("ft_linearize");

  if ((wlist=(FT_WORD *) my_malloc(sizeof(FT_WORD)*
				   (1+wtree->elements_in_tree),MYF(0))))
  {
    docstat.list=wlist;
    docstat.uniq=wtree->elements_in_tree;
#ifdef EVAL_RUN
    docstat.nsum=docstat.nsum2=docstat.max=docstat.words=docstat.totlen=
#endif /* EVAL_RUN */
    docstat.sum=0;
    tree_walk(wtree,(tree_walk_action)&walk_and_copy,&docstat,left_root_right);
  }
  delete_tree(wtree);
  if (!wlist)
    DBUG_RETURN(NULL);

  docstat.list->pos=NULL;

  for (p=wlist;p->pos;p++)
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

  for (p=wlist;p->pos;p++)
  {
    p->weight/=NORM_IN_USE;
  }

  DBUG_RETURN(wlist);
}

/* returns:
 * 0 - eof
 * 1 - word found
 * 2 - left bracket
 * 3 - right bracket
 */
byte ft_get_word(byte **start, byte *end, FT_WORD *word, FTB_PARAM *param)
{
  byte *doc=*start;
  int mwc;

  param->yesno=(FTB_YES==' ') ? 1 : (param->quot != 0);
  param->plusminus=param->pmsign=0;

  while (doc<end)
  {
    for (;doc<end;doc++)
    {
      if (true_word_char(*doc)) break;
      if (*doc == FTB_RQUOT && param->quot)
      {
        param->quot=doc;
        *start=doc+1;
        return 3; /* FTB_RBR */
      }
      if (!param->quot)
      {
        if (*doc == FTB_LBR || *doc == FTB_RBR || *doc == FTB_LQUOT)
        {
          /* param->prev=' '; */
          *start=doc+1;
          if (*doc == FTB_LQUOT) param->quot=*start;
          return (*doc == FTB_RBR)+2;
        }
        if (param->prev == ' ')
        {
          if (*doc == FTB_YES ) { param->yesno=+1;    continue; } else
          if (*doc == FTB_EGAL) { param->yesno= 0;    continue; } else
          if (*doc == FTB_NO  ) { param->yesno=-1;    continue; } else
          if (*doc == FTB_INC ) { param->plusminus++; continue; } else
          if (*doc == FTB_DEC ) { param->plusminus--; continue; } else
          if (*doc == FTB_NEG ) { param->pmsign=!param->pmsign; continue; }
        }
      }
      param->prev=*doc;
      param->yesno=(FTB_YES==' ') ? 1 : (param->quot != 0);
      param->plusminus=param->pmsign=0;
    }

    mwc=0;
    for (word->pos=doc; doc<end; doc++)
      if (true_word_char(*doc))
        mwc=0;
      else if (!misc_word_char(*doc) || mwc++)
        break;

    param->prev='A'; /* be sure *prev is true_word_char */
    word->len= (uint)(doc-word->pos) - mwc;
    if ((param->trunc=(doc<end && *doc == FTB_TRUNC)))
      doc++;

    if (((word->len >= ft_min_word_len && !is_stopword(word->pos, word->len))
         || param->trunc) && word->len < ft_max_word_len)
    {
      *start=doc;
      return 1;
    }
  }
  if (param->quot)
  {
    param->quot=*start=doc;
    return 3; /* FTB_RBR */
  }
  return 0;
}

byte ft_simple_get_word(byte **start, byte *end, FT_WORD *word)
{
  byte *doc=*start;
  int mwc;
  DBUG_ENTER("ft_simple_get_word");

  while (doc<end)
  {
    for (;doc<end;doc++)
    {
      if (true_word_char(*doc)) break;
    }

    mwc=0;
    for(word->pos=doc; doc<end; doc++)
      if (true_word_char(*doc))
        mwc=0;
      else if (!misc_word_char(*doc) || mwc++)
        break;

    word->len= (uint)(doc-word->pos) - mwc;

    if (word->len >= ft_min_word_len && word->len < ft_max_word_len &&
        !is_stopword(word->pos, word->len))
    {
      *start=doc;
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

void ft_parse_init(TREE *wtree, CHARSET_INFO *cs)
{
  DBUG_ENTER("ft_parse_init");
  if (!is_tree_inited(wtree))
    init_tree(wtree,0,0,sizeof(FT_WORD),(qsort_cmp2)&FT_WORD_cmp,0,NULL, cs);
  DBUG_VOID_RETURN;
}

int ft_parse(TREE *wtree, byte *doc, int doclen)
{
  byte   *end=doc+doclen;
  FT_WORD w;
  DBUG_ENTER("ft_parse");

  while (ft_simple_get_word(&doc,end,&w))
  {
    if (!tree_insert(wtree, &w, 0))
      goto err;
  }
  DBUG_RETURN(0);

err:
  delete_tree(wtree);
  DBUG_RETURN(1);
}

