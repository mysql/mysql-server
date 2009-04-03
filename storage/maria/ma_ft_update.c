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

/* functions to work with full-text indices */

#include "ma_ftdefs.h"
#include <math.h>

void _ma_ft_segiterator_init(MARIA_HA *info, uint keynr, const uchar *record,
			     FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_ma_ft_segiterator_init");

  ftsi->num=info->s->keyinfo[keynr].keysegs;
  ftsi->seg=info->s->keyinfo[keynr].seg;
  ftsi->rec=record;
  DBUG_VOID_RETURN;
}

void _ma_ft_segiterator_dummy_init(const uchar *record, uint len,
				   FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_ma_ft_segiterator_dummy_init");

  ftsi->num=1;
  ftsi->seg=0;
  ftsi->pos=record;
  ftsi->len=len;
  DBUG_VOID_RETURN;
}

/*
  This function breaks convention "return 0 in success"
  but it's easier to use like this

     while(_ma_ft_segiterator())

  so "1" means "OK", "0" means "EOF"
*/

uint _ma_ft_segiterator(register FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_ma_ft_segiterator");

  if (!ftsi->num)
    DBUG_RETURN(0);

  ftsi->num--;
  if (!ftsi->seg)
    DBUG_RETURN(1);

  ftsi->seg--;

  if (ftsi->seg->null_bit &&
      (ftsi->rec[ftsi->seg->null_pos] & ftsi->seg->null_bit))
  {
    ftsi->pos=0;
    DBUG_RETURN(1);
  }
  ftsi->pos= ftsi->rec+ftsi->seg->start;
  if (ftsi->seg->flag & HA_VAR_LENGTH_PART)
  {
    uint pack_length= (ftsi->seg->bit_start);
    ftsi->len= (pack_length == 1 ? (uint) * ftsi->pos :
                uint2korr(ftsi->pos));
    ftsi->pos+= pack_length;			 /* Skip VARCHAR length */
    DBUG_RETURN(1);
  }
  if (ftsi->seg->flag & HA_BLOB_PART)
  {
    ftsi->len= _ma_calc_blob_length(ftsi->seg->bit_start,ftsi->pos);
    memcpy_fixed((char*) &ftsi->pos, ftsi->pos+ftsi->seg->bit_start,
		 sizeof(char*));
    DBUG_RETURN(1);
  }
  ftsi->len=ftsi->seg->length;
  DBUG_RETURN(1);
}


/* parses a document i.e. calls maria_ft_parse for every keyseg */

uint _ma_ft_parse(TREE *parsed, MARIA_HA *info, uint keynr, const uchar *record,
                  MYSQL_FTPARSER_PARAM *param, MEM_ROOT *mem_root)
{
  FT_SEG_ITERATOR ftsi;
  struct st_mysql_ftparser *parser;
  DBUG_ENTER("_ma_ft_parse");

  _ma_ft_segiterator_init(info, keynr, record, &ftsi);

  maria_ft_parse_init(parsed, info->s->keyinfo[keynr].seg->charset);
  parser= info->s->keyinfo[keynr].parser;
  while (_ma_ft_segiterator(&ftsi))
  {
    /** @todo this casts ftsi.pos (const) to non-const */
    if (ftsi.pos)
      if (maria_ft_parse(parsed, (uchar *)ftsi.pos, ftsi.len, parser, param,
                         mem_root))
        DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

FT_WORD * _ma_ft_parserecord(MARIA_HA *info, uint keynr, const uchar *record,
                             MEM_ROOT *mem_root)
{
  TREE ptree;
  MYSQL_FTPARSER_PARAM *param;
  DBUG_ENTER("_ma_ft_parserecord");
  if (! (param= maria_ftparser_call_initializer(info, keynr, 0)))
    DBUG_RETURN(NULL);
  bzero((char*) &ptree, sizeof(ptree));
  param->flags= 0;
  if (_ma_ft_parse(&ptree, info, keynr, record, param, mem_root))
    DBUG_RETURN(NULL);

  DBUG_RETURN(maria_ft_linearize(&ptree, mem_root));
}

static int _ma_ft_store(MARIA_HA *info, uint keynr, uchar *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  DBUG_ENTER("_ma_ft_store");

  for (; wlist->pos; wlist++)
  {
    MARIA_KEY key;
    _ma_ft_make_key(info, &key, keynr, keybuf, wlist, filepos);
    if (_ma_ck_write(info, &key))
      DBUG_RETURN(1);
   }
   DBUG_RETURN(0);
}

static int _ma_ft_erase(MARIA_HA *info, uint keynr, uchar *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint err=0;
  DBUG_ENTER("_ma_ft_erase");

  for (; wlist->pos; wlist++)
  {
    MARIA_KEY key;
    _ma_ft_make_key(info, &key, keynr, keybuf, wlist, filepos);
    if (_ma_ck_delete(info, &key))
      err=1;
   }
   DBUG_RETURN(err);
}

/*
  Compares an appropriate parts of two WORD_KEY keys directly out of records
  returns 1 if they are different
*/

#define THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT 1
#define GEE_THEY_ARE_ABSOLUTELY_IDENTICAL	 0

int _ma_ft_cmp(MARIA_HA *info, uint keynr, const uchar *rec1, const uchar *rec2)
{
  FT_SEG_ITERATOR ftsi1, ftsi2;
  CHARSET_INFO *cs=info->s->keyinfo[keynr].seg->charset;
  DBUG_ENTER("_ma_ft_cmp");

  _ma_ft_segiterator_init(info, keynr, rec1, &ftsi1);
  _ma_ft_segiterator_init(info, keynr, rec2, &ftsi2);

  while (_ma_ft_segiterator(&ftsi1) && _ma_ft_segiterator(&ftsi2))
  {
    if ((ftsi1.pos != ftsi2.pos) &&
        (!ftsi1.pos || !ftsi2.pos ||
         ha_compare_text(cs, ftsi1.pos,ftsi1.len,
                         ftsi2.pos,ftsi2.len,0,0)))
      DBUG_RETURN(THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT);
  }
  DBUG_RETURN(GEE_THEY_ARE_ABSOLUTELY_IDENTICAL);
}


/* update a document entry */

int _ma_ft_update(MARIA_HA *info, uint keynr, uchar *keybuf,
                  const uchar *oldrec, const uchar *newrec, my_off_t pos)
{
  int error= -1;
  FT_WORD *oldlist,*newlist, *old_word, *new_word;
  CHARSET_INFO *cs=info->s->keyinfo[keynr].seg->charset;
  int cmp, cmp2;
  DBUG_ENTER("_ma_ft_update");

  if (!(old_word=oldlist=_ma_ft_parserecord(info, keynr, oldrec,
                                            &info->ft_memroot)) ||
      !(new_word=newlist=_ma_ft_parserecord(info, keynr, newrec,
                                            &info->ft_memroot)))
    goto err;

  error=0;
  while(old_word->pos && new_word->pos)
  {
    cmp= ha_compare_text(cs, (uchar*) old_word->pos,old_word->len,
                             (uchar*) new_word->pos,new_word->len,0,0);
    cmp2= cmp ? 0 : (fabs(old_word->weight - new_word->weight) > 1.e-5);

    if (cmp < 0 || cmp2)
    {
      MARIA_KEY key;
      _ma_ft_make_key(info, &key, keynr, keybuf, old_word, pos);
      if (_ma_ck_delete(info, &key))
      {
        error= -1;
        goto err;
      }
    }
    if (cmp > 0 || cmp2)
    {
      MARIA_KEY key;
      _ma_ft_make_key(info, &key, keynr, keybuf, new_word,pos);
      if ((error= _ma_ck_write(info, &key)))
        goto err;
    }
    if (cmp<=0) old_word++;
    if (cmp>=0) new_word++;
 }
 if (old_word->pos)
   error= _ma_ft_erase(info,keynr,keybuf,old_word,pos);
 else if (new_word->pos)
   error= _ma_ft_store(info,keynr,keybuf,new_word,pos);

err:
  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_RETURN(error);
}


/* adds a document to the collection */

int _ma_ft_add(MARIA_HA *info, uint keynr, uchar *keybuf, const uchar *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  DBUG_ENTER("_ma_ft_add");
  DBUG_PRINT("enter",("keynr: %d",keynr));

  if ((wlist= _ma_ft_parserecord(info, keynr, record, &info->ft_memroot)))
    error= _ma_ft_store(info,keynr,keybuf,wlist,pos);
  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
}


/* removes a document from the collection */

int _ma_ft_del(MARIA_HA *info, uint keynr, uchar *keybuf, const uchar *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  DBUG_ENTER("_ma_ft_del");
  DBUG_PRINT("enter",("keynr: %d",keynr));

  if ((wlist= _ma_ft_parserecord(info, keynr, record, &info->ft_memroot)))
    error= _ma_ft_erase(info,keynr,keybuf,wlist,pos);
  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
}


MARIA_KEY *_ma_ft_make_key(MARIA_HA *info, MARIA_KEY *key, uint keynr,
                           uchar *keybuf,
                           FT_WORD *wptr, my_off_t filepos)
{
  uchar buf[HA_FT_MAXBYTELEN+16];
  DBUG_ENTER("_ma_ft_make_key");

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  {
    float weight=(float) ((filepos==HA_OFFSET_ERROR) ? 0 : wptr->weight);
    mi_float4store(buf,weight);
  }
#else
#error
#endif

  int2store(buf+HA_FT_WLEN,wptr->len);
  memcpy(buf+HA_FT_WLEN+2,wptr->pos,wptr->len);
  /* Can't be spatial so it's ok to call _ma_make_key directly here */
  DBUG_RETURN(_ma_make_key(info, key, keynr, keybuf, buf, filepos, 0));
}


/*
  convert key value to ft2
*/

my_bool _ma_ft_convert_to_ft2(MARIA_HA *info, MARIA_KEY *key)
{
  MARIA_SHARE *share= info->s;
  my_off_t root;
  DYNAMIC_ARRAY *da=info->ft1_to_ft2;
  MARIA_KEYDEF *keyinfo=&share->ft2_keyinfo;
  uchar *key_ptr= (uchar*) dynamic_array_ptr(da, 0), *end;
  uint length, key_length;
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  MARIA_KEY tmp_key;
  MARIA_PAGE page;
  DBUG_ENTER("_ma_ft_convert_to_ft2");

  /* we'll generate one pageful at once, and insert the rest one-by-one */
  /* calculating the length of this page ...*/
  length=(keyinfo->block_length-2) / keyinfo->keylength;
  set_if_smaller(length, da->elements);
  length=length * keyinfo->keylength;

  get_key_full_length_rdonly(key_length, key->data);
  while (_ma_ck_delete(info, key) == 0)
  {
    /*
      nothing to do here.
      _ma_ck_delete() will populate info->ft1_to_ft2 with deleted keys
    */
  }

  /* creating pageful of keys */
  bzero(info->buff, share->keypage_header);
  _ma_store_keynr(share, info->buff, keyinfo->key_nr);
  _ma_store_page_used(share, info->buff, length + share->keypage_header);
  memcpy(info->buff + share->keypage_header, key_ptr, length);
  info->keyread_buff_used= info->page_changed=1;      /* info->buff is used */
  /**
    @todo RECOVERY BUG this is not logged yet. Ok as this code is never
    called, but soon it will be.
  */
  if ((root= _ma_new(info, DFLT_INIT_HITS, &page_link)) == HA_OFFSET_ERROR)
    DBUG_RETURN(1);

  _ma_page_setup(&page, info, keyinfo, root, info->buff);
  if (_ma_write_keypage(&page, page_link->write_lock, DFLT_INIT_HITS))
    DBUG_RETURN(1);

  /* inserting the rest of key values */
  end= (uchar*) dynamic_array_ptr(da, da->elements);
  tmp_key.keyinfo= keyinfo;
  tmp_key.data_length= keyinfo->keylength;
  tmp_key.ref_length= 0;
  tmp_key.flag= 0;
  for (key_ptr+=length; key_ptr < end; key_ptr+=keyinfo->keylength)
  {
    tmp_key.data= key_ptr;
    if (_ma_ck_real_write_btree(info, key, &root, SEARCH_SAME))
      DBUG_RETURN(1);
  }

  /* now, writing the word key entry */
  ft_intXstore(key->data + key_length, - (int) da->elements);
  _ma_dpointer(share, key->data + key_length + HA_FT_WLEN, root);

  DBUG_RETURN(_ma_ck_real_write_btree(info, key,
                                      &share->state.key_root[key->keyinfo->
                                                             key_nr],
                                      SEARCH_SAME));
}
