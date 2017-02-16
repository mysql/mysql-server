/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#define FT_CORE
#include "ma_ftdefs.h"

/* search with natural language queries */

typedef struct ft_doc_rec
{
  my_off_t  dpos;
  double    weight;
} FT_DOC;

struct st_ft_info
{
  struct _ft_vft *please;
  MARIA_HA  *info;
  int       ndocs;
  int       curdoc;
  FT_DOC    doc[1];
};

typedef struct st_all_in_one
{
  MARIA_HA    *info;
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
  uint	       doc_cnt;
  FT_SUPERDOC  sdoc, *sptr;
  TREE_ELEMENT *selem;
  double       gweight=1;
  MARIA_HA     *info= aio->info;
  uchar        *keybuff= aio->keybuff;
  MARIA_KEYDEF *keyinfo= info->s->keyinfo+aio->keynr;
  my_off_t     key_root=info->s->state.key_root[aio->keynr];
  uint         extra=HA_FT_WLEN+info->s->rec_reflength;
  MARIA_KEY    key;
#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float tmp_weight;
#else
#error
#endif
  DBUG_ENTER("walk_and_match");
  LINT_INIT(subkeys.i);

  LINT_INIT_STRUCT(subkeys);

  word->weight=LWS_FOR_QUERY;

  _ma_ft_make_key(info, &key, aio->keynr, keybuff, word, 0);
  key.data_length-= HA_FT_WLEN;
  doc_cnt=0;

  /* Skip rows inserted by current inserted */
  for (r= _ma_search(info, &key, SEARCH_FIND, key_root) ;
       !r &&
         (subkeys.i= ft_sintXkorr(info->last_key.data +
                                  info->last_key.data_length +
                                  info->last_key.ref_length - extra)) > 0 &&
         info->cur_row.lastpos >= info->state->data_file_length ;
       r= _ma_search_next(info, &info->last_key, SEARCH_BIGGER, key_root))
    ;

  info->update|= HA_STATE_AKTIV;              /* for _ma_test_if_changed() */

  /* The following should be safe, even if we compare doubles */
  while (!r && gweight)
  {

    if (key.data_length &&
        ha_compare_text(aio->charset,
                        info->last_key.data+1,
                        info->last_key.data_length +
                        info->last_key.ref_length - extra - 1,
                        key.data+1, key.data_length-1, 0, 0))
     break;

    if (subkeys.i < 0)
    {
      if (doc_cnt)
        DBUG_RETURN(1); /* index is corrupted */
      /*
        TODO here: unsafe optimization, should this word
        be skipped (based on subkeys) ?
      */
      keybuff+= key.data_length;
      keyinfo= &info->s->ft2_keyinfo;
      key_root= info->cur_row.lastpos;
      key.data_length= 0;
      r= _ma_search_first(info, keyinfo, key_root);
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

    sdoc.doc.dpos= info->cur_row.lastpos;

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

    if (_ma_test_if_changed(info) == 0)
	r= _ma_search_next(info, &info->last_key, SEARCH_BIGGER, key_root);
    else
	r= _ma_search(info, &info->last_key, SEARCH_BIGGER, key_root);
do_skip:
    while ((subkeys.i= ft_sintXkorr(info->last_key.data +
                                    info->last_key.data_length +
                                    info->last_key.ref_length - extra)) > 0 &&
           !r && info->cur_row.lastpos >= info->state->data_file_length)
      r= _ma_search_next(info, &info->last_key, SEARCH_BIGGER, key_root);

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


FT_INFO *maria_ft_init_nlq_search(MARIA_HA *info, uint keynr, uchar *query,
                                  uint query_len, uint flags, uchar *record)
{
  TREE	      wtree;
  ALL_IN_ONE  aio;
  FT_DOC     *dptr;
  FT_INFO    *dlist=NULL;
  MARIA_RECORD_POS saved_lastpos= info->cur_row.lastpos;
  struct st_mysql_ftparser *parser;
  MYSQL_FTPARSER_PARAM *ftparser_param;
  DBUG_ENTER("maria_ft_init_nlq_search");

  /* black magic ON */
  if ((int) (keynr = _ma_check_index(info,keynr)) < 0)
    DBUG_RETURN(NULL);
  if (_ma_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(NULL);
  /* black magic OFF */

  aio.info=info;
  aio.keynr=keynr;
  aio.charset=info->s->keyinfo[keynr].seg->charset;
  aio.keybuff= info->lastkey_buff2;
  parser= info->s->keyinfo[keynr].parser;
  if (! (ftparser_param= maria_ftparser_call_initializer(info, keynr, 0)))
    goto err;

  bzero(&wtree,sizeof(wtree));

  init_tree(&aio.dtree,0,0,sizeof(FT_SUPERDOC),(qsort_cmp2)&FT_SUPERDOC_cmp,0,
            NULL, NULL);

  maria_ft_parse_init(&wtree, aio.charset);
  ftparser_param->flags= 0;
  if (maria_ft_parse(&wtree, query, query_len, parser, ftparser_param,
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
      if (!(*info->read_record)(info, record, docid))
      {
        info->update|= HA_STATE_AKTIV;
        ftparser_param->flags= MYSQL_FTFLAGS_NEED_COPY;
        if (unlikely(_ma_ft_parse(&wtree, info, keynr, record, ftparser_param,
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

  dlist->please= (struct _ft_vft *) & _ma_ft_vft_nlq;
  dlist->ndocs=aio.dtree.elements_in_tree;
  dlist->curdoc=-1;
  dlist->info=aio.info;
  dptr=dlist->doc;

  tree_walk(&aio.dtree, (tree_walk_action) &walk_and_copy,
	    &dptr, left_root_right);

  if (flags & FT_SORTED)
    my_qsort2(dlist->doc, dlist->ndocs, sizeof(FT_DOC),
              (qsort2_cmp)&FT_DOC_cmp, 0);

err:
  delete_tree(&aio.dtree);
  delete_tree(&wtree);
  info->cur_row.lastpos= saved_lastpos;
  DBUG_RETURN(dlist);
}


int maria_ft_nlq_read_next(FT_INFO *handler, char *record)
{
  MARIA_HA *info= (MARIA_HA *) handler->info;

  if (++handler->curdoc >= handler->ndocs)
  {
    --handler->curdoc;
    return HA_ERR_END_OF_FILE;
  }

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  info->cur_row.lastpos= handler->doc[handler->curdoc].dpos;
  if (!(*info->read_record)(info, (uchar *) record, info->cur_row.lastpos))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return 0;
  }
  return my_errno;
}


float maria_ft_nlq_find_relevance(FT_INFO *handler,
			    uchar *record __attribute__((unused)),
			    uint length __attribute__((unused)))
{
  int a,b,c;
  FT_DOC  *docs=handler->doc;
  MARIA_RECORD_POS docid= handler->info->cur_row.lastpos;

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


void maria_ft_nlq_close_search(FT_INFO *handler)
{
  my_free(handler);
}


float maria_ft_nlq_get_relevance(FT_INFO *handler)
{
  return (float) handler->doc[handler->curdoc].weight;
}


void maria_ft_nlq_reinit_search(FT_INFO *handler)
{
  handler->curdoc=-1;
}

