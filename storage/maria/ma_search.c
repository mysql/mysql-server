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

/* key handling functions */

#include "ma_fulltext.h"
#include "m_ctype.h"

static int _ma_search_no_save(register MARIA_HA *info, MARIA_KEY *key,
                              uint32 nextflag, register my_off_t pos,
                              MARIA_PINNED_PAGE **res_page_link,
                              uchar **res_page_buff);
static my_bool _ma_get_prev_key(MARIA_KEY *key, MARIA_PAGE *ma_page,
                                uchar *keypos);


/* Check that new index is ok */

int _ma_check_index(MARIA_HA *info, int inx)
{
  if (inx < 0 || ! maria_is_key_active(info->s->state.key_map, inx))
  {
    my_errno=HA_ERR_WRONG_INDEX;
    return -1;
  }
  if (info->lastinx != inx)             /* Index changed */
  {
    info->lastinx = inx;
    info->page_changed=1;
    info->update= ((info->update & (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED)) |
                   HA_STATE_NEXT_FOUND | HA_STATE_PREV_FOUND);
  }
  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    return(-1);
  return(inx);
} /* _ma_check_index */


/**
   @breif Search after row by a key

   @note
     Position to row is stored in info->lastpos

   @return
   @retval  0   ok (key found)
   @retval -1   Not found
   @retval  1   If one should continue search on higher level
*/

int _ma_search(register MARIA_HA *info, MARIA_KEY *key, uint32 nextflag,
               my_off_t pos)
{
  int error;
  MARIA_PINNED_PAGE *page_link;
  uchar *page_buff;

  info->page_changed= 1;                        /* If page not saved */
  if (!(error= _ma_search_no_save(info, key, nextflag, pos, &page_link,
                                  &page_buff)))
  {
    if (nextflag & SEARCH_SAVE_BUFF)
    {
      bmove512(info->keyread_buff, page_buff, info->s->block_size);

      /* Save position for a possible read next / previous */
      info->int_keypos= info->keyread_buff + info->keypos_offset;
      info->int_maxpos= info->keyread_buff + info->maxpos_offset;
      info->int_keytree_version= key->keyinfo->version;
      info->last_search_keypage= info->last_keypage;
      info->page_changed= 0;
      info->keyread_buff_used= 0;
    }
  }
  _ma_unpin_all_pages(info, LSN_IMPOSSIBLE);
  return (error);
}

/**
   @breif Search after row by a key

   ret_page_link	Will contain pointer to page where we found key

   @note
     Position to row is stored in info->lastpos

   @return
   @retval  0   ok (key found)
   @retval -1   Not found
   @retval  1   If one should continue search on higher level
*/

static int _ma_search_no_save(register MARIA_HA *info, MARIA_KEY *key,
                              uint32 nextflag, register my_off_t pos,
                              MARIA_PINNED_PAGE **res_page_link,
                              uchar **res_page_buff)
{
  my_bool last_key_not_used;
  int error,flag;
  uint page_flag, nod_flag, used_length;
  uchar *keypos,*maxpos;
  uchar lastkey[MARIA_MAX_KEY_BUFF];
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  MARIA_PINNED_PAGE *page_link;
  DBUG_ENTER("_ma_search");
  DBUG_PRINT("enter",("page: %lu  nextflag: %u  lastpos: %lu",
                      (ulong) (pos / info->s->block_size),
                      nextflag, (ulong) info->cur_row.lastpos));
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, key););

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;                      /* Didn't find key */
    info->cur_row.lastpos= HA_OFFSET_ERROR;
    if (!(nextflag & (SEARCH_SMALLER | SEARCH_BIGGER | SEARCH_LAST)))
      DBUG_RETURN(-1);                          /* Not found ; return error */
    DBUG_RETURN(1);                             /* Search at upper levels */
  }

  if (_ma_fetch_keypage(&page, info, keyinfo, pos,
                        PAGECACHE_LOCK_READ, DFLT_INIT_HITS, 0, 0))
    goto err;
  page_link= dynamic_element(&info->pinned_pages,
                             info->pinned_pages.elements-1,
                             MARIA_PINNED_PAGE*);
  DBUG_DUMP("page", page.buff, page.size);

  flag= (*keyinfo->bin_search)(key, &page, nextflag, &keypos, lastkey,
                               &last_key_not_used);
  if (flag == MARIA_FOUND_WRONG_KEY)
    DBUG_RETURN(-1);
  page_flag=   page.flag;
  used_length= page.size;
  nod_flag=    page.node;
  maxpos=      page.buff + used_length -1;

  if (flag)
  {
    if ((error= _ma_search_no_save(info, key, nextflag,
                                   _ma_kpos(nod_flag,keypos),
                                   res_page_link, res_page_buff)) <= 0)
      DBUG_RETURN(error);

    if (flag >0)
    {
      if (nextflag & (SEARCH_SMALLER | SEARCH_LAST) &&
          keypos == page.buff + info->s->keypage_header + nod_flag)
        DBUG_RETURN(1);                                 /* Bigger than key */
    }
    else if (nextflag & SEARCH_BIGGER && keypos >= maxpos)
      DBUG_RETURN(1);                                   /* Smaller than key */
  }
  else
  {
    /* Found matching key */
    if ((nextflag & SEARCH_FIND) && nod_flag &&
	((keyinfo->flag & (HA_NOSAME | HA_NULL_PART)) != HA_NOSAME ||
	 (key->flag & SEARCH_PART_KEY) || info->s->base.born_transactional))
    {
      if ((error= _ma_search_no_save(info, key, (nextflag | SEARCH_FIND) &
                                     ~(SEARCH_BIGGER | SEARCH_SMALLER |
                                       SEARCH_LAST),
                                     _ma_kpos(nod_flag,keypos),
                                     res_page_link, res_page_buff)) >= 0 ||
          my_errno != HA_ERR_KEY_NOT_FOUND)
        DBUG_RETURN(error);
    }
  }

  info->last_key.keyinfo= keyinfo;
  if ((nextflag & (SEARCH_SMALLER | SEARCH_LAST)) && flag != 0)
  {
    uint not_used[2];
    if (_ma_get_prev_key(&info->last_key, &page, keypos))
      goto err;
    /*
      We have to use key->flag >> 1 here to transform
      SEARCH_PAGE_KEY_HAS_TRANSID to SEARCH_USER_KEY_HAS_TRANSID
    */
    if (!(nextflag & SEARCH_SMALLER) &&
        ha_key_cmp(keyinfo->seg, info->last_key.data, key->data,
                   key->data_length + key->ref_length,
                   SEARCH_FIND | (key->flag >> 1) | info->last_key.flag,
                   not_used))
    {
      my_errno=HA_ERR_KEY_NOT_FOUND;                    /* Didn't find key */
      goto err;
    }
  }
  else
  {
    /* Set info->last_key to temporarily point to last key value */
    info->last_key.data= lastkey;
    /* Get key value (if not packed key) and position after key */
    if (!(*keyinfo->get_key)(&info->last_key, page_flag, nod_flag, &keypos))
      goto err;
    memcpy(info->lastkey_buff, lastkey,
           info->last_key.data_length + info->last_key.ref_length);
    info->last_key.data= info->lastkey_buff;
  }
  info->cur_row.lastpos= _ma_row_pos_from_key(&info->last_key);
  info->cur_row.trid=    _ma_trid_from_key(&info->last_key);

  /* Store offset to key */
  info->keypos_offset= (uint) (keypos - page.buff);
  info->maxpos_offset= (uint) (maxpos - page.buff);
  info->int_nod_flag= nod_flag;
  info->last_keypage= pos;
  *res_page_link= page_link;
  *res_page_buff= page.buff;
  
  DBUG_PRINT("exit",("found key at %lu",(ulong) info->cur_row.lastpos));
  DBUG_RETURN(0);

err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  info->page_changed=1;
  DBUG_RETURN (-1);
}


/*
  Search after key in page-block

  @fn    _ma_bin_search
  @param key		Search after this key
  @param page		Start of data page
  @param comp_flag	How key should be compared
  @param ret_pos
  @param buff		Buffer for holding a key (not used here)
  @param last_key

  @note
   If keys are packed, then smaller or identical key is stored in buff

  @return
  @retval <0, 0 , >0 depending on if if found is smaller, equal or bigger than
          'key'
  @retval ret_pos   Points to where the identical or bigger key starts
  @retval last_key  Set to 1 if key is the last key in the page.
*/

int _ma_bin_search(const MARIA_KEY *key, const MARIA_PAGE *ma_page,
                   uint32 comp_flag, uchar **ret_pos,
                   uchar *buff __attribute__((unused)), my_bool *last_key)
{
  int flag;
  uint page_flag;
  uint start, mid, end, save_end, totlength, nod_flag;
  uint not_used[2];
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_SHARE *share=  keyinfo->share;
  uchar *page;
  DBUG_ENTER("_ma_bin_search");

  LINT_INIT(flag);

  page_flag= ma_page->flag;
  if (page_flag & KEYPAGE_FLAG_HAS_TRANSID)
  {
    /* Keys have varying length, can't use binary search */
    DBUG_RETURN(_ma_seq_search(key, ma_page, comp_flag, ret_pos, buff,
                               last_key));
  }

  nod_flag=    ma_page->node;
  totlength= keyinfo->keylength + nod_flag;
  DBUG_ASSERT(ma_page->size >= share->keypage_header + nod_flag + totlength);

  start=0;
  mid=1;
  save_end= end= ((ma_page->size - nod_flag - share->keypage_header) /
                  totlength-1);
  DBUG_PRINT("test",("page_length: %u  end: %u", ma_page->size, end));
  page= ma_page->buff + share->keypage_header + nod_flag;

  while (start != end)
  {
    mid= (start+end)/2;
    if ((flag=ha_key_cmp(keyinfo->seg, page + (uint) mid * totlength,
                         key->data, key->data_length + key->ref_length,
                         comp_flag, not_used))
        >= 0)
      end=mid;
    else
      start=mid+1;
  }
  if (mid != start)
    flag=ha_key_cmp(keyinfo->seg, page + (uint) start * totlength,
                    key->data, key->data_length + key->ref_length, comp_flag,
                    not_used);
  if (flag < 0)
    start++;                    /* point at next, bigger key */
  *ret_pos= (page + (uint) start * totlength);
  *last_key= end == save_end;
  DBUG_PRINT("exit",("flag: %d  keypos: %d",flag,start));
  DBUG_RETURN(flag);
} /* _ma_bin_search */


/**
   Locate a packed key in a key page.

   @fn    _ma_seq_search()
   @param key                       Search key.
   @param page                      Key page (beginning).
   @param comp_flag                 Search flags like SEARCH_SAME etc.
   @param ret_pos
   @param buff                      Buffer for holding temp keys
   @param last_key

   @description
   Used instead of _ma_bin_search() when key is packed.
   Puts smaller or identical key in buff.
   Key is searched sequentially.

   @todo
   Don't copy key to buffer if we are not using key with prefix packing

   @return
   @retval > 0         Key in 'buff' is smaller than search key.
   @retval 0           Key in 'buff' is identical to search key.
   @retval < 0         Not found.

   @retval ret_pos   Points to where the identical or bigger key starts
   @retval last_key  Set to 1 if key is the last key in the page
   @retval buff      Copy of previous or identical unpacked key
*/

int _ma_seq_search(const MARIA_KEY *key, const MARIA_PAGE *ma_page,
                   uint32 comp_flag, uchar **ret_pos,
                   uchar *buff, my_bool *last_key)
{
  int flag;
  uint page_flag, nod_flag, length, not_used[2];
  uchar t_buff[MARIA_MAX_KEY_BUFF], *end;
  uchar *page;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_SHARE *share= keyinfo->share;
  MARIA_KEY tmp_key;
  DBUG_ENTER("_ma_seq_search");

  LINT_INIT(flag);
  LINT_INIT(length);

  page_flag= ma_page->flag;
  nod_flag=  ma_page->node;
  page=      ma_page->buff;
  end= page + ma_page->size;
  page+= share->keypage_header + nod_flag;
  *ret_pos= page;
  t_buff[0]=0;                                  /* Avoid bugs */

  tmp_key.data= t_buff;
  tmp_key.keyinfo= keyinfo;
  while (page < end)
  {
    length=(*keyinfo->get_key)(&tmp_key, page_flag, nod_flag, &page);
    if (length == 0 || page > end)
    {
      maria_print_error(share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_PRINT("error",
                 ("Found wrong key:  length: %u  page: 0x%lx  end: 0x%lx",
                  length, (long) page, (long) end));
      DBUG_RETURN(MARIA_FOUND_WRONG_KEY);
    }
    if ((flag= ha_key_cmp(keyinfo->seg, t_buff, key->data,
                          key->data_length + key->ref_length,
                          comp_flag | tmp_key.flag,
                          not_used)) >= 0)
      break;
    DBUG_PRINT("loop_extra",("page: 0x%lx  key: '%s'  flag: %d",
                             (long) page, t_buff, flag));
    memcpy(buff,t_buff,length);
    *ret_pos=page;
  }
  if (flag == 0)
    memcpy(buff,t_buff,length);                 /* Result is first key */
  *last_key= page == end;
  DBUG_PRINT("exit",("flag: %d  ret_pos: 0x%lx", flag, (long) *ret_pos));
  DBUG_RETURN(flag);
} /* _ma_seq_search */


/**
   Search for key on key page with string prefix compression

   @notes
   This is an optimized function compared to calling _ma_get_pack_key()
   for each key in the buffer

   Same interface as for _ma_seq_search()
*/

int _ma_prefix_search(const MARIA_KEY *key, const MARIA_PAGE *ma_page,
                      uint32 nextflag, uchar **ret_pos, uchar *buff,
                      my_bool *last_key)
{
  /*
    my_flag is raw comparison result to be changed according to
    SEARCH_NO_FIND,SEARCH_LAST and HA_REVERSE_SORT flags.
    flag is the value returned by ha_key_cmp and as treated as final
  */
  int flag=0, my_flag=-1;
  uint nod_flag, length, len, matched, cmplen, kseg_len;
  uint page_flag, prefix_len,suffix_len;
  int key_len_skip, seg_len_pack, key_len_left;
  uchar *end, *vseg, *saved_vseg, *saved_from;
  uchar *page;
  uchar tt_buff[MARIA_MAX_KEY_BUFF+2], *t_buff=tt_buff+2;
  uchar  *saved_to;
  const uchar *kseg;
  uint  saved_length=0, saved_prefix_len=0;
  uint  length_pack;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_SHARE *share= keyinfo->share;
  const uchar *sort_order= keyinfo->seg->charset->sort_order;
  DBUG_ENTER("_ma_prefix_search");

  LINT_INIT(length);
  LINT_INIT(prefix_len);
  LINT_INIT(seg_len_pack);
  LINT_INIT(saved_from);
  LINT_INIT(saved_to);
  LINT_INIT(saved_vseg);

  t_buff[0]=0;                                  /* Avoid bugs */
  page_flag=   ma_page->flag;
  nod_flag=    ma_page->node;
  page_flag&= KEYPAGE_FLAG_HAS_TRANSID;         /* For faster test in loop */
  page= ma_page->buff;
  end= page + ma_page->size;
  page+= share->keypage_header + nod_flag;
  *ret_pos= page;
  kseg= key->data;

  get_key_pack_length(kseg_len, length_pack, kseg);
  key_len_skip=length_pack+kseg_len;
  key_len_left=(int) (key->data_length + key->ref_length) - (int) key_len_skip;
  /* If key_len is 0, then length_pack is 1, then key_len_left is -1. */
  cmplen= ((key_len_left>=0) ? kseg_len :
           (key->data_length + key->ref_length - length_pack));
  DBUG_PRINT("info",("key: '%.*s'",kseg_len,kseg));

  /*
    Keys are compressed the following way:

    If the max length of first key segment <= 127 bytes the prefix is
    1 uchar else it's 2 byte

    (prefix) length  The high bit is set if this is a prefix for the prev key.
    [suffix length]  Packed length of suffix if the previous was a prefix.
    (suffix) data    Key data bytes (past the common prefix or whole segment).
    [next-key-seg]   Next key segments (([packed length], data), ...)
    pointer          Reference to the data file (last_keyseg->length).
  */

  matched=0;  /* how many char's from prefix were alredy matched */
  len=0;      /* length of previous key unpacked */

  while (page < end)
  {
    uint packed= *page & 128;
    uint key_flag;

    vseg= page;
    if (keyinfo->seg->length >= 127)
    {
      suffix_len=mi_uint2korr(vseg) & 32767;
      vseg+=2;
    }
    else
      suffix_len= *vseg++ & 127;

    if (packed)
    {
      if (suffix_len == 0)
      {
        /* == 0x80 or 0x8000, same key, prefix length == old key length. */
        prefix_len=len;
      }
      else
      {
        /* > 0x80 or 0x8000, this is prefix lgt, packed suffix lgt follows. */
        prefix_len=suffix_len;
        get_key_length(suffix_len,vseg);
      }
    }
    else
    {
      /* Not packed. No prefix used from last key. */
      prefix_len=0;
    }

    len=prefix_len+suffix_len;
    seg_len_pack=get_pack_length(len);
    t_buff=tt_buff+3-seg_len_pack;
    store_key_length(t_buff,len);

    if (prefix_len > saved_prefix_len)
      memcpy(t_buff+seg_len_pack+saved_prefix_len,saved_vseg,
             prefix_len-saved_prefix_len);
    saved_vseg=vseg;
    saved_prefix_len=prefix_len;

    DBUG_PRINT("loop",("page: '%.*s%.*s'",prefix_len,t_buff+seg_len_pack,
		       suffix_len,vseg));
    {
      /* Calculate length of one key */
      uchar *from= vseg+suffix_len;
      HA_KEYSEG *keyseg;

      for (keyseg=keyinfo->seg+1 ; keyseg->type ; keyseg++ )
      {
        if (keyseg->flag & HA_NULL_PART)
        {
          if (!(*from++))
            continue;
        }
        if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK))
        {
          uint key_part_length;
          get_key_length(key_part_length,from);
          from+= key_part_length;
        }
        else
          from+= keyseg->length;
      }
      from+= keyseg->length;
      key_flag=0;

      if (page_flag && key_has_transid(from-1))
      {
        from+= transid_packed_length(from);
        key_flag= SEARCH_PAGE_KEY_HAS_TRANSID;
      }
      page= from + nod_flag;
      length= (uint) (from-vseg);
    }

    if (page > end)
    {
      maria_print_error(share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_PRINT("error",
                 ("Found wrong key:  length: %u  page: 0x%lx  end: %lx",
                  length, (long) page, (long) end));
      DBUG_RETURN(MARIA_FOUND_WRONG_KEY);
    }

    if (matched >= prefix_len)
    {
      /* We have to compare. But we can still skip part of the key */
      uint  left;
      const uchar *k= kseg+prefix_len;

      /*
        If prefix_len > cmplen then we are in the end-space comparison
        phase. Do not try to acces the key any more ==> left= 0.
      */
      left= ((len <= cmplen) ? suffix_len :
             ((prefix_len < cmplen) ? cmplen - prefix_len : 0));

      matched=prefix_len+left;

      if (sort_order)
      {
        for (my_flag=0;left;left--)
          if ((my_flag= (int) sort_order[*vseg++] - (int) sort_order[*k++]))
            break;
      }
      else
      {
        for (my_flag=0;left;left--)
          if ((my_flag= (int) *vseg++ - (int) *k++))
            break;
      }

      if (my_flag>0)      /* mismatch */
        break;
      if (my_flag==0) /* match */
      {
	/*
        **  len cmplen seg_left_len more_segs
        **     <                               matched=len; continue search
        **     >      =                        prefix ? found : (matched=len;
        *                                      continue search)
        **     >      <                 -      ok, found
        **     =      <                 -      ok, found
        **     =      =                 -      ok, found
        **     =      =                 +      next seg
        */
        if (len < cmplen)
        {
	  if ((keyinfo->seg->type != HA_KEYTYPE_TEXT &&
	       keyinfo->seg->type != HA_KEYTYPE_VARTEXT1 &&
               keyinfo->seg->type != HA_KEYTYPE_VARTEXT2))
	    my_flag= -1;
	  else
	  {
	    /* We have to compare k and vseg as if they were space extended */
	    const uchar *k_end= k+ (cmplen - len);
	    for ( ; k < k_end && *k == ' '; k++) ;
	    if (k == k_end)
	      goto cmp_rest;		/* should never happen */
	    if ((uchar) *k < (uchar) ' ')
	    {
	      my_flag= 1;		/* Compared string is smaller */
	      break;
	    }
	    my_flag= -1;		/* Continue searching */
	  }
        }
        else if (len > cmplen)
        {
	  uchar *vseg_end;
	  if ((nextflag & SEARCH_PREFIX) && key_len_left == 0)
	    goto fix_flag;

	  /* We have to compare k and vseg as if they were space extended */
	  for (vseg_end= vseg + (len-cmplen) ;
	       vseg < vseg_end && *vseg == (uchar) ' ';
	       vseg++, matched++) ;
	  DBUG_ASSERT(vseg < vseg_end);

	  if ((uchar) *vseg > (uchar) ' ')
	  {
	    my_flag= 1;			/* Compared string is smaller */
	    break;
	  }
	  my_flag= -1;			/* Continue searching */
        }
        else
	{
      cmp_rest:
	  if (key_len_left>0)
	  {
	    uint not_used[2];
	    if ((flag = ha_key_cmp(keyinfo->seg+1,vseg,
				   k, key_len_left, nextflag | key_flag,
                                   not_used)) >= 0)
	      break;
	  }
	  else
	  {
	    /*
	      at this line flag==-1 if the following lines were already
	      visited and 0 otherwise,  i.e. flag <=0 here always !!!
	    */
	fix_flag:
	    DBUG_ASSERT(flag <= 0);
	    if (nextflag & (SEARCH_NO_FIND | SEARCH_LAST))
	      flag=(nextflag & (SEARCH_BIGGER | SEARCH_LAST)) ? -1 : 1;
	    if (flag>=0)
	      break;
	  }
	}
      }
      matched-=left;
    }
    /* else (matched < prefix_len) ---> do nothing. */

    memcpy(buff,t_buff,saved_length=seg_len_pack+prefix_len);
    saved_to= buff+saved_length;
    saved_from= saved_vseg;
    saved_length=length;
    *ret_pos=page;
  }
  if (my_flag)
    flag=(keyinfo->seg->flag & HA_REVERSE_SORT) ? -my_flag : my_flag;
  if (flag == 0)
  {
    memcpy(buff,t_buff,saved_length=seg_len_pack+prefix_len);
    saved_to= buff+saved_length;
    saved_from= saved_vseg;
    saved_length=length;
  }
  if (saved_length)
    memcpy(saved_to, saved_from, saved_length);

  *last_key= page == end;

  DBUG_PRINT("exit",("flag: %d  ret_pos: 0x%lx", flag, (long) *ret_pos));
  DBUG_RETURN(flag);
} /* _ma_prefix_search */


/* Get pos to a key_block */

my_off_t _ma_kpos(uint nod_flag, const uchar *after_key)
{
  after_key-=nod_flag;
  switch (nod_flag) {
#if SIZEOF_OFF_T > 4
  case 7:
    return mi_uint7korr(after_key)*maria_block_size;
  case 6:
    return mi_uint6korr(after_key)*maria_block_size;
  case 5:
    return mi_uint5korr(after_key)*maria_block_size;
#else
  case 7:
    after_key++;
  case 6:
    after_key++;
  case 5:
    after_key++;
#endif
  case 4:
    return ((my_off_t) mi_uint4korr(after_key))*maria_block_size;
  case 3:
    return ((my_off_t) mi_uint3korr(after_key))*maria_block_size;
  case 2:
    return (my_off_t) (mi_uint2korr(after_key)*maria_block_size);
  case 1:
    return (uint) (*after_key)*maria_block_size;
  case 0:                                       /* At leaf page */
  default:                                      /* Impossible */
    return(HA_OFFSET_ERROR);
  }
} /* _kpos */


/* Save pos to a key_block */

void _ma_kpointer(register MARIA_HA *info, register uchar *buff, my_off_t pos)
{
  pos/=maria_block_size;
  switch (info->s->base.key_reflength) {
#if SIZEOF_OFF_T > 4
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#else
  case 7: *buff++=0;
    /* fall trough */
  case 6: *buff++=0;
    /* fall trough */
  case 5: *buff++=0;
    /* fall trough */
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,(uint) pos); break;
  case 1: buff[0]= (uchar) pos; break;
  default: abort();                             /* impossible */
  }
} /* _ma_kpointer */


/* Calc pos to a data-record from a key */

MARIA_RECORD_POS _ma_row_pos_from_key(const MARIA_KEY *key)
{
  my_off_t pos;
  const uchar *after_key= key->data + key->data_length;
  MARIA_SHARE *share= key->keyinfo->share;
  switch (share->rec_reflength) {
#if SIZEOF_OFF_T > 4
  case 8:  pos= (my_off_t) mi_uint8korr(after_key);  break;
  case 7:  pos= (my_off_t) mi_uint7korr(after_key);  break;
  case 6:  pos= (my_off_t) mi_uint6korr(after_key);  break;
  case 5:  pos= (my_off_t) mi_uint5korr(after_key);  break;
#else
  case 8:  pos= (my_off_t) mi_uint4korr(after_key+4);   break;
  case 7:  pos= (my_off_t) mi_uint4korr(after_key+3);   break;
  case 6:  pos= (my_off_t) mi_uint4korr(after_key+2);   break;
  case 5:  pos= (my_off_t) mi_uint4korr(after_key+1);   break;
#endif
  case 4:  pos= (my_off_t) mi_uint4korr(after_key);  break;
  case 3:  pos= (my_off_t) mi_uint3korr(after_key);  break;
  case 2:  pos= (my_off_t) mi_uint2korr(after_key);  break;
  default:
    pos=0L;                                     /* Shut compiler up */
  }
  return (*share->keypos_to_recpos)(share, pos);
}


/**
   Get trid from a key

   @param key	Maria key read from a page

   @retval 0    If key doesn't have a trid
   @retval trid
*/

TrID _ma_trid_from_key(const MARIA_KEY *key)
{
  if (!(key->flag & (SEARCH_PAGE_KEY_HAS_TRANSID |
                     SEARCH_USER_KEY_HAS_TRANSID)))
    return 0;
  return transid_get_packed(key->keyinfo->share,
                            key->data + key->data_length +
                            key->keyinfo->share->rec_reflength);
}


/* Calc position from a record pointer ( in delete link chain ) */

MARIA_RECORD_POS _ma_rec_pos(MARIA_SHARE *share, uchar *ptr)
{
  my_off_t pos;
  switch (share->rec_reflength) {
#if SIZEOF_OFF_T > 4
  case 8:
    pos= (my_off_t) mi_uint8korr(ptr);
    if (pos == HA_OFFSET_ERROR)
      return HA_OFFSET_ERROR;                   /* end of list */
    break;
  case 7:
    pos= (my_off_t) mi_uint7korr(ptr);
    if (pos == (((my_off_t) 1) << 56) -1)
      return HA_OFFSET_ERROR;                   /* end of list */
    break;
  case 6:
    pos= (my_off_t) mi_uint6korr(ptr);
    if (pos == (((my_off_t) 1) << 48) -1)
      return HA_OFFSET_ERROR;                   /* end of list */
    break;
  case 5:
    pos= (my_off_t) mi_uint5korr(ptr);
    if (pos == (((my_off_t) 1) << 40) -1)
      return HA_OFFSET_ERROR;                   /* end of list */
    break;
#else
  case 8:
  case 7:
  case 6:
  case 5:
    ptr+= (share->rec_reflength-4);
    /* fall through */
#endif
  case 4:
    pos= (my_off_t) mi_uint4korr(ptr);
    if (pos == (my_off_t) (uint32) ~0L)
      return  HA_OFFSET_ERROR;
    break;
  case 3:
    pos= (my_off_t) mi_uint3korr(ptr);
    if (pos == (my_off_t) (1 << 24) -1)
      return HA_OFFSET_ERROR;
    break;
  case 2:
    pos= (my_off_t) mi_uint2korr(ptr);
    if (pos == (my_off_t) (1 << 16) -1)
      return HA_OFFSET_ERROR;
    break;
  default: abort();                             /* Impossible */
  }
  return (*share->keypos_to_recpos)(share, pos);
}


/* save position to record */

void _ma_dpointer(MARIA_SHARE *share, uchar *buff, my_off_t pos)
{
  if (pos != HA_OFFSET_ERROR)
    pos= (*share->recpos_to_keypos)(share, pos);

  switch (share->rec_reflength) {
#if SIZEOF_OFF_T > 4
  case 8: mi_int8store(buff,pos); break;
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#else
  case 8: *buff++=0;
    /* fall trough */
  case 7: *buff++=0;
    /* fall trough */
  case 6: *buff++=0;
    /* fall trough */
  case 5: *buff++=0;
    /* fall trough */
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,(uint) pos); break;
  default: abort();                             /* Impossible */
  }
} /* _ma_dpointer */


my_off_t _ma_static_keypos_to_recpos(MARIA_SHARE *share, my_off_t pos)
{
  return pos * share->base.pack_reclength;
}


my_off_t _ma_static_recpos_to_keypos(MARIA_SHARE *share, my_off_t pos)
{
  return pos / share->base.pack_reclength;
}

my_off_t _ma_transparent_recpos(MARIA_SHARE *share __attribute__((unused)),
                                my_off_t pos)
{
  return pos;
}

my_off_t _ma_transaction_keypos_to_recpos(MARIA_SHARE *share
                                          __attribute__((unused)),
                                          my_off_t pos)
{
  /* We need one bit to store if there is transid's after position */
  return pos >> 1;
}

my_off_t _ma_transaction_recpos_to_keypos(MARIA_SHARE *share
                                          __attribute__((unused)),
                                          my_off_t pos)
{
  return pos << 1;
}

/*
  @brief Get key from key-block

  @param key         Should contain previous key. Will contain new key
  @param page_flag   Flag on page block
  @param nod_flag    Is set to nod length if we on nod
  @param page        Points at previous key; Its advanced to point at next key

  @notes
    Same as _ma_get_key but used with fixed length keys

  @return
  @retval key_length + length of data pointer (without nod length)
 */

uint _ma_get_static_key(MARIA_KEY *key, uint page_flag, uint nod_flag,
                        register uchar **page)
{
  register MARIA_KEYDEF *keyinfo= key->keyinfo;
  size_t key_length= keyinfo->keylength;

  key->ref_length=  keyinfo->share->rec_reflength;
  key->data_length= key_length - key->ref_length;
  key->flag= 0;
  if (page_flag & KEYPAGE_FLAG_HAS_TRANSID)
  {
    uchar *end= *page + keyinfo->keylength;
    if (key_has_transid(end-1))
    {
      uint trans_length= transid_packed_length(end);
      key->ref_length+= trans_length;
      key_length+= trans_length;
      key->flag= SEARCH_PAGE_KEY_HAS_TRANSID;
    }
  }
  key_length+= nod_flag;
  memcpy(key->data, *page, key_length);
  *page+= key_length;
  return key_length - nod_flag;
} /* _ma_get_static_key */


/**
   Skip over static length key from key-block

  @fn _ma_skip_static_key()
  @param key       Keyinfo and buffer that can be used
  @param nod_flag  If nod: Length of node pointer, else zero.
  @param key       Points at key

  @retval pointer to next key
*/

uchar *_ma_skip_static_key(MARIA_KEY *key, uint page_flag,
                           uint nod_flag, uchar *page)
{
  page+= key->keyinfo->keylength;
  if ((page_flag & KEYPAGE_FLAG_HAS_TRANSID) && key_has_transid(page-1))
    page+= transid_packed_length(page);
  return page+ nod_flag;
}


/*
  get key which is packed against previous key or key with a NULL column.

  SYNOPSIS
    _ma_get_pack_key()
    @param int_key   Should contain previous key. Will contain new key
    @param page_flag page_flag from page
    @param nod_flag  If nod: Length of node pointer, else zero.
    @param page_pos  Points at previous key; Its advanced to point at next key

    @return
    @retval key_length + length of data pointer
*/

uint _ma_get_pack_key(MARIA_KEY *int_key, uint page_flag,
                      uint nod_flag, uchar **page_pos)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *page= *page_pos;
  uint length;
  uchar *key= int_key->data;
  MARIA_KEYDEF *keyinfo= int_key->keyinfo;

  for (keyseg=keyinfo->seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->flag & HA_PACK_KEY)
    {
      /* key with length, packed to previous key */
      uchar *start= key;
      uint packed= *page & 128,tot_length,rest_length;
      if (keyseg->length >= 127)
      {
        length=mi_uint2korr(page) & 32767;
        page+=2;
      }
      else
        length= *page++ & 127;

      if (packed)
      {
	if (length > (uint) keyseg->length)
	{
          maria_print_error(keyinfo->share, HA_ERR_CRASHED);
	  my_errno=HA_ERR_CRASHED;
	  return 0;				/* Error */
	}
	if (length == 0)			/* Same key */
	{
	  if (keyseg->flag & HA_NULL_PART)
	    *key++=1;				/* Can't be NULL */
	  get_key_length(length,key);
	  key+= length;				/* Same diff_key as prev */
	  if (length > keyseg->length)
	  {
	    DBUG_PRINT("error",
                       ("Found too long null packed key: %u of %u at 0x%lx",
                        length, keyseg->length, (long) *page_pos));
	    DBUG_DUMP("key", *page_pos, 16);
            maria_print_error(keyinfo->share, HA_ERR_CRASHED);
	    my_errno=HA_ERR_CRASHED;
	    return 0;
	  }
	  continue;
	}
	if (keyseg->flag & HA_NULL_PART)
	{
	  key++;				/* Skip null marker*/
	  start++;
	}

	get_key_length(rest_length,page);
	tot_length=rest_length+length;

	/* If the stored length has changed, we must move the key */
	if (tot_length >= 255 && *start != 255)
	{
	  /* length prefix changed from a length of one to a length of 3 */
	  bmove_upp(key+length+3, key+length+1, length);
	  *key=255;
	  mi_int2store(key+1,tot_length);
	  key+=3+length;
	}
	else if (tot_length < 255 && *start == 255)
	{
	  bmove(key+1,key+3,length);
	  *key=tot_length;
	  key+=1+length;
	}
	else
	{
	  store_key_length_inc(key,tot_length);
	  key+=length;
	}
	memcpy(key,page,rest_length);
	page+=rest_length;
	key+=rest_length;
	continue;
      }
      else
      {
        /* Key that is not packed against previous key */
        if (keyseg->flag & HA_NULL_PART)
        {
          if (!length--)                        /* Null part */
          {
            *key++=0;
            continue;
          }
          *key++=1;                             /* Not null */
        }
      }
      if (length > (uint) keyseg->length)
      {
        DBUG_PRINT("error",("Found too long packed key: %u of %u at 0x%lx",
                            length, keyseg->length, (long) *page_pos));
        DBUG_DUMP("key", *page_pos, 16);
        maria_print_error(keyinfo->share, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        return 0;                               /* Error */
      }
      store_key_length_inc(key,length);
    }
    else
    {
      if (keyseg->flag & HA_NULL_PART)
      {
        if (!(*key++ = *page++))
          continue;
      }
      if (keyseg->flag &
          (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK))
      {
        uchar *tmp=page;
        get_key_length(length,tmp);
        length+=(uint) (tmp-page);
      }
      else
        length=keyseg->length;
    }
    memcpy(key, page,(size_t) length);
    key+=length;
    page+=length;
  }

  int_key->data_length= (key - int_key->data);
  int_key->flag= 0;
  length= keyseg->length;
  if (page_flag & KEYPAGE_FLAG_HAS_TRANSID)
  {
    uchar *end= page + length;
    if (key_has_transid(end-1))
    {
      length+= transid_packed_length(end);
      int_key->flag= SEARCH_PAGE_KEY_HAS_TRANSID;
    }
  }
  int_key->ref_length= length;
  length+= nod_flag;
  bmove(key, page, length);
  *page_pos= page+length;

  return (int_key->data_length + int_key->ref_length);
} /* _ma_get_pack_key */


/**
  skip key which is packed against previous key or key with a NULL column.

  @fn _ma_skip_pack_key()
  @param key       Keyinfo and buffer that can be used
  @param nod_flag  If nod: Length of node pointer, else zero.
  @param key       Points at key

  @note
  This is in principle a simpler version of _ma_get_pack_key()

  @retval pointer to next key
*/

uchar *_ma_skip_pack_key(MARIA_KEY *key, uint page_flag,
                         uint nod_flag, uchar *page)
{
  reg1 HA_KEYSEG *keyseg;
  for (keyseg= key->keyinfo->seg ; keyseg->type ; keyseg++)
  {
    if (keyseg->flag & HA_PACK_KEY)
    {
      /* key with length, packed to previous key */
      uint packed= *page & 128, length;
      if (keyseg->length >= 127)
      {
        length= mi_uint2korr(page) & 32767;
        page+= 2;
      }
      else
        length= *page++ & 127;

      if (packed)
      {
	if (length == 0)			/* Same key */
	  continue;
	get_key_length(length,page);
	page+= length;
	continue;
      }
      if ((keyseg->flag & HA_NULL_PART) && length)
      {
        /*
          Keys that can have null use length+1 as the length for date as the
          number 0 is reserved for keys that have a NULL value
        */
        length--;
      }
      page+= length;
    }
    else
    {
      if (keyseg->flag & HA_NULL_PART)
        if (!*page++)
          continue;
      if (keyseg->flag & (HA_SPACE_PACK | HA_BLOB_PART | HA_VAR_LENGTH_PART))
      {
        uint length;
        get_key_length(length,page);
        page+=length;
      }
      else
        page+= keyseg->length;
    }
  }
  page+= keyseg->length;
  if ((page_flag & KEYPAGE_FLAG_HAS_TRANSID) && key_has_transid(page-1))
    page+= transid_packed_length(page);
  return page + nod_flag;
}


/* Read key that is packed relatively to previous */

uint _ma_get_binary_pack_key(MARIA_KEY *int_key, uint page_flag, uint nod_flag,
                             register uchar **page_pos)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *page, *page_end, *from, *from_end, *key;
  uint length,tmp;
  MARIA_KEYDEF *keyinfo= int_key->keyinfo;
  DBUG_ENTER("_ma_get_binary_pack_key");

  page= *page_pos;
  page_end=page + MARIA_MAX_KEY_BUFF + 1;
  key= int_key->data;

  /*
    Keys are compressed the following way:

    prefix length      Packed length of prefix common with prev key.
                       (1 or 3 bytes)
    for each key segment:
      [is null]        Null indicator if can be null (1 byte, zero means null)
      [length]         Packed length if varlength (1 or 3 bytes)
      key segment      'length' bytes of key segment value
    pointer          Reference to the data file (last_keyseg->length).

    get_key_length() is a macro. It gets the prefix length from 'page'
    and puts it into 'length'. It increments 'page' by 1 or 3, depending
    on the packed length of the prefix length.
  */
  get_key_length(length,page);
  if (length)
  {
    if (length > keyinfo->maxlength)
    {
      DBUG_PRINT("error",
                 ("Found too long binary packed key: %u of %u at 0x%lx",
                  length, keyinfo->maxlength, (long) *page_pos));
      DBUG_DUMP("key", *page_pos, 16);
      maria_print_error(keyinfo->share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(0);                                 /* Wrong key */
    }
    /* Key is packed against prev key, take prefix from prev key. */
    from= key;
    from_end= key + length;
  }
  else
  {
    /* Key is not packed against prev key, take all from page buffer. */
    from= page;
    from_end= page_end;
  }

  /*
    The trouble is that key can be split in two parts:
      The first part (prefix) is in from .. from_end - 1.
      The second part starts at page.
    The split can be at every byte position. So we need to check for
    the end of the first part before using every byte.
  */
  for (keyseg=keyinfo->seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->flag & HA_NULL_PART)
    {
      /* If prefix is used up, switch to rest. */
      if (from == from_end)
      {
        from=page;
        from_end=page_end;
      }
      if (!(*key++ = *from++))
        continue;                               /* Null part */
    }
    if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK))
    {
      /* If prefix is used up, switch to rest. */
      if (from == from_end) { from=page;  from_end=page_end; }
      /* Get length of dynamic length key part */
      if ((length= (uint) (uchar) (*key++ = *from++)) == 255)
      {
        /* If prefix is used up, switch to rest. */
        if (from == from_end) { from=page;  from_end=page_end; }
        length= ((uint) (uchar) ((*key++ = *from++))) << 8;
        /* If prefix is used up, switch to rest. */
        if (from == from_end) { from=page;  from_end=page_end; }
        length+= (uint) (uchar) ((*key++ = *from++));
      }
    }
    else
      length=keyseg->length;

    if ((tmp=(uint) (from_end-from)) <= length)
    {
      key+=tmp;                                 /* Use old key */
      length-=tmp;
      from=page; from_end=page_end;
    }
    DBUG_ASSERT((int) length >= 0);
    DBUG_PRINT("info",("key: 0x%lx  from: 0x%lx  length: %u",
		       (long) key, (long) from, length));
    memmove(key, from, (size_t) length);
    key+=length;
    from+=length;
  }
  /*
    Last segment (type == 0) contains length of data pointer.
    If we have mixed key blocks with data pointer and key block pointer,
    we have to copy both.
  */
  int_key->data_length= (key - int_key->data);
  int_key->ref_length= length= keyseg->length;
  int_key->flag= 0;
  if ((tmp=(uint) (from_end-from)) <= length)
  {
    /* Skip over the last common part of the data */
    key+= tmp;
    length-= tmp;
    from= page;
  }
  else
  {
    /*
      Remaining length is greater than max possible length.
      This can happen only if we switched to the new key bytes already.
      'page_end' is calculated with MARIA_MAX_KEY_BUFF. So it can be far
      behind the real end of the key.
    */
    if (from_end != page_end)
    {
      DBUG_PRINT("error",("Error when unpacking key"));
      maria_print_error(keyinfo->share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(0);                                 /* Error */
    }
  }
  if (page_flag & KEYPAGE_FLAG_HAS_TRANSID)
  {
    uchar *end= from + length;
    if (key_has_transid(end-1))
    {
      uint trans_length= transid_packed_length(end);
      length+= trans_length;
      int_key->ref_length+= trans_length;
      int_key->flag= SEARCH_PAGE_KEY_HAS_TRANSID;
    }
  }

  /* Copy rest of data ptr and, if appropriate, trans_id and node_ptr */
  memcpy(key, from, length + nod_flag);
  *page_pos= from + length + nod_flag;
  
  DBUG_RETURN(int_key->data_length + int_key->ref_length);
}

/**
  skip key which is ptefix packed against previous key

  @fn _ma_skip_binary_key()
  @param key       Keyinfo and buffer that can be used
  @param nod_flag  If nod: Length of node pointer, else zero.
  @param key       Points at key

  @note
  We have to copy the key as otherwise we don't know how much left
  data there is of the key.

  @todo
  Implement more efficient version of this. We can ignore to copy any rest
  key parts that are not null or not packed. We also don't have to copy
  rowid or transid.

  @retval pointer to next key
*/

uchar *_ma_skip_binary_pack_key(MARIA_KEY *key, uint page_flag,
                                uint nod_flag, uchar *page)
{
  if (!_ma_get_binary_pack_key(key, page_flag, nod_flag, &page))
    return 0;
  return page;
}


/**
  @brief Get key at position without knowledge of previous key

  @return pointer to next key
*/

uchar *_ma_get_key(MARIA_KEY *key, MARIA_PAGE *ma_page, uchar *keypos)
{
  uint page_flag, nod_flag;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  uchar *page;
  DBUG_ENTER("_ma_get_key");

  page=       ma_page->buff;
  page_flag=  ma_page->flag;
  nod_flag=   ma_page->node;

  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      ! (page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    bmove(key->data, keypos, keyinfo->keylength+nod_flag);
    key->ref_length= keyinfo->share->rec_reflength;
    key->data_length= keyinfo->keylength - key->ref_length;
    key->flag= 0;
    DBUG_RETURN(keypos+keyinfo->keylength+nod_flag);
  }
  else
  {
    page+= keyinfo->share->keypage_header + nod_flag;
    key->data[0]= 0;                            /* safety */
    while (page <= keypos)
    {
      if (!(*keyinfo->get_key)(key, page_flag, nod_flag, &page))
      {
        maria_print_error(keyinfo->share, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(0);
      }
    }
  }
  DBUG_PRINT("exit",("page: 0x%lx  length: %u", (long) page,
                     key->data_length + key->ref_length));
  DBUG_RETURN(page);
} /* _ma_get_key */


/*
  @brief Get key at position without knowledge of previous key

  @return
  @retval 0  ok
  @retval 1  error
*/

static my_bool _ma_get_prev_key(MARIA_KEY *key, MARIA_PAGE *ma_page,
                                uchar *keypos)
{
  uint page_flag, nod_flag;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("_ma_get_prev_key");

  page_flag= ma_page->flag;
  nod_flag=  ma_page->node;

  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      ! (page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    bmove(key->data, keypos - keyinfo->keylength - nod_flag,
          keyinfo->keylength);
    key->ref_length= keyinfo->share->rec_reflength;
    key->data_length= keyinfo->keylength - key->ref_length;
    key->flag= 0;
    DBUG_RETURN(0);
  }
  else
  {
    uchar *page;

    page= ma_page->buff + keyinfo->share->keypage_header + nod_flag;
    key->data[0]= 0;                            /* safety */
    DBUG_ASSERT(page != keypos);
    while (page < keypos)
    {
      if (! (*keyinfo->get_key)(key, page_flag, nod_flag, &page))
      {
        maria_print_error(keyinfo->share, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
} /* _ma_get_prev_key */


/*
  @brief Get last key from key-page before 'endpos'

  @note
  endpos may be either end of buffer or start of a key

  @return
  @retval pointer to where key starts
*/

uchar *_ma_get_last_key(MARIA_KEY *key, MARIA_PAGE *ma_page, uchar *endpos)
{
  uint page_flag,nod_flag;
  uchar *lastpos, *page;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("_ma_get_last_key");
  DBUG_PRINT("enter",("page: 0x%lx  endpos: 0x%lx", (long) ma_page->buff,
                      (long) endpos));

  page_flag= ma_page->flag;
  nod_flag=  ma_page->node;
  page= ma_page->buff + keyinfo->share->keypage_header + nod_flag;

  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      ! (page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    lastpos= endpos-keyinfo->keylength-nod_flag;
    key->ref_length= keyinfo->share->rec_reflength;
    key->data_length= keyinfo->keylength - key->ref_length;
    key->flag= 0;
    if (lastpos >= page)
      bmove(key->data, lastpos, keyinfo->keylength + nod_flag);
  }
  else
  {
    lastpos= page;
    key->data[0]=0;                             /* safety */
    while (page < endpos)
    {
      lastpos= page;
      if (!(*keyinfo->get_key)(key, page_flag, nod_flag, &page))
      {
        DBUG_PRINT("error",("Couldn't find last key:  page: 0x%lx",
                            (long) page));
        maria_print_error(keyinfo->share, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(0);
      }
    }
  }
  DBUG_PRINT("exit",("lastpos: 0x%lx  length: %u", (ulong) lastpos,
                     key->data_length + key->ref_length));
  DBUG_RETURN(lastpos);
} /* _ma_get_last_key */


/**
   Calculate length of unpacked key

   @param info	       Maria handler
   @param keyinfo      key handler
   @param key	       data for key

   @notes
     This function is very seldom used.  It's mainly used for debugging
     or when calculating a key length from a stored key in batch insert.

     This function does *NOT* calculate length of transid size!
     This function can't be used against a prefix packed key on a page

   @return
   @retval total length for key
*/

uint _ma_keylength(MARIA_KEYDEF *keyinfo, const uchar *key)
{
  reg1 HA_KEYSEG *keyseg;
  const uchar *start;

  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
    return (keyinfo->keylength);

  start= key;
  for (keyseg=keyinfo->seg ; keyseg->type ; keyseg++)
  {
    if (keyseg->flag & HA_NULL_PART)
      if (!*key++)
        continue;
    if (keyseg->flag & (HA_SPACE_PACK | HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {
      uint length;
      get_key_length(length,key);
      key+=length;
    }
    else
      key+= keyseg->length;
  }
  return((uint) (key-start)+keyseg->length);
} /* _ma_keylength */


/*
  Calculate length of part key.

  Used in maria_rkey() to find the key found for the key-part that was used.
  This is needed in case of multi-byte character sets where we may search
  after '0xDF' but find 'ss'
*/

uint _ma_keylength_part(MARIA_KEYDEF *keyinfo, register const uchar *key,
			HA_KEYSEG *end)
{
  reg1 HA_KEYSEG *keyseg;
  const uchar *start= key;

  for (keyseg=keyinfo->seg ; keyseg != end ; keyseg++)
  {
    if (keyseg->flag & HA_NULL_PART)
      if (!*key++)
        continue;
    if (keyseg->flag & (HA_SPACE_PACK | HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {
      uint length;
      get_key_length(length,key);
      key+=length;
    }
    else
      key+= keyseg->length;
  }
  return (uint) (key-start);
}


/*
  Find next/previous record with same key

  WARNING
    This can't be used when database is touched after last read
*/

int _ma_search_next(register MARIA_HA *info, MARIA_KEY *key,
                    uint32 nextflag, my_off_t pos)
{
  int error;
  uchar lastkey[MARIA_MAX_KEY_BUFF];
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_KEY tmp_key;
  MARIA_PAGE page;
  DBUG_ENTER("_ma_search_next");
  DBUG_PRINT("enter",("nextflag: %u  lastpos: %lu  int_keypos: 0x%lx  page_changed %d  keyread_buff_used: %d",
                      nextflag, (ulong) info->cur_row.lastpos,
                      (ulong) info->int_keypos,
                      info->page_changed, info->keyread_buff_used));
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, key););

  /*
    Force full read if we are at last key or if we are not on a leaf
    and the key tree has changed since we used it last time
    Note that even if the key tree has changed since last read, we can use
    the last read data from the leaf if we haven't used the buffer for
    something else.
  */

  if (((nextflag & SEARCH_BIGGER) && info->int_keypos >= info->int_maxpos) ||
      info->page_changed ||
      (info->int_keytree_version != keyinfo->version &&
       (info->int_nod_flag || info->keyread_buff_used)))
    DBUG_RETURN(_ma_search(info, key, nextflag | SEARCH_SAVE_BUFF,
                           pos));

  if (info->keyread_buff_used)
  {
    if (_ma_fetch_keypage(&page, info, keyinfo, info->last_search_keypage,
                          PAGECACHE_LOCK_LEFT_UNLOCKED,
                          DFLT_INIT_HITS, info->keyread_buff, 0))
      DBUG_RETURN(-1);
    info->keyread_buff_used=0;
  }
  else
  {
    /* Last used buffer is in info->keyread_buff */
    /* Todo:  Add info->keyread_page to keep track of this */
    _ma_page_setup(&page, info, keyinfo, 0, info->keyread_buff);
  }

  tmp_key.data=   lastkey;
  info->last_key.keyinfo= tmp_key.keyinfo= keyinfo;

  if (nextflag & SEARCH_BIGGER)                                 /* Next key */
  {
    if (page.node)
    {
      my_off_t tmp_pos= _ma_kpos(page.node, info->int_keypos);

      if ((error= _ma_search(info, key, nextflag | SEARCH_SAVE_BUFF,
                             tmp_pos)) <=0)
        DBUG_RETURN(error);
    }
    if (keyinfo->flag & (HA_PACK_KEY | HA_BINARY_PACK_KEY) &&
        info->last_key.data != key->data)
      memcpy(info->last_key.data, key->data,
             key->data_length + key->ref_length);
    if (!(*keyinfo->get_key)(&info->last_key, page.flag, page.node,
                             &info->int_keypos))
      DBUG_RETURN(-1);
  }
  else                                                  /* Previous key */
  {
    /* Find start of previous key */
    info->int_keypos= _ma_get_last_key(&tmp_key, &page, info->int_keypos);
    if (!info->int_keypos)
      DBUG_RETURN(-1);
    if (info->int_keypos == info->keyread_buff + info->s->keypage_header)
    {
      /* Previous key was first key, read key before this one */
      DBUG_RETURN(_ma_search(info, key, nextflag | SEARCH_SAVE_BUFF,
                             pos));
    }
    if (page.node &&
        (error= _ma_search(info, key, nextflag | SEARCH_SAVE_BUFF,
                           _ma_kpos(page.node,info->int_keypos))) <= 0)
      DBUG_RETURN(error);

    /* QQ: We should be able to optimize away the following call */
    if (! _ma_get_last_key(&info->last_key, &page, info->int_keypos))
      DBUG_RETURN(-1);
  }
  info->cur_row.lastpos= _ma_row_pos_from_key(&info->last_key);
  info->cur_row.trid=    _ma_trid_from_key(&info->last_key);
  DBUG_PRINT("exit",("found key at %lu",(ulong) info->cur_row.lastpos));
  DBUG_RETURN(0);
} /* _ma_search_next */


/**
  Search after position for the first row in an index

  @return
  Found row is stored in info->cur_row.lastpos
*/

int _ma_search_first(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                     my_off_t pos)
{
  uchar *first_pos;
  MARIA_PAGE page;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_search_first");

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    info->cur_row.lastpos= HA_OFFSET_ERROR;
    DBUG_RETURN(-1);
  }

  do
  {
    if (_ma_fetch_keypage(&page, info, keyinfo, pos,
                          PAGECACHE_LOCK_LEFT_UNLOCKED,
                          DFLT_INIT_HITS, info->keyread_buff, 0))
    {
      info->cur_row.lastpos= HA_OFFSET_ERROR;
      DBUG_RETURN(-1);
    }
    first_pos= page.buff + share->keypage_header + page.node;
  } while ((pos= _ma_kpos(page.node, first_pos)) != HA_OFFSET_ERROR);

  info->last_key.keyinfo= keyinfo;

  if (!(*keyinfo->get_key)(&info->last_key, page.flag, page.node, &first_pos))
    DBUG_RETURN(-1);                            /* Crashed */

  info->int_keypos=   first_pos;
  info->int_maxpos=   (page.buff + page.size -1);
  info->int_nod_flag= page.node;
  info->int_keytree_version= keyinfo->version;
  info->last_search_keypage= info->last_keypage;
  info->page_changed=info->keyread_buff_used=0;
  info->cur_row.lastpos= _ma_row_pos_from_key(&info->last_key);
  info->cur_row.trid=    _ma_trid_from_key(&info->last_key);

  DBUG_PRINT("exit",("found key at %lu", (ulong) info->cur_row.lastpos));
  DBUG_RETURN(0);
} /* _ma_search_first */


/**
   Search after position for the last row in an index

  @return
  Found row is stored in info->cur_row.lastpos
*/

int _ma_search_last(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                    my_off_t pos)
{
  uchar *end_of_page;
  MARIA_PAGE page;
  DBUG_ENTER("_ma_search_last");

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;                      /* Didn't find key */
    info->cur_row.lastpos= HA_OFFSET_ERROR;
    DBUG_RETURN(-1);
  }

  do
  {
    if (_ma_fetch_keypage(&page, info, keyinfo, pos,
                          PAGECACHE_LOCK_LEFT_UNLOCKED,
                          DFLT_INIT_HITS, info->keyread_buff, 0))
    {
      info->cur_row.lastpos= HA_OFFSET_ERROR;
      DBUG_RETURN(-1);
    }
    end_of_page= page.buff + page.size;
  } while ((pos= _ma_kpos(page.node, end_of_page)) != HA_OFFSET_ERROR);

  info->last_key.keyinfo= keyinfo;

  if (!_ma_get_last_key(&info->last_key, &page, end_of_page))
    DBUG_RETURN(-1);
  info->cur_row.lastpos= _ma_row_pos_from_key(&info->last_key);
  info->cur_row.trid=    _ma_trid_from_key(&info->last_key);
  info->int_keypos=      info->int_maxpos= end_of_page;
  info->int_nod_flag=    page.node;
  info->int_keytree_version= keyinfo->version;
  info->last_search_keypage= info->last_keypage;
  info->page_changed=info->keyread_buff_used=0;

  DBUG_PRINT("exit",("found key at %lu",(ulong) info->cur_row.lastpos));
  DBUG_RETURN(0);
} /* _ma_search_last */



/****************************************************************************
**
** Functions to store and pack a key in a page
**
** maria_calc_xx_key_length takes the following arguments:
**  nod_flag    If nod: Length of nod-pointer
**  next_key    Position to pos after the new key in buffer
**  org_key     Key that was before the next key in buffer
**  prev_key    Last key before current key
**  key         Key that will be stored
**  s_temp      Information how next key will be packed
****************************************************************************/

/* Static length key */

int
_ma_calc_static_key_length(const MARIA_KEY *key, uint nod_flag,
                           uchar *next_pos  __attribute__((unused)),
                           uchar *org_key  __attribute__((unused)),
                           uchar *prev_key __attribute__((unused)),
                           MARIA_KEY_PARAM *s_temp)
{
  s_temp->key= key->data;
  return (int) (s_temp->move_length= key->data_length + key->ref_length +
                nod_flag);
}

/* Variable length key */

int
_ma_calc_var_key_length(const MARIA_KEY *key, uint nod_flag,
                        uchar *next_pos  __attribute__((unused)),
                        uchar *org_key  __attribute__((unused)),
                        uchar *prev_key __attribute__((unused)),
                        MARIA_KEY_PARAM *s_temp)
{
  s_temp->key= key->data;
  return (int) (s_temp->move_length= key->data_length + key->ref_length +
                nod_flag);
}

/**
   @brief Calc length needed to store prefixed compressed keys

  @info
    Variable length first segment which is prefix compressed
    (maria_chk reports 'packed + stripped')

    Keys are compressed the following way:

    If the max length of first key segment <= 127 bytes the prefix is
    1 uchar else it's 2 byte

    prefix byte(s) The high bit is set if this is a prefix for the prev key
    length         Packed length if the previous was a prefix byte
    [data_length]  data bytes ('length' bytes)
    next-key-seg   Next key segments

    If the first segment can have NULL:
       If key was packed
         data_length is length of rest of key
       If key was not packed
         The data_length is 0 for NULLS and 1+data_length for not null columns
*/

int
_ma_calc_var_pack_key_length(const MARIA_KEY *int_key, uint nod_flag,
                             uchar *next_key, uchar *org_key, uchar *prev_key,
                             MARIA_KEY_PARAM *s_temp)
{
  reg1 HA_KEYSEG *keyseg;
  int length;
  uint key_length,ref_length,org_key_length=0,
       length_pack,new_key_length,diff_flag,pack_marker;
  const uchar *key, *start, *end, *key_end;
  const uchar *sort_order;
  my_bool same_length;
  MARIA_KEYDEF *keyinfo= int_key->keyinfo;

  key= int_key->data;
  length_pack=s_temp->ref_length=s_temp->n_ref_length=s_temp->n_length=0;
  same_length=0; keyseg=keyinfo->seg;
  key_length= int_key->data_length + int_key->ref_length + nod_flag;

  sort_order=0;
  if ((keyinfo->flag & HA_FULLTEXT) &&
      ((keyseg->type == HA_KEYTYPE_TEXT) ||
       (keyseg->type == HA_KEYTYPE_VARTEXT1) ||
       (keyseg->type == HA_KEYTYPE_VARTEXT2)) &&
      !use_strnxfrm(keyseg->charset))
    sort_order= keyseg->charset->sort_order;

  /* diff flag contains how many bytes is needed to pack key */
  if (keyseg->length >= 127)
  {
    diff_flag=2;
    pack_marker=32768;
  }
  else
  {
    diff_flag= 1;
    pack_marker=128;
  }
  s_temp->pack_marker=pack_marker;

  /* Handle the case that the first part have NULL values */
  if (keyseg->flag & HA_NULL_PART)
  {
    if (!*key++)
    {
      s_temp->key= key;
      s_temp->key_length= 0;
      s_temp->totlength= key_length-1+diff_flag;
      s_temp->next_key_pos= 0;                   /* No next key */
      return (s_temp->move_length= s_temp->totlength);
    }
    s_temp->store_not_null=1;
    key_length--;                               /* We don't store NULL */
    if (prev_key && !*prev_key++)
      org_key=prev_key=0;                       /* Can't pack against prev */
    else if (org_key)
      org_key++;                                /* Skip NULL */
  }
  else
    s_temp->store_not_null=0;
  s_temp->prev_key= org_key;

  /* The key part will start with a packed length */

  get_key_pack_length(new_key_length,length_pack,key);
  end= key_end= key+ new_key_length;
  start= key;

  /* Calc how many characters are identical between this and the prev. key */
  if (prev_key)
  {
    get_key_length(org_key_length,prev_key);
    s_temp->prev_key=prev_key;          /* Pointer at data */
    /* Don't use key-pack if length == 0 */
    if (new_key_length && new_key_length == org_key_length)
      same_length=1;
    else if (new_key_length > org_key_length)
      end= key + org_key_length;

    if (sort_order)                             /* SerG */
    {
      while (key < end &&
             sort_order[*key] == sort_order[*prev_key])
      {
        key++; prev_key++;
      }
    }
    else
    {
      while (key < end && *key == *prev_key)
      {
        key++; prev_key++;
      }
    }
  }

  s_temp->key=key;
  s_temp->key_length= (uint) (key_end-key);

  if (same_length && key == key_end)
  {
    /* identical variable length key */
    s_temp->ref_length= pack_marker;
    length=(int) key_length-(int) (key_end-start)-length_pack;
    length+= diff_flag;
    if (next_key)
    {                                           /* Can't combine with next */
      s_temp->n_length= *next_key;              /* Needed by _ma_store_key */
      next_key=0;
    }
  }
  else
  {
    if (start != key)
    {                                           /* Starts as prev key */
      ref_length= (uint) (key-start);
      s_temp->ref_length= ref_length + pack_marker;
      length= (int) (key_length - ref_length);

      length-= length_pack;
      length+= diff_flag;
      length+= ((new_key_length-ref_length) >= 255) ? 3 : 1;/* Rest_of_key */
    }
    else
    {
      s_temp->key_length+=s_temp->store_not_null;       /* If null */
      length= key_length - length_pack+ diff_flag;
    }
  }
  s_temp->totlength=(uint) length;
  s_temp->prev_length=0;
  DBUG_PRINT("test",("tot_length: %u  length: %d  uniq_key_length: %u",
                     key_length, length, s_temp->key_length));

        /* If something after that hasn't length=0, test if we can combine */
  if ((s_temp->next_key_pos=next_key))
  {
    uint packed,n_length;

    packed = *next_key & 128;
    if (diff_flag == 2)
    {
      n_length= mi_uint2korr(next_key) & 32767; /* Length of next key */
      next_key+=2;
    }
    else
      n_length= *next_key++ & 127;
    if (!packed)
      n_length-= s_temp->store_not_null;

    if (n_length || packed)             /* Don't pack 0 length keys */
    {
      uint next_length_pack, new_ref_length=s_temp->ref_length;

      if (packed)
      {
        /* If first key and next key is packed (only on delete) */
        if (!prev_key && org_key)
        {
          get_key_length(org_key_length,org_key);
          key=start;
          if (sort_order)                       /* SerG */
          {
            while (key < end &&
                   sort_order[*key] == sort_order[*org_key])
            {
              key++; org_key++;
            }
          }
          else
          {
            while (key < end && *key == *org_key)
            {
              key++; org_key++;
            }
          }
          if ((new_ref_length= (uint) (key - start)))
            new_ref_length+=pack_marker;
        }

        if (!n_length)
        {
          /*
            We put a different key between two identical variable length keys
            Extend next key to have same prefix as this key
          */
          if (new_ref_length)                   /* prefix of previus key */
          {                                     /* make next key longer */
            s_temp->part_of_prev_key= new_ref_length;
            s_temp->prev_length=          org_key_length -
              (new_ref_length-pack_marker);
            s_temp->n_ref_length= s_temp->part_of_prev_key;
            s_temp->n_length= s_temp->prev_length;
            n_length=             get_pack_length(s_temp->prev_length);
            s_temp->prev_key+=    (new_ref_length - pack_marker);
            length+=              s_temp->prev_length + n_length;
          }
          else
          {                                     /* Can't use prev key */
            s_temp->part_of_prev_key=0;
            s_temp->prev_length= org_key_length;
            s_temp->n_ref_length=s_temp->n_length=  org_key_length;
            length+=           org_key_length;
          }
          return (s_temp->move_length= (int) length);
        }

        ref_length=n_length;
        /* Get information about not packed key suffix */
        get_key_pack_length(n_length,next_length_pack,next_key);

        /* Test if new keys has fewer characters that match the previous key */
        if (!new_ref_length)
        {                                       /* Can't use prev key */
          s_temp->part_of_prev_key=     0;
          s_temp->prev_length=          ref_length;
          s_temp->n_ref_length= s_temp->n_length= n_length+ref_length;
          return s_temp->move_length= ((int) length+ref_length-
                                       next_length_pack);
        }
        if (ref_length+pack_marker > new_ref_length)
        {
          uint new_pack_length=new_ref_length-pack_marker;
          /* We must copy characters from the original key to the next key */
          s_temp->part_of_prev_key= new_ref_length;
          s_temp->prev_length=      ref_length - new_pack_length;
          s_temp->n_ref_length=s_temp->n_length=n_length + s_temp->prev_length;
          s_temp->prev_key+=        new_pack_length;
          length-= (next_length_pack - get_pack_length(s_temp->n_length));
          return s_temp->move_length= ((int) length + s_temp->prev_length);
        }
      }
      else
      {
        /* Next key wasn't a prefix of previous key */
        ref_length=0;
        next_length_pack=0;
     }
      DBUG_PRINT("test",("length: %d  next_key: 0x%lx", length,
                         (long) next_key));

      {
        uint tmp_length;
        key=(start+=ref_length);
        if (key+n_length < key_end)             /* Normalize length based */
          key_end= key+n_length;
        if (sort_order)                         /* SerG */
        {
          while (key < key_end &&
                 sort_order[*key] == sort_order[*next_key])
          {
            key++; next_key++;
          }
        }
        else
        {
          while (key < key_end && *key == *next_key)
          {
            key++; next_key++;
          }
        }
        if (!(tmp_length=(uint) (key-start)))
        {                                       /* Key can't be re-packed */
          s_temp->next_key_pos=0;
          return (s_temp->move_length= length);
        }
        ref_length+=tmp_length;
        n_length-=tmp_length;
        length-=tmp_length+next_length_pack;    /* We gained these chars */
      }
      if (n_length == 0 && ref_length == new_key_length)
      {
        s_temp->n_ref_length=pack_marker;       /* Same as prev key */
      }
      else
      {
        s_temp->n_ref_length=ref_length | pack_marker;
        length+= get_pack_length(n_length);
        s_temp->n_length=n_length;
      }
    }
  }
  return (s_temp->move_length= length);
}


/* Length of key which is prefix compressed */

int _ma_calc_bin_pack_key_length(const MARIA_KEY *int_key,
                                 uint nod_flag,
                                 uchar *next_key,
                                 uchar *org_key, uchar *prev_key,
                                 MARIA_KEY_PARAM *s_temp)
{
  uint length,key_length,ref_length;
  const uchar *key= int_key->data;

  s_temp->totlength= key_length= (int_key->data_length + int_key->ref_length+
                                  nod_flag);
#ifdef HAVE_valgrind
  s_temp->n_length= s_temp->n_ref_length=0;	/* For valgrind */
#endif
  s_temp->key=key;
  s_temp->prev_key=org_key;
  if (prev_key)                                 /* If not first key in block */
  {
    /* pack key against previous key */
    /*
      As keys may be identical when running a sort in maria_chk, we
      have to guard against the case where keys may be identical
    */
    const uchar *end;
    end=key+key_length;
    for ( ; *key == *prev_key && key < end; key++,prev_key++) ;
    s_temp->ref_length= ref_length=(uint) (key-s_temp->key);
    length=key_length - ref_length + get_pack_length(ref_length);
  }
  else
  {
    /* No previous key */
    s_temp->ref_length=ref_length=0;
    length=key_length+1;
  }
  if ((s_temp->next_key_pos=next_key))          /* If another key after */
  {
    /* pack key against next key */
    uint next_length,next_length_pack;
    get_key_pack_length(next_length,next_length_pack,next_key);

    /* If first key and next key is packed (only on delete) */
    if (!prev_key && org_key && next_length)
    {
      const uchar *end;
      for (key= s_temp->key, end=key+next_length ;
           *key == *org_key && key < end;
           key++,org_key++) ;
      ref_length= (uint) (key - s_temp->key);
    }

    if (next_length > ref_length)
    {
      /*
        We put a key with different case between two keys with the same prefix
        Extend next key to have same prefix as this key
      */
      s_temp->n_ref_length= ref_length;
      s_temp->prev_length=  next_length-ref_length;
      s_temp->prev_key+=    ref_length;
      return s_temp->move_length= ((int) (length+ s_temp->prev_length -
                                          next_length_pack +
                                          get_pack_length(ref_length)));
    }
    /* Check how many characters are identical to next key */
    key= s_temp->key+next_length;
    s_temp->prev_length= 0;
    while (*key++ == *next_key++) ;
    if ((ref_length= (uint) (key - s_temp->key)-1) == next_length)
    {
      s_temp->next_key_pos=0;
      return (s_temp->move_length= length);  /* Can't pack next key */
    }
    s_temp->n_ref_length=ref_length;
    return s_temp->move_length= (int) (length-(ref_length - next_length) -
                                       next_length_pack +
                                       get_pack_length(ref_length));
  }
  return (s_temp->move_length= (int) length);
}


/*
** store a key packed with _ma_calc_xxx_key_length in page-buffert
*/

/* store key without compression */

void _ma_store_static_key(MARIA_KEYDEF *keyinfo __attribute__((unused)),
                          register uchar *key_pos,
                          register MARIA_KEY_PARAM *s_temp)
{
  memcpy(key_pos, s_temp->key,(size_t) s_temp->move_length);
  s_temp->changed_length= s_temp->move_length;
}


/* store variable length key with prefix compression */

#define store_pack_length(test,pos,length) { \
  if (test) { *((pos)++) = (uchar) (length); } else \
  { *((pos)++) = (uchar) ((length) >> 8); *((pos)++) = (uchar) (length);  } }


void _ma_store_var_pack_key(MARIA_KEYDEF *keyinfo  __attribute__((unused)),
                            register uchar *key_pos,
                            register MARIA_KEY_PARAM *s_temp)
{
  uint length;
  uchar *org_key_pos= key_pos;

  if (s_temp->ref_length)
  {
    /* Packed against previous key */
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->ref_length);
    /* If not same key after */
    if (s_temp->ref_length != s_temp->pack_marker)
      store_key_length_inc(key_pos,s_temp->key_length);
  }
  else
  {
    /* Not packed against previous key */
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->key_length);
  }
  bmove(key_pos, s_temp->key,
        (length= s_temp->totlength - (uint) (key_pos-org_key_pos)));

  key_pos+= length;

  if (!s_temp->next_key_pos)                    /* No following key */
    goto end;

  if (s_temp->prev_length)
  {
    /* Extend next key because new key didn't have same prefix as prev key */
    if (s_temp->part_of_prev_key)
    {
      store_pack_length(s_temp->pack_marker == 128,key_pos,
                        s_temp->part_of_prev_key);
      store_key_length_inc(key_pos,s_temp->n_length);
    }
    else
    {
      s_temp->n_length+= s_temp->store_not_null;
      store_pack_length(s_temp->pack_marker == 128,key_pos,
                        s_temp->n_length);
    }
    memcpy(key_pos, s_temp->prev_key, s_temp->prev_length);
    key_pos+= s_temp->prev_length;
  }
  else if (s_temp->n_ref_length)
  {
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->n_ref_length);
    if (s_temp->n_ref_length != s_temp->pack_marker)
    {
      /* Not identical key */
      store_key_length_inc(key_pos,s_temp->n_length);
    }
  }
  else
  {
    s_temp->n_length+= s_temp->store_not_null;
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->n_length);
  }

end:
  s_temp->changed_length= (uint) (key_pos - org_key_pos);
}


/* variable length key with prefix compression */

void _ma_store_bin_pack_key(MARIA_KEYDEF *keyinfo  __attribute__((unused)),
                            register uchar *key_pos,
                            register MARIA_KEY_PARAM *s_temp)
{
  uchar *org_key_pos= key_pos;
  size_t length= s_temp->totlength - s_temp->ref_length;

  store_key_length_inc(key_pos,s_temp->ref_length);
  memcpy(key_pos, s_temp->key+s_temp->ref_length, length);
  key_pos+= length;

  if (s_temp->next_key_pos)
  {
    store_key_length_inc(key_pos,s_temp->n_ref_length);
    if (s_temp->prev_length)                    /* If we must extend key */
    {
      memcpy(key_pos,s_temp->prev_key,s_temp->prev_length);
      key_pos+= s_temp->prev_length;
    }
  }
  s_temp->changed_length= (uint) (key_pos - org_key_pos);
}
