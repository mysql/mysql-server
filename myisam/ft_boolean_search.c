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

/* search with boolean queries */

typedef struct st_all_in_one {
  MI_INFO    *info;
  uint	      keynr;
  uchar      *keybuff;
  MI_KEYDEF  *keyinfo;
  my_off_t    key_root;
  TREE	      dtree;
  byte       *start, *end;
  uint        total_yes, total_no;
} ALL_IN_ONE;

typedef struct st_ft_superdoc {
    FT_DOC   doc;
    //FT_WORD *word_ptr;
    //double   tmp_weight;
    uint     yes;
    uint     no;
    uint     wno;
    ALL_IN_ONE *aio;
} FT_SUPERDOC;

static int FT_SUPERDOC_cmp(FT_SUPERDOC *p1, FT_SUPERDOC *p2)
{
  if (p1->doc.dpos < p2->doc.dpos)
    return -1;
  if (p1->doc.dpos == p2->doc.dpos)
    return 0;
  return 1;
}

static int walk_and_copy(FT_SUPERDOC *from,
			 uint32 count __attribute__((unused)), FT_DOC **to)
{
    if (from->yes == from->aio->total_yes && !from->no)
    {
      (*to)->dpos=from->doc.dpos;
      (*to)->weight=from->doc.weight;
      (*to)++;
    }
    return 0;
}

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
static double *wghts=_wghts+5; // wghts[i] = 1.5**i

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
static double *nwghts=_nwghts+5; // nwghts[i] = -0.5*1.5**i

int do_boolean(ALL_IN_ONE *aio, uint nested __attribute__((unused)),
	       int yesno __attribute__((unused)),
	       int plusminus, bool pmsign)
{
  int r, res;
  uint keylen, wno;
  FT_SUPERDOC  sdoc, *sptr;
  TREE_ELEMENT *selem;
  FT_WORD w;
  FTB_PARAM param;

#ifdef EVAL_RUN
  return 1;
#endif /* EVAL_RUN */

  param.prev=' ';

  for(wno=1; (res=ft_get_word(&aio->start,aio->end,&w,&param)); wno++)
  {
    r=plusminus+param.plusminus;
    if (param.pmsign^pmsign)
      w.weight=nwghts[(r>5)?5:((r<-5)?-5:r)];
    else
      w.weight=wghts[(r>5)?5:((r<-5)?-5:r)];

    if (param.yesno>0) aio->total_yes++;
    if (param.yesno<0) aio->total_no++;

    switch (res) {
    case FTB_LBR:    // (
      //if (do_boolean(aio,nested+1,my_yesno,plusminus+my_plusminus))
      //  return 1;
      // ???
      break;
    case 1:          // word
      keylen=_ft_make_key(aio->info,aio->keynr,(char*) aio->keybuff,&w,0);
      keylen-=HA_FT_WLEN;

      r=_mi_search(aio->info, aio->keyinfo, aio->keybuff, keylen,
                   SEARCH_FIND | SEARCH_PREFIX, aio->key_root);

      while (!r)
      {
        if (param.trunc)
           r=_mi_compare_text(default_charset_info,
                              aio->info->lastkey+1,keylen-1,
                              aio->keybuff+1,keylen-1,0);
        else
           r=_mi_compare_text(default_charset_info,
                              aio->info->lastkey,keylen,
                              aio->keybuff,keylen,0);
        if (r) break;

        sdoc.doc.dpos=aio->info->lastpos;

        /* saving document matched into dtree */
        if (!(selem=tree_insert(&aio->dtree, &sdoc, 0))) return 1;

        sptr=(FT_SUPERDOC *)ELEMENT_KEY((&aio->dtree), selem);

        if (selem->count==1) /* document's first match */
        {
          sptr->yes=sptr->no=sptr->doc.weight=0;
          sptr->aio=aio;
          sptr->wno=0;
        }
        if (sptr->wno != wno)
        {
          if (param.yesno>0) sptr->yes++;
          if (param.yesno<0) sptr->no++;
          sptr->wno=wno;
        }
        sptr->doc.weight+=w.weight;

        if (_mi_test_if_changed(aio->info) == 0)
          r=_mi_search_next(aio->info, aio->keyinfo, aio->info->lastkey,
                            aio->info->lastkey_length, SEARCH_BIGGER,
                            aio->key_root);
        else
          r=_mi_search(aio->info, aio->keyinfo, aio->info->lastkey,
                       aio->info->lastkey_length, SEARCH_BIGGER,
                       aio->key_root);
      }
      break;
    case FTB_RBR:    // )
      break;
    }
  }
  return 0;
}

FT_DOCLIST *ft_boolean_search(MI_INFO *info, uint keynr, byte *query,
		              uint query_len)
{
  ALL_IN_ONE aio;
  FT_DOC     *dptr;
  FT_DOCLIST *dlist=NULL;

  aio.info=info;
  aio.keynr=keynr;
  aio.keybuff=aio.info->lastkey+aio.info->s->base.max_key_length;
  aio.keyinfo=aio.info->s->keyinfo+keynr;
  aio.key_root=aio.info->s->state.key_root[keynr];
  aio.start=query;
  aio.end=query+query_len;
  aio.total_yes=aio.total_no=0;

  init_tree(&aio.dtree,0,sizeof(FT_SUPERDOC),(qsort_cmp)&FT_SUPERDOC_cmp,0,
            NULL);

  if (do_boolean(&aio,0,0,0,0))
    goto err;

  dlist=(FT_DOCLIST *)my_malloc(sizeof(FT_DOCLIST)+sizeof(FT_DOC)*(aio.dtree.elements_in_tree-1),MYF(0));
  if(!dlist)
    goto err;

  dlist->ndocs=aio.dtree.elements_in_tree;
  dlist->curdoc=-1;
  dlist->info=aio.info;
  dptr=dlist->doc;

  tree_walk(&aio.dtree, (tree_walk_action)&walk_and_copy, &dptr, left_root_right);

  dlist->ndocs=dptr - dlist->doc;

err:
  delete_tree(&aio.dtree);
  return dlist;
}

