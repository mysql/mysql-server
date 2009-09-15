/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin & MySQL Finland AB
   & TCX DataKonsult AB

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

#include "maria_def.h"
#include "trnman.h"
#include "ma_key_recover.h"

#ifdef HAVE_RTREE_KEYS

#include "ma_rt_index.h"
#include "ma_rt_key.h"
#include "ma_rt_mbr.h"

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
  uint n_pages;
  uint m_pages;
  stPageLevel *pages;
} stPageList;


/*
   Find next key in r-tree according to search_flag recursively

   NOTES
     Used in maria_rtree_find_first() and maria_rtree_find_next()

   RETURN
     -1	 Error
     0   Found
     1   Not found
*/

static int maria_rtree_find_req(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                                uint32 search_flag,
                                uint nod_cmp_flag, my_off_t page_pos,
                                int level)
{
  MARIA_SHARE *share= info->s;
  uint nod_flag;
  int res;
  uchar *page_buf, *k, *last;
  int key_data_length;
  uint *saved_key= (uint*) (info->maria_rtree_recursion_state) + level;
  MARIA_PAGE page;

  if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    return -1;
  }
  if (_ma_fetch_keypage(&page, info, keyinfo, page_pos,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        DFLT_INIT_HITS, page_buf, 0))
    goto err;
  nod_flag= page.node;

  key_data_length= keyinfo->keylength - share->base.rec_reflength;

  if (info->maria_rtree_recursion_depth >= level)
  {
    k= page_buf + *saved_key;
  }
  else
  {
    k= rt_PAGE_FIRST_KEY(share, page_buf, nod_flag);
  }
  last= rt_PAGE_END(&page);

  for (; k < last; k= rt_PAGE_NEXT_KEY(share, k, key_data_length, nod_flag))
  {
    if (nod_flag)
    {
      /* this is an internal node in the tree */
      if (!(res= maria_rtree_key_cmp(keyinfo->seg,
                                      info->first_mbr_key, k,
                                      info->last_rkey_length, nod_cmp_flag)))
      {
        switch ((res= maria_rtree_find_req(info, keyinfo, search_flag,
                                            nod_cmp_flag,
                                            _ma_kpos(nod_flag, k),
                                            level + 1)))
        {
          case 0: /* found - exit from recursion */
            *saved_key= k - page_buf;
            goto ok;
          case 1: /* not found - continue searching */
            info->maria_rtree_recursion_depth= level;
            break;
          default: /* error */
          case -1:
            goto err;
        }
      }
    }
    else
    {
      /* this is a leaf */
      if (!maria_rtree_key_cmp(keyinfo->seg, info->first_mbr_key,
                               k, info->last_rkey_length, search_flag))
      {
        uchar *after_key= rt_PAGE_NEXT_KEY(share, k, key_data_length, 0);
        MARIA_KEY tmp_key;
        
        /*
          We don't need to set all MARIA_KEY elements here as
          _ma_row_pos_from_key() only uses a few of them.
         */
        tmp_key.keyinfo= keyinfo;
        tmp_key.data= k;
        tmp_key.data_length= key_data_length;

        info->cur_row.lastpos= _ma_row_pos_from_key(&tmp_key);
        info->last_key.keyinfo= keyinfo;
        info->last_key.data_length= key_data_length;
        info->last_key.ref_length=  share->base.rec_reflength;
        info->last_key.flag= 0;
        memcpy(info->last_key.data, k,
               info->last_key.data_length + info->last_key.ref_length);
        info->maria_rtree_recursion_depth= level;
        *saved_key= last - page_buf;

        if (after_key < last)
        {
          uchar *keyread_buff= info->keyread_buff;
          info->int_keypos= keyread_buff;
          info->int_maxpos= keyread_buff + (last - after_key);
          memcpy(keyread_buff, after_key, last - after_key);
          info->keyread_buff_used= 0;
        }
        else
        {
	  info->keyread_buff_used= 1;
        }

        res= 0;
        goto ok;
      }
    }
  }
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  my_errno= HA_ERR_KEY_NOT_FOUND;
  res= 1;

ok:
  my_afree(page_buf);
  return res;

err:
  my_afree(page_buf);
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  return -1;
}


/*
  Find first key in r-tree according to search_flag condition

  SYNOPSIS
   maria_rtree_find_first()
   info			Handler to MARIA file
   key			Key to search for
   search_flag		Bitmap of flags how to do the search

  RETURN
    -1  Error
    0   Found
    1   Not found
*/

int maria_rtree_find_first(MARIA_HA *info, MARIA_KEY *key, uint32 search_flag)
{
  my_off_t root;
  uint nod_cmp_flag;
  MARIA_KEYDEF *keyinfo= key->keyinfo;

  if ((root= info->s->state.key_root[keyinfo->key_nr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }

  /*
    Save searched key, include data pointer.
    The data pointer is required if the search_flag contains MBR_DATA.
    (minimum bounding rectangle)
  */
  memcpy(info->first_mbr_key, key->data, key->data_length + key->ref_length);
  info->last_rkey_length= key->data_length;

  info->maria_rtree_recursion_depth= -1;
  info->keyread_buff_used= 1;

  nod_cmp_flag= ((search_flag & (MBR_EQUAL | MBR_WITHIN)) ?
                 MBR_WITHIN : MBR_INTERSECT);
  return maria_rtree_find_req(info, keyinfo, search_flag, nod_cmp_flag, root,
                              0);
}


/*
   Find next key in r-tree according to search_flag condition

  SYNOPSIS
   maria_rtree_find_next()
   info			Handler to MARIA file
   uint keynr		Key number to use
   search_flag		Bitmap of flags how to do the search

   RETURN
     -1  Error
     0   Found
     1   Not found
*/

int maria_rtree_find_next(MARIA_HA *info, uint keynr, uint32 search_flag)
{
  my_off_t root;
  uint32 nod_cmp_flag;
  MARIA_KEYDEF *keyinfo= info->s->keyinfo + keynr;
  DBUG_ASSERT(info->last_key.keyinfo == keyinfo);

  if (info->update & HA_STATE_DELETED)
    return maria_rtree_find_first(info, &info->last_key, search_flag);

  if (!info->keyread_buff_used)
  {
    uchar *key= info->int_keypos;

    while (key < info->int_maxpos)
    {
      if (!maria_rtree_key_cmp(keyinfo->seg,
                               info->first_mbr_key, key,
                               info->last_rkey_length, search_flag))
      {
        uchar *after_key= key + keyinfo->keylength;
        MARIA_KEY tmp_key;
        
        /*
          We don't need to set all MARIA_KEY elements here as
          _ma_row_pos_from_key only uses a few of them.
         */
        tmp_key.keyinfo= keyinfo;
        tmp_key.data= key;
        tmp_key.data_length= keyinfo->keylength - info->s->base.rec_reflength;

        info->cur_row.lastpos= _ma_row_pos_from_key(&tmp_key);
        memcpy(info->last_key.data, key, info->last_key.data_length);

        if (after_key < info->int_maxpos)
	  info->int_keypos= after_key;
        else
	  info->keyread_buff_used= 1;
        return 0;
      }
      key+= keyinfo->keylength;
    }
  }
  if ((root= info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }

  nod_cmp_flag= (((search_flag & (MBR_EQUAL | MBR_WITHIN)) ?
                  MBR_WITHIN : MBR_INTERSECT));
  return maria_rtree_find_req(info, keyinfo, search_flag, nod_cmp_flag, root,
                              0);
}


/*
  Get next key in r-tree recursively

  NOTES
    Used in maria_rtree_get_first() and maria_rtree_get_next()

  RETURN
    -1  Error
    0   Found
    1   Not found
*/

static int maria_rtree_get_req(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                               uint key_length, my_off_t page_pos, int level)
{
  MARIA_SHARE *share= info->s;
  uchar *page_buf, *last, *k;
  uint nod_flag, key_data_length;
  int res;
  uint *saved_key= (uint*) (info->maria_rtree_recursion_state) + level;
  MARIA_PAGE page;

  if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length)))
    return -1;
  if (_ma_fetch_keypage(&page, info, keyinfo, page_pos,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                         DFLT_INIT_HITS, page_buf, 0))
    goto err;
  nod_flag= page.node;

  key_data_length= keyinfo->keylength - share->base.rec_reflength;

  if (info->maria_rtree_recursion_depth >= level)
  {
    k= page.buff + *saved_key;
    if (!nod_flag)
    {
      /* Only leaf pages contain data references. */
      /* Need to check next key with data reference. */
      k= rt_PAGE_NEXT_KEY(share, k, key_data_length, nod_flag);
    }
  }
  else
  {
    k= rt_PAGE_FIRST_KEY(share, page.buff, nod_flag);
  }
  last= rt_PAGE_END(&page);

  for (; k < last; k= rt_PAGE_NEXT_KEY(share, k, key_data_length, nod_flag))
  {
    if (nod_flag)
    {
      /* this is an internal node in the tree */
      switch ((res= maria_rtree_get_req(info, keyinfo, key_length,
                                         _ma_kpos(nod_flag, k), level + 1)))
      {
        case 0: /* found - exit from recursion */
          *saved_key= k - page.buff;
          goto ok;
        case 1: /* not found - continue searching */
          info->maria_rtree_recursion_depth= level;
          break;
        default:
        case -1: /* error */
          goto err;
      }
    }
    else
    {
      /* this is a leaf */
      uchar *after_key= rt_PAGE_NEXT_KEY(share, k, key_data_length, 0);
      MARIA_KEY tmp_key;
        
      /*
        We don't need to set all MARIA_KEY elements here as
        _ma_row_pos_from_key() only uses a few of them.
      */
      tmp_key.keyinfo= keyinfo;
      tmp_key.data= k;
      tmp_key.data_length= key_data_length;

      info->cur_row.lastpos= _ma_row_pos_from_key(&tmp_key);
      info->last_key.data_length= key_data_length;
      info->last_key.ref_length= share->base.rec_reflength;

      memcpy(info->last_key.data, k,
             info->last_key.data_length + info->last_key.ref_length);

      info->maria_rtree_recursion_depth= level;
      *saved_key= k - page.buff;

      if (after_key < last)
      {
        uchar *keyread_buff= info->keyread_buff;
        info->last_rtree_keypos= saved_key;
        memcpy(keyread_buff, page.buff, page.size);
        info->int_maxpos= keyread_buff + page.size;
        info->keyread_buff_used= 0;
      }
      else
      {
	info->keyread_buff_used= 1;
      }

      res= 0;
      goto ok;
    }
  }
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  my_errno= HA_ERR_KEY_NOT_FOUND;
  res= 1;

ok:
  my_afree(page_buf);
  return res;

err:
  my_afree(page_buf);
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  return -1;
}


/*
  Get first key in r-tree

  RETURN
    -1	Error
    0	Found
    1	Not found
*/

int maria_rtree_get_first(MARIA_HA *info, uint keynr, uint key_length)
{
  my_off_t root;
  MARIA_KEYDEF *keyinfo= info->s->keyinfo + keynr;

  if ((root= info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    return -1;
  }

  info->maria_rtree_recursion_depth= -1;
  info->keyread_buff_used= 1;

  return maria_rtree_get_req(info, keyinfo, key_length, root, 0);
}


/*
  Get next key in r-tree

  RETURN
    -1	Error
    0	Found
    1	Not found
*/

int maria_rtree_get_next(MARIA_HA *info, uint keynr, uint key_length)
{
  my_off_t root;
  MARIA_KEYDEF *keyinfo= info->s->keyinfo + keynr;
  uchar *keyread_buff= info->keyread_buff;

  if (!info->keyread_buff_used)
  {
    uint key_data_length= keyinfo->keylength - info->s->base.rec_reflength;
    /* rt_PAGE_NEXT_KEY(*info->last_rtree_keypos) */
    uchar *key= keyread_buff + *info->last_rtree_keypos + keyinfo->keylength;
    /* rt_PAGE_NEXT_KEY(key) */
    uchar *after_key= key + keyinfo->keylength;
    MARIA_KEY tmp_key;

    tmp_key.keyinfo= keyinfo;
    tmp_key.data= key;
    tmp_key.data_length= key_data_length;
    tmp_key.ref_length= info->s->base.rec_reflength;
    tmp_key.flag= 0;

    info->cur_row.lastpos= _ma_row_pos_from_key(&tmp_key);
    _ma_copy_key(&info->last_key, &tmp_key);

    *info->last_rtree_keypos= (uint) (key - keyread_buff);
    if (after_key >= info->int_maxpos)
    {
      info->keyread_buff_used= 1;
    }

    return 0;
  }
  else
  {
    if ((root= info->s->state.key_root[keynr]) == HA_OFFSET_ERROR)
    {
      my_errno= HA_ERR_END_OF_FILE;
      return -1;
    }

    return maria_rtree_get_req(info, &keyinfo[keynr], key_length, root, 0);
  }
}


/*
  Choose non-leaf better key for insertion

  Returns a pointer inside the page_buf buffer.
*/
#ifdef PICK_BY_PERIMETER
static const uchar *maria_rtree_pick_key(const MARIA_KEY *key,
                                         const MARIA_PAGE *page)
{
  double increase;
  double best_incr;
  double perimeter;
  double best_perimeter;
  uchar *best_key= NULL;
  const MARIA_HA *info= page->info;

  uchar *k= rt_PAGE_FIRST_KEY(info->s, page->buf, page->node);
  uchar *last= rt_PAGE_END(info, page);

  LINT_INIT(best_perimeter);
  LINT_INIT(best_key);
  LINT_INIT(best_incr);

  for (; k < last; k= rt_PAGE_NEXT_KEY(k, key->data_length, nod_flag))
  {
    if ((increase= maria_rtree_perimeter_increase(keyinfo->seg, k, key,
                                                  &perimeter)) == -1)
      return NULL;
    if ((increase < best_incr)||
	(increase == best_incr && perimeter < best_perimeter))
    {
      best_key= k;
      best_perimeter= perimeter;
      best_incr= increase;
    }
  }
  return best_key;
}

#endif /*PICK_BY_PERIMETER*/

#ifdef PICK_BY_AREA
static const uchar *maria_rtree_pick_key(const MARIA_KEY *key,
                                         const MARIA_PAGE *page)
{
  const MARIA_HA *info= page->info;
  MARIA_SHARE *share= info->s;
  double increase;
  double best_incr= DBL_MAX;
  double area;
  double best_area;
  const uchar *best_key= NULL;
  const uchar *k= rt_PAGE_FIRST_KEY(share, page->buff, page->node);
  const uchar *last= rt_PAGE_END(page);

  LINT_INIT(best_area);

  for (; k < last;
       k= rt_PAGE_NEXT_KEY(share, k, key->data_length, page->node))
  {
    /* The following is safe as -1.0 is an exact number */
    if ((increase= maria_rtree_area_increase(key->keyinfo->seg, k, key->data,
                                             key->data_length +
                                             key->ref_length,
                                             &area)) == -1.0)
      return NULL;
    /* The following should be safe, even if we compare doubles */
    if (!best_key || increase < best_incr ||
        ((increase == best_incr) && (area < best_area)))
    {
      best_key= k;
      best_area= area;
      best_incr= increase;
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

static int maria_rtree_insert_req(MARIA_HA *info, MARIA_KEY *key,
                                  my_off_t page_pos, my_off_t *new_page,
                                  int ins_level, int level)
{
  uint nod_flag;
  uint key_length= key->data_length;
  int res;
  uchar *page_buf, *k;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  DBUG_ENTER("maria_rtree_insert_req");

  if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length +
                                     MARIA_MAX_KEY_BUFF)))
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1); /* purecov: inspected */
  }
  if (_ma_fetch_keypage(&page, info, keyinfo, page_pos, PAGECACHE_LOCK_WRITE,
                        DFLT_INIT_HITS, page_buf, 0))
    goto err;
  nod_flag= page.node;
  DBUG_PRINT("rtree", ("page: %lu  level: %d  ins_level: %d  nod_flag: %u",
                       (ulong) page.pos, level, ins_level, nod_flag));

  if ((ins_level == -1 && nod_flag) ||       /* key: go down to leaf */
      (ins_level > -1 && ins_level > level)) /* branch: go down to ins_level */
  {
    if (!(k= (uchar *)maria_rtree_pick_key(key, &page)))
      goto err;
    /* k is now a pointer inside the page_buf buffer */
    switch ((res= maria_rtree_insert_req(info, key,
                                         _ma_kpos(nod_flag, k), new_page,
                                         ins_level, level + 1)))
    {
      case 0: /* child was not split, most common case */
      {
        maria_rtree_combine_rect(keyinfo->seg, k, key->data, k, key_length);
        if (share->now_transactional &&
            _ma_log_change(&page, k, key_length))
          goto err;
        page_mark_changed(info, &page);
        if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                              DFLT_INIT_HITS))
          goto err;
        goto ok;
      }
      case 1: /* child was split */
      {
        /* Set new_key to point to a free buffer area */
        uchar *new_key_buff= page_buf + keyinfo->block_length + nod_flag;
        MARIA_KEY new_key;
        MARIA_KEY k_key;

        DBUG_ASSERT(nod_flag);
        k_key.keyinfo= new_key.keyinfo= keyinfo;
        new_key.data= new_key_buff;
        k_key.data= k;
        k_key.data_length= new_key.data_length= key->data_length;
        k_key.ref_length=  new_key.ref_length=  key->ref_length;
        k_key.flag= new_key.flag= 0;            /* Safety */

        /* set proper MBR for key */
        if (maria_rtree_set_key_mbr(info, &k_key, _ma_kpos(nod_flag, k)))
          goto err;
        if (share->now_transactional &&
            _ma_log_change(&page, k, key_length))
          goto err;
        /* add new key for new page */
        _ma_kpointer(info, new_key_buff - nod_flag, *new_page);
        if (maria_rtree_set_key_mbr(info, &new_key, *new_page))
          goto err;
        res= maria_rtree_add_key(&new_key, &page, new_page);
        page_mark_changed(info, &page);
        if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                              DFLT_INIT_HITS))
          goto err;
        goto ok;
      }
      default:
      case -1: /* error */
      {
        goto err;
      }
    }
  }
  else
  {
    res= maria_rtree_add_key(key, &page, new_page);
    page_mark_changed(info, &page);
    if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                          DFLT_INIT_HITS))
      goto err;
  }

ok:
  my_afree(page_buf);
  DBUG_RETURN(res);

err:
  res= -1;                                   /* purecov: inspected */
  goto ok;                                   /* purecov: inspected */
}


/**
  Insert key into the tree

  @param  info             table
  @param  key              KEY to insert
  @param  ins_level        at which level key insertion should start
  @param  root             put new key_root there

  @return Operation result
    @retval  -1 Error
    @retval   0 Root was not split
    @retval   1 Root was split
*/

int maria_rtree_insert_level(MARIA_HA *info, MARIA_KEY *key, int ins_level,
                             my_off_t *root)
{
  my_off_t old_root;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  int res;
  my_off_t new_page;
  enum pagecache_page_lock write_lock;
  DBUG_ENTER("maria_rtree_insert_level");

  if ((old_root= share->state.key_root[keyinfo->key_nr]) == HA_OFFSET_ERROR)
  {
    MARIA_PINNED_PAGE tmp_page_link, *page_link;
    MARIA_PAGE page;

    page_link= &tmp_page_link;
    if ((old_root= _ma_new(info, DFLT_INIT_HITS, &page_link)) ==
        HA_OFFSET_ERROR)
      DBUG_RETURN(-1);
    write_lock= page_link->write_lock;
    info->keyread_buff_used= 1;
    bzero(info->buff, share->block_size);
    _ma_store_keynr(share, info->buff, keyinfo->key_nr);
    _ma_store_page_used(share, info->buff, share->keypage_header);
    _ma_page_setup(&page, info, keyinfo, old_root, info->buff);

    if (share->now_transactional && _ma_log_new(&page, 1))
      DBUG_RETURN(1);

    res= maria_rtree_add_key(key, &page, NULL);
    if (_ma_write_keypage(&page, write_lock, DFLT_INIT_HITS))
      DBUG_RETURN(1);
    *root= old_root;
    DBUG_RETURN(res);
  }

  switch ((res= maria_rtree_insert_req(info, key, old_root, &new_page,
                                       ins_level, 0)))
  {
    case 0: /* root was not split */
    {
      break;
    }
    case 1: /* root was split, grow a new root; very rare */
    {
      uchar *new_root_buf, *new_key_buff;
      my_off_t new_root;
      uint nod_flag= share->base.key_reflength;
      MARIA_PINNED_PAGE tmp_page_link, *page_link;
      MARIA_KEY new_key;
      MARIA_PAGE page;
      page_link= &tmp_page_link;

      DBUG_PRINT("rtree", ("root was split, grow a new root"));
      if (!(new_root_buf= (uchar*) my_alloca((uint) keyinfo->block_length +
                                             MARIA_MAX_KEY_BUFF)))
      {
        my_errno= HA_ERR_OUT_OF_MEM;
        DBUG_RETURN(-1); /* purecov: inspected */
      }

      bzero(new_root_buf, share->block_size);
      _ma_store_keypage_flag(share, new_root_buf, KEYPAGE_FLAG_ISNOD);
      _ma_store_keynr(share, new_root_buf, keyinfo->key_nr);
      _ma_store_page_used(share, new_root_buf, share->keypage_header);
      if ((new_root= _ma_new(info, DFLT_INIT_HITS, &page_link)) ==
	  HA_OFFSET_ERROR)
        goto err;
      write_lock= page_link->write_lock;

      _ma_page_setup(&page, info, keyinfo, new_root, new_root_buf);

      if (share->now_transactional && _ma_log_new(&page, 1))
        goto err;

      /* Point to some free space */
      new_key_buff= new_root_buf + keyinfo->block_length + nod_flag;
      new_key.keyinfo=     keyinfo;
      new_key.data=        new_key_buff;
      new_key.data_length= key->data_length;
      new_key.ref_length=  key->ref_length;
      new_key.flag= 0;

      _ma_kpointer(info, new_key_buff - nod_flag, old_root);
      if (maria_rtree_set_key_mbr(info, &new_key, old_root))
        goto err;
      if (maria_rtree_add_key(&new_key, &page, NULL)
          == -1)
        goto err;
      _ma_kpointer(info, new_key_buff - nod_flag, new_page);
      if (maria_rtree_set_key_mbr(info, &new_key, new_page))
        goto err;
      if (maria_rtree_add_key(&new_key, &page, NULL)
          == -1)
        goto err;
      if (_ma_write_keypage(&page, write_lock, DFLT_INIT_HITS))
        goto err;
      *root= new_root;
      DBUG_PRINT("rtree", ("new root page: %lu  level: %d  nod_flag: %u",
                           (ulong) new_root, 0, page.node));

      my_afree(new_root_buf);
      break;
err:
      my_afree(new_root_buf);
      DBUG_RETURN(-1); /* purecov: inspected */
    }
    default:
    case -1: /* error */
    {
      DBUG_ASSERT(0);
      break;
    }
  }
  DBUG_RETURN(res);
}


/*
  Insert key into the tree - interface function

  RETURN
    1	Error
    0	OK
*/

my_bool maria_rtree_insert(MARIA_HA *info, MARIA_KEY *key)
{
  int res;
  MARIA_SHARE *share= info->s;
  my_off_t *root,  new_root;
  LSN lsn= LSN_IMPOSSIBLE;
  DBUG_ENTER("maria_rtree_insert");

  if (!key)
    DBUG_RETURN(1);                       /* _ma_sp_make_key failed */

  root= &share->state.key_root[key->keyinfo->key_nr];
  new_root= *root;

  if ((res= (maria_rtree_insert_level(info, key, -1, &new_root) == -1)))
    goto err;
  if (share->now_transactional)
    res= _ma_write_undo_key_insert(info, key, root, new_root, &lsn);
  else
  {
    *root= new_root;
    _ma_fast_unlock_key_del(info);
  }
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
err:
  DBUG_RETURN(res != 0);
}


/*
  Fill reinsert page buffer

  RETURN
    1	Error
    0	OK
*/

static my_bool maria_rtree_fill_reinsert_list(stPageList *ReinsertList,
                                              my_off_t page, int level)
{
  DBUG_ENTER("maria_rtree_fill_reinsert_list");
  DBUG_PRINT("rtree", ("page: %lu  level: %d", (ulong) page, level));
  if (ReinsertList->n_pages == ReinsertList->m_pages)
  {
    ReinsertList->m_pages += REINSERT_BUFFER_INC;
    if (!(ReinsertList->pages= (stPageLevel*)my_realloc((uchar*)ReinsertList->pages,
      ReinsertList->m_pages * sizeof(stPageLevel), MYF(MY_ALLOW_ZERO_PTR))))
      goto err;
  }
  /* save page to ReinsertList */
  ReinsertList->pages[ReinsertList->n_pages].offs= page;
  ReinsertList->pages[ReinsertList->n_pages].level= level;
  ReinsertList->n_pages++;
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);                             /* purecov: inspected */
}


/*
  Go down and delete key from the tree

  RETURN
    -1	Error
    0	Deleted
    1	Not found
    2	Empty leaf
*/

static int maria_rtree_delete_req(MARIA_HA *info, const MARIA_KEY *key,
                                  my_off_t page_pos, uint *page_size,
                                  stPageList *ReinsertList, int level)
{
  ulong i;
  uint nod_flag;
  int res;
  uchar *page_buf, *last, *k;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  DBUG_ENTER("maria_rtree_delete_req");

  if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1); /* purecov: inspected */
  }
  if (_ma_fetch_keypage(&page, info, keyinfo, page_pos, PAGECACHE_LOCK_WRITE,
                        DFLT_INIT_HITS, page_buf, 0))
    goto err;
  nod_flag= page.node;
  DBUG_PRINT("rtree", ("page: %lu  level: %d  nod_flag: %u",
                       (ulong) page_pos, level, nod_flag));

  k= rt_PAGE_FIRST_KEY(share, page_buf, nod_flag);
  last= rt_PAGE_END(&page);

  for (i= 0;
       k < last;
       k= rt_PAGE_NEXT_KEY(share, k, key->data_length, nod_flag), i++)
  {
    if (nod_flag)
    {
      /* not leaf */
      if (!maria_rtree_key_cmp(keyinfo->seg, key->data, k, key->data_length,
                               MBR_WITHIN))
      {
        switch ((res= maria_rtree_delete_req(info, key,
                                             _ma_kpos(nod_flag, k),
                                             page_size, ReinsertList,
                                             level + 1)))
        {
          case 0: /* deleted */
          {
            /* test page filling */
            if (*page_size + key->data_length >=
                rt_PAGE_MIN_SIZE(keyinfo->block_length))
            {
              /* OK */
              /* Calculate a new key value (MBR) for the shrinked block. */
              MARIA_KEY tmp_key;
              tmp_key.keyinfo= keyinfo;
              tmp_key.data= k;
              tmp_key.data_length= key->data_length;
              tmp_key.ref_length=  key->ref_length;
              tmp_key.flag= 0;                  /* Safety */

              if (maria_rtree_set_key_mbr(info, &tmp_key,
                                          _ma_kpos(nod_flag, k)))
                goto err;
              if (share->now_transactional &&
                  _ma_log_change(&page, k, key->data_length))
                goto err;
              page_mark_changed(info, &page)
              if (_ma_write_keypage(&page,
                                    PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                    DFLT_INIT_HITS))
                goto err;
            }
            else
            {
              /*
                Too small: delete key & add it descendant to reinsert list.
                Store position and level of the block so that it can be
                accessed later for inserting the remaining keys.
              */
              DBUG_PRINT("rtree", ("too small. move block to reinsert list"));
              if (maria_rtree_fill_reinsert_list(ReinsertList,
                                                 _ma_kpos(nod_flag, k),
                                                 level + 1))
                goto err;
              /*
                Delete the key that references the block. This makes the
                block disappear from the index. Hence we need to insert
                its remaining keys later. Note: if the block is a branch
                block, we do not only remove this block, but the whole
                subtree. So we need to re-insert its keys on the same
                level later to reintegrate the subtrees.
              */
              if (maria_rtree_delete_key(&page, k, key->data_length))
                goto err;
              page_mark_changed(info, &page);
              if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                    DFLT_INIT_HITS))
                goto err;
              *page_size= page.size;
            }

            goto ok;
          }
          case 1: /* not found - continue searching */
          {
            break;
          }
          case 2: /* vacuous case: last key in the leaf */
          {
            if (maria_rtree_delete_key(&page, k, key->data_length))
              goto err;
            page_mark_changed(info, &page);
            if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                  DFLT_INIT_HITS))
              goto err;
            *page_size= page.size;
            res= 0;
            goto ok;
          }
          default: /* error */
          case -1:
          {
            goto err;
          }
        }
      }
    }
    else
    {
      /* leaf */
      if (!maria_rtree_key_cmp(keyinfo->seg, key->data, k, key->data_length,
                               MBR_EQUAL | MBR_DATA))
      {
        page_mark_changed(info, &page);
        if (maria_rtree_delete_key(&page, k, key->data_length))
          goto err;
        *page_size= page.size;
        if (*page_size == info->s->keypage_header)
        {
          /* last key in the leaf */
          res= 2;
          if (_ma_dispose(info, page.pos, 0))
            goto err;
        }
        else
        {
          res= 0;
          if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                DFLT_INIT_HITS))
            goto err;
        }
        goto ok;
      }
    }
  }
  res= 1;

ok:
  my_afree(page_buf);
  DBUG_RETURN(res);

err:
  my_afree(page_buf);
  DBUG_RETURN(-1); /* purecov: inspected */
}


/*
  Delete key - interface function

  RETURN
    1	Error
    0	Deleted
*/

my_bool maria_rtree_delete(MARIA_HA *info, MARIA_KEY *key)
{
  MARIA_SHARE *share= info->s;
  my_off_t new_root= share->state.key_root[key->keyinfo->key_nr];
  int res;
  LSN lsn= LSN_IMPOSSIBLE;
  DBUG_ENTER("maria_rtree_delete");

  if ((res= maria_rtree_real_delete(info, key, &new_root)))
    goto err;

  if (share->now_transactional)
    res= _ma_write_undo_key_delete(info, key, new_root, &lsn);
  else
    share->state.key_root[key->keyinfo->key_nr]= new_root;

err:
  _ma_fast_unlock_key_del(info);
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res != 0);
}


my_bool maria_rtree_real_delete(MARIA_HA *info, MARIA_KEY *key,
                                my_off_t *root)
{
  uint page_size;
  stPageList ReinsertList;
  my_off_t old_root;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  uint key_data_length= key->data_length;
  DBUG_ENTER("maria_rtree_real_delete");

  if ((old_root= share->state.key_root[keyinfo->key_nr]) ==
      HA_OFFSET_ERROR)
  {
    my_errno= HA_ERR_END_OF_FILE;
    DBUG_RETURN(1);                           /* purecov: inspected */
  }
  DBUG_PRINT("rtree", ("starting deletion at root page: %lu",
                       (ulong) old_root));

  ReinsertList.pages= NULL;
  ReinsertList.n_pages= 0;
  ReinsertList.m_pages= 0;

  switch (maria_rtree_delete_req(info, key, old_root, &page_size,
                                 &ReinsertList, 0)) {
  case 2: /* empty */
  {
    *root= HA_OFFSET_ERROR;
    break;
  }
  case 0: /* deleted */
  {
    uint nod_flag;
    ulong i;
    uchar *page_buf;
    MARIA_PAGE page;
    MARIA_KEY tmp_key;
    tmp_key.keyinfo=     key->keyinfo;
    tmp_key.data_length= key->data_length;
    tmp_key.ref_length=  key->ref_length;
    tmp_key.flag=        0;                     /* Safety */

    if (ReinsertList.n_pages)
    {
      if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length)))
      {
        my_errno= HA_ERR_OUT_OF_MEM;
        goto err;
      }

      for (i= 0; i < ReinsertList.n_pages; ++i)
      {
        uchar *k, *last;
        if (_ma_fetch_keypage(&page, info, keyinfo, ReinsertList.pages[i].offs,
                              PAGECACHE_LOCK_WRITE,
                              DFLT_INIT_HITS, page_buf, 0))
          goto err;
        nod_flag= page.node;
        DBUG_PRINT("rtree", ("reinserting keys from "
                             "page: %lu  level: %d  nod_flag: %u",
                             (ulong) ReinsertList.pages[i].offs,
                             ReinsertList.pages[i].level, nod_flag));

        k= rt_PAGE_FIRST_KEY(share, page.buff, nod_flag);
        last= rt_PAGE_END(&page);
        for (; k < last; k= rt_PAGE_NEXT_KEY(share, k, key_data_length,
                                             nod_flag))
        {
          int res;
          tmp_key.data= k;
          if ((res= maria_rtree_insert_level(info, &tmp_key,
                                             ReinsertList.pages[i].level,
                                             root)) == -1)
          {
            my_afree(page_buf);
            goto err;
          }
          if (res)
          {
            uint j;
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
        page_mark_changed(info, &page);
        if (_ma_dispose(info, page.pos, 0))
        {
          my_afree(page_buf);
          goto err;
        }
      }
      my_afree(page_buf);
      my_free(ReinsertList.pages, MYF(0));
    }

    /* check for redundant root (not leaf, 1 child) and eliminate */
    if ((old_root= *root) == HA_OFFSET_ERROR)
      goto err;
    if (_ma_fetch_keypage(&page, info, keyinfo, old_root,
                          PAGECACHE_LOCK_WRITE,
                          DFLT_INIT_HITS, info->buff, 0))
      goto err;
    nod_flag= page.node;
    if (nod_flag && (page.size == share->keypage_header + key_data_length +
                     nod_flag))
    {
      *root= _ma_kpos(nod_flag,
                      rt_PAGE_FIRST_KEY(share, info->buff, nod_flag));
      page_mark_changed(info, &page);
      if (_ma_dispose(info, page.pos, 0))
        goto err;
    }
    info->update= HA_STATE_DELETED;
    break;
  }
  case 1:                                     /* not found */
  {
    my_errno= HA_ERR_KEY_NOT_FOUND;
    goto err;
  }
  case -1:                                    /* error */
  default:
    goto err;                                 /* purecov: inspected */
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


/*
  Estimate number of suitable keys in the tree

  RETURN
    estimated value
*/

ha_rows maria_rtree_estimate(MARIA_HA *info, MARIA_KEY *key, uint32 flag)
{
  my_off_t root;
  uint i= 0;
  uint nod_flag, key_data_length;
  uchar *page_buf, *k, *last;
  double area= 0;
  ha_rows res= 0;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;

  if (flag & MBR_DISJOINT)
    return info->state->records;

  if ((root= share->state.key_root[key->keyinfo->key_nr]) == HA_OFFSET_ERROR)
    return HA_POS_ERROR;
  if (!(page_buf= (uchar*) my_alloca((uint) keyinfo->block_length)))
    return HA_POS_ERROR;
  if (_ma_fetch_keypage(&page, info, keyinfo, root,
                        PAGECACHE_LOCK_LEFT_UNLOCKED, DFLT_INIT_HITS, page_buf,
                        0))
    goto err;
  nod_flag= page.node;

  key_data_length= key->data_length;

  k= rt_PAGE_FIRST_KEY(share, page.buff, nod_flag);
  last= rt_PAGE_END(&page);

  for (; k < last;
       k= rt_PAGE_NEXT_KEY(share, k, key_data_length, nod_flag), i++)
  {
    if (nod_flag)
    {
      double k_area= maria_rtree_rect_volume(keyinfo->seg, k, key_data_length);

      /* The following should be safe, even if we compare doubles */
      if (k_area == 0)
      {
        if (flag & (MBR_CONTAIN | MBR_INTERSECT))
        {
          area+= 1;
        }
        else if (flag & (MBR_WITHIN | MBR_EQUAL))
        {
          if (!maria_rtree_key_cmp(keyinfo->seg, key->data, k, key_data_length,
                                   MBR_WITHIN))
            area+= 1;
        }
        else
          goto err;
      }
      else
      {
        if (flag & (MBR_CONTAIN | MBR_INTERSECT))
        {
          area+= maria_rtree_overlapping_area(keyinfo->seg, key->data, k,
                                              key_data_length) / k_area;
        }
        else if (flag & (MBR_WITHIN | MBR_EQUAL))
        {
          if (!maria_rtree_key_cmp(keyinfo->seg, key->data, k, key_data_length,
                                   MBR_WITHIN))
            area+= (maria_rtree_rect_volume(keyinfo->seg, key->data,
                                            key_data_length) / k_area);
        }
        else
          goto err;
      }
    }
    else
    {
      if (!maria_rtree_key_cmp(keyinfo->seg, key->data, k, key_data_length,
                               flag))
        ++res;
    }
  }
  if (nod_flag)
  {
    if (i)
      res= (ha_rows) (area / i * info->state->records);
    else
      res= HA_POS_ERROR;
  }

  my_afree(page_buf);
  return res;

err:
  my_afree(page_buf);
  return HA_POS_ERROR;
}

#endif /*HAVE_RTREE_KEYS*/
