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

/* queries isam and returns list of documents matched */

typedef struct st_all_in_one {
  MI_INFO    *info;
  uint	      keynr;
  uchar      *keybuff;
  MI_KEYDEF  *keyinfo;
  my_off_t    key_root;
  TREE	      dtree;
} ALL_IN_ONE;

typedef struct st_ft_superdoc {
    FT_DOC   doc;
    FT_WORD *word_ptr;
    double   tmp_weight;
} FT_SUPERDOC;

static int FT_SUPERDOC_cmp(FT_SUPERDOC *p1, FT_SUPERDOC *p2)
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
#ifdef EVAL_RUN
  uint	       cnt;
  double       sum, sum2, suml;
#endif /* EVAL_RUN */
  FT_SUPERDOC  sdoc, *sptr;
  TREE_ELEMENT *selem;
#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float tmp_weight;
#else
#error
#endif

  word->weight=LWS_FOR_QUERY;

  keylen=_ft_make_key(aio->info,aio->keynr,(char*) aio->keybuff,word,0);
#ifdef EVAL_RUN
  keylen-=1+HA_FT_WLEN;
#else /* EVAL_RUN */
  keylen-=HA_FT_WLEN;
#endif /* EVAL_RUN */

#ifdef EVAL_RUN
  sum=sum2=suml=
#endif /* EVAL_RUN */
  doc_cnt=0;

  r=_mi_search(aio->info, aio->keyinfo, aio->keybuff, keylen,
	       SEARCH_FIND | SEARCH_PREFIX, aio->key_root);

  while(!r)
  {
    if (_mi_compare_text(default_charset_info,
			 aio->info->lastkey,keylen,
			 aio->keybuff,keylen,0)) break;

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
#ifdef EVAL_RUN
    mi_float4get(tmp_weight,aio->info->lastkey+keylen+1);
#else /* EVAL_RUN */
    mi_float4get(tmp_weight,aio->info->lastkey+keylen);
#endif /* EVAL_RUN */
#else
#error
#endif
    if(tmp_weight==0) return doc_cnt; /* stopword, doc_cnt should be 0 */

#ifdef EVAL_RUN
    cnt=*(byte *)(aio->info->lastkey+keylen);
#endif /* EVAL_RUN */

    sdoc.doc.dpos=aio->info->lastpos;

    /* saving document matched into dtree */
    if(!(selem=tree_insert(&aio->dtree, &sdoc, 0))) return 1;

    sptr=(FT_SUPERDOC *)ELEMENT_KEY((&aio->dtree), selem);

    if(selem->count==1) /* document's first match */
      sptr->doc.weight=0;
    else
      sptr->doc.weight+=sptr->tmp_weight*sptr->word_ptr->weight;

    sptr->word_ptr=word;
    sptr->tmp_weight=tmp_weight;

    doc_cnt++;
#ifdef EVAL_RUN
    sum +=cnt;
    sum2+=cnt*cnt;
    suml+=cnt*log(cnt);
#endif /* EVAL_RUN */

    if (_mi_test_if_changed(aio->info) == 0)
	r=_mi_search_next(aio->info, aio->keyinfo, aio->info->lastkey,
			  aio->info->lastkey_length, SEARCH_BIGGER,
			  aio->key_root);
    else
	r=_mi_search(aio->info, aio->keyinfo, aio->info->lastkey,
		     aio->info->lastkey_length, SEARCH_BIGGER,
		     aio->key_root);
  }
  if(doc_cnt) {
    word->weight*=GWS_IN_USE;
    if(word->weight < 0) word->weight=0;
  }

  return 0;
}

static int walk_and_copy(FT_SUPERDOC *from,
			 uint32 count __attribute__((unused)), FT_DOC **to)
{
    from->doc.weight+=from->tmp_weight*from->word_ptr->weight;
    (*to)->dpos=from->doc.dpos;
    (*to)->weight=from->doc.weight;
    (*to)++;
    return 0;
}

static int FT_DOC_cmp(FT_DOC *a, FT_DOC *b)
{
    return sgn(b->weight - a->weight);
}

FT_DOCLIST * ft_init_search(void *info, uint keynr, byte *key,
			    uint key_len, my_bool presort)
{
  TREE	     *wtree;
  ALL_IN_ONE aio;
  FT_DOCLIST *dlist;
  FT_DOC     *dptr;
  my_off_t saved_lastpos=((MI_INFO *)info)->lastpos;

/* black magic ON */
  if ((int) (keynr = _mi_check_index((MI_INFO *)info,keynr)) < 0)
    return NULL;
  if (_mi_readinfo((MI_INFO *)info,F_RDLCK,1))
    return NULL;
/* black magic OFF */

  dlist=NULL;
  aio.info=(MI_INFO *)info;
  aio.keynr=keynr;
  aio.keybuff=aio.info->lastkey+aio.info->s->base.max_key_length;
  aio.keyinfo=aio.info->s->keyinfo+keynr;
  aio.key_root=aio.info->s->state.key_root[keynr];

  if (!(wtree=ft_parse(NULL,key,key_len))) return NULL;

  init_tree(&aio.dtree,0,sizeof(FT_SUPERDOC),(qsort_cmp)&FT_SUPERDOC_cmp,0,
	    NULL);

  if (tree_walk(wtree, (tree_walk_action)&walk_and_match, &aio,
		left_root_right))
    goto err;

  dlist=(FT_DOCLIST *) my_malloc(sizeof(FT_DOCLIST)+sizeof(FT_DOC)*
				 (aio.dtree.elements_in_tree-1),MYF(0));
  if (!dlist)
    goto err;

  dlist->ndocs=aio.dtree.elements_in_tree;
  dlist->curdoc=-1;
  dlist->info=aio.info;
  dptr=dlist->doc;

  tree_walk(&aio.dtree, (tree_walk_action)&walk_and_copy, &dptr,
	    left_root_right);

  if (presort)
  {
    qsort(dlist->doc, dlist->ndocs, sizeof(FT_DOC), (qsort_cmp)&FT_DOC_cmp);
  }

err:
  delete_tree(&aio.dtree);
  delete_tree(wtree);
  my_free((char*) wtree,MYF(0));
  ((MI_INFO *)info)->lastpos=saved_lastpos;
  return dlist;
}

int ft_read_next(FT_DOCLIST *handler, char *record)
{
  MI_INFO *info=handler->info;

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
