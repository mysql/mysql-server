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

/* S|ker efter positionen f|r en nyckel samt d{rmedh|rande funktioner */

#include "isamdef.h"
#include "m_ctype.h"

#define CMP(a,b) (a<b ? -1 : a == b ? 0 : 1)

	/* Check index */

int _nisam_check_index(N_INFO *info, int inx)
{
  if (inx == -1)			/* Use last index */
    inx=info->lastinx;
  if (inx >= (int) info->s->state.keys || inx < 0)
  {
    my_errno=HA_ERR_WRONG_INDEX;
    return -1;
  }
  if (info->lastinx != inx)		/* Index changed */
  {
    info->lastinx = inx;
    info->lastpos = NI_POS_ERROR;
    info->update= ((info->update & (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED)) |
		   HA_STATE_NEXT_FOUND | HA_STATE_PREV_FOUND);
  }
  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    return(-1);
  return(inx);
} /* ni_check_index */


	/* S|ker reda p} positionen f|r ett record p} basen av en nyckel */
	/* Positionen l{ggs i info->lastpos */
	/* Returns -1 if not found and 1 if search at upper levels */

int _nisam_search(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key, uint key_len, uint nextflag, register ulong pos)
{
  int error,flag;
  uint nod_flag;
  uchar *keypos,*maxpos;
  uchar lastkey[N_MAX_KEY_BUFF],*buff;
  DBUG_ENTER("_nisam_search");
  DBUG_PRINT("enter",("pos: %ld  nextflag: %d  lastpos: %ld",
		      pos,nextflag,info->lastpos));

  if (pos == NI_POS_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;			/* Didn't find key */
    info->lastpos= NI_POS_ERROR;
    if (!(nextflag & (SEARCH_SMALLER | SEARCH_BIGGER | SEARCH_LAST)))
      DBUG_RETURN(-1);				/* Not found ; return error */
    DBUG_RETURN(1);				/* Search at upper levels */
  }

  if (!(buff=_nisam_fetch_keypage(info,keyinfo,pos,info->buff,
			       test(!(nextflag & SEARCH_SAVE_BUFF)))))
    goto err;
  DBUG_DUMP("page",(byte*) buff,getint(buff));

  flag=(*keyinfo->bin_search)(info,keyinfo,buff,key,key_len,nextflag,
			      &keypos,lastkey);
  nod_flag=test_if_nod(buff);
  maxpos=buff+getint(buff)-1;

  if (flag)
  {
    if ((error=_nisam_search(info,keyinfo,key,key_len,nextflag,
			  _nisam_kpos(nod_flag,keypos))) <= 0)
      DBUG_RETURN(error);

    if (flag >0)
    {
      if ((nextflag & (SEARCH_SMALLER | SEARCH_LAST)) &&
	  keypos == buff+2+nod_flag)
	DBUG_RETURN(1);					/* Bigger than key */
    }
    else if (nextflag & SEARCH_BIGGER && keypos >= maxpos)
      DBUG_RETURN(1);					/* Smaller than key */
  }
  else
  {
    if (nextflag & SEARCH_FIND && (!(keyinfo->base.flag & HA_NOSAME)
				   || key_len) && nod_flag)
    {
      if ((error=_nisam_search(info,keyinfo,key,key_len,SEARCH_FIND,
			    _nisam_kpos(nod_flag,keypos))) >= 0 ||
	  my_errno != HA_ERR_KEY_NOT_FOUND)
	DBUG_RETURN(error);
      info->int_pos= NI_POS_ERROR;		/* Buffer not in memory */
    }
  }
  if (pos != info->int_pos)
  {
    uchar *old_buff=buff;
    if (!(buff=_nisam_fetch_keypage(info,keyinfo,pos,info->buff,
				 test(!(nextflag & SEARCH_SAVE_BUFF)))))
      goto err;
    keypos=buff+(keypos-old_buff);
    maxpos=buff+(maxpos-old_buff);
  }

  if ((nextflag & (SEARCH_SMALLER | SEARCH_LAST)) && flag != 0)
  {
    keypos=_nisam_get_last_key(info,keyinfo,buff,lastkey,keypos);
    if ((nextflag & SEARCH_LAST) &&
	_nisam_key_cmp(keyinfo->seg, info->lastkey, key, key_len, SEARCH_FIND))
    {
      my_errno=HA_ERR_KEY_NOT_FOUND;			/* Didn't find key */
      goto err;
    }
  }

  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey));
  VOID(_nisam_move_key(keyinfo,info->lastkey,lastkey));
  info->lastpos=_nisam_dpos(info,nod_flag,keypos);
  info->int_keypos=info->buff+ (keypos-buff);
  info->int_maxpos=info->buff+ (maxpos-buff);
  info->page_changed=0;
  info->buff_used= (info->buff != buff);
  info->last_search_keypage=info->int_pos;

  DBUG_PRINT("exit",("found key at %ld",info->lastpos));
  DBUG_RETURN(0);
err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  info->lastpos= NI_POS_ERROR;
  DBUG_RETURN (-1);
} /* _nisam_search */


	/* Search after key in page-block */
	/* If packed key puts smaller or identical key in buff */
	/* ret_pos point to where find or bigger key starts */
	/* ARGSUSED */

int _nisam_bin_search(N_INFO *info, register N_KEYDEF *keyinfo, uchar *page,
		   uchar *key, uint key_len, uint comp_flag, uchar **ret_pos,
		   uchar *buff __attribute__((unused)))
{
  reg4 int start,mid,end;
  int flag;
  uint totlength,nod_flag;
  DBUG_ENTER("_nisam_bin_search");

  LINT_INIT(flag);
  totlength=keyinfo->base.keylength+(nod_flag=test_if_nod(page));
  start=0; mid=1;
  end= (int) ((getint(page)-2-nod_flag)/totlength-1);
  DBUG_PRINT("test",("getint: %d  end: %d",getint(page),end));
  page+=2+nod_flag;

  while (start != end)
  {
    mid= (start+end)/2;
    if ((flag=_nisam_key_cmp(keyinfo->seg,page+(uint) mid*totlength,key,key_len,
			  comp_flag))
	>= 0)
      end=mid;
    else
      start=mid+1;
  }
  if (mid != start)
    flag=_nisam_key_cmp(keyinfo->seg,page+(uint) start*totlength,key,key_len,
		     comp_flag);
  if (flag < 0)
    start++;			/* point at next, bigger key */
  *ret_pos=page+(uint) start*totlength;
  DBUG_PRINT("exit",("flag: %d  keypos: %d",flag,start));
  DBUG_RETURN(flag);
} /* _nisam_bin_search */


	/* Used instead of _nisam_bin_search() when key is packed */
	/* Puts smaller or identical key in buff */
	/* Key is searched sequentially */

int _nisam_seq_search(N_INFO *info, register N_KEYDEF *keyinfo, uchar *page, uchar *key, uint key_len, uint comp_flag, uchar **ret_pos, uchar *buff)
{
  int flag;
  uint nod_flag,length;
  uchar t_buff[N_MAX_KEY_BUFF],*end;
  DBUG_ENTER("_nisam_seq_search");

  LINT_INIT(flag); LINT_INIT(length);
  end= page+getint(page);
  nod_flag=test_if_nod(page);
  page+=2+nod_flag;
  *ret_pos=page;
  while (page < end)
  {
    length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,t_buff);
    if ((flag=_nisam_key_cmp(keyinfo->seg,t_buff,key,key_len,comp_flag)) >= 0)
      break;
#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("page: %lx  key: '%s'  flag: %d",page,t_buff,flag));
#endif
    memcpy(buff,t_buff,length);
    *ret_pos=page;
  }
  if (flag == 0)
    memcpy(buff,t_buff,length);			/* Result is first key */
  DBUG_PRINT("exit",("flag: %d  ret_pos: %lx",flag,*ret_pos));
  DBUG_RETURN(flag);
} /* _nisam_seq_search */


	/* Get pos to a key_block */

ulong _nisam_kpos(uint nod_flag, uchar *after_key)
{
  after_key-=nod_flag;
  switch (nod_flag) {
  case 3:
    return uint3korr(after_key)*512L;
  case 2:
    return uint2korr(after_key)*512L;
  case 1:
    return (uint) (*after_key)*512L;
  case 0:					/* At leaf page */
  default:					/* Impossible */
    return(NI_POS_ERROR);
  }
} /* _kpos */


	/* Save pos to a key_block */

void _nisam_kpointer(register N_INFO *info, register uchar *buff, ulong pos)
{
  pos/=512L;
  switch (info->s->base.key_reflength) {
  case 3: int3store(buff,pos); break;
  case 2: int2store(buff,(uint) pos); break;
  case 1: buff[0]= (uchar) pos; break;
  default: abort();				/* impossible */
  }
} /* _nisam_kpointer */


	/* Calc pos to a data-record */

ulong _nisam_dpos(N_INFO *info, uint nod_flag, uchar *after_key)
{
  ulong pos;
  after_key-=(nod_flag + info->s->rec_reflength);
  switch (info->s->rec_reflength) {
  case 4:
    pos= (ulong) uint4korr(after_key);
    break;
  case 3:
    pos= (ulong) uint3korr(after_key);
    break;
  case 2:
    pos= (ulong) uint2korr(after_key);
    break;
  default:
    pos=0L;			/* Shut compiler up */
  }
  return (info->s->base.options &
	  (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ? pos :
	    pos*info->s->base.reclength;
}

	/* save pos to record */

void _nisam_dpointer(N_INFO *info, uchar *buff, ulong pos)
{
  if (!(info->s->base.options &
	(HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    pos/=info->s->base.reclength;

  switch (info->s->rec_reflength) {
  case 4: int4store(buff,pos); break;
  case 3: int3store(buff,pos); break;
  case 2: int2store(buff,(uint) pos); break;
  default: abort();			/* Impossible */
  }
} /* _nisam_dpointer */


	/*
	** Compare two keys with is bigger
	** Returns <0, 0, >0 acording to with is bigger
	** Key_length specifies length of key to use.  Number-keys can't
	** be splitted
	** If flag <> SEARCH_FIND compare also position
	*/
int _nisam_key_cmp(register N_KEYSEG *keyseg, register uchar *a, register uchar *b, uint key_length, uint nextflag)
{
  reg4 int flag,length_diff;
  int16 s_1,s_2;
  int32	l_1,l_2;
  uint32 u_1,u_2;
  float f_1,f_2;
  double d_1,d_2;
  reg5 uchar *end;

  if (!(nextflag & (SEARCH_FIND | SEARCH_NO_FIND | SEARCH_LAST))
      || key_length == 0)
    key_length=N_MAX_KEY_BUFF*2;

  for ( ; (int) key_length >0 ; key_length-= (keyseg++)->base.length)
  {
    end= a+ min(keyseg->base.length,key_length);
    switch ((enum ha_base_keytype) keyseg->base.type) {
    case HA_KEYTYPE_TEXT:			/* Ascii; Key is converted */
    case HA_KEYTYPE_BINARY:
      if (keyseg->base.flag & HA_SPACE_PACK)
      {
	uchar *as, *bs;
	int length,b_length;

	as=a++; bs=b++;
	length= (length_diff= ((int) *as - (b_length= (int) *bs))) < 0 ?
	  (int) *as : b_length;
	end= a+ min(key_length,(uint) length);

#ifdef USE_STRCOLL
        if (use_strcoll(default_charset_info)) {
          if (((enum ha_base_keytype) keyseg->base.type) == HA_KEYTYPE_BINARY)
          {
            while (a < end)
              if ((flag= (int) *a++ - (int) *b++))
                return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
          }
          else
          {
            if ((flag = my_strnncoll(default_charset_info,
				     a, (int) (end-a), b, b_length)))
              return (keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag;
            b+= (uint) (end-a);
            a=end;
          }
        }
        else
#endif
	{
          while (a < end)
            if ((flag= (int) *a++ - (int) *b++))
              return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
	}
	if (key_length < (uint) keyseg->base.length)
	{						/* key_part */
	  if (length_diff)
	  {
	    if (length_diff < 0 || (uint) *as <= key_length)
	      return ((keyseg->base.flag & HA_REVERSE_SORT) ?
		      -length_diff : length_diff);
	    for (length= (int) key_length-b_length; length-- > 0 ;)
	    {
	      if (*a++ != ' ')
		return ((keyseg->base.flag & HA_REVERSE_SORT) ? -1 : 1);
	    }
	  }
	  if (nextflag & SEARCH_NO_FIND)	/* Find record after key */
	    return (nextflag & SEARCH_BIGGER) ? -1 : 1;
	  return 0;
	}
	else
	{
	  if (length_diff)
	    return ((keyseg->base.flag & HA_REVERSE_SORT) ?
		    -length_diff : length_diff);
	}
	a=as+ (uint) *as+1 ; b= bs+ b_length+1;		/* to next key */
      }
      else
      {
#ifdef USE_STRCOLL
        if (use_strcoll(default_charset_info)) {
          if (((enum ha_base_keytype) keyseg->base.type) == HA_KEYTYPE_BINARY)
          {
            while (a < end)
              if ((flag= (int) *a++ - (int) *b++))
                return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
          }
          else
          {
            if ((flag = my_strnncoll(default_charset_info,
				     a, (int) (end-a), b, (int) (end-a))))
              return (keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag;
            b+= (uint) (end-a);
            a=end;
          }
        }
        else
#endif
	{
          while (a < end)
            if ((flag= (int) *a++ - (int) *b++))
              return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
	}
      }
      break;
    case HA_KEYTYPE_INT8:
    {
      int i_1= (int) *((signed char*) a);
      int i_2= (int) *((signed char*) b);
      if ((flag = CMP(i_1,i_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a= end;
      b++;
      break;
    }
    case HA_KEYTYPE_SHORT_INT:
      shortget(s_1,a);
      shortget(s_2,b);
      if ((flag = CMP(s_1,s_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 2; /* sizeof(short int); */
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
	uint16 us_1,us_2;
	ushortget(us_1,a);
	ushortget(us_2,b);
	if ((flag = CMP(us_1,us_2)))
	  return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
	a=  end;
	b+=2; /* sizeof(short int); */
	break;
      }
    case HA_KEYTYPE_LONG_INT:
      longget(l_1,a);
      longget(l_2,b);
      if ((flag = CMP(l_1,l_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_ULONG_INT:
      ulongget(u_1,a);
      ulongget(u_2,b);
      if ((flag = CMP(u_1,u_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_INT24:
      l_1=sint3korr(a);
      l_2=sint3korr(b);
      if ((flag = CMP(l_1,l_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_UINT24:
      l_1=(long) uint3korr(a);
      l_2=(long) uint3korr(b);
      if ((flag = CMP(l_1,l_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_FLOAT:
      bmove((byte*) &f_1,(byte*) a,(int) sizeof(float));
      bmove((byte*) &f_2,(byte*) b,(int) sizeof(float));
      if ((flag = CMP(f_1,f_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= sizeof(float);
      break;
    case HA_KEYTYPE_DOUBLE:
      doubleget(d_1,a);
      doubleget(d_2,b);
      if ((flag = CMP(d_1,d_2)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= sizeof(double);
      break;
    case HA_KEYTYPE_NUM:				/* Numeric key */
    {
      int swap_flag=keyseg->base.flag & HA_REVERSE_SORT;
      if (keyseg->base.flag & HA_SPACE_PACK)
      {
	int alength,blength;

	if (swap_flag)
	  swap(uchar*,a,b);
	alength= *a++; blength= *b++;
	if ((flag=(int) (keyseg->base.length-key_length)) < 0)
	  flag=0;
	if (alength != blength+flag)
	{
	  if ((alength > blength+flag && *a != '-') ||
	      (alength < blength+flag && *b == '-'))
	    return 1;
	  else
	    return -1;
	}
	if (*a == '-' && *b == '-')
	{
	  swap_flag=1;
	  swap(uchar*,a,b);
	}
	end=a+alength;
	while (a < end)
	  if (*a++ !=  *b++)
	  {
	    a--; b--;
	    if (isdigit((char) *a) && isdigit((char) *b))
	      return ((int) *a - (int) *b);
	    if (*a == '-' || isdigit((char) *b))
	      return (-1);
	    if (*b == '-' || *b++ == ' ' || isdigit((char) *a))
	      return (1);
	    if (*a++ == ' ')
	      return (-1);
	  }
      }
      else
      {
	for ( ; a < end && *a == ' ' && *b == ' ' ; a++, b++) ;
	if (*a == '-' && *b == '-')
	  swap_flag=1;
	if (swap_flag)
	{
	  end=b+(int) (end-a);
	  swap(uchar*,a,b);
	}
	while (a < end)
	  if (*a++ != *b++)
	  {
	    a--; b--;
	    if (isdigit((char) *a) && isdigit((char) *b))
	      return ((int) *a - (int) *b);
	    if (*a == '-' || isdigit((char) *b))
	      return (-1);
	    if (*b == '-' || *b++ == ' ' || isdigit((char) *a))
	      return (1);
	    if (*a++ == ' ')
	      return -1;
	  }
      }
      if (swap_flag)
	swap(uchar*,a,b);
      break;
    }
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      longlong ll_a,ll_b;
      longlongget(ll_a,a);
      longlongget(ll_b,b);
      if ((flag = CMP(ll_a,ll_b)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= sizeof(longlong);
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      ulonglong ll_a,ll_b;
      longlongget(ll_a,a);
      longlongget(ll_b,b);
      if ((flag = CMP(ll_a,ll_b)))
	return ((keyseg->base.flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= sizeof(ulonglong);
      break;
    }
#endif
    case HA_KEYTYPE_END:			/* Ready */
    case HA_KEYTYPE_VARTEXT:			/* Impossible */
    case HA_KEYTYPE_VARBINARY:			/* Impossible */
      goto end;
    }
  }
end:
  if (!(nextflag & SEARCH_FIND))
  {
    if (nextflag & (SEARCH_NO_FIND | SEARCH_LAST)) /* Find record after key */
      return (nextflag & (SEARCH_BIGGER | SEARCH_LAST)) ? -1 : 1;
    LINT_INIT(l_1); LINT_INIT(l_2);
    switch (keyseg->base.length) {
    case 4:
      u_1= (ulong) uint4korr(a);
      u_2= (ulong) uint4korr(b);
      break;
    case 3:
      u_1= (ulong) uint3korr(a);
      u_2= (ulong) uint3korr(b);
      break;
    case 2:
      u_1= (ulong) uint2korr(a);
      u_2= (ulong) uint2korr(b);
      break;
    default: abort();				/* Impossible */
    }
    flag = CMP(u_1,u_2);

    if (nextflag & SEARCH_SAME)
      return (flag);				/* read same */
    if (nextflag & SEARCH_BIGGER)
      return (flag <= 0 ? -1 : 1);		/* read next */
    return (flag < 0 ? -1 : 1);			/* read previous */
  }
  return 0;
} /* _nisam_key_cmp */


	/* Get key from key-block */
	/* page points at previous key; its advanced to point at next key */
	/* key should contain previous key */
	/* Returns length of found key + pointers */
	/* nod_flag is a flag if we are on nod */

uint _nisam_get_key(register N_KEYDEF *keyinfo, uint nod_flag,
		 register uchar **page, register uchar *key)
{
  reg1 N_KEYSEG *keyseg;
  uchar *start,*start_key;
  uint length,c_length;

  LINT_INIT(start);
  start_key=key; c_length=0;
  for (keyseg=keyinfo->seg ; keyseg->base.type ;keyseg++)
  {
    if (keyseg->base.flag & (HA_SPACE_PACK | HA_PACK_KEY))
    {
      start=key;
      if (keyseg->base.flag & HA_SPACE_PACK)
	key++;
      if ((length= *(*page)++) & 128)
      {
	key+= (c_length=(length & 127));
	if (c_length == 0)	/* Same key */
	{
	  key+= *start;		/* Same diff_key as prev */
	  length=0;
	}
	else
	{
	  if (keyseg->base.flag & HA_SPACE_PACK)
	    length= *(*page)++;
	  else
	    length=keyseg->base.length-length+128; /* Rest of key */
	  /* Prevent core dumps if wrong data formats */
	  if (length > keyseg->base.length)
	    length=0;
	}
      }
    }
    else
      length=keyseg->base.length;
    memcpy((byte*) key,(byte*) *page,(size_t) length); key+=length;
    if (keyseg->base.flag & HA_SPACE_PACK)
      *start= (uchar) ((key-start)-1);
    *page+=length;
  }
  length=keyseg->base.length+nod_flag;
  bmove((byte*) key,(byte*) *page,length);
  *page+=length;
  return((uint) (key-start_key)+keyseg->base.length);
} /* _nisam_get_key */


	/* same as _nisam_get_key but used with fixed length keys */

uint _nisam_get_static_key(register N_KEYDEF *keyinfo, uint nod_flag, register uchar **page, register uchar *key)
{
  memcpy((byte*) key,(byte*) *page,
	 (size_t) (keyinfo->base.keylength+nod_flag));
  *page+=keyinfo->base.keylength+nod_flag;
  return(keyinfo->base.keylength);
} /* _nisam_get_static_key */


	/* Get last key from key-block, starting from keypos */
	/* Return pointer to where keystarts */

uchar *_nisam_get_last_key(N_INFO *info, N_KEYDEF *keyinfo, uchar *keypos, uchar *lastkey, uchar *endpos)
{
  uint nod_flag;
  uchar *lastpos;

  nod_flag=test_if_nod(keypos);
  if (! (keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)))
  {
    lastpos=endpos-keyinfo->base.keylength-nod_flag;
    if (lastpos > keypos)
      bmove((byte*) lastkey,(byte*) lastpos,keyinfo->base.keylength+nod_flag);
  }
  else
  {
    lastpos=0 ; keypos+=2+nod_flag;
    lastkey[0]=0;
    while (keypos < endpos)
    {
      lastpos=keypos;
      VOID(_nisam_get_key(keyinfo,nod_flag,&keypos,lastkey));
    }
  }
  return lastpos;
} /* _nisam_get_last_key */


	/* Calculate length of key */

uint _nisam_keylength(N_KEYDEF *keyinfo, register uchar *key)
{
  reg1 N_KEYSEG *keyseg;
  uchar *start;

  if (! (keyinfo->base.flag & HA_SPACE_PACK_USED))
    return (keyinfo->base.keylength);

  start=key;
  for (keyseg=keyinfo->seg ; keyseg->base.type ; keyseg++)
  {
    if (keyseg->base.flag & HA_SPACE_PACK)
      key+= *key+1;
    else
      key+= keyseg->base.length;
  }
  return((uint) (key-start)+keyseg->base.length);
} /* _nisam_keylength */


	/* Move a key */

uchar *_nisam_move_key(N_KEYDEF *keyinfo, uchar *to, uchar *from)
{
  reg1 uint length;
  memcpy((byte*) to, (byte*) from,
	 (size_t) (length=_nisam_keylength(keyinfo,from)));
  return to+length;
}

	/* Find next/previous record with same key */
	/* This can't be used when database is touched after last read */

int _nisam_search_next(register N_INFO *info, register N_KEYDEF *keyinfo,
		    uchar *key, uint nextflag, ulong pos)
{
  int error;
  uint nod_flag;
  uchar lastkey[N_MAX_KEY_BUFF];
  DBUG_ENTER("_nisam_search_next");
  DBUG_PRINT("enter",("nextflag: %d  lastpos: %d  int_keypos: %lx",
		       nextflag,info->lastpos,info->int_keypos));
  DBUG_EXECUTE("key",_nisam_print_key(DBUG_FILE,keyinfo->seg,key););

  if ((nextflag & SEARCH_BIGGER && info->int_keypos >= info->int_maxpos) ||
      info->int_pos == NI_POS_ERROR || info->page_changed)
    DBUG_RETURN(_nisam_search(info,keyinfo,key,0,nextflag | SEARCH_SAVE_BUFF,
			   pos));

  if (info->buff_used)
  {
    if (!_nisam_fetch_keypage(info,keyinfo,info->last_search_keypage,
			      info->buff,0))
    {
      info->lastpos= NI_POS_ERROR;
      DBUG_RETURN(-1);
    }
    info->buff_used=0;
  }

  /* Last used buffer is in info->buff */

  nod_flag=test_if_nod(info->buff);
  VOID(_nisam_move_key(keyinfo,lastkey,key));

  if (nextflag & SEARCH_BIGGER)					/* Next key */
  {
    ulong tmp_pos=_nisam_kpos(nod_flag,info->int_keypos);
    if (tmp_pos != NI_POS_ERROR)
    {
      if ((error=_nisam_search(info,keyinfo,key,0,nextflag | SEARCH_SAVE_BUFF,
			    tmp_pos)) <=0)
	DBUG_RETURN(error);
    }
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&info->int_keypos,lastkey));
  }
  else							/* Previous key */
  {
    info->int_keypos=_nisam_get_last_key(info,keyinfo,info->buff,lastkey,
				      info->int_keypos);
    if (info->int_keypos == info->buff+2)
      DBUG_RETURN(_nisam_search(info,keyinfo,key,0,nextflag | SEARCH_SAVE_BUFF,
			     pos));
    if ((error=_nisam_search(info,keyinfo,key,0,nextflag | SEARCH_SAVE_BUFF,
			  _nisam_kpos(nod_flag,info->int_keypos))) <= 0)
      DBUG_RETURN(error);
  }

  info->int_keypos=_nisam_get_last_key(info,keyinfo,info->buff,lastkey,
				    info->int_keypos);
  VOID(_nisam_move_key(keyinfo,info->lastkey,lastkey));
  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&info->int_keypos,info->lastkey));
  info->lastpos=_nisam_dpos(info,nod_flag,info->int_keypos);
  DBUG_PRINT("exit",("found key at %d",info->lastpos));
  DBUG_RETURN(0);
} /* _nisam_search_next */


	/* S|ker reda p} positionen f|r f|rsta recordet i ett index */
	/* Positionen l{ggs i info->lastpos */

int _nisam_search_first(register N_INFO *info, register N_KEYDEF *keyinfo, register ulong pos)
{
  uint nod_flag;
  uchar *page;
  DBUG_ENTER("_nisam_search_first");

  if (pos == NI_POS_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    info->lastpos= NI_POS_ERROR;
    DBUG_RETURN(-1);
  }

  do
  {
    if (!_nisam_fetch_keypage(info,keyinfo,pos,info->buff,0))
    {
      info->lastpos= NI_POS_ERROR;
      DBUG_RETURN(-1);
    }
    nod_flag=test_if_nod(info->buff);
    page=info->buff+2+nod_flag;
  } while ((pos=_nisam_kpos(nod_flag,page)) != NI_POS_ERROR);

  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&page,info->lastkey));
  info->int_keypos=page; info->int_maxpos=info->buff+getint(info->buff)-1;
  info->lastpos=_nisam_dpos(info,nod_flag,page);
  info->page_changed=info->buff_used=0;
  info->last_search_keypage=info->int_pos;

  DBUG_PRINT("exit",("found key at %d",info->lastpos));
  DBUG_RETURN(0);
} /* _nisam_search_first */


	/* S|ker reda p} positionen f|r sista recordet i ett index */
	/* Positionen l{ggs i info->lastpos */

int _nisam_search_last(register N_INFO *info, register N_KEYDEF *keyinfo, register ulong pos)
{
  uint nod_flag;
  uchar *buff,*page;
  DBUG_ENTER("_nisam_search_last");

  if (pos == NI_POS_ERROR)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;			/* Didn't find key */
    info->lastpos= NI_POS_ERROR;
    DBUG_RETURN(-1);
  }

  buff=info->buff;
  do
  {
    if (!_nisam_fetch_keypage(info,keyinfo,pos,buff,0))
    {
      info->lastpos= NI_POS_ERROR;
      DBUG_RETURN(-1);
    }
    page= buff+getint(buff);
    nod_flag=test_if_nod(buff);
  } while ((pos=_nisam_kpos(nod_flag,page)) != NI_POS_ERROR);

  VOID(_nisam_get_last_key(info,keyinfo,buff,info->lastkey,page));
  info->lastpos=_nisam_dpos(info,nod_flag,page);
  info->int_keypos=info->int_maxpos=page;
  info->page_changed=info->buff_used=0;
  info->last_search_keypage=info->int_pos;

  DBUG_PRINT("exit",("found key at %d",info->lastpos));
  DBUG_RETURN(0);
} /* _nisam_search_last */
