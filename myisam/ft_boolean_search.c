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

/*  TODO: add caching - pre-read several index entries at once */

#define FT_CORE
#include "ftdefs.h"
#include <queues.h>

/* search with boolean queries */

static double _wghts[11]={
  0.131687242798354,
  0.197530864197531,
  0.296296296296296,
  0.444444444444444,
  0.666666666666667,
  1.000000000000000,
  1.500000000000000,
  2.250000000000000,
  3.375000000000000,
  5.062500000000000,
  7.593750000000000};
static double *wghts=_wghts+5; /* wghts[i] = 1.5**i */

static double _nwghts[11]={
 -0.065843621399177,
 -0.098765432098766,
 -0.148148148148148,
 -0.222222222222222,
 -0.333333333333334,
 -0.500000000000000,
 -0.750000000000000,
 -1.125000000000000,
 -1.687500000000000,
 -2.531250000000000,
 -3.796875000000000};
static double *nwghts=_nwghts+5; /* nwghts[i] = -0.5*1.5**i */

typedef struct st_ftb_expr FTB_EXPR;
struct st_ftb_expr {
  FTB_EXPR *up;
  float     weight;
  int       yesno;
  my_off_t  docid;
  float     cur_weight;
  int       yesses;               /* number of "yes" words matched */
  int       nos;                  /* number of "no"  words matched */
  int       ythresh;              /* number of "yes" words in expr */
};

typedef struct {
  FTB_EXPR *up;
  float     weight;
  int       yesno;
  int       trunc;
  my_off_t  docid;
  uint      ndepth;
  int       len;
  /* ... there can be docid cache added here. SerG */
  byte      word[1];
} FTB_WORD;

typedef struct st_ft_info {
  struct _ft_vft *please;
  MI_INFO  *info;
  uint       keynr;
  enum { UNINITIALIZED, READY, INDEX_SEARCH, INDEX_DONE, SCAN } state;
  FTB_EXPR  *root;
  QUEUE      queue;
  MEM_ROOT   mem_root;
} FTB;

int FTB_WORD_cmp(void *v, byte *a, byte *b)
{
  /* ORDER BY docid, ndepth DESC */
  int i=CMP_NUM(((FTB_WORD *)a)->docid, ((FTB_WORD *)b)->docid);
  if (!i)
    i=CMP_NUM(((FTB_WORD *)b)->ndepth,((FTB_WORD *)a)->ndepth);
  return i;
}

void _ftb_parse_query(FTB *ftb, byte **start, byte *end,
                          FTB_EXPR *up, uint ndepth, uint depth)
{
  byte        res;
  FTB_PARAM   param;
  FT_WORD     w;
  FTB_WORD   *ftbw;
  FTB_EXPR   *ftbe;
  uint  extra=HA_FT_WLEN+ftb->info->s->rec_reflength; /* just a shortcut */

  if (ftb->state != UNINITIALIZED)
    return;

  param.prev=' ';
  while (res=ft_get_word(start,end,&w,&param))
  {
    byte  r=param.plusminus;
    float weight=(param.pmsign ? nwghts : wghts)[(r>5)?5:((r<-5)?-5:r)];
    switch (res) {
      case 1: /* word found */
        ftbw=(FTB_WORD *)alloc_root(&ftb->mem_root,
            sizeof(FTB_WORD) + (param.trunc ? MI_MAX_KEY_BUFF : w.len+extra));
        ftbw->len=w.len+1;
        ftbw->yesno=param.yesno;
        ftbw->trunc=param.trunc; /* 0 or 1 */
        ftbw->weight=weight;
        ftbw->up=up;
        ftbw->docid=HA_POS_ERROR;
        ftbw->ndepth= param.yesno<0 ? depth : ndepth;
        memcpy(ftbw->word+1, w.pos, w.len);
        ftbw->word[0]=w.len;
        if (ftbw->yesno > 0) up->ythresh++;
        queue_insert(& ftb->queue, (byte *)ftbw);
        break;
      case 2: /* left bracket */
        ftbe=(FTB_EXPR *)alloc_root(&ftb->mem_root, sizeof(FTB_EXPR));
        ftbe->yesno=param.yesno;
        ftbe->weight=weight;
        ftbe->up=up;
        ftbe->ythresh=0;
        ftbe->docid=HA_POS_ERROR;
        if (ftbe->yesno > 0) up->ythresh++;
        _ftb_parse_query(ftb, start, end, ftbe, depth+1,
                         (param.yesno<0 ? depth+1 : ndepth));
        break;
      case 3: /* right bracket */
        return;
    }
  }
  return;
}

void  _ftb_init_index_search(FT_INFO *ftb)
{
  int i, r;
  FTB_WORD *ftbw;
  MI_INFO    *info=ftb->info;
  MI_KEYDEF  *keyinfo;
  my_off_t    keyroot;

  if (ftb->state != READY || ftb->keynr == NO_SUCH_KEY)
    return;
  ftb->state=INDEX_SEARCH;

  keyinfo=info->s->keyinfo+ftb->keynr;
  keyroot=info->s->state.key_root[ftb->keynr];

  for (i=ftb->queue.elements; i; i--)
  {
    ftbw=(FTB_WORD *)(ftb->queue.root[i]);

    r=_mi_search(info, keyinfo, (uchar*) ftbw->word, ftbw->len,
                 SEARCH_FIND | SEARCH_PREFIX, keyroot);
    if (!r)
    {
      r=_mi_compare_text(default_charset_info,
                         info->lastkey+ftbw->trunc,ftbw->len-ftbw->trunc,
                         (uchar*) ftbw->word+ftbw->trunc,ftbw->len-ftbw->trunc,
			 0);
    }
    if (r) /* not found */
    {
      if (ftbw->yesno>0 && ftbw->up->up==0)
      { /* this word MUST BE present in every document returned,
           so we can abort the search right now */
        ftb->state=INDEX_DONE;
        return;
      }
    }
    else
    {
      memcpy(ftbw->word, info->lastkey, info->lastkey_length);
      ftbw->docid=info->lastpos;
    }
  }
  queue_fix(& ftb->queue);
}

FT_INFO * ft_init_boolean_search(MI_INFO *info, uint keynr, byte *query,
                    uint query_len, my_bool presort __attribute__((unused)))
{
  FTB       *ftb;
  FTB_EXPR  *ftbe;
  uint       res;

  if (!(ftb=(FTB *)my_malloc(sizeof(FTB), MYF(MY_WME))))
    return 0;
  ftb->please= (struct _ft_vft *) & _ft_vft_boolean;
  ftb->state=UNINITIALIZED;
  ftb->info=info;
  ftb->keynr=keynr;

  init_alloc_root(&ftb->mem_root, 1024, 1024);

  /* hack: instead of init_queue, we'll use reinit queue to be able
   * to alloc queue with alloc_root()
   */
  res=ftb->queue.max_elements=query_len/(ft_min_word_len+1);
  ftb->queue.root=(byte **)alloc_root(&ftb->mem_root, (res+1)*sizeof(void*));
  reinit_queue(& ftb->queue, res, 0, 0, FTB_WORD_cmp, ftb);
  ftbe=(FTB_EXPR *)alloc_root(&ftb->mem_root, sizeof(FTB_EXPR));
  ftbe->weight=ftbe->yesno=ftbe->nos=1;
  ftbe->up=0;
  ftbe->ythresh=0;
  ftbe->docid=HA_POS_ERROR;
  ftb->root=ftbe;
  _ftb_parse_query(ftb, &query, query+query_len, ftbe, 0, 0);
  ftb->state=READY;
  return ftb;
}

void _ftb_climb_the_tree(FTB_WORD *ftbw, my_off_t curdoc)
{
  FTB_EXPR *ftbe;
  float weight=ftbw->weight;
  int  yn=ftbw->yesno;

  for (ftbe=ftbw->up; ftbe; ftbe=ftbe->up)
  {
    if (ftbe->docid != curdoc)
    {
      ftbe->cur_weight=ftbe->yesses=ftbe->nos=0;
      ftbe->docid=curdoc;
    }
    if (yn>0)
    {
      ftbe->cur_weight+=weight;
      if (++ftbe->yesses >= ftbe->ythresh && !ftbe->nos)
      {
        yn=ftbe->yesno;
        weight=ftbe->cur_weight*ftbe->weight;
      }
      else
        break;
    }
    else
    if (yn<0)
    {
     /* NOTE: special sort function of queue assures that all yn<0
      * events for every particular subexpression will
      * "auto-magically" happen BEFORE all the yn>=0 events. So no
      * already matched expression can become not-matched again.
      */
      ++ftbe->nos;
      break;
    }
    else
 /* if (yn==0) */
    {
      ftbe->cur_weight+=weight;
      if (ftbe->yesses >= ftbe->ythresh && !ftbe->nos)
      {
        yn=ftbe->yesno;
        weight*=ftbe->weight;
      }
      else
      {
        break;
      }
    }
  }
}

int ft_boolean_read_next(FT_INFO *ftb, char *record)
{
  FTB_EXPR  *ftbe, *up;
  FTB_WORD  *ftbw;
  MI_INFO   *info=ftb->info;
  MI_KEYDEF *keyinfo=info->s->keyinfo+ftb->keynr;
  my_off_t   keyroot=info->s->state.key_root[ftb->keynr];
  my_off_t   curdoc;
  int        r;

  if (ftb->state != INDEX_SEARCH && ftb->state != INDEX_DONE)
    return -1;
  
  /* black magic ON */
  if ((int) _mi_check_index(info, ftb->keynr) < 0)
    return my_errno;
  if (_mi_readinfo(info, F_RDLCK, 1))
    return my_errno;
  /* black magic OFF */

  if (!ftb->queue.elements)
    return my_errno=HA_ERR_END_OF_FILE;

  while(ftb->state == INDEX_SEARCH &&
    (curdoc=((FTB_WORD *)queue_top(& ftb->queue))->docid) != HA_POS_ERROR)
  {
    while (curdoc==(ftbw=(FTB_WORD *)queue_top(& ftb->queue))->docid)
    {
      _ftb_climb_the_tree(ftbw, curdoc);

      /* update queue */
      r=_mi_search(info, keyinfo, (uchar*) ftbw->word, USE_WHOLE_KEY,
                   SEARCH_BIGGER , keyroot);
      if (!r)
      {
        r=_mi_compare_text(default_charset_info,
                           info->lastkey+ftbw->trunc,ftbw->len-ftbw->trunc,
                           (uchar*) ftbw->word+ftbw->trunc,
			   ftbw->len-ftbw->trunc,0);
      }
      if (r) /* not found */
      {
        ftbw->docid=HA_POS_ERROR;
        if (ftbw->yesno>0 && ftbw->up->up==0)
        { /* this word MUST BE present in every document returned,
             so we can stop the search right now */
          ftb->state=INDEX_DONE;
        }
      }
      else
      {
        memcpy(ftbw->word, info->lastkey, info->lastkey_length);
        ftbw->docid=info->lastpos;
      }
      queue_replaced(& ftb->queue);
    }

    ftbe=ftb->root;
    if (ftbe->docid==curdoc && ftbe->cur_weight>0 &&
        ftbe->yesses>=ftbe->ythresh && !ftbe->nos)
    {
      /* curdoc matched ! */
      info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED); /* why is this ? */

      info->lastpos=curdoc;
      if (!(*info->read_record)(info,curdoc,record))
      {
        info->update|= HA_STATE_AKTIV;          /* Record is read */
        return 0;
      }
      return my_errno;
    }
  }
  ftb->state=INDEX_DONE;
  return my_errno=HA_ERR_END_OF_FILE;
}

float ft_boolean_find_relevance(FT_INFO *ftb, byte *record, uint length)
{
  TREE      ptree;
  FT_WORD   word;
  FTB_WORD *ftbw;
  FTB_EXPR *ftbe;
  uint      i;
  my_off_t  docid=ftb->info->lastpos;

  if (docid == HA_POS_ERROR)
    return -2.0;
  if (ftb->state == READY || ftb->state == INDEX_DONE)
  {
    for (i=1; i<=ftb->queue.elements; i++)
    {
      ftbw=(FTB_WORD *)(ftb->queue.root[i]);
      ftbw->docid=HA_POS_ERROR;
      for (ftbe=ftbw->up; ftbe; ftbe=ftbe->up)
      {
        if (ftbe->docid != HA_POS_ERROR)
        {
          ftbe->cur_weight=ftbe->yesses=ftbe->nos=0;
          ftbe->docid=HA_POS_ERROR;
        }
        else
          break;
      }
    }

    queue_fix(& ftb->queue);
    ftb->state=SCAN;
  }
  else if (ftb->state != SCAN)
    return -3.0;

  bzero(&ptree, sizeof(ptree));
  if ((ftb->keynr==NO_SUCH_KEY)
       ? ft_parse(& ptree, record, length)
       : _mi_ft_parse(& ptree, ftb->info, ftb->keynr, record))
    return -4.0;

  for (i=1; i<=ftb->queue.elements; i++)
  {
    ftbw=(FTB_WORD *)(ftb->queue.root[i]);
    ptree.custom_arg=(void *)(ftbw->trunc);
    word.pos=ftbw->word+1;
    word.len=ftbw->len-1;
    if (tree_search(& ptree, & word))
    { /* found! */
      _ftb_climb_the_tree(ftbw, docid);
    }
    else
    { /* not found! */
      if (ftbw->yesno>0 && ftbw->up->up==0)
      { /* but this word MUST BE present in every document matched,
           so we can stop the search right now */
        break;
      }
    }
  }
  delete_tree(& ptree);
  ftbe=ftb->root;
  if (ftbe->docid==docid && ftbe->cur_weight>0 &&
      ftbe->yesses>=ftbe->ythresh && !ftbe->nos)
  { /* row matched ! */
    return ftbe->cur_weight;
  }
  else
  { /* match failed ! */
    return 0.0;
  }
}

void ft_boolean_close_search(FT_INFO *ftb)
{
  free_root(& ftb->mem_root, MYF(0));
  my_free((gptr)ftb,MYF(0));
}

float ft_boolean_get_relevance(FT_INFO *ftb)
{
  return ftb->root->cur_weight;
}

void ft_boolean_reinit_search(FT_INFO *ftb)
{
  _ftb_init_index_search(ftb);
}

