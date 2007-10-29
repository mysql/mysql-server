/* Copyright (C) 2002-2006 MySQL AB & Ramil Kalimullin
   
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

#include "myisamdef.h"

#ifdef HAVE_RTREE_KEYS

#include "rt_index.h"
#include "rt_key.h"
#include "rt_mbr.h"

#define REINSERT_BUFFER_INC 10
#define PICK_BY_AREA
/*#define PICK_BY_PERIMETER*/

typedef struct st_page_level
{
  uint level;
  my_off_t offs;
} stPageLevel;

typedef struct st_page_list
{   
  ulong n_pages;
  ulong m_pages;
  stPageLevel *pages;
} stPageList;


/* 
   Find next key in r-tree according to search_flag recursively

   NOTES
     Used in rtree_find_first() and rtree_find_next()

   RETURN
     -1	 Error
     0   Found
     1   Not found 
*/

static int rtree_find_req(MI_INFO *info, MI_KEYDEF *keyinfo, uint search_flag,
			  uint nod_cmp_flag, my_off_t page, int level)
{
  uchar *k;
  uchar *last;
  uint nod_flag;
  int res;
  uchar *page_buf;
  int k_len;
  uint *saved_key = (uint*) (info->rtree_recursion_state) + level;
  
  if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length)))
  {
    my_errno = HA_ERR_OUT_OF_MEM;
    return -1;
  }
  if (!_mi_fetch_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf, 0))
    goto err1;
  nod_flag = mi_test_if_nod(page_buf);

  k_len = keyinfo->keylength - info->s->base.rec_reflength;
  
  if(info->rtree_recursion_depth >= level)
  {
    k = page_buf + *saved_key;
  }
  else
  {
    k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  }
  last = rt_PAGE_END(page_buf);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, k_len, nod_flag))
  {
    if (nod_flag) 
    { 
      /* this is an internal node in the tree */
      if (!(res = rtree_key_cmp(keyinfo->seg, info->first_mbr_key, k, 
                            info->last_rkey_length, nod_cmp_flag)))
      {
        switch ((res = rtree_find_req(info, keyinfo, search_flag, nod_cmp_flag,
                                      _mi_kpos(nod_flag, k), level + 1)))
        {
          case 0: /* found - exit from recursion */
            *saved_key = k - page_buf;
            goto ok;
          case 1: /* not found - continue searching */
            info->rtree_recursion_depth = level;
            break;
          default: /* error */
          case -1:
            goto err1;
        }
      }
    }
    else 
    { 
      /* this is a leaf */
      if (!rtree_key_cmp(keyinfo->seg, info->first_mbr_key, k, 
                         info->last_rkey_length, search_flag))
      {
        uchar *after_key = rt_PAGE_NEXT_KEY(k, k_len, nod_flag);
        info->lastpos = _mi_dpos(info, 0, after_key);
        info->lastkey_length = k_len + info->s->base.rec_reflength;
        memcpy(info->lastkey, k, info->lastkey_length);
        info->rtree_recursion_depth = level;
        *saved_key = last - page_buf;

        if (after_key < last)
        {
          info->int_keypos = info->buff;
          info->int_maxpos = info->buff + (last - after_key);
          memcpy(info->buff, after_key, last - after_key);
          info->buff_used = 0;
        }
        else
        {
	  info->buff_used = 1;
        }

        res = 0;
        goto ok;
      }
    }
  }
  info->lastpos = HA_OFFSET_ERROR;
  my_errno = HA_ERR_KEY_NOT_FOUND;
  res = 1;

ok:
  my_afree((uchar*)page_buf);
  return res;

err1:
  my_afree((uchar*)page_buf);
  info->lastpos = HA_OFFSET_ERROR;
  return -1;
}


/*
  Find first key in r-tree according to search_flag condition

  SYNOPSIS
   rtree_find_first()
   info			Handler to MyISAM file
   uint keynr		Key number to use
   key			Key to search for
   key_length		Length of 'key' 
   search_flag		Bitmap of flags how to do the search

  RETURN
    -1  Error
    0   Found
    1   Not found
*/

int rtree_find_first(MI_INFO *info, uint keynr, uchar *key, uint key_length, 
                    uint search_flag)
{
  my_off_t root;
  uint nod_cmp_flag;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;

  if ((root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }

  /*
    Save searched key, include data pointer.
    The data pointer is required if the search_flag contains MBR_DATA.
    (minimum bounding rectangle)
  */
  memcpy(info->first_mbr_key, key, keyinfo->keylength);
  info->last_rkey_length = key_length;

  info->rtree_recursion_depth = -1;
  info->buff_used = 1;
  
  nod_cmp_flag = ((search_flag & (MBR_EQUAL | MBR_WITHIN)) ? 
        MBR_WITHIN : MBR_INTERSECT);
  return rtree_find_req(info, keyinfo, search_flag, nod_cmp_flag, root, 0);
}


/* 
   Find next key in r-tree according to search_flag condition

  SYNOPSIS
   rtree_find_next()
   info			Handler to MyISAM file
   uint keynr		Key number to use
   search_flag		Bitmap of flags how to do the search

   RETURN
     -1  Error
     0   Found
     1   Not found
*/

int rtree_find_next(MI_INFO *info, uint keynr, uint search_flag)
{
  my_off_t root;
  uint nod_cmp_flag;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;

  if (info->update & HA_STATE_DELETED)
    return rtree_find_first(info, keynr, info->lastkey, info->lastkey_length,
			    search_flag);

  if (!info->buff_used)
  {
    uchar *key= info->int_keypos;

    while (key < info->int_maxpos)
    {
      if (!rtree_key_cmp(keyinfo->seg, info->first_mbr_key, key, 
                         info->last_rkey_length, search_flag))
      {
        uchar *after_key = key + keyinfo->keylength;

        info->lastpos= _mi_dpos(info, 0, after_key);
        memcpy(info->lastkey, key, info->lastkey_length);

        if (after_key < info->int_maxpos)
	  info->int_keypos= after_key;
        else
	  info->buff_used= 1;
        return 0;
      }
      key+= keyinfo->keylength;
    }
  }
  if ((root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }
  
  nod_cmp_flag = ((search_flag & (MBR_EQUAL | MBR_WITHIN)) ? 
        MBR_WITHIN : MBR_INTERSECT);
  return rtree_find_req(info, keyinfo, search_flag, nod_cmp_flag, root, 0);
}


/*
  Get next key in r-tree recursively

  NOTES
    Used in rtree_get_first() and rtree_get_next()

  RETURN
    -1  Error
    0   Found
    1   Not found
*/

static int rtree_get_req(MI_INFO *info, MI_KEYDEF *keyinfo, uint key_length, 
                         my_off_t page, int level)
{
  uchar *k;
  uchar *last;
  uint nod_flag;
  int res;
  uchar *page_buf;
  uint k_len;
  uint *saved_key = (uint*) (info->rtree_recursion_state) + level;
  
  if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length)))
    return -1;
  if (!_mi_fetch_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf, 0))
    goto err1;
  nod_flag = mi_test_if_nod(page_buf);

  k_len = keyinfo->keylength - info->s->base.rec_reflength;

  if(info->rtree_recursion_depth >= level)
  {
    k = page_buf + *saved_key;
    if (!nod_flag)
    {
      /* Only leaf pages contain data references. */
      /* Need to check next key with data reference. */
      k = rt_PAGE_NEXT_KEY(k, k_len, nod_flag);
    }
  }
  else
  {
    k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  }
  last = rt_PAGE_END(page_buf);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, k_len, nod_flag))
  {
    if (nod_flag) 
    { 
      /* this is an internal node in the tree */
      switch ((res = rtree_get_req(info, keyinfo, key_length, 
                                  _mi_kpos(nod_flag, k), level + 1)))
      {
        case 0: /* found - exit from recursion */
          *saved_key = k - page_buf;
          goto ok;
        case 1: /* not found - continue searching */
          info->rtree_recursion_depth = level;
          break;
        default:
        case -1: /* error */
          goto err1;
      }
    }
    else 
    { 
      /* this is a leaf */
      uchar *after_key = rt_PAGE_NEXT_KEY(k, k_len, nod_flag);
      info->lastpos = _mi_dpos(info, 0, after_key);
      info->lastkey_length = k_len + info->s->base.rec_reflength;
      memcpy(info->lastkey, k, info->lastkey_length);

      info->rtree_recursion_depth = level;
      *saved_key = k - page_buf;

      if (after_key < last)
      {
        info->int_keypos = (uchar*)saved_key;
        memcpy(info->buff, page_buf, keyinfo->block_length);
        info->int_maxpos = rt_PAGE_END(info->buff);
        info->buff_used = 0;
      }
      else
      {
	info->buff_used = 1;
      }

      res = 0;
      goto ok;
    }
  }
  info->lastpos = HA_OFFSET_ERROR;
  my_errno = HA_ERR_KEY_NOT_FOUND;
  res = 1;

ok:
  my_afree((uchar*)page_buf);
  return res;

err1:
  my_afree((uchar*)page_buf);
  info->lastpos = HA_OFFSET_ERROR;
  return -1;
}


/*
  Get first key in r-tree

  RETURN
    -1	Error
    0	Found
    1	Not found
*/

int rtree_get_first(MI_INFO *info, uint keynr, uint key_length)
{
  my_off_t root;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;

  if ((root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }

  info->rtree_recursion_depth = -1;
  info->buff_used = 1;
  
  return rtree_get_req(info, &keyinfo[keynr], key_length, root, 0);
}


/* 
  Get next key in r-tree

  RETURN
    -1	Error
    0	Found
    1	Not found
*/

int rtree_get_next(MI_INFO *info, uint keynr, uint key_length)
{
  my_off_t root;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;

  if (!info->buff_used)
  {
    uint k_len = keyinfo->keylength - info->s->base.rec_reflength;
    /* rt_PAGE_NEXT_KEY(info->int_keypos) */
    uchar *key = info->buff + *(int*)info->int_keypos + k_len + 
                 info->s->base.rec_reflength;
    /* rt_PAGE_NEXT_KEY(key) */
    uchar *after_key = key + k_len + info->s->base.rec_reflength; 

    info->lastpos = _mi_dpos(info, 0, after_key);
    info->lastkey_length = k_len + info->s->base.rec_reflength;
    memcpy(info->lastkey, key, k_len + info->s->base.rec_reflength);

    *(int*)info->int_keypos = key - info->buff;
    if (after_key >= info->int_maxpos)
    {
      info->buff_used = 1;
    }

    return 0;
  }
  else
  {
    if ((root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
    {
      my_errno= HA_ERR_END_OF_FILE;
      return -1;
    }
  
    return rtree_get_req(info, &keyinfo[keynr], key_length, root, 0);
  }
}


/*
  Choose non-leaf better key for insertion
*/

#ifdef PICK_BY_PERIMETER
static uchar *rtree_pick_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
			     uint key_length, uchar *page_buf, uint nod_flag)
{
  double increase;
  double best_incr = DBL_MAX;
  double perimeter;
  double best_perimeter;
  uchar *best_key;
  uchar *k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  uchar *last = rt_PAGE_END(page_buf);

  LINT_INIT(best_perimeter);
  LINT_INIT(best_key);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, key_length, nod_flag))
  {
    if ((increase = rtree_perimeter_increase(keyinfo->seg, k, key, key_length,
					     &perimeter)) == -1)
      return NULL;
    if ((increase < best_incr)||
	(increase == best_incr && perimeter < best_perimeter))
    {
      best_key = k;
      best_perimeter= perimeter;
      best_incr = increase;
    }
  }
  return best_key;
}

#endif /*PICK_BY_PERIMETER*/

#ifdef PICK_BY_AREA
static uchar *rtree_pick_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
			     uint key_length, uchar *page_buf, uint nod_flag)
{
  double increase;
  double best_incr;
  double area;
  double best_area;
  uchar *best_key= NULL;
  uchar *k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  uchar *last = rt_PAGE_END(page_buf);

  LINT_INIT(best_area);
  LINT_INIT(best_key);
  LINT_INIT(best_incr);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, key_length, nod_flag))
  {
    /* The following is safe as -1.0 is an exact number */
    if ((increase = rtree_area_increase(keyinfo->seg, k, key, key_length, 
                                        &area)) == -1.0)
      return NULL;
    /* The following should be safe, even if we compare doubles */
    if (!best_key || increase < best_incr ||
        ((increase == best_incr) && (area < best_area)))
    {
      best_key = k;
      best_area = area;
      best_incr = increase;
    }
  }
  return best_key;
}

#endif /*PICK_BY_AREA*/

/*
  Go down and insert key into tree

  RETURN
    -1	Error
    0	Child was not split
    1	Child was split
*/

static int rtree_insert_req(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
                            uint key_length, my_off_t page, my_off_t *new_page, 
                            int ins_level, int level)
{
  uchar *k;
  uint nod_flag;
  uchar *page_buf;
  int res;
  DBUG_ENTER("rtree_insert_req");

  if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length + 
                                     MI_MAX_KEY_BUFF)))
  {
    my_errno = HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1); /* purecov: inspected */
  }
  if (!_mi_fetch_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf, 0))
    goto err1;
  nod_flag = mi_test_if_nod(page_buf);
  DBUG_PRINT("rtree", ("page: %lu  level: %d  ins_level: %d  nod_flag: %u",
                       (ulong) page, level, ins_level, nod_flag));

  if ((ins_level == -1 && nod_flag) ||       /* key: go down to leaf */
      (ins_level > -1 && ins_level > level)) /* branch: go down to ins_level */
  { 
    if ((k = rtree_pick_key(info, keyinfo, key, key_length, page_buf, 
                             nod_flag)) == NULL)
      goto err1;
    switch ((res = rtree_insert_req(info, keyinfo, key, key_length, 
                     _mi_kpos(nod_flag, k), new_page, ins_level, level + 1)))
    {
      case 0: /* child was not split */
      {
        rtree_combine_rect(keyinfo->seg, k, key, k, key_length);
        if (_mi_write_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf))
          goto err1;
        goto ok;
      }
      case 1: /* child was split */
      {
        uchar *new_key = page_buf + keyinfo->block_length + nod_flag;
        /* set proper MBR for key */
        if (rtree_set_key_mbr(info, keyinfo, k, key_length, 
                            _mi_kpos(nod_flag, k)))
          goto err1;
        /* add new key for new page */
        _mi_kpointer(info, new_key - nod_flag, *new_page);
        if (rtree_set_key_mbr(info, keyinfo, new_key, key_length, *new_page))
          goto err1;
        res = rtree_add_key(info, keyinfo, new_key, key_length, 
                           page_buf, new_page);
        if (_mi_write_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf))
          goto err1;
        goto ok;
      }
      default:
      case -1: /* error */
      {
        goto err1;
      }
    }
  }
  else
  { 
    res = rtree_add_key(info, keyinfo, key, key_length, page_buf, new_page);
    if (_mi_write_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf))
      goto err1;
    goto ok;
  }

ok:
  my_afree((uchar*)page_buf);
  DBUG_RETURN(res);

err1:
  my_afree((uchar*)page_buf);
  DBUG_RETURN(-1); /* purecov: inspected */
}


/*
  Insert key into the tree

  RETURN
    -1	Error
    0	Root was not split
    1	Root was split
*/

static int rtree_insert_level(MI_INFO *info, uint keynr, uchar *key, 
                             uint key_length, int ins_level)
{
  my_off_t old_root;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;
  int res;
  my_off_t new_page;
  DBUG_ENTER("rtree_insert_level");

  if ((old_root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    if ((old_root = _mi_new(info, keyinfo, DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
      DBUG_RETURN(-1);
    info->buff_used = 1;
    mi_putint(info->buff, 2, 0);
    res = rtree_add_key(info, keyinfo, key, key_length, info->buff, NULL);
    if (_mi_write_keypage(info, keyinfo, old_root, DFLT_INIT_HITS, info->buff))
      DBUG_RETURN(1);
    info->s->state.key_root[keynr] = old_root;
    DBUG_RETURN(res);
  }

  switch ((res = rtree_insert_req(info, keyinfo, key, key_length, 
                                  old_root, &new_page, ins_level, 0)))
  {
    case 0: /* root was not split */
    {
      break;
    }
    case 1: /* root was split, grow a new root */
    { 
      uchar *new_root_buf;
      my_off_t new_root;
      uchar *new_key;
      uint nod_flag = info->s->base.key_reflength;

      DBUG_PRINT("rtree", ("root was split, grow a new root"));
      if (!(new_root_buf = (uchar*)my_alloca((uint)keyinfo->block_length + 
                                             MI_MAX_KEY_BUFF)))
      {
        my_errno = HA_ERR_OUT_OF_MEM;
        DBUG_RETURN(-1); /* purecov: inspected */
      }

      mi_putint(new_root_buf, 2, nod_flag);
      if ((new_root = _mi_new(info, keyinfo, DFLT_INIT_HITS)) ==
	  HA_OFFSET_ERROR)
        goto err1;

      new_key = new_root_buf + keyinfo->block_length + nod_flag;

      _mi_kpointer(info, new_key - nod_flag, old_root);
      if (rtree_set_key_mbr(info, keyinfo, new_key, key_length, old_root))
        goto err1;
      if (rtree_add_key(info, keyinfo, new_key, key_length, new_root_buf, NULL) 
          == -1)
        goto err1;
      _mi_kpointer(info, new_key - nod_flag, new_page);
      if (rtree_set_key_mbr(info, keyinfo, new_key, key_length, new_page))
        goto err1;
      if (rtree_add_key(info, keyinfo, new_key, key_length, new_root_buf, NULL) 
          == -1)
        goto err1;
      if (_mi_write_keypage(info, keyinfo, new_root,
                            DFLT_INIT_HITS, new_root_buf))
        goto err1;
      info->s->state.key_root[keynr] = new_root;
      DBUG_PRINT("rtree", ("new root page: %lu  level: %d  nod_flag: %u",
                           (ulong) new_root, 0, mi_test_if_nod(new_root_buf)));

      my_afree((uchar*)new_root_buf);
      break;
err1:
      my_afree((uchar*)new_root_buf);
      DBUG_RETURN(-1); /* purecov: inspected */
    }
    default:
    case -1: /* error */
    {
      break;
    }
  }
  DBUG_RETURN(res);
}


/*
  Insert key into the tree - interface function

  RETURN
    -1	Error
    0	OK
*/

int rtree_insert(MI_INFO *info, uint keynr, uchar *key, uint key_length)
{
  DBUG_ENTER("rtree_insert");
  DBUG_RETURN((!key_length ||
               (rtree_insert_level(info, keynr, key, key_length, -1) == -1)) ?
              -1 : 0);
}


/* 
  Fill reinsert page buffer 

  RETURN
    -1	Error
    0	OK
*/

static int rtree_fill_reinsert_list(stPageList *ReinsertList, my_off_t page, 
                                    int level)
{
  DBUG_ENTER("rtree_fill_reinsert_list");
  DBUG_PRINT("rtree", ("page: %lu  level: %d", (ulong) page, level));
  if (ReinsertList->n_pages == ReinsertList->m_pages)
  {
    ReinsertList->m_pages += REINSERT_BUFFER_INC;
    if (!(ReinsertList->pages = (stPageLevel*)my_realloc((uchar*)ReinsertList->pages, 
      ReinsertList->m_pages * sizeof(stPageLevel), MYF(MY_ALLOW_ZERO_PTR))))
      goto err1;
  }
  /* save page to ReinsertList */
  ReinsertList->pages[ReinsertList->n_pages].offs = page;
  ReinsertList->pages[ReinsertList->n_pages].level = level;
  ReinsertList->n_pages++;
  DBUG_RETURN(0);

err1:
  DBUG_RETURN(-1); /* purecov: inspected */
}


/*
  Go down and delete key from the tree

  RETURN
    -1	Error
    0	Deleted
    1	Not found
    2	Empty leaf
*/

static int rtree_delete_req(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
                           uint key_length, my_off_t page, uint *page_size, 
                           stPageList *ReinsertList, int level)
{
  uchar *k;
  uchar *last;
  ulong i;
  uint nod_flag;
  uchar *page_buf;
  int res;
  DBUG_ENTER("rtree_delete_req");

  if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length)))
  {
    my_errno = HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1); /* purecov: inspected */
  }
  if (!_mi_fetch_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf, 0))
    goto err1;
  nod_flag = mi_test_if_nod(page_buf);
  DBUG_PRINT("rtree", ("page: %lu  level: %d  nod_flag: %u",
                       (ulong) page, level, nod_flag));

  k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  last = rt_PAGE_END(page_buf);

  for (i = 0; k < last; k = rt_PAGE_NEXT_KEY(k, key_length, nod_flag), ++i)
  {
    if (nod_flag)
    { 
      /* not leaf */
      if (!rtree_key_cmp(keyinfo->seg, key, k, key_length, MBR_WITHIN))
      {
        switch ((res = rtree_delete_req(info, keyinfo, key, key_length, 
                  _mi_kpos(nod_flag, k), page_size, ReinsertList, level + 1)))
        {
          case 0: /* deleted */
          { 
            /* test page filling */
            if (*page_size + key_length >= rt_PAGE_MIN_SIZE(keyinfo->block_length)) 
            { 
              /* OK */
              /* Calculate a new key value (MBR) for the shrinked block. */
              if (rtree_set_key_mbr(info, keyinfo, k, key_length, 
                                  _mi_kpos(nod_flag, k)))
                goto err1;
              if (_mi_write_keypage(info, keyinfo, page,
                                    DFLT_INIT_HITS, page_buf))
                goto err1;
            }
            else
            { 
              /*
                Too small: delete key & add it descendant to reinsert list.
                Store position and level of the block so that it can be
                accessed later for inserting the remaining keys.
              */
              DBUG_PRINT("rtree", ("too small. move block to reinsert list"));
              if (rtree_fill_reinsert_list(ReinsertList, _mi_kpos(nod_flag, k),
                                           level + 1))
                goto err1;
              /*
                Delete the key that references the block. This makes the
                block disappear from the index. Hence we need to insert
                its remaining keys later. Note: if the block is a branch
                block, we do not only remove this block, but the whole
                subtree. So we need to re-insert its keys on the same
                level later to reintegrate the subtrees.
              */
              rtree_delete_key(info, page_buf, k, key_length, nod_flag);
              if (_mi_write_keypage(info, keyinfo, page,
                                    DFLT_INIT_HITS, page_buf))
                goto err1;
              *page_size = mi_getint(page_buf);
            }
            
            goto ok;
          }
          case 1: /* not found - continue searching */
          {
            break;
          }
          case 2: /* vacuous case: last key in the leaf */
          {
            rtree_delete_key(info, page_buf, k, key_length, nod_flag);
            if (_mi_write_keypage(info, keyinfo, page,
                                  DFLT_INIT_HITS, page_buf))
              goto err1;
            *page_size = mi_getint(page_buf);
            res = 0;
            goto ok;
          }
          default: /* error */
          case -1:
          {
            goto err1;
          }
        }
      }
    }
    else  
    {
      /* leaf */
      if (!rtree_key_cmp(keyinfo->seg, key, k, key_length, MBR_EQUAL | MBR_DATA))
      {
        rtree_delete_key(info, page_buf, k, key_length, nod_flag);
        *page_size = mi_getint(page_buf);
        if (*page_size == 2) 
        {
          /* last key in the leaf */
          res = 2;
          if (_mi_dispose(info, keyinfo, page, DFLT_INIT_HITS))
            goto err1;
        }
        else
        {
          res = 0;
          if (_mi_write_keypage(info, keyinfo, page, DFLT_INIT_HITS, page_buf))
            goto err1;
        }
        goto ok;
      }
    }
  }
  res = 1;

ok:
  my_afree((uchar*)page_buf);
  DBUG_RETURN(res);

err1:
  my_afree((uchar*)page_buf);
  DBUG_RETURN(-1); /* purecov: inspected */
}


/*
  Delete key - interface function

  RETURN
    -1	Error
    0	Deleted
*/

int rtree_delete(MI_INFO *info, uint keynr, uchar *key, uint key_length)
{
  uint page_size;
  stPageList ReinsertList;
  my_off_t old_root;
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;
  DBUG_ENTER("rtree_delete");

  if ((old_root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    DBUG_RETURN(-1); /* purecov: inspected */
  }
  DBUG_PRINT("rtree", ("starting deletion at root page: %lu",
                       (ulong) old_root));

  ReinsertList.pages = NULL;
  ReinsertList.n_pages = 0;
  ReinsertList.m_pages = 0;
  
  switch (rtree_delete_req(info, keyinfo, key, key_length, old_root, 
                                 &page_size, &ReinsertList, 0))
  {
    case 2: /* empty */
    {
      info->s->state.key_root[keynr] = HA_OFFSET_ERROR;
      DBUG_RETURN(0);
    }
    case 0: /* deleted */
    {
      uint nod_flag;
      ulong i;
      for (i = 0; i < ReinsertList.n_pages; ++i)
      {
        uchar *page_buf;
        uchar *k;
        uchar *last;

        if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length)))
        {
          my_errno = HA_ERR_OUT_OF_MEM;
          goto err1;
        }
        if (!_mi_fetch_keypage(info, keyinfo, ReinsertList.pages[i].offs, 
                               DFLT_INIT_HITS, page_buf, 0))
          goto err1;
        nod_flag = mi_test_if_nod(page_buf);
        DBUG_PRINT("rtree", ("reinserting keys from "
                             "page: %lu  level: %d  nod_flag: %u",
                             (ulong) ReinsertList.pages[i].offs,
                             ReinsertList.pages[i].level, nod_flag));

        k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
        last = rt_PAGE_END(page_buf);
        for (; k < last; k = rt_PAGE_NEXT_KEY(k, key_length, nod_flag))
        {
          int res;
          if ((res= rtree_insert_level(info, keynr, k, key_length,
                                       ReinsertList.pages[i].level)) == -1)
          {
            my_afree((uchar*)page_buf);
            goto err1;
          }
          if (res)
          {
            ulong j;
            DBUG_PRINT("rtree", ("root has been split, adjust levels"));
            for (j= i; j < ReinsertList.n_pages; j++)
            {
              ReinsertList.pages[j].level++;
              DBUG_PRINT("rtree", ("keys from page: %lu  now level: %d",
                                   (ulong) ReinsertList.pages[i].offs,
                                   ReinsertList.pages[i].level));
            }
          }
        }
        my_afree((uchar*)page_buf);
        if (_mi_dispose(info, keyinfo, ReinsertList.pages[i].offs,
            DFLT_INIT_HITS))
          goto err1;
      }
      if (ReinsertList.pages)
        my_free((uchar*) ReinsertList.pages, MYF(0));

      /* check for redundant root (not leaf, 1 child) and eliminate */
      if ((old_root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
        goto err1;
      if (!_mi_fetch_keypage(info, keyinfo, old_root, DFLT_INIT_HITS,
                             info->buff, 0))
        goto err1;
      nod_flag = mi_test_if_nod(info->buff);
      page_size = mi_getint(info->buff);
      if (nod_flag && (page_size == 2 + key_length + nod_flag))
      {
        my_off_t new_root = _mi_kpos(nod_flag,
                                     rt_PAGE_FIRST_KEY(info->buff, nod_flag));
        if (_mi_dispose(info, keyinfo, old_root, DFLT_INIT_HITS))
          goto err1;
        info->s->state.key_root[keynr] = new_root;
      }
      info->update= HA_STATE_DELETED;
      DBUG_RETURN(0);

err1:
      DBUG_RETURN(-1); /* purecov: inspected */
    }
    case 1: /* not found */
    {
      my_errno = HA_ERR_KEY_NOT_FOUND;
      DBUG_RETURN(-1); /* purecov: inspected */
    }
    default:
    case -1: /* error */
    {
      DBUG_RETURN(-1); /* purecov: inspected */
    }
  }
}


/* 
  Estimate number of suitable keys in the tree 

  RETURN
    estimated value
*/

ha_rows rtree_estimate(MI_INFO *info, uint keynr, uchar *key, 
                       uint key_length, uint flag)
{
  MI_KEYDEF *keyinfo = info->s->keyinfo + keynr;
  my_off_t root;
  uint i = 0;
  uchar *k;
  uchar *last;
  uint nod_flag;
  uchar *page_buf;
  uint k_len;
  double area = 0;
  ha_rows res = 0;

  if (flag & MBR_DISJOINT)
    return info->state->records;

  if ((root = info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
    return HA_POS_ERROR;
  if (!(page_buf = (uchar*)my_alloca((uint)keyinfo->block_length)))
    return HA_POS_ERROR;
  if (!_mi_fetch_keypage(info, keyinfo, root, DFLT_INIT_HITS, page_buf, 0))
    goto err1;
  nod_flag = mi_test_if_nod(page_buf);

  k_len = keyinfo->keylength - info->s->base.rec_reflength;

  k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  last = rt_PAGE_END(page_buf);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, k_len, nod_flag), ++i)
  {
    if (nod_flag)
    {
      double k_area = rtree_rect_volume(keyinfo->seg, k, key_length);

      /* The following should be safe, even if we compare doubles */
      if (k_area == 0)
      {
        if (flag & (MBR_CONTAIN | MBR_INTERSECT))
        {
          area += 1;
        }
        else if (flag & (MBR_WITHIN | MBR_EQUAL))
        {
          if (!rtree_key_cmp(keyinfo->seg, key, k, key_length, MBR_WITHIN))
            area += 1;
        }
        else
          goto err1;
      }
      else
      {
        if (flag & (MBR_CONTAIN | MBR_INTERSECT))
        {
          area += rtree_overlapping_area(keyinfo->seg, key, k, key_length) / 
                  k_area;
        }
        else if (flag & (MBR_WITHIN | MBR_EQUAL))
        {
          if (!rtree_key_cmp(keyinfo->seg, key, k, key_length, MBR_WITHIN))
            area += rtree_rect_volume(keyinfo->seg, key, key_length) /
                    k_area;
        }
        else
          goto err1;
      }
    }
    else
    {
      if (!rtree_key_cmp(keyinfo->seg, key, k, key_length, flag))
        ++res;
    }
  }
  if (nod_flag)
  {
    if (i)
      res = (ha_rows) (area / i * info->state->records);
    else 
      res = HA_POS_ERROR;
  }

  my_afree((uchar*)page_buf);
  return res;

err1:
  my_afree((uchar*)page_buf);
  return HA_POS_ERROR;
}

#endif /*HAVE_RTREE_KEYS*/

