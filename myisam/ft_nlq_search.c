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

#define FT_CORE
#include "ftdefs.h"

/* search with natural language queries */

typedef struct ft_doc_rec
{
  my_off_t  dpos;
  double    weight;
} FT_DOC;

struct st_ft_info
{
  struct _ft_vft *please;
  MI_INFO  *info;
  int       ndocs;
  int       curdoc;
  FT_DOC    doc[1];
};

typedef struct st_all_in_one
{
  MI_INFO    *info;
  uint	      keynr;
  CHARSET_INFO *charset;
  uchar      *keybuff;
  MI_KEYDEF  *keyinfo;
  my_off_t    key_root;
  TREE	      dtree;
} ALL_IN_ONE;

typedef struct st_ft_superdoc
{
    FT_DOC   doc;
    FT_WORD *word_ptr;
    double   tmp_weight;
} FT_SUPERDOC;

static int FT_SUPERDOC_cmp(void* cmp_arg __attribute__((unused)),
			   FT_SUPERDOC *p1, FT_SUPERDOC *p2)
{
  if (p1->doc.dpos < p2->doc.dpos)
    return -1;
  if (p1->doc.dpos == p2->doc.dpos)
    return 0;
  return 1;
}

static int walk_and_match(FT_WORD *word, uint32 count, ALL_IN_ONE *aio)
{
  uint	       keylen, r, doc_cnt;
  FT_SUPERDOC  sdoc, *sptr;
  TREE_ELEMENT *selem;
  double        gweight=1;
#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float tmp_weight;
#else
#error
#endif

  DBUG_ENTER("walk_and_match");

  word->weight=LWS_FOR_QUERY;

  keylen=_ft_make_key(aio->info,aio->keynr,(char*) aio->keybuff,word,0);
  keylen-=HA_FT_WLEN;

  doc_cnt=0;

  for (
      r=_mi_search(aio->info, aio->keyinfo, aio->keybuff, keylen,
                   SEARCH_FIND | SEARCH_PREFIX, aio->key_root) ;
       !r && aio->info->lastpos >= aio->info->state->data_file_length ;
       r=_mi_search_next(aio->info, aio->keyinfo, aio->info->lastkey,
                         aio->info->lastkey_length, SEARCH_BIGGER, aio->key_root)
      );
  aio->info->update|= HA_STATE_AKTIV;  /* for _mi_test_if_changed() */

  while (!r && gweight)
  {
    if (_mi_compare_text(aio->charset,
			 aio->info->lastkey,keylen,
			 aio->keybuff,keylen,0)) break;

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
    mi_float4get(tmp_weight,aio->info->lastkey+keylen);
#else
#error
#endif
    if(tmp_weight==0) DBUG_RETURN(doc_cnt); /* stopword, doc_cnt should be 0 */

    sdoc.doc.dpos=aio->info->lastpos;

    /* saving document matched into dtree */
    if (!(selem=tree_insert(&aio->dtree, &sdoc, 0)))
      DBUG_RETURN(1);

    sptr=(FT_SUPERDOC *)ELEMENT_KEY((&aio->dtree), selem);

    if (selem->count==1) /* document's first match */
      sptr->doc.weight=0;
    else
      sptr->doc.weight+=sptr->tmp_weight*sptr->word_ptr->weight;

    sptr->word_ptr=word;
    sptr->tmp_weight=tmp_weight;

    doc_cnt++;

    gweight=word->weight*GWS_IN_USE;
    if (gweight < 0 || doc_cnt > 2000000)
      gweight=0;

    if (_mi_test_if_changed(aio->info) == 0)
	r=_mi_search_next(aio->info, aio->keyinfo, aio->info->lastkey,
			  aio->info->lastkey_length, SEARCH_BIGGER,
			  aio->key_root);
    else
	r=_mi_search(aio->info, aio->keyinfo, aio->info->lastkey,
		     aio->info->lastkey_length, SEARCH_BIGGER,
		     aio->key_root);

    while (!r && aio->info->lastpos >= aio->info->state->data_file_length)
	r=_mi_search(aio->info, aio->keyinfo, aio->info->lastkey,
		     aio->info->lastkey_length, SEARCH_BIGGER,
		     aio->key_root);
  }

  word->weight=gweight;

  DBUG_RETURN(0);
}


static int walk_and_copy(FT_SUPERDOC *from,
			 uint32 count __attribute__((unused)), FT_DOC **to)
{
  DBUG_ENTER("walk_and_copy");
  from->doc.weight+=from->tmp_weight*from->word_ptr->weight;
  (*to)->dpos=from->doc.dpos;
  (*to)->weight=from->doc.weight;
  (*to)++;
  DBUG_RETURN(0);
}


static int FT_DOC_cmp(FT_DOC *a, FT_DOC *b)
{
  return sgn(b->weight - a->weight);
}


FT_INFO *ft_init_nlq_search(MI_INFO *info, uint keynr, byte *query,
			    uint query_len, my_bool presort)
{
  TREE	     allocated_wtree, *wtree=&allocated_wtree;
  ALL_IN_ONE  aio;
  FT_DOC     *dptr;
  FT_INFO    *dlist=NULL;
  my_off_t    saved_lastpos=info->lastpos;
  DBUG_ENTER("ft_init_nlq_search");

/* black magic ON */
  if ((int) (keynr = _mi_check_index(info,keynr)) < 0)
    DBUG_RETURN(NULL);
  if (_mi_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(NULL);
/* black magic OFF */

  aio.info=info;
  aio.keynr=keynr;
  aio.keyinfo=info->s->keyinfo+keynr;
  aio.charset=aio.keyinfo->seg->charset;
  aio.keybuff=info->lastkey+info->s->base.max_key_length;
  aio.key_root=info->s->state.key_root[keynr];

  bzero(&allocated_wtree,sizeof(allocated_wtree));

  init_tree(&aio.dtree,0,0,sizeof(FT_SUPERDOC),(qsort_cmp2)&FT_SUPERDOC_cmp,0,
            NULL, NULL);

  ft_parse_init(&allocated_wtree, aio.charset);
  if (ft_parse(&allocated_wtree,query,query_len))
    goto err;

  if (tree_walk(wtree, (tree_walk_action)&walk_and_match, &aio,
		left_root_right))
    goto err2;

  /*
    If ndocs == 0, this will not allocate RAM for FT_INFO.doc[],
    so if ndocs == 0, FT_INFO.doc[] must not be accessed.
   */
  dlist=(FT_INFO *)my_malloc(sizeof(FT_INFO)+
			     sizeof(FT_DOC)*(aio.dtree.elements_in_tree-1),
			     MYF(0));
  if(!dlist)
    goto err2;

  dlist->please= (struct _ft_vft *) & _ft_vft_nlq;
  dlist->ndocs=aio.dtree.elements_in_tree;
  dlist->curdoc=-1;
  dlist->info=aio.info;
  dptr=dlist->doc;

  tree_walk(&aio.dtree, (tree_walk_action) &walk_and_copy,
	    &dptr, left_root_right);

  if (presort)
    qsort(dlist->doc, dlist->ndocs, sizeof(FT_DOC), (qsort_cmp)&FT_DOC_cmp);

err2:
  delete_tree(wtree);
  delete_tree(&aio.dtree);

err:
  info->lastpos=saved_lastpos;
  DBUG_RETURN(dlist);
}


int ft_nlq_read_next(FT_INFO *handler, char *record)
{
  MI_INFO *info= (MI_INFO *) handler->info;

  if (++handler->curdoc >= handler->ndocs)
  {
    --handler->curdoc;
    return HA_ERR_END_OF_FILE;
  }

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  info->lastpos=handler->doc[handler->curdoc].dpos;
  if (!(*info->read_record)(info,info->lastpos,record))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return 0;
  }
  return my_errno;
}


float ft_nlq_find_relevance(FT_INFO *handler,
			    byte *record __attribute__((unused)),
			    uint length __attribute__((unused)))
{
  int a,b,c;
  FT_DOC  *docs=handler->doc;
  my_off_t docid=handler->info->lastpos;

  if (docid == HA_POS_ERROR)
    return -5.0;

  /* Assuming docs[] is sorted by dpos... */

  for (a=0, b=handler->ndocs, c=(a+b)/2; b-a>1; c=(a+b)/2)
  {
    if (docs[c].dpos > docid)
      b=c;
    else
      a=c;
  }
  /* bounds check to avoid accessing unallocated handler->doc  */
  if (a < handler->ndocs && docs[a].dpos == docid)
    return (float) docs[a].weight;
  else
    return 0.0;
}


void ft_nlq_close_search(FT_INFO *handler)
{
  my_free((gptr)handler,MYF(0));
}


float ft_nlq_get_relevance(FT_INFO *handler)
{
  return (float) handler->doc[handler->curdoc].weight;
}


void ft_nlq_reinit_search(FT_INFO *handler)
{
  handler->curdoc=-1;
}

