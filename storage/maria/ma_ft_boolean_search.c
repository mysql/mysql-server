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

/*  TODO: add caching - pre-read several index entries at once */

/*
  Added optimization for full-text queries with plus-words. It was
  implemented by sharing maximal document id (max_docid) variable
  inside plus subtree. max_docid could be used by any word in plus
  subtree, but it could be updated by plus-word only.

  Fulltext "smarter index merge" optimization assumes that rows
  it gets are ordered by doc_id. That is not the case when we
  search for a word with truncation operator. It may return
  rows in random order. Thus we may not use "smarter index merge"
  optimization with "trunc-words".

  The idea is: there is no need to search for docid smaller than
  biggest docid inside current plus subtree or any upper plus subtree.

  Examples:
  +word1 word2
    share same max_docid
    max_docid updated by word1
  +word1 +(word2 word3)
    share same max_docid
    max_docid updated by word1
  +(word1 -word2) +(+word3 word4)
    share same max_docid
    max_docid updated by word3
   +word1 word2 (+word3 word4 (+word5 word6))
    three subexpressions (including the top-level one),
    every one has its own max_docid, updated by its plus word.
    but for the search word6 uses
    max(word1.max_docid, word3.max_docid, word5.max_docid),
    while word4 uses, accordingly,
    max(word1.max_docid, word3.max_docid).
*/

#define FT_CORE
#include "ma_ftdefs.h"

/* search with boolean queries */

static double _wghts[11]=
{
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

static double _nwghts[11]=
{
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

#define FTB_FLAG_TRUNC 1
/* At most one of the following flags can be set */
#define FTB_FLAG_YES   2
#define FTB_FLAG_NO    4
#define FTB_FLAG_WONLY 8

typedef struct st_ftb_expr FTB_EXPR;
struct st_ftb_expr
{
  FTB_EXPR *up;
  uint      flags;
/* ^^^^^^^^^^^^^^^^^^ FTB_{EXPR,WORD} common section */
  my_off_t  docid[2];
  my_off_t  max_docid;
  float     weight;
  float     cur_weight;
  LIST     *phrase;               /* phrase words */
  LIST     *document;             /* for phrase search */
  uint      yesses;               /* number of "yes" words matched */
  uint      nos;                  /* number of "no"  words matched */
  uint      ythresh;              /* number of "yes" words in expr */
  uint      yweaks;               /* number of "yes" words for scan only */
};

typedef struct st_ftb_word
{
  FTB_EXPR  *up;
  uint       flags;
/* ^^^^^^^^^^^^^^^^^^ FTB_{EXPR,WORD} common section */
  my_off_t   docid[2];             /* for index search and for scan */
  my_off_t   key_root;
  FTB_EXPR  *max_docid_expr;
  MARIA_KEYDEF *keyinfo;
  struct st_ftb_word *prev;
  float      weight;
  uint       ndepth;
  uint       len;
  uchar      off;
  uchar      word[1];
} FTB_WORD;

typedef struct st_ft_info
{
  struct _ft_vft *please;
  MARIA_HA   *info;
  CHARSET_INFO *charset;
  FTB_EXPR  *root;
  FTB_WORD **list;
  FTB_WORD  *last_word;
  MEM_ROOT   mem_root;
  QUEUE      queue;
  TREE       no_dupes;
  my_off_t   lastpos;
  uint       keynr;
  uchar      with_scan;
  enum { UNINITIALIZED, READY, INDEX_SEARCH, INDEX_DONE } state;
} FTB;

static int FTB_WORD_cmp(my_off_t *v, FTB_WORD *a, FTB_WORD *b)
{
  int i;

  /* if a==curdoc, take it as  a < b */
  if (v && a->docid[0] == *v)
    return -1;

  /* ORDER BY docid, ndepth DESC */
  i=CMP_NUM(a->docid[0], b->docid[0]);
  if (!i)
    i=CMP_NUM(b->ndepth,a->ndepth);
  return i;
}

static int FTB_WORD_cmp_list(CHARSET_INFO *cs, FTB_WORD **a, FTB_WORD **b)
{
  /* ORDER BY word, ndepth */
  int i= ha_compare_text(cs, (uchar*) (*a)->word + 1,(*a)->len - 1,
                             (uchar*) (*b)->word + 1,(*b)->len - 1, 0, 0);
  if (!i)
    i=CMP_NUM((*a)->ndepth, (*b)->ndepth);
  return i;
}


typedef struct st_my_ftb_param
{
  FTB *ftb;
  FTB_EXPR *ftbe;
  uchar *up_quot;
  uint depth;
} MY_FTB_PARAM;


static int ftb_query_add_word(MYSQL_FTPARSER_PARAM *param,
                              const char *word, int word_len,
                              MYSQL_FTPARSER_BOOLEAN_INFO *info)
{
  MY_FTB_PARAM *ftb_param= param->mysql_ftparam;
  FTB_WORD *ftbw;
  FTB_EXPR *ftbe, *tmp_expr;
  FT_WORD *phrase_word;
  LIST *tmp_element;
  int r= info->weight_adjust;
  float weight= (float)
        (info->wasign ? nwghts : wghts)[(r>5)?5:((r<-5)?-5:r)];

  switch (info->type) {
    case FT_TOKEN_WORD:
      ftbw= (FTB_WORD *)alloc_root(&ftb_param->ftb->mem_root,
                                   sizeof(FTB_WORD) +
                                   (info->trunc ? MARIA_MAX_KEY_BUFF :
                                    word_len * ftb_param->ftb->charset->mbmaxlen +
                                    HA_FT_WLEN +
                                    ftb_param->ftb->info->s->rec_reflength));
      ftbw->len= word_len + 1;
      ftbw->flags= 0;
      ftbw->off= 0;
      if (info->yesno > 0) ftbw->flags|= FTB_FLAG_YES;
      if (info->yesno < 0) ftbw->flags|= FTB_FLAG_NO;
      if (info->trunc) ftbw->flags|= FTB_FLAG_TRUNC;
      ftbw->weight= weight;
      ftbw->up= ftb_param->ftbe;
      ftbw->docid[0]= ftbw->docid[1]= HA_OFFSET_ERROR;
      ftbw->ndepth= (info->yesno < 0) + ftb_param->depth;
      ftbw->key_root= HA_OFFSET_ERROR;
      memcpy(ftbw->word + 1, word, word_len);
      ftbw->word[0]= word_len;
      if (info->yesno > 0) ftbw->up->ythresh++;
      ftb_param->ftb->queue.max_elements++;
      ftbw->prev= ftb_param->ftb->last_word;
      ftb_param->ftb->last_word= ftbw;
      ftb_param->ftb->with_scan|= (info->trunc & FTB_FLAG_TRUNC);
      for (tmp_expr= ftb_param->ftbe; tmp_expr->up; tmp_expr= tmp_expr->up)
        if (! (tmp_expr->flags & FTB_FLAG_YES))
          break;
      ftbw->max_docid_expr= tmp_expr;
      /* fall through */
    case FT_TOKEN_STOPWORD:
      if (! ftb_param->up_quot) break;
      phrase_word= (FT_WORD *)alloc_root(&ftb_param->ftb->mem_root, sizeof(FT_WORD));
      tmp_element= (LIST *)alloc_root(&ftb_param->ftb->mem_root, sizeof(LIST));
      phrase_word->pos= (uchar*)word;
      phrase_word->len= word_len;
      tmp_element->data= (void *)phrase_word;
      ftb_param->ftbe->phrase= list_add(ftb_param->ftbe->phrase, tmp_element);
      /* Allocate document list at this point.
         It allows to avoid huge amount of allocs/frees for each row.*/
      tmp_element= (LIST *)alloc_root(&ftb_param->ftb->mem_root, sizeof(LIST));
      tmp_element->data= alloc_root(&ftb_param->ftb->mem_root, sizeof(FT_WORD));
      ftb_param->ftbe->document=
        list_add(ftb_param->ftbe->document, tmp_element);
      break;
    case FT_TOKEN_LEFT_PAREN:
      ftbe=(FTB_EXPR *)alloc_root(&ftb_param->ftb->mem_root, sizeof(FTB_EXPR));
      ftbe->flags= 0;
      if (info->yesno > 0) ftbe->flags|= FTB_FLAG_YES;
      if (info->yesno < 0) ftbe->flags|= FTB_FLAG_NO;
      ftbe->weight= weight;
      ftbe->up= ftb_param->ftbe;
      ftbe->max_docid= ftbe->ythresh= ftbe->yweaks= 0;
      ftbe->docid[0]= ftbe->docid[1]= HA_OFFSET_ERROR;
      ftbe->phrase= NULL;
      ftbe->document= 0;
      if (info->quot) ftb_param->ftb->with_scan|= 2;
      if (info->yesno > 0) ftbe->up->ythresh++;
      ftb_param->ftbe= ftbe;
      ftb_param->depth++;
      ftb_param->up_quot= (uchar*)info->quot;
      break;
    case FT_TOKEN_RIGHT_PAREN:
      if (ftb_param->ftbe->document)
      {
        /* Circuit document list */
        for (tmp_element= ftb_param->ftbe->document;
             tmp_element->next; tmp_element= tmp_element->next) /* no-op */;
        tmp_element->next= ftb_param->ftbe->document;
        ftb_param->ftbe->document->prev= tmp_element;
      }
      info->quot= 0;
      if (ftb_param->ftbe->up)
      {
        DBUG_ASSERT(ftb_param->depth);
        ftb_param->ftbe= ftb_param->ftbe->up;
        ftb_param->depth--;
        ftb_param->up_quot= 0;
      }
      break;
    case FT_TOKEN_EOF:
    default:
      break;
  }
  return(0);
}


static int ftb_parse_query_internal(MYSQL_FTPARSER_PARAM *param,
                                    const char *query, int len)
{
  MY_FTB_PARAM *ftb_param= param->mysql_ftparam;
  MYSQL_FTPARSER_BOOLEAN_INFO info;
  CHARSET_INFO *cs= ftb_param->ftb->charset;
  const uchar **start= (const uchar**) &query;
  uchar *end= (uchar*) query + len;
  FT_WORD w;

  info.prev= ' ';
  info.quot= 0;
  while (maria_ft_get_word(cs, start, end, &w, &info))
    param->mysql_add_word(param, (char*)w.pos, w.len, &info);
  return(0);
}


static int _ftb_parse_query(FTB *ftb, uchar *query, uint len,
                            struct st_mysql_ftparser *parser)
{
  MYSQL_FTPARSER_PARAM *param;
  MY_FTB_PARAM ftb_param;
  DBUG_ENTER("_ftb_parse_query");
  DBUG_ASSERT(parser);

  if (ftb->state != UNINITIALIZED)
    DBUG_RETURN(0);
  if (! (param= maria_ftparser_call_initializer(ftb->info, ftb->keynr, 0)))
    DBUG_RETURN(1);

  ftb_param.ftb= ftb;
  ftb_param.depth= 0;
  ftb_param.ftbe= ftb->root;
  ftb_param.up_quot= 0;

  param->mysql_parse= ftb_parse_query_internal;
  param->mysql_add_word= ftb_query_add_word;
  param->mysql_ftparam= (void *)&ftb_param;
  param->cs= ftb->charset;
  param->doc= (char*)query;
  param->length= len;
  param->flags= 0;
  param->mode= MYSQL_FTPARSER_FULL_BOOLEAN_INFO;
  DBUG_RETURN(parser->parse(param));
}


static int _ftb_no_dupes_cmp(void* not_used __attribute__((unused)),
                             const void *a,const void *b)
{
  return CMP_NUM((*((my_off_t*)a)), (*((my_off_t*)b)));
}


/* returns 1 if the search was finished (must-word wasn't found) */

static int _ft2_search(FTB *ftb, FTB_WORD *ftbw, my_bool init_search)
{
  int r;
  int subkeys=1;
  my_bool can_go_down;
  MARIA_HA *info=ftb->info;
  uint UNINIT_VAR(off), extra=HA_FT_WLEN+info->s->base.rec_reflength;
  uchar *lastkey_buf= ftbw->word+ftbw->off;
  MARIA_KEY key;

  if (ftbw->flags & FTB_FLAG_TRUNC)
    lastkey_buf+=ftbw->len;

  if (init_search)
  {
    ftbw->key_root=info->s->state.key_root[ftb->keynr];
    ftbw->keyinfo=info->s->keyinfo+ftb->keynr;
    info->last_key.keyinfo= key.keyinfo= ftbw->keyinfo;
    info->lastinx= ~0;                            /* Safety */
    key.data= ftbw->word;
    key.data_length= ftbw->len;
    key.ref_length= 0;
    key.flag= 0;

    r= _ma_search(info, &key, SEARCH_FIND | SEARCH_BIGGER, ftbw->key_root);
  }
  else
  {
    uint sflag= SEARCH_BIGGER;
    my_off_t max_docid=0;
    FTB_EXPR *tmp;

    for (tmp= ftbw->max_docid_expr; tmp; tmp= tmp->up)
      set_if_bigger(max_docid, tmp->max_docid);

    if (ftbw->docid[0] < max_docid)
    {
      sflag|= SEARCH_SAME;
      _ma_dpointer(info->s, (uchar*) (ftbw->word + ftbw->len + HA_FT_WLEN),
                   max_docid);
    }

    info->last_key.keyinfo= key.keyinfo= ftbw->keyinfo;
    info->lastinx= ~0;                            /* Safety */
    key.data= lastkey_buf;
    key.data_length= USE_WHOLE_KEY;
    key.ref_length= 0;
    key.flag= 0;

    r= _ma_search(info, &key, sflag, ftbw->key_root);
  }

  can_go_down=(!ftbw->off && (init_search || (ftbw->flags & FTB_FLAG_TRUNC)));
  /* Skip rows inserted by concurrent insert */
  while (!r)
  {
    if (can_go_down)
    {
      /* going down ? */
      off= info->last_key.data_length + info->last_key.ref_length - extra;
      subkeys=ft_sintXkorr(info->last_key.data + off);
    }
    if (subkeys<0 || info->cur_row.lastpos < info->state->data_file_length)
      break;
    r= _ma_search_next(info, &info->last_key, SEARCH_BIGGER, ftbw->key_root);
  }

  if (!r && !ftbw->off)
  {
    r= ha_compare_text(ftb->charset,
                       info->last_key.data+1,
                       info->last_key.data_length + info->last_key.ref_length-
                       extra-1,
                       (uchar*) ftbw->word+1,
                       ftbw->len-1,
                       (my_bool) (ftbw->flags & FTB_FLAG_TRUNC), 0);
  }

  if (r) /* not found */
  {
    if (!ftbw->off || !(ftbw->flags & FTB_FLAG_TRUNC))
    {
      ftbw->docid[0]=HA_OFFSET_ERROR;
      if ((ftbw->flags & FTB_FLAG_YES) && ftbw->up->up==0)
      {
        /*
          This word MUST BE present in every document returned,
          so we can stop the search right now
        */
        ftb->state=INDEX_DONE;
        return 1; /* search is done */
      }
      else
        return 0;
    }

    /* going up to the first-level tree to continue search there */
    _ma_dpointer(info->s, (lastkey_buf+HA_FT_WLEN), ftbw->key_root);
    ftbw->key_root=info->s->state.key_root[ftb->keynr];
    ftbw->keyinfo=info->s->keyinfo+ftb->keynr;
    ftbw->off=0;
    return _ft2_search(ftb, ftbw, 0);
  }

  /* matching key found */
  memcpy(lastkey_buf, info->last_key.data,
         info->last_key.data_length + info->last_key.ref_length);
  if (lastkey_buf == ftbw->word)
    ftbw->len= info->last_key.data_length + info->last_key.ref_length - extra;

  /* going down ? */
  if (subkeys<0)
  {
    /*
      yep, going down, to the second-level tree
      TODO here: subkey-based optimization
    */
    ftbw->off=off;
    ftbw->key_root= info->cur_row.lastpos;
    ftbw->keyinfo=& info->s->ft2_keyinfo;
    r= _ma_search_first(info, ftbw->keyinfo, ftbw->key_root);
    DBUG_ASSERT(r==0);  /* found something */
    memcpy(lastkey_buf+off, info->last_key.data,
           info->last_key.data_length + info->last_key.ref_length);
  }
  ftbw->docid[0]= info->cur_row.lastpos;
  if (ftbw->flags & FTB_FLAG_YES && !(ftbw->flags & FTB_FLAG_TRUNC))
    ftbw->max_docid_expr->max_docid= info->cur_row.lastpos;
  return 0;
}

static void _ftb_init_index_search(FT_INFO *ftb)
{
  int i;
  FTB_WORD   *ftbw;

  if (ftb->state == UNINITIALIZED || ftb->keynr == NO_SUCH_KEY)
    return;
  ftb->state=INDEX_SEARCH;

  for (i= queue_last_element(&ftb->queue);
       (int) i >= (int) queue_first_element(&ftb->queue);
       i--)
  {
    ftbw=(FTB_WORD *)(queue_element(&ftb->queue, i));

    if (ftbw->flags & FTB_FLAG_TRUNC)
    {
      /*
        special treatment for truncation operator
        1. there are some (besides this) +words
           | no need to search in the index, it can never ADD new rows
           | to the result, and to remove half-matched rows we do scan anyway
        2. -trunc*
           | same as 1.
        3. in 1 and 2, +/- need not be on the same expr. level,
           but can be on any upper level, as in +word +(trunc1* trunc2*)
        4. otherwise
           | We have to index-search for this prefix.
           | It may cause duplicates, as in the index (sorted by <word,docid>)
           |   <aaaa,row1>
           |   <aabb,row2>
           |   <aacc,row1>
           | Searching for "aa*" will find row1 twice...
      */
      FTB_EXPR *ftbe;
      for (ftbe=(FTB_EXPR*)ftbw;
           ftbe->up && !(ftbe->up->flags & FTB_FLAG_TRUNC);
           ftbe->up->flags|= FTB_FLAG_TRUNC, ftbe=ftbe->up)
      {
        if (ftbe->flags & FTB_FLAG_NO ||                     /* 2 */
            ftbe->up->ythresh - ftbe->up->yweaks >
            (uint) test(ftbe->flags & FTB_FLAG_YES))         /* 1 */
        {
          FTB_EXPR *top_ftbe=ftbe->up;
          ftbw->docid[0]=HA_OFFSET_ERROR;
          for (ftbe=(FTB_EXPR *)ftbw;
               ftbe != top_ftbe && !(ftbe->flags & FTB_FLAG_NO);
               ftbe=ftbe->up)
              ftbe->up->yweaks++;
          ftbe=0;
          break;
        }
      }
      if (!ftbe)
        continue;
      /* 4 */
      if (!is_tree_inited(& ftb->no_dupes))
        init_tree(& ftb->no_dupes,0,0,sizeof(my_off_t),
            _ftb_no_dupes_cmp,0,0,0);
      else
        reset_tree(& ftb->no_dupes);
    }

    ftbw->off=0; /* in case of reinit */
    if (_ft2_search(ftb, ftbw, 1))
      return;
  }
  queue_fix(& ftb->queue);
}


FT_INFO * maria_ft_init_boolean_search(MARIA_HA *info, uint keynr,
                                       uchar *query, uint query_len,
                                       CHARSET_INFO *cs)
{
  FTB       *ftb;
  FTB_EXPR  *ftbe;
  FTB_WORD  *ftbw;

  if (!(ftb=(FTB *)my_malloc(sizeof(FTB), MYF(MY_WME))))
    return 0;
  ftb->please= (struct _ft_vft *) & _ma_ft_vft_boolean;
  ftb->state=UNINITIALIZED;
  ftb->info=info;
  ftb->keynr=keynr;
  ftb->charset=cs;
  DBUG_ASSERT(keynr==NO_SUCH_KEY || cs == info->s->keyinfo[keynr].seg->charset);
  ftb->with_scan=0;
  ftb->lastpos=HA_OFFSET_ERROR;
  bzero(& ftb->no_dupes, sizeof(TREE));
  ftb->last_word= 0;

  init_alloc_root(&ftb->mem_root, 1024, 1024);
  ftb->queue.max_elements= 0;
  if (!(ftbe=(FTB_EXPR *)alloc_root(&ftb->mem_root, sizeof(FTB_EXPR))))
    goto err;
  ftbe->weight=1;
  ftbe->flags=FTB_FLAG_YES;
  ftbe->nos=1;
  ftbe->up=0;
  ftbe->max_docid= ftbe->ythresh= ftbe->yweaks= 0;
  ftbe->docid[0]=ftbe->docid[1]=HA_OFFSET_ERROR;
  ftbe->phrase= NULL;
  ftbe->document= 0;
  ftb->root=ftbe;
  if (unlikely(_ftb_parse_query(ftb, query, query_len,
                                keynr == NO_SUCH_KEY ? &ft_default_parser :
                                info->s->keyinfo[keynr].parser)))
    goto err;
  /*
    Hack: instead of init_queue, we'll use reinit queue to be able
    to alloc queue with alloc_root()
  */
  if (! (ftb->queue.root= (uchar **)alloc_root(&ftb->mem_root,
                                              (ftb->queue.max_elements + 1) *
                                              sizeof(void *))))
    goto err;
  reinit_queue(&ftb->queue, ftb->queue.max_elements, 0, 0,
               (int (*)(void*, uchar*, uchar*))FTB_WORD_cmp, 0, 0, 0);
  for (ftbw= ftb->last_word; ftbw; ftbw= ftbw->prev)
    queue_insert(&ftb->queue, (uchar *)ftbw);
  ftb->list=(FTB_WORD **)alloc_root(&ftb->mem_root,
                                     sizeof(FTB_WORD *)*ftb->queue.elements);
  memcpy(ftb->list, ftb->queue.root+1, sizeof(FTB_WORD *)*ftb->queue.elements);
  my_qsort2(ftb->list, ftb->queue.elements, sizeof(FTB_WORD *),
            (qsort2_cmp)FTB_WORD_cmp_list, (void*) ftb->charset);
  if (ftb->queue.elements<2) ftb->with_scan &= ~FTB_FLAG_TRUNC;
  ftb->state=READY;
  return ftb;
err:
  free_root(& ftb->mem_root, MYF(0));
  my_free(ftb);
  return 0;
}


typedef struct st_my_ftb_phrase_param
{
  LIST *phrase;
  LIST *document;
  CHARSET_INFO *cs;
  uint phrase_length;
  uint document_length;
  uint match;
} MY_FTB_PHRASE_PARAM;


static int ftb_phrase_add_word(MYSQL_FTPARSER_PARAM *param,
                               const char *word, int word_len,
                               MYSQL_FTPARSER_BOOLEAN_INFO
                               *boolean_info __attribute__((unused)))
{
  MY_FTB_PHRASE_PARAM *phrase_param= param->mysql_ftparam;
  FT_WORD *w= (FT_WORD *)phrase_param->document->data;
  LIST *phrase, *document;
  w->pos= (uchar*)word;
  w->len= word_len;
  phrase_param->document= phrase_param->document->prev;
  if (phrase_param->phrase_length > phrase_param->document_length)
  {
    phrase_param->document_length++;
    return 0;
  }
  /* TODO: rewrite phrase search to avoid
     comparing the same word twice. */
  for (phrase= phrase_param->phrase, document= phrase_param->document->next;
       phrase; phrase= phrase->next, document= document->next)
  {
    FT_WORD *phrase_word= (FT_WORD *)phrase->data;
    FT_WORD *document_word= (FT_WORD *)document->data;
    if (my_strnncoll(phrase_param->cs, (uchar*) phrase_word->pos,
                     phrase_word->len,
                     (uchar*) document_word->pos, document_word->len))
      return 0;
  }
  phrase_param->match++;
  return 0;
}


static int ftb_check_phrase_internal(MYSQL_FTPARSER_PARAM *param,
                                     const char *document, int len)
{
  FT_WORD word;
  MY_FTB_PHRASE_PARAM *phrase_param= param->mysql_ftparam;
  const uchar *docend= (uchar*)document + len;
  while (maria_ft_simple_get_word(phrase_param->cs, (uchar**)&document,
                                  docend, &word, FALSE))
  {
    param->mysql_add_word(param, (char*)word.pos, word.len, 0);
    if (phrase_param->match)
      break;
  }
  return 0;
}


/*
  Checks if given buffer matches phrase list.

  SYNOPSIS
    _ftb_check_phrase()
    s0     start of buffer
    e0     end of buffer
    phrase broken into list phrase
    cs     charset info

  RETURN VALUE
    1 is returned if phrase found, 0 else.
    -1 is returned if error occurs.
*/

static int _ftb_check_phrase(FTB *ftb, const uchar *document, uint len,
                             FTB_EXPR *ftbe, struct st_mysql_ftparser *parser)
{
  MY_FTB_PHRASE_PARAM ftb_param;
  MYSQL_FTPARSER_PARAM *param;
  DBUG_ENTER("_ftb_check_phrase");
  DBUG_ASSERT(parser);

  if (! (param= maria_ftparser_call_initializer(ftb->info, ftb->keynr, 1)))
    DBUG_RETURN(0);
  ftb_param.phrase= ftbe->phrase;
  ftb_param.document= ftbe->document;
  ftb_param.cs= ftb->charset;
  ftb_param.phrase_length= list_length(ftbe->phrase);
  ftb_param.document_length= 1;
  ftb_param.match= 0;

  param->mysql_parse= ftb_check_phrase_internal;
  param->mysql_add_word= ftb_phrase_add_word;
  param->mysql_ftparam= (void *)&ftb_param;
  param->cs= ftb->charset;
  param->doc= (char *)document;
  param->length= len;
  param->flags= 0;
  param->mode= MYSQL_FTPARSER_WITH_STOPWORDS;
  if (unlikely(parser->parse(param)))
    return -1;
  DBUG_RETURN(ftb_param.match ? 1 : 0);
}


static int _ftb_climb_the_tree(FTB *ftb, FTB_WORD *ftbw, FT_SEG_ITERATOR *ftsi_orig)
{
  FT_SEG_ITERATOR ftsi;
  FTB_EXPR *ftbe;
  float weight=ftbw->weight;
  int  yn_flag= ftbw->flags, ythresh, mode=(ftsi_orig != 0);
  my_off_t curdoc=ftbw->docid[mode];
  struct st_mysql_ftparser *parser= ftb->keynr == NO_SUCH_KEY ?
                                    &ft_default_parser :
                                    ftb->info->s->keyinfo[ftb->keynr].parser;

  for (ftbe=ftbw->up; ftbe; ftbe=ftbe->up)
  {
    ythresh = ftbe->ythresh - (mode ? 0 : ftbe->yweaks);
    if (ftbe->docid[mode] != curdoc)
    {
      ftbe->cur_weight=0;
      ftbe->yesses=ftbe->nos=0;
      ftbe->docid[mode]=curdoc;
    }
    if (ftbe->nos)
      break;
    if (yn_flag & FTB_FLAG_YES)
    {
      weight /= ftbe->ythresh;
      ftbe->cur_weight += weight;
      if ((int) ++ftbe->yesses == ythresh)
      {
        yn_flag=ftbe->flags;
        weight=ftbe->cur_weight*ftbe->weight;
        if (mode && ftbe->phrase)
        {
          int found= 0;

          memcpy(&ftsi, ftsi_orig, sizeof(ftsi));
          while (_ma_ft_segiterator(&ftsi) && !found)
          {
            if (!ftsi.pos)
              continue;
            found= _ftb_check_phrase(ftb, ftsi.pos, ftsi.len, ftbe, parser);
            if (unlikely(found < 0))
              return 1;
          }
          if (!found)
            break;
        } /* ftbe->quot */
      }
      else
        break;
    }
    else
    if (yn_flag & FTB_FLAG_NO)
    {
      /*
        NOTE: special sort function of queue assures that all
        (yn_flag & FTB_FLAG_NO) != 0
        events for every particular subexpression will
        "auto-magically" happen BEFORE all the
        (yn_flag & FTB_FLAG_YES) != 0 events. So no
        already matched expression can become not-matched again.
      */
      ++ftbe->nos;
      break;
    }
    else
    {
      if (ftbe->ythresh)
        weight/=3;
      ftbe->cur_weight +=  weight;
      if ((int) ftbe->yesses < ythresh)
        break;
      if (!(yn_flag & FTB_FLAG_WONLY))
        yn_flag= ((int) ftbe->yesses++ == ythresh) ? ftbe->flags : FTB_FLAG_WONLY ;
      weight*= ftbe->weight;
    }
  }
  return 0;
}


int maria_ft_boolean_read_next(FT_INFO *ftb, char *record)
{
  FTB_EXPR  *ftbe;
  FTB_WORD  *ftbw;
  MARIA_HA   *info=ftb->info;
  my_off_t   curdoc;

  if (ftb->state != INDEX_SEARCH && ftb->state != INDEX_DONE)
    return -1;

  /* black magic ON */
  if ((int) _ma_check_index(info, ftb->keynr) < 0)
    return my_errno;
  if (_ma_readinfo(info, F_RDLCK, 1))
    return my_errno;
  /* black magic OFF */

  if (!ftb->queue.elements)
    return my_errno=HA_ERR_END_OF_FILE;

  /* Attention!!! Address of a local variable is used here! See err: label */
  ftb->queue.first_cmp_arg=(void *)&curdoc;

  while (ftb->state == INDEX_SEARCH &&
         (curdoc=((FTB_WORD *)queue_top(& ftb->queue))->docid[0]) !=
         HA_OFFSET_ERROR)
  {
    while (curdoc == (ftbw=(FTB_WORD *)queue_top(& ftb->queue))->docid[0])
    {
      if (unlikely(_ftb_climb_the_tree(ftb, ftbw, 0)))
      {
        my_errno= HA_ERR_OUT_OF_MEM;
        goto err;
      }

      /* update queue */
      _ft2_search(ftb, ftbw, 0);
      queue_replace_top(&ftb->queue);
    }

    ftbe=ftb->root;
    if (ftbe->docid[0]==curdoc && ftbe->cur_weight>0 &&
        ftbe->yesses>=(ftbe->ythresh-ftbe->yweaks) && !ftbe->nos)
    {
      /* curdoc matched ! */
      if (is_tree_inited(&ftb->no_dupes) &&
          tree_insert(&ftb->no_dupes, &curdoc, 0,
                      ftb->no_dupes.custom_arg)->count >1)
        /* but it managed already to get past this line once */
        continue;

      info->cur_row.lastpos= curdoc;
      /* Clear all states, except that the table was updated */
      info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

      if (!(*info->read_record)(info, (uchar *) record, curdoc))
      {
        info->update|= HA_STATE_AKTIV;          /* Record is read */
        if (ftb->with_scan &&
            maria_ft_boolean_find_relevance(ftb, (uchar*)record, 0)==0)
            continue; /* no match */
        my_errno=0;
        goto err;
      }
      goto err;
    }
  }
  ftb->state=INDEX_DONE;
  my_errno=HA_ERR_END_OF_FILE;
err:
  ftb->queue.first_cmp_arg=(void *)0;
  return my_errno;
}


typedef struct st_my_ftb_find_param
{
  FT_INFO *ftb;
  FT_SEG_ITERATOR *ftsi;
} MY_FTB_FIND_PARAM;


static int ftb_find_relevance_add_word(MYSQL_FTPARSER_PARAM *param,
                                       const char *word, int len,
                                       MYSQL_FTPARSER_BOOLEAN_INFO
                                       *boolean_info __attribute__((unused)))
{
  MY_FTB_FIND_PARAM *ftb_param= param->mysql_ftparam;
  FT_INFO *ftb= ftb_param->ftb;
  FTB_WORD *ftbw;
  int a, b, c;
  /*
    Find right-most element in the array of query words matching this
    word from a document.
  */
  for (a= 0, b= ftb->queue.elements, c= (a+b)/2; b-a>1; c= (a+b)/2)
  {
    ftbw= ftb->list[c];
    if (ha_compare_text(ftb->charset, (uchar*)word, len,
                        (uchar*)ftbw->word+1, ftbw->len-1,
                        (my_bool)(ftbw->flags&FTB_FLAG_TRUNC), 0) < 0)
      b= c;
    else
      a= c;
  }
  /*
    If there were no words with truncation operator, we iterate to the
    beginning of an array until array element is equal to the word from
    a document. This is done mainly because the same word may be
    mentioned twice (or more) in the query.

    In case query has words with truncation operator we must iterate
    to the beginning of the array. There may be non-matching query words
    between matching word with truncation operator and the right-most
    matching element. E.g., if we're looking for 'aaa15' in an array of
    'aaa1* aaa14 aaa15 aaa16'.

    Worse of that there still may be match even if the binary search
    above didn't find matching element. E.g., if we're looking for
    'aaa15' in an array of 'aaa1* aaa14 aaa16'. The binary search will
    stop at 'aaa16'.
  */
  for (; c >= 0; c--)
  {
    ftbw= ftb->list[c];
    if (ha_compare_text(ftb->charset, (uchar*)word, len,
                        (uchar*)ftbw->word + 1,ftbw->len - 1,
                        (my_bool)(ftbw->flags & FTB_FLAG_TRUNC), 0))
    {
      if (ftb->with_scan & FTB_FLAG_TRUNC)
        continue;
      else
        break;
    }
    if (ftbw->docid[1] == ftb->info->cur_row.lastpos)
      continue;
    ftbw->docid[1]= ftb->info->cur_row.lastpos;
    if (unlikely(_ftb_climb_the_tree(ftb, ftbw, ftb_param->ftsi)))
      return 1;
  }
  return(0);
}


static int ftb_find_relevance_parse(MYSQL_FTPARSER_PARAM *param,
                                    const char *doc, int len)
{
  MY_FTB_FIND_PARAM *ftb_param= param->mysql_ftparam;
  FT_INFO *ftb= ftb_param->ftb;
  uchar *end= (uchar*) doc + len;
  FT_WORD w;
  while (maria_ft_simple_get_word(ftb->charset, (uchar**)&doc, end, &w, TRUE))
    param->mysql_add_word(param, (char*)w.pos, w.len, 0);
  return(0);
}


float maria_ft_boolean_find_relevance(FT_INFO *ftb, uchar *record, uint length)
{
  FTB_EXPR *ftbe;
  FT_SEG_ITERATOR ftsi, ftsi2;
  MARIA_RECORD_POS docid= ftb->info->cur_row.lastpos;
  MY_FTB_FIND_PARAM ftb_param;
  MYSQL_FTPARSER_PARAM *param;
  struct st_mysql_ftparser *parser= ftb->keynr == NO_SUCH_KEY ?
                                    &ft_default_parser :
                                    ftb->info->s->keyinfo[ftb->keynr].parser;

  if (docid == HA_OFFSET_ERROR)
    return -2.0;
  if (!ftb->queue.elements)
    return 0;
  if (! (param= maria_ftparser_call_initializer(ftb->info, ftb->keynr, 0)))
    return 0;

  if (ftb->state != INDEX_SEARCH && docid <= ftb->lastpos)
  {
    FTB_EXPR *x;
    uint i;

    for (i=0; i < ftb->queue.elements; i++)
    {
      ftb->list[i]->docid[1]=HA_OFFSET_ERROR;
      for (x=ftb->list[i]->up; x; x=x->up)
        x->docid[1]=HA_OFFSET_ERROR;
    }
  }

  ftb->lastpos=docid;

  if (ftb->keynr==NO_SUCH_KEY)
    _ma_ft_segiterator_dummy_init(record, length, &ftsi);
  else
    _ma_ft_segiterator_init(ftb->info, ftb->keynr, record, &ftsi);
  memcpy(&ftsi2, &ftsi, sizeof(ftsi));

  ftb_param.ftb= ftb;
  ftb_param.ftsi= &ftsi2;
  param->mysql_parse= ftb_find_relevance_parse;
  param->mysql_add_word= ftb_find_relevance_add_word;
  param->mysql_ftparam= (void *)&ftb_param;
  param->flags= 0;
  param->cs= ftb->charset;
  param->mode= MYSQL_FTPARSER_SIMPLE_MODE;

  while (_ma_ft_segiterator(&ftsi))
  {
    if (!ftsi.pos)
      continue;
    param->doc= (char *)ftsi.pos;
    param->length= ftsi.len;
    if (unlikely(parser->parse(param)))
      return 0;
  }
  ftbe=ftb->root;
  if (ftbe->docid[1]==docid && ftbe->cur_weight>0 &&
      ftbe->yesses>=ftbe->ythresh && !ftbe->nos)
  { /* row matched ! */
    return ftbe->cur_weight;
  }
  else
  { /* match failed ! */
    return 0.0;
  }
}


void maria_ft_boolean_close_search(FT_INFO *ftb)
{
  if (is_tree_inited(& ftb->no_dupes))
  {
    delete_tree(& ftb->no_dupes);
  }
  free_root(& ftb->mem_root, MYF(0));
  my_free(ftb);
}


float maria_ft_boolean_get_relevance(FT_INFO *ftb)
{
  return ftb->root->cur_weight;
}


void maria_ft_boolean_reinit_search(FT_INFO *ftb)
{
  _ftb_init_index_search(ftb);
}
