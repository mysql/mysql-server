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

/* key handling functions */

#include "fulltext.h"
#include "m_ctype.h"

static my_bool _mi_get_prev_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                                uchar *key, uchar *keypos,
                                uint *return_key_length);

        /* Check index */

int _mi_check_index(MI_INFO *info, int inx)
{
  if (inx == -1)                        /* Use last index */
    inx=info->lastinx;
  if (inx < 0 || ! mi_is_key_active(info->s->state.key_map, inx))
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
} /* mi_check_index */


        /*
        ** Search after row by a key
        ** Position to row is stored in info->lastpos
        ** Return: -1 if not found
        **          1 if one should continue search on higher level
        */

int _mi_search(register MI_INFO *info, register MI_KEYDEF *keyinfo,
               uchar *key, uint key_len, uint nextflag, register my_off_t pos)
{
  my_bool last_key;
  int error,flag;
  uint nod_flag;
  uchar *keypos,*maxpos;
  uchar lastkey[MI_MAX_KEY_BUFF],*buff;
  DBUG_ENTER("_mi_search");
  DBUG_PRINT("enter",("pos: %lu  nextflag: %u  lastpos: %lu",
                      (ulong) pos, nextflag, (ulong) info->lastpos));
  DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,keyinfo->seg,key,key_len););

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;                      /* Didn't find key */
    info->lastpos= HA_OFFSET_ERROR;
    if (!(nextflag & (SEARCH_SMALLER | SEARCH_BIGGER | SEARCH_LAST)))
      DBUG_RETURN(-1);                          /* Not found ; return error */
    DBUG_RETURN(1);                             /* Search at upper levels */
  }

  if (!(buff=_mi_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,info->buff,
                               test(!(nextflag & SEARCH_SAVE_BUFF)))))
    goto err;
  DBUG_DUMP("page",(byte*) buff,mi_getint(buff));

  flag=(*keyinfo->bin_search)(info,keyinfo,buff,key,key_len,nextflag,
                              &keypos,lastkey, &last_key);
  if (flag == MI_FOUND_WRONG_KEY)
    DBUG_RETURN(-1);
  nod_flag=mi_test_if_nod(buff);
  maxpos=buff+mi_getint(buff)-1;

  if (flag)
  {
    if ((error=_mi_search(info,keyinfo,key,key_len,nextflag,
                          _mi_kpos(nod_flag,keypos))) <= 0)
      DBUG_RETURN(error);

    if (flag >0)
    {
      if (nextflag & (SEARCH_SMALLER | SEARCH_LAST) &&
          keypos == buff+2+nod_flag)
        DBUG_RETURN(1);                                 /* Bigger than key */
    }
    else if (nextflag & SEARCH_BIGGER && keypos >= maxpos)
      DBUG_RETURN(1);                                   /* Smaller than key */
  }
  else
  {
    if ((nextflag & SEARCH_FIND) && nod_flag &&
	((keyinfo->flag & (HA_NOSAME | HA_NULL_PART)) != HA_NOSAME ||
	 key_len != USE_WHOLE_KEY))
    {
      if ((error=_mi_search(info,keyinfo,key,key_len,SEARCH_FIND,
                            _mi_kpos(nod_flag,keypos))) >= 0 ||
          my_errno != HA_ERR_KEY_NOT_FOUND)
        DBUG_RETURN(error);
      info->last_keypage= HA_OFFSET_ERROR;              /* Buffer not in mem */
    }
  }
  if (pos != info->last_keypage)
  {
    uchar *old_buff=buff;
    if (!(buff=_mi_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,info->buff,
                                 test(!(nextflag & SEARCH_SAVE_BUFF)))))
      goto err;
    keypos=buff+(keypos-old_buff);
    maxpos=buff+(maxpos-old_buff);
  }

  if ((nextflag & (SEARCH_SMALLER | SEARCH_LAST)) && flag != 0)
  {
    uint not_used;
    if (_mi_get_prev_key(info,keyinfo, buff, info->lastkey, keypos,
                         &info->lastkey_length))
      goto err;
    if (!(nextflag & SEARCH_SMALLER) &&
        ha_key_cmp(keyinfo->seg, info->lastkey, key, key_len, SEARCH_FIND,
                    &not_used))
    {
      my_errno=HA_ERR_KEY_NOT_FOUND;                    /* Didn't find key */
      goto err;
    }
  }
  else
  {
    info->lastkey_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey);
    if (!info->lastkey_length)
      goto err;
    memcpy(info->lastkey,lastkey,info->lastkey_length);
  }
  info->lastpos=_mi_dpos(info,0,info->lastkey+info->lastkey_length);
  /* Save position for a possible read next / previous */
  info->int_keypos=info->buff+ (keypos-buff);
  info->int_maxpos=info->buff+ (maxpos-buff);
  info->int_nod_flag=nod_flag;
  info->int_keytree_version=keyinfo->version;
  info->last_search_keypage=info->last_keypage;
  info->page_changed=0;
  info->buff_used= (info->buff != buff);        /* If we have to reread buff */

  DBUG_PRINT("exit",("found key at %lu",(ulong) info->lastpos));
  DBUG_RETURN(0);

err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  info->lastpos= HA_OFFSET_ERROR;
  info->page_changed=1;
  DBUG_RETURN (-1);
} /* _mi_search */


        /* Search after key in page-block */
        /* If packed key puts smaller or identical key in buff */
        /* ret_pos point to where find or bigger key starts */
        /* ARGSUSED */

int _mi_bin_search(MI_INFO *info, register MI_KEYDEF *keyinfo, uchar *page,
                   uchar *key, uint key_len, uint comp_flag, uchar **ret_pos,
                   uchar *buff __attribute__((unused)), my_bool *last_key)
{
  reg4 int start,mid,end,save_end;
  int flag;
  uint totlength,nod_flag,not_used;
  DBUG_ENTER("_mi_bin_search");

  LINT_INIT(flag);
  totlength=keyinfo->keylength+(nod_flag=mi_test_if_nod(page));
  start=0; mid=1;
  save_end=end=(int) ((mi_getint(page)-2-nod_flag)/totlength-1);
  DBUG_PRINT("test",("mi_getint: %d  end: %d",mi_getint(page),end));
  page+=2+nod_flag;

  while (start != end)
  {
    mid= (start+end)/2;
    if ((flag=ha_key_cmp(keyinfo->seg,page+(uint) mid*totlength,key,key_len,
                          comp_flag,&not_used))
        >= 0)
      end=mid;
    else
      start=mid+1;
  }
  if (mid != start)
    flag=ha_key_cmp(keyinfo->seg,page+(uint) start*totlength,key,key_len,
                     comp_flag,&not_used);
  if (flag < 0)
    start++;                    /* point at next, bigger key */
  *ret_pos=page+(uint) start*totlength;
  *last_key= end == save_end;
  DBUG_PRINT("exit",("flag: %d  keypos: %d",flag,start));
  DBUG_RETURN(flag);
} /* _mi_bin_search */


/*
  Locate a packed key in a key page.

  SYNOPSIS
    _mi_seq_search()
    info                        Open table information.
    keyinfo                     Key definition information.
    page                        Key page (beginning).
    key                         Search key.
    key_len                     Length to use from search key or USE_WHOLE_KEY
    comp_flag                   Search flags like SEARCH_SAME etc.
    ret_pos             RETURN  Position in key page behind this key.
    buff                RETURN  Copy of previous or identical unpacked key.
    last_key            RETURN  If key is last in page.

  DESCRIPTION
    Used instead of _mi_bin_search() when key is packed.
    Puts smaller or identical key in buff.
    Key is searched sequentially.

  RETURN
    > 0         Key in 'buff' is smaller than search key.
    0           Key in 'buff' is identical to search key.
    < 0         Not found.
*/

int _mi_seq_search(MI_INFO *info, register MI_KEYDEF *keyinfo, uchar *page,
                   uchar *key, uint key_len, uint comp_flag, uchar **ret_pos,
                   uchar *buff, my_bool *last_key)
{
  int flag;
  uint nod_flag,length,not_used;
  uchar t_buff[MI_MAX_KEY_BUFF],*end;
  DBUG_ENTER("_mi_seq_search");

  LINT_INIT(flag); LINT_INIT(length);
  end= page+mi_getint(page);
  nod_flag=mi_test_if_nod(page);
  page+=2+nod_flag;
  *ret_pos=page;
  t_buff[0]=0;                                  /* Avoid bugs */
  while (page < end)
  {
    length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,t_buff);
    if (length == 0 || page > end)
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_PRINT("error",("Found wrong key:  length: %u  page: %p  end: %p",
                          length, page, end));
      DBUG_RETURN(MI_FOUND_WRONG_KEY);
    }
    if ((flag=ha_key_cmp(keyinfo->seg,t_buff,key,key_len,comp_flag,
                          &not_used)) >= 0)
      break;
#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("page: %p  key: '%s'  flag: %d", page, t_buff, flag));
#endif
    memcpy(buff,t_buff,length);
    *ret_pos=page;
  }
  if (flag == 0)
    memcpy(buff,t_buff,length);                 /* Result is first key */
  *last_key= page == end;
  DBUG_PRINT("exit",("flag: %d  ret_pos: %p", flag, *ret_pos));
  DBUG_RETURN(flag);
} /* _mi_seq_search */


int _mi_prefix_search(MI_INFO *info, register MI_KEYDEF *keyinfo, uchar *page,
                      uchar *key, uint key_len, uint nextflag, uchar **ret_pos,
                      uchar *buff, my_bool *last_key)
{
  /*
    my_flag is raw comparison result to be changed according to
    SEARCH_NO_FIND,SEARCH_LAST and HA_REVERSE_SORT flags.
    flag is the value returned by ha_key_cmp and as treated as final
  */
  int flag=0, my_flag=-1;
  uint nod_flag, length, len, matched, cmplen, kseg_len;
  uint prefix_len,suffix_len;
  int key_len_skip, seg_len_pack, key_len_left;
  uchar *end, *kseg, *vseg;
  uchar *sort_order=keyinfo->seg->charset->sort_order;
  uchar tt_buff[MI_MAX_KEY_BUFF+2], *t_buff=tt_buff+2;
  uchar *saved_from, *saved_to, *saved_vseg;
  uint  saved_length=0, saved_prefix_len=0;
  uint  length_pack;
  DBUG_ENTER("_mi_prefix_search");

  LINT_INIT(length);
  LINT_INIT(prefix_len);
  LINT_INIT(seg_len_pack);
  LINT_INIT(saved_from);
  LINT_INIT(saved_to);
  LINT_INIT(saved_vseg);

  t_buff[0]=0;                                  /* Avoid bugs */
  end= page+mi_getint(page);
  nod_flag=mi_test_if_nod(page);
  page+=2+nod_flag;
  *ret_pos=page;
  kseg=key;

  get_key_pack_length(kseg_len,length_pack,kseg);
  key_len_skip=length_pack+kseg_len;
  key_len_left=(int) key_len- (int) key_len_skip;
  /* If key_len is 0, then lenght_pack is 1, then key_len_left is -1. */
  cmplen=(key_len_left>=0) ? kseg_len : key_len-length_pack;
  DBUG_PRINT("info",("key: '%.*s'",kseg_len,kseg));

  /*
    Keys are compressed the following way:

    If the max length of first key segment <= 127 bytes the prefix is
    1 byte else it's 2 byte

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

    vseg=page;
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
      uchar *from=vseg+suffix_len;
      HA_KEYSEG *keyseg;
      uint l;

      for (keyseg=keyinfo->seg+1 ; keyseg->type ; keyseg++ )
      {

        if (keyseg->flag & HA_NULL_PART)
        {
          if (!(*from++))
            continue;
        }
        if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK))
        {
          get_key_length(l,from);
        }
        else
          l=keyseg->length;

        from+=l;
      }
      from+=keyseg->length;
      page=from+nod_flag;
      length=from-vseg;
    }

    if (page > end)
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_PRINT("error",("Found wrong key:  length: %u  page: %p  end: %p",
                          length, page, end));
      DBUG_RETURN(MI_FOUND_WRONG_KEY);
    }

    if (matched >= prefix_len)
    {
      /* We have to compare. But we can still skip part of the key */
      uint  left;
      uchar *k=kseg+prefix_len;

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
        **     >      =                        prefix ? found : (matched=len; continue search)
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
	    uchar *end= k+ (cmplen - len);
	    for ( ; k < end && *k == ' '; k++) ;
	    if (k == end)
	      goto cmp_rest;		/* should never happen */
	    if (*k < (uchar) ' ')
	    {
	      my_flag= 1;		/* Compared string is smaller */
	      break;
	    }
	    my_flag= -1;		/* Continue searching */
	  }
        }
        else if (len > cmplen)
        {
	  uchar *end;
	  if ((nextflag & SEARCH_PREFIX) && key_len_left == 0)
	    goto fix_flag;

	  /* We have to compare k and vseg as if they were space extended */
	  for (end=vseg + (len-cmplen) ;
	       vseg < end && *vseg == (uchar) ' ';
	       vseg++, matched++) ;
	  DBUG_ASSERT(vseg < end);

	  if (*vseg > (uchar) ' ')
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
	    uint not_used;
	    if ((flag = ha_key_cmp(keyinfo->seg+1,vseg,
				   k,key_len_left,nextflag,&not_used)) >= 0)
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
    saved_to=buff+saved_length;
    saved_from=saved_vseg;
    saved_length=length;
    *ret_pos=page;
  }
  if (my_flag)
    flag=(keyinfo->seg->flag & HA_REVERSE_SORT) ? -my_flag : my_flag;
  if (flag == 0)
  {
    memcpy(buff,t_buff,saved_length=seg_len_pack+prefix_len);
    saved_to=buff+saved_length;
    saved_from=saved_vseg;
    saved_length=length;
  }
  if (saved_length)
    memcpy(saved_to,saved_from,saved_length);

  *last_key= page == end;

  DBUG_PRINT("exit",("flag: %d  ret_pos: %p", flag, *ret_pos));
  DBUG_RETURN(flag);
} /* _mi_prefix_search */


        /* Get pos to a key_block */

my_off_t _mi_kpos(uint nod_flag, uchar *after_key)
{
  after_key-=nod_flag;
  switch (nod_flag) {
#if SIZEOF_OFF_T > 4
  case 7:
    return mi_uint7korr(after_key)*MI_MIN_KEY_BLOCK_LENGTH;
  case 6:
    return mi_uint6korr(after_key)*MI_MIN_KEY_BLOCK_LENGTH;
  case 5:
    return mi_uint5korr(after_key)*MI_MIN_KEY_BLOCK_LENGTH;
#else
  case 7:
    after_key++;
  case 6:
    after_key++;
  case 5:
    after_key++;
#endif
  case 4:
    return ((my_off_t) mi_uint4korr(after_key))*MI_MIN_KEY_BLOCK_LENGTH;
  case 3:
    return ((my_off_t) mi_uint3korr(after_key))*MI_MIN_KEY_BLOCK_LENGTH;
  case 2:
    return (my_off_t) (mi_uint2korr(after_key)*MI_MIN_KEY_BLOCK_LENGTH);
  case 1:
    return (uint) (*after_key)*MI_MIN_KEY_BLOCK_LENGTH;
  case 0:                                       /* At leaf page */
  default:                                      /* Impossible */
    return(HA_OFFSET_ERROR);
  }
} /* _kpos */


        /* Save pos to a key_block */

void _mi_kpointer(register MI_INFO *info, register uchar *buff, my_off_t pos)
{
  pos/=MI_MIN_KEY_BLOCK_LENGTH;
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
} /* _mi_kpointer */


        /* Calc pos to a data-record from a key */


my_off_t _mi_dpos(MI_INFO *info, uint nod_flag, uchar *after_key)
{
  my_off_t pos;
  after_key-=(nod_flag + info->s->rec_reflength);
  switch (info->s->rec_reflength) {
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
  return (info->s->options &
          (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ? pos :
            pos*info->s->base.pack_reclength;
}


/* Calc position from a record pointer ( in delete link chain ) */

my_off_t _mi_rec_pos(MYISAM_SHARE *s, uchar *ptr)
{
  my_off_t pos;
  switch (s->rec_reflength) {
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
    ptr+= (s->rec_reflength-4);
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
  return ((s->options &
          (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ? pos :
          pos*s->base.pack_reclength);
}


        /* save position to record */

void _mi_dpointer(MI_INFO *info, uchar *buff, my_off_t pos)
{
  if (!(info->s->options &
        (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) &&
      pos != HA_OFFSET_ERROR)
    pos/=info->s->base.pack_reclength;

  switch (info->s->rec_reflength) {
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
} /* _mi_dpointer */


        /* Get key from key-block */
        /* page points at previous key; its advanced to point at next key */
        /* key should contain previous key */
        /* Returns length of found key + pointers */
        /* nod_flag is a flag if we are on nod */

        /* same as _mi_get_key but used with fixed length keys */

uint _mi_get_static_key(register MI_KEYDEF *keyinfo, uint nod_flag,
                       register uchar **page, register uchar *key)
{
  memcpy((byte*) key,(byte*) *page,
         (size_t) (keyinfo->keylength+nod_flag));
  *page+=keyinfo->keylength+nod_flag;
  return(keyinfo->keylength);
} /* _mi_get_static_key */


/*
  get key witch is packed against previous key or key with a NULL column.

  SYNOPSIS
    _mi_get_pack_key()
    keyinfo                     key definition information.
    nod_flag                    If nod: Length of node pointer, else zero.
    page_pos            RETURN  position in key page behind this key.
    key                 IN/OUT  in: prev key, out: unpacked key.

  RETURN
    key_length + length of data pointer
*/

uint _mi_get_pack_key(register MI_KEYDEF *keyinfo, uint nod_flag,
                      register uchar **page_pos, register uchar *key)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *start_key,*page=*page_pos;
  uint length;

  start_key=key;
  for (keyseg=keyinfo->seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->flag & HA_PACK_KEY)
    {
      /* key with length, packed to previous key */
      uchar *start=key;
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
          mi_print_error(keyinfo->share, HA_ERR_CRASHED);
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
                       ("Found too long null packed key: %u of %u at %p",
                        length, keyseg->length, *page_pos));
	    DBUG_DUMP("key",(char*) *page_pos,16);
            mi_print_error(keyinfo->share, HA_ERR_CRASHED);
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
	  bmove_upp((char*) key+length+3,(char*) key+length+1,length);
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
        DBUG_PRINT("error",("Found too long packed key: %u of %u at %p",
                            length, keyseg->length, *page_pos));
        DBUG_DUMP("key",(char*) *page_pos,16);
        mi_print_error(keyinfo->share, HA_ERR_CRASHED);
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
    memcpy((byte*) key,(byte*) page,(size_t) length);
    key+=length;
    page+=length;
  }
  length=keyseg->length+nod_flag;
  bmove((byte*) key,(byte*) page,length);
  *page_pos= page+length;
  return ((uint) (key-start_key)+keyseg->length);
} /* _mi_get_pack_key */



/* key that is packed relatively to previous */

uint _mi_get_binary_pack_key(register MI_KEYDEF *keyinfo, uint nod_flag,
                             register uchar **page_pos, register uchar *key)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *start_key,*page,*page_end,*from,*from_end;
  uint length,tmp;
  DBUG_ENTER("_mi_get_binary_pack_key");

  page= *page_pos;
  page_end=page+MI_MAX_KEY_BUFF+1;
  start_key=key;

  /*
    Keys are compressed the following way:

    prefix length    Packed length of prefix for the prev key. (1 or 3 bytes)
    for each key segment:
      [is null]        Null indicator if can be null (1 byte, zero means null)
      [length]         Packed length if varlength (1 or 3 bytes)
    pointer          Reference to the data file (last_keyseg->length).
  */
  get_key_length(length,page);
  if (length)
  {
    if (length > keyinfo->maxlength)
    {
      DBUG_PRINT("error",("Found too long binary packed key: %u of %u at %p",
                          length, keyinfo->maxlength, *page_pos));
      DBUG_DUMP("key",(char*) *page_pos,16);
      mi_print_error(keyinfo->share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(0);                                 /* Wrong key */
    }
    from=key;  from_end=key+length;
  }
  else
  {
    from=page; from_end=page_end;               /* Not packed key */
  }

  /*
    The trouble is that key is split in two parts:
     The first part is in from ...from_end-1.
     The second part starts at page
  */
  for (keyseg=keyinfo->seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->flag & HA_NULL_PART)
    {
      if (from == from_end) { from=page;  from_end=page_end; }
      if (!(*key++ = *from++))
        continue;                               /* Null part */
    }
    if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART | HA_SPACE_PACK))
    {
      /* Get length of dynamic length key part */
      if (from == from_end) { from=page;  from_end=page_end; }
      if ((length= (*key++ = *from++)) == 255)
      {
        if (from == from_end) { from=page;  from_end=page_end; }
        length= (uint) ((*key++ = *from++)) << 8;
        if (from == from_end) { from=page;  from_end=page_end; }
        length+= (uint) ((*key++ = *from++));
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
    DBUG_PRINT("info",("key: %p  from: %p  length: %u",
		       key, from, length));
    memcpy_overlap((byte*) key, (byte*) from, (size_t) length);
    key+=length;
    from+=length;
  }
  length=keyseg->length+nod_flag;
  if ((tmp=(uint) (from_end-from)) <= length)
  {
    memcpy(key+tmp,page,length-tmp);            /* Get last part of key */
    *page_pos= page+length-tmp;
  }
  else
  {
    if (from_end != page_end)
    {
      DBUG_PRINT("error",("Error when unpacking key"));
      mi_print_error(keyinfo->share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(0);                                 /* Error */
    }
    memcpy((byte*) key,(byte*) from,(size_t) length);
    *page_pos= from+length;
  }
  DBUG_RETURN((uint) (key-start_key)+keyseg->length);
}


        /* Get key at position without knowledge of previous key */
        /* Returns pointer to next key */

uchar *_mi_get_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                   uchar *key, uchar *keypos, uint *return_key_length)
{
  uint nod_flag;
  DBUG_ENTER("_mi_get_key");

  nod_flag=mi_test_if_nod(page);
  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
  {
    bmove((byte*) key,(byte*) keypos,keyinfo->keylength+nod_flag);
    DBUG_RETURN(keypos+keyinfo->keylength+nod_flag);
  }
  else
  {
    page+=2+nod_flag;
    key[0]=0;                                   /* safety */
    while (page <= keypos)
    {
      *return_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,key);
      if (*return_key_length == 0)
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(0);
      }
    }
  }
  DBUG_PRINT("exit",("page: %p  length: %u", page, *return_key_length));
  DBUG_RETURN(page);
} /* _mi_get_key */


        /* Get key at position without knowledge of previous key */
        /* Returns 0 if ok */

static my_bool _mi_get_prev_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                                uchar *key, uchar *keypos,
                                uint *return_key_length)
{
  uint nod_flag;
  DBUG_ENTER("_mi_get_prev_key");

  nod_flag=mi_test_if_nod(page);
  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
  {
    *return_key_length=keyinfo->keylength;
    bmove((byte*) key,(byte*) keypos- *return_key_length-nod_flag,
          *return_key_length);
    DBUG_RETURN(0);
  }
  else
  {
    page+=2+nod_flag;
    key[0]=0;                                   /* safety */
    while (page < keypos)
    {
      *return_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,key);
      if (*return_key_length == 0)
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
} /* _mi_get_key */



        /* Get last key from key-page */
        /* Return pointer to where key starts */

uchar *_mi_get_last_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                        uchar *lastkey, uchar *endpos, uint *return_key_length)
{
  uint nod_flag;
  uchar *lastpos;
  DBUG_ENTER("_mi_get_last_key");
  DBUG_PRINT("enter",("page: %p  endpos:  %p", page, endpos));

  nod_flag=mi_test_if_nod(page);
  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
  {
    lastpos=endpos-keyinfo->keylength-nod_flag;
    *return_key_length=keyinfo->keylength;
    if (lastpos > page)
      bmove((byte*) lastkey,(byte*) lastpos,keyinfo->keylength+nod_flag);
  }
  else
  {
    lastpos=(page+=2+nod_flag);
    lastkey[0]=0;
    while (page < endpos)
    {
      lastpos=page;
      *return_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,lastkey);
      if (*return_key_length == 0)
      {
        DBUG_PRINT("error",("Couldn't find last key:  page: %p", page));
        mi_print_error(info->s, HA_ERR_CRASHED);
        my_errno=HA_ERR_CRASHED;
        DBUG_RETURN(0);
      }
    }
  }
  DBUG_PRINT("exit",("lastpos: %p  length: %u", lastpos, *return_key_length));
  DBUG_RETURN(lastpos);
} /* _mi_get_last_key */


        /* Calculate length of key */

uint _mi_keylength(MI_KEYDEF *keyinfo, register uchar *key)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *start;

  if (! (keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
    return (keyinfo->keylength);

  start=key;
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
} /* _mi_keylength */


/*
  Calculate length of part key.

  Used in mi_rkey() to find the key found for the key-part that was used.
  This is needed in case of multi-byte character sets where we may search
  after '0xDF' but find 'ss'
*/

uint _mi_keylength_part(MI_KEYDEF *keyinfo, register uchar *key,
			HA_KEYSEG *end)
{
  reg1 HA_KEYSEG *keyseg;
  uchar *start= key;

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

        /* Move a key */

uchar *_mi_move_key(MI_KEYDEF *keyinfo, uchar *to, uchar *from)
{
  reg1 uint length;
  memcpy((byte*) to, (byte*) from,
         (size_t) (length=_mi_keylength(keyinfo,from)));
  return to+length;
}

        /* Find next/previous record with same key */
        /* This can't be used when database is touched after last read */

int _mi_search_next(register MI_INFO *info, register MI_KEYDEF *keyinfo,
                    uchar *key, uint key_length, uint nextflag, my_off_t pos)
{
  int error;
  uint nod_flag;
  uchar lastkey[MI_MAX_KEY_BUFF];
  DBUG_ENTER("_mi_search_next");
  DBUG_PRINT("enter",("nextflag: %u  lastpos: %lu  int_keypos: %lu",
                      nextflag, (ulong) info->lastpos,
                      (ulong) info->int_keypos));
  DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,keyinfo->seg,key,key_length););

  /* Force full read if we are at last key or if we are not on a leaf
     and the key tree has changed since we used it last time
     Note that even if the key tree has changed since last read, we can use
     the last read data from the leaf if we haven't used the buffer for
     something else.
  */

  if (((nextflag & SEARCH_BIGGER) && info->int_keypos >= info->int_maxpos) ||
      info->page_changed ||
      (info->int_keytree_version != keyinfo->version &&
       (info->int_nod_flag || info->buff_used)))
    DBUG_RETURN(_mi_search(info,keyinfo,key, USE_WHOLE_KEY,
                           nextflag | SEARCH_SAVE_BUFF, pos));

  if (info->buff_used)
  {
    if (!_mi_fetch_keypage(info,keyinfo,info->last_search_keypage,
                           DFLT_INIT_HITS,info->buff,0))
      DBUG_RETURN(-1);
    info->buff_used=0;
  }

  /* Last used buffer is in info->buff */
  nod_flag=mi_test_if_nod(info->buff);

  if (nextflag & SEARCH_BIGGER)                                 /* Next key */
  {
    my_off_t tmp_pos=_mi_kpos(nod_flag,info->int_keypos);
    if (tmp_pos != HA_OFFSET_ERROR)
    {
      if ((error=_mi_search(info,keyinfo,key, USE_WHOLE_KEY,
                            nextflag | SEARCH_SAVE_BUFF, tmp_pos)) <=0)
        DBUG_RETURN(error);
    }
    memcpy(lastkey,key,key_length);
    if (!(info->lastkey_length=(*keyinfo->get_key)(keyinfo,nod_flag,
                                                   &info->int_keypos,lastkey)))
      DBUG_RETURN(-1);
  }
  else                                                  /* Previous key */
  {
    uint length;
    /* Find start of previous key */
    info->int_keypos=_mi_get_last_key(info,keyinfo,info->buff,lastkey,
                                      info->int_keypos, &length);
    if (!info->int_keypos)
      DBUG_RETURN(-1);
    if (info->int_keypos == info->buff+2)
      DBUG_RETURN(_mi_search(info,keyinfo,key, USE_WHOLE_KEY,
                             nextflag | SEARCH_SAVE_BUFF, pos));
    if ((error=_mi_search(info,keyinfo,key, USE_WHOLE_KEY,
			  nextflag | SEARCH_SAVE_BUFF,
                          _mi_kpos(nod_flag,info->int_keypos))) <= 0)
      DBUG_RETURN(error);

    /* QQ: We should be able to optimize away the following call */
    if (! _mi_get_last_key(info,keyinfo,info->buff,lastkey,
                           info->int_keypos,&info->lastkey_length))
      DBUG_RETURN(-1);
  }
  memcpy(info->lastkey,lastkey,info->lastkey_length);
  info->lastpos=_mi_dpos(info,0,info->lastkey+info->lastkey_length);
  DBUG_PRINT("exit",("found key at %lu",(ulong) info->lastpos));
  DBUG_RETURN(0);
} /* _mi_search_next */


        /* Search after position for the first row in an index */
        /* This is stored in info->lastpos */

int _mi_search_first(register MI_INFO *info, register MI_KEYDEF *keyinfo,
                     register my_off_t pos)
{
  uint nod_flag;
  uchar *page;
  DBUG_ENTER("_mi_search_first");

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    info->lastpos= HA_OFFSET_ERROR;
    DBUG_RETURN(-1);
  }

  do
  {
    if (!_mi_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,info->buff,0))
    {
      info->lastpos= HA_OFFSET_ERROR;
      DBUG_RETURN(-1);
    }
    nod_flag=mi_test_if_nod(info->buff);
    page=info->buff+2+nod_flag;
  } while ((pos=_mi_kpos(nod_flag,page)) != HA_OFFSET_ERROR);

  if (!(info->lastkey_length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,
                                                 info->lastkey)))
    DBUG_RETURN(-1);                            /* Crashed */

  info->int_keypos=page; info->int_maxpos=info->buff+mi_getint(info->buff)-1;
  info->int_nod_flag=nod_flag;
  info->int_keytree_version=keyinfo->version;
  info->last_search_keypage=info->last_keypage;
  info->page_changed=info->buff_used=0;
  info->lastpos=_mi_dpos(info,0,info->lastkey+info->lastkey_length);

  DBUG_PRINT("exit",("found key at %lu", (ulong) info->lastpos));
  DBUG_RETURN(0);
} /* _mi_search_first */


        /* Search after position for the last row in an index */
        /* This is stored in info->lastpos */

int _mi_search_last(register MI_INFO *info, register MI_KEYDEF *keyinfo,
                    register my_off_t pos)
{
  uint nod_flag;
  uchar *buff,*page;
  DBUG_ENTER("_mi_search_last");

  if (pos == HA_OFFSET_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;                      /* Didn't find key */
    info->lastpos= HA_OFFSET_ERROR;
    DBUG_RETURN(-1);
  }

  buff=info->buff;
  do
  {
    if (!_mi_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,buff,0))
    {
      info->lastpos= HA_OFFSET_ERROR;
      DBUG_RETURN(-1);
    }
    page= buff+mi_getint(buff);
    nod_flag=mi_test_if_nod(buff);
  } while ((pos=_mi_kpos(nod_flag,page)) != HA_OFFSET_ERROR);

  if (!_mi_get_last_key(info,keyinfo,buff,info->lastkey,page,
                        &info->lastkey_length))
    DBUG_RETURN(-1);
  info->lastpos=_mi_dpos(info,0,info->lastkey+info->lastkey_length);
  info->int_keypos=info->int_maxpos=page;
  info->int_nod_flag=nod_flag;
  info->int_keytree_version=keyinfo->version;
  info->last_search_keypage=info->last_keypage;
  info->page_changed=info->buff_used=0;

  DBUG_PRINT("exit",("found key at %lu",(ulong) info->lastpos));
  DBUG_RETURN(0);
} /* _mi_search_last */



/****************************************************************************
**
** Functions to store and pack a key in a page
**
** mi_calc_xx_key_length takes the following arguments:
**  nod_flag    If nod: Length of nod-pointer
**  next_key    Position to pos after the new key in buffer
**  org_key     Key that was before the next key in buffer
**  prev_key    Last key before current key
**  key         Key that will be stored
**  s_temp      Information how next key will be packed
****************************************************************************/

/* Static length key */

int
_mi_calc_static_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
                           uchar *next_pos  __attribute__((unused)),
                           uchar *org_key  __attribute__((unused)),
                           uchar *prev_key __attribute__((unused)),
                           uchar *key, MI_KEY_PARAM *s_temp)
{
  s_temp->key=key;
  return (int) (s_temp->totlength=keyinfo->keylength+nod_flag);
}

/* Variable length key */

int
_mi_calc_var_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
                        uchar *next_pos  __attribute__((unused)),
                        uchar *org_key  __attribute__((unused)),
                        uchar *prev_key __attribute__((unused)),
                        uchar *key, MI_KEY_PARAM *s_temp)
{
  s_temp->key=key;
  return (int) (s_temp->totlength=_mi_keylength(keyinfo,key)+nod_flag);
}

/*
  length of key with a variable length first segment which is prefix
  compressed (myisamchk reports 'packed + stripped')

  Keys are compressed the following way:

  If the max length of first key segment <= 127 bytes the prefix is
  1 byte else it's 2 byte

  prefix byte(s) The high bit is set if this is a prefix for the prev key
  length         Packed length if the previous was a prefix byte
  [length]       data bytes ('length' bytes)
  next-key-seg   Next key segments

  If the first segment can have NULL:
  The length is 0 for NULLS and 1+length for not null columns.

*/

int
_mi_calc_var_pack_key_length(MI_KEYDEF *keyinfo,uint nod_flag,uchar *next_key,
                             uchar *org_key, uchar *prev_key, uchar *key,
                             MI_KEY_PARAM *s_temp)
{
  reg1 HA_KEYSEG *keyseg;
  int length;
  uint key_length,ref_length,org_key_length=0,
       length_pack,new_key_length,diff_flag,pack_marker;
  uchar *start,*end,*key_end,*sort_order;
  bool same_length;

  length_pack=s_temp->ref_length=s_temp->n_ref_length=s_temp->n_length=0;
  same_length=0; keyseg=keyinfo->seg;
  key_length=_mi_keylength(keyinfo,key)+nod_flag;

  sort_order=0;
  if ((keyinfo->flag & HA_FULLTEXT) &&
      ((keyseg->type == HA_KEYTYPE_TEXT) ||
       (keyseg->type == HA_KEYTYPE_VARTEXT1) ||
       (keyseg->type == HA_KEYTYPE_VARTEXT2)) &&
      !use_strnxfrm(keyseg->charset))
    sort_order=keyseg->charset->sort_order;

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
      s_temp->key=key;
      s_temp->ref_length=s_temp->key_length=0;
      s_temp->totlength=key_length-1+diff_flag;
      s_temp->next_key_pos=0;                   /* No next key */
      return (s_temp->totlength);
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
  s_temp->prev_key=org_key;

  /* The key part will start with a packed length */

  get_key_pack_length(new_key_length,length_pack,key);
  end=key_end= key+ new_key_length;
  start=key;

  /* Calc how many characters are identical between this and the prev. key */
  if (prev_key)
  {
    get_key_length(org_key_length,prev_key);
    s_temp->prev_key=prev_key;          /* Pointer at data */
    /* Don't use key-pack if length == 0 */
    if (new_key_length && new_key_length == org_key_length)
      same_length=1;
    else if (new_key_length > org_key_length)
      end=key + org_key_length;

    if (sort_order)                             /* SerG */
    {
      while (key < end && sort_order[*key] == sort_order[*prev_key])
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
      s_temp->n_length= *next_key;              /* Needed by _mi_store_key */
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
            while (key < end && sort_order[*key] == sort_order[*org_key])
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
            /* +get_pack_length(org_key_length); */
          }
          return (int) length;
        }

        ref_length=n_length;
        get_key_pack_length(n_length,next_length_pack,next_key);

        /* Test if new keys has fewer characters that match the previous key */
        if (!new_ref_length)
        {                                       /* Can't use prev key */
          s_temp->part_of_prev_key=     0;
          s_temp->prev_length=          ref_length;
          s_temp->n_ref_length= s_temp->n_length= n_length+ref_length;
          /* s_temp->prev_key+=         get_pack_length(org_key_length); */
          return (int) length+ref_length-next_length_pack;
        }
        if (ref_length+pack_marker > new_ref_length)
        {
          uint new_pack_length=new_ref_length-pack_marker;
          /* We must copy characters from the original key to the next key */
          s_temp->part_of_prev_key= new_ref_length;
          s_temp->prev_length=      ref_length - new_pack_length;
          s_temp->n_ref_length=s_temp->n_length=n_length + s_temp->prev_length;
          s_temp->prev_key+=        new_pack_length;
/*                                  +get_pack_length(org_key_length); */
          length= length-get_pack_length(ref_length)+
            get_pack_length(new_pack_length);
          return (int) length + s_temp->prev_length;
        }
      }
      else
      {
        /* Next key wasn't a prefix of previous key */
        ref_length=0;
        next_length_pack=0;
     }
      DBUG_PRINT("test",("length: %d  next_key: %p", length, next_key));

      {
        uint tmp_length;
        key=(start+=ref_length);
        if (key+n_length < key_end)             /* Normalize length based */
          key_end=key+n_length;
        if (sort_order)                         /* SerG */
        {
          while (key < key_end && sort_order[*key] ==
                 sort_order[*next_key])
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
          return length;
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
  return length;
}


/* Length of key which is prefix compressed */

int
_mi_calc_bin_pack_key_length(MI_KEYDEF *keyinfo,uint nod_flag,uchar *next_key,
                             uchar *org_key, uchar *prev_key, uchar *key,
                             MI_KEY_PARAM *s_temp)
{
  uint length,key_length,ref_length;

  s_temp->totlength=key_length=_mi_keylength(keyinfo,key)+nod_flag;
#ifdef HAVE_purify
  s_temp->n_length= s_temp->n_ref_length=0;	/* For valgrind */
#endif
  s_temp->key=key;
  s_temp->prev_key=org_key;
  if (prev_key)                                 /* If not first key in block */
  {
    /* pack key against previous key */
    /*
      As keys may be identical when running a sort in myisamchk, we
      have to guard against the case where keys may be identical
    */
    uchar *end;
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
      uchar *end;
      for (key= s_temp->key, end=key+next_length ;
           *key == *org_key && key < end;
           key++,org_key++) ;
      ref_length= (uint) (key - s_temp->key);
    }

    if (next_length > ref_length)
    {
      /* We put a key with different case between two keys with the same prefix
         Extend next key to have same prefix as
         this key */
      s_temp->n_ref_length= ref_length;
      s_temp->prev_length=  next_length-ref_length;
      s_temp->prev_key+=    ref_length;
      return (int) (length+ s_temp->prev_length - next_length_pack +
                    get_pack_length(ref_length));
    }
    /* Check how many characters are identical to next key */
    key= s_temp->key+next_length;
    while (*key++ == *next_key++) ;
    if ((ref_length= (uint) (key - s_temp->key)-1) == next_length)
    {
      s_temp->next_key_pos=0;
      return length;                            /* can't pack next key */
    }
    s_temp->prev_length=0;
    s_temp->n_ref_length=ref_length;
    return (int) (length-(ref_length - next_length) - next_length_pack +
                  get_pack_length(ref_length));
  }
  return (int) length;
}


/*
** store a key packed with _mi_calc_xxx_key_length in page-buffert
*/

/* store key without compression */

void _mi_store_static_key(MI_KEYDEF *keyinfo __attribute__((unused)),
                          register uchar *key_pos,
                          register MI_KEY_PARAM *s_temp)
{
  memcpy((byte*) key_pos,(byte*) s_temp->key,(size_t) s_temp->totlength);
}


/* store variable length key with prefix compression */

#define store_pack_length(test,pos,length) { \
  if (test) { *((pos)++) = (uchar) (length); } else \
  { *((pos)++) = (uchar) ((length) >> 8); *((pos)++) = (uchar) (length);  } }


void _mi_store_var_pack_key(MI_KEYDEF *keyinfo  __attribute__((unused)),
                            register uchar *key_pos,
                            register MI_KEY_PARAM *s_temp)
{
  uint length;
  uchar *start;

  start=key_pos;

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
  bmove((byte*) key_pos,(byte*) s_temp->key,
        (length=s_temp->totlength-(uint) (key_pos-start)));

  if (!s_temp->next_key_pos)                    /* No following key */
    return;
  key_pos+=length;

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
  }
  else if (s_temp->n_ref_length)
  {
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->n_ref_length);
    if (s_temp->n_ref_length == s_temp->pack_marker)
      return;                                   /* Identical key */
    store_key_length(key_pos,s_temp->n_length);
  }
  else
  {
    s_temp->n_length+= s_temp->store_not_null;
    store_pack_length(s_temp->pack_marker == 128,key_pos,s_temp->n_length);
  }
}


/* variable length key with prefix compression */

void _mi_store_bin_pack_key(MI_KEYDEF *keyinfo  __attribute__((unused)),
                            register uchar *key_pos,
                            register MI_KEY_PARAM *s_temp)
{
  store_key_length_inc(key_pos,s_temp->ref_length);
  memcpy((char*) key_pos,(char*) s_temp->key+s_temp->ref_length,
          (size_t) s_temp->totlength-s_temp->ref_length);

  if (s_temp->next_key_pos)
  {
    key_pos+=(uint) (s_temp->totlength-s_temp->ref_length);
    store_key_length_inc(key_pos,s_temp->n_ref_length);
    if (s_temp->prev_length)                    /* If we must extend key */
    {
      memcpy(key_pos,s_temp->prev_key,s_temp->prev_length);
    }
  }
}
