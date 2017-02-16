/* Copyright (c) 2001, 2011, Oracle and/or its affiliates.

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
  FT_WEIGTH    subkeys;
  int          r;
  uint	       keylen, doc_cnt;
  FT_SUPERDOC  sdoc, *sptr;
  TREE_ELEMENT *selem;
  double       gweight=1;
  MI_INFO      *info=aio->info;
  MYISAM_SHARE *share= info->s;
  uchar        *keybuff=aio->keybuff;
  MI_KEYDEF    *keyinfo=info->s->keyinfo+aio->keynr;
  my_off_t     key_root;
  uint         extra= HA_FT_WLEN + info->s->rec_reflength;
#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float tmp_weight;
#else
#error
#endif
  DBUG_ENTER("walk_and_match");
  LINT_INIT_STRUCT(subkeys);

  word->weight=LWS_FOR_QUERY;

  keylen=_ft_make_key(info,aio->keynr,keybuff,word,0);
  keylen-=HA_FT_WLEN;
  doc_cnt=0;

  if (share->concurrent_insert)
    mysql_rwlock_rdlock(&share->key_root_lock[aio->keynr]);

  key_root= share->state.key_root[aio->keynr];

  /* Skip rows inserted by current inserted */
  for (r=_mi_search(info, keyinfo, keybuff, keylen, SEARCH_FIND, key_root) ;
       !r &&
         (subkeys.i= ft_sintXkorr(info->lastkey+info->lastkey_length-extra)) > 0 &&
         info->lastpos >= info->state->data_file_length ;
       r= _mi_search_next(info, keyinfo, info->lastkey,
                          info->lastkey_length, SEARCH_BIGGER, key_root))
    ;

  if (share->concurrent_insert)
    mysql_rwlock_unlock(&share->key_root_lock[aio->keynr]);

  info->update|= HA_STATE_AKTIV;              /* for _mi_test_if_changed() */

  /* The following should be safe, even if we compare doubles */
  while (!r && gweight)
  {

    if (keylen &&
        ha_compare_text(aio->charset,info->lastkey+1,
                        info->lastkey_length-extra-1, keybuff+1,keylen-1,0,0))
     break;

    if (subkeys.i < 0)
    {
      if (doc_cnt)
        DBUG_RETURN(1); /* index is corrupted */
      /*
        TODO here: unsafe optimization, should this word
        be skipped (based on subkeys) ?
      */
      keybuff+=keylen;
      keyinfo=& info->s->ft2_keyinfo;
      key_root=info->lastpos;
      keylen=0;
      if (share->concurrent_insert)
        mysql_rwlock_rdlock(&share->key_root_lock[aio->keynr]);
      r=_mi_search_first(info, keyinfo, key_root);
      goto do_skip;
    }
#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
    /* The weight we read was actually a float */
    tmp_weight= subkeys.f;
#else
#error
#endif
  /* The following should be safe, even if we compare doubles */
    if (tmp_weight==0)
      DBUG_RETURN(doc_cnt); /* stopword, doc_cnt should be 0 */

    sdoc.doc.dpos=info->lastpos;

    /* saving document matched into dtree */
    if (!(selem=tree_insert(&aio->dtree, &sdoc, 0, aio->dtree.custom_arg)))
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

    if (share->concurrent_insert)
      mysql_rwlock_rdlock(&share->key_root_lock[aio->keynr]);

    if (_mi_test_if_changed(info) == 0)
	r=_mi_search_next(info, keyinfo, info->lastkey, info->lastkey_length,
                          SEARCH_BIGGER, key_root);
    else
	r=_mi_search(info, keyinfo, info->lastkey, info->lastkey_length,
                     SEARCH_BIGGER, key_root);
do_skip:
    while ((subkeys.i= ft_sintXkorr(info->lastkey+info->lastkey_length-extra)) > 0 &&
           !r && info->lastpos >= info->state->data_file_length)
      r= _mi_search_next(info, keyinfo, info->lastkey, info->lastkey_length,
                         SEARCH_BIGGER, key_root);

    if (share->concurrent_insert)
      mysql_rwlock_unlock(&share->key_root_lock[aio->keynr]);
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

static int walk_and_push(FT_SUPERDOC *from,
			 uint32 count __attribute__((unused)), QUEUE *best)
{
  DBUG_ENTER("walk_and_copy");
  from->doc.weight+=from->tmp_weight*from->word_ptr->weight;
  set_if_smaller(best->elements, ft_query_expansion_limit-1);
  queue_insert(best, (uchar *)& from->doc);
  DBUG_RETURN(0);
}


static int FT_DOC_cmp(void *unused __attribute__((unused)),
                      FT_DOC *a, FT_DOC *b)
{
  return CMP_NUM(b->weight, a->weight);
}


FT_INFO *ft_init_nlq_search(MI_INFO *info, uint keynr, uchar *query,
			    uint query_len, uint flags, uchar *record)
{
  TREE	      wtree;
  ALL_IN_ONE  aio;
  FT_DOC     *dptr;
  FT_INFO    *dlist=NULL;
  my_off_t    saved_lastpos=info->lastpos;
  struct st_mysql_ftparser *parser;
  MYSQL_FTPARSER_PARAM *ftparser_param;
  DBUG_ENTER("ft_init_nlq_search");

/* black magic ON */
  if ((int) (keynr = _mi_check_index(info,keynr)) < 0)
    DBUG_RETURN(NULL);
  if (_mi_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(NULL);
/* black magic OFF */

  aio.info=info;
  aio.keynr=keynr;
  aio.charset=info->s->keyinfo[keynr].seg->charset;
  aio.keybuff=info->lastkey+info->s->base.max_key_length;
  parser= info->s->keyinfo[keynr].parser;
  if (! (ftparser_param= ftparser_call_initializer(info, keynr, 0)))
    goto err;

  bzero(&wtree,sizeof(wtree));

  init_tree(&aio.dtree,0,0,sizeof(FT_SUPERDOC),(qsort_cmp2)&FT_SUPERDOC_cmp,0,
            NULL, NULL);

  ft_parse_init(&wtree, aio.charset);
  ftparser_param->flags= 0;
  if (ft_parse(&wtree, query, query_len, parser, ftparser_param,
               &wtree.mem_root))
    goto err;

  if (tree_walk(&wtree, (tree_walk_action)&walk_and_match, &aio,
		left_root_right))
    goto err;

  if (flags & FT_EXPAND && ft_query_expansion_limit)
  {
    QUEUE best;
    init_queue(&best,ft_query_expansion_limit,0,0, (queue_compare) &FT_DOC_cmp,
	       0, 0, 0);
    tree_walk(&aio.dtree, (tree_walk_action) &walk_and_push,
              &best, left_root_right);
    while (best.elements)
    {
      my_off_t docid= ((FT_DOC *)queue_remove_top(&best))->dpos;
      if (!(*info->read_record)(info,docid,record))
      {
        info->update|= HA_STATE_AKTIV;
        ftparser_param->flags= MYSQL_FTFLAGS_NEED_COPY;
        if (unlikely(_mi_ft_parse(&wtree, info, keynr, record, ftparser_param,
                                  &wtree.mem_root)))
        {
          delete_queue(&best);
          goto err;
        }
      }
    }
    delete_queue(&best);
    reset_tree(&aio.dtree);
    if (tree_walk(&wtree, (tree_walk_action)&walk_and_match, &aio,
                  left_root_right))
      goto err;

  }

  /*
    If ndocs == 0, this will not allocate RAM for FT_INFO.doc[],
    so if ndocs == 0, FT_INFO.doc[] must not be accessed.
   */
  dlist=(FT_INFO *)my_malloc(sizeof(FT_INFO)+
			     sizeof(FT_DOC)*
			     (int)(aio.dtree.elements_in_tree-1),
			     MYF(0));
  if (!dlist)
    goto err;

  dlist->please= (struct _ft_vft *) & _ft_vft_nlq;
  dlist->ndocs=aio.dtree.elements_in_tree;
  dlist->curdoc=-1;
  dlist->info=aio.info;
  dptr=dlist->doc;

  tree_walk(&aio.dtree, (tree_walk_action) &walk_and_copy,
	    &dptr, left_root_right);

  if (flags & FT_SORTED)
    my_qsort2(dlist->doc, dlist->ndocs, sizeof(FT_DOC), (qsort2_cmp)&FT_DOC_cmp,
              0);

err:
  delete_tree(&aio.dtree);
  delete_tree(&wtree);
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
  if (!(*info->read_record)(info,info->lastpos,(uchar*) record))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return 0;
  }
  return my_errno;
}


float ft_nlq_find_relevance(FT_INFO *handler,
			    uchar *record __attribute__((unused)),
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
  my_free(handler);
}


float ft_nlq_get_relevance(FT_INFO *handler)
{
  return (float) handler->doc[handler->curdoc].weight;
}


void ft_nlq_reinit_search(FT_INFO *handler)
{
  handler->curdoc=-1;
}

