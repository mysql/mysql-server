/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* functions to work with full-text indices */

#include "ftdefs.h"
#include <math.h>

void _mi_ft_segiterator_init(MI_INFO *info, uint keynr, const uchar *record,
			     FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_mi_ft_segiterator_init");

  ftsi->num=info->s->keyinfo[keynr].keysegs;
  ftsi->seg=info->s->keyinfo[keynr].seg;
  ftsi->rec=record;
  DBUG_VOID_RETURN;
}

void _mi_ft_segiterator_dummy_init(const uchar *record, uint len,
				   FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_mi_ft_segiterator_dummy_init");

  ftsi->num=1;
  ftsi->seg=0;
  ftsi->pos=record;
  ftsi->len=len;
  DBUG_VOID_RETURN;
}

/*
  This function breaks convention "return 0 in success"
  but it's easier to use like this

     while(_mi_ft_segiterator())

  so "1" means "OK", "0" means "EOF"
*/

uint _mi_ft_segiterator(register FT_SEG_ITERATOR *ftsi)
{
  DBUG_ENTER("_mi_ft_segiterator");

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
    ftsi->len= (pack_length == 1 ? (uint) *(uchar*) ftsi->pos :
                uint2korr(ftsi->pos));
    ftsi->pos+= pack_length;			 /* Skip VARCHAR length */
    DBUG_RETURN(1);
  }
  if (ftsi->seg->flag & HA_BLOB_PART)
  {
    ftsi->len=_mi_calc_blob_length(ftsi->seg->bit_start,ftsi->pos);
    memcpy(&ftsi->pos, ftsi->pos+ftsi->seg->bit_start, sizeof(char*));
    DBUG_RETURN(1);
  }
  ftsi->len=ftsi->seg->length;
  DBUG_RETURN(1);
}


/* parses a document i.e. calls ft_parse for every keyseg */

uint _mi_ft_parse(TREE *parsed, MI_INFO *info, uint keynr, const uchar *record,
                  MYSQL_FTPARSER_PARAM *param, MEM_ROOT *mem_root)
{
  FT_SEG_ITERATOR ftsi;
  struct st_mysql_ftparser *parser;
  DBUG_ENTER("_mi_ft_parse");

  _mi_ft_segiterator_init(info, keynr, record, &ftsi);

  ft_parse_init(parsed, info->s->keyinfo[keynr].seg->charset);
  parser= info->s->keyinfo[keynr].parser;
  while (_mi_ft_segiterator(&ftsi))
  {
    if (ftsi.pos)
      if (ft_parse(parsed, (uchar *)ftsi.pos, ftsi.len, parser, param, mem_root))
        DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

FT_WORD *_mi_ft_parserecord(MI_INFO *info, uint keynr, const uchar *record,
                             MEM_ROOT *mem_root)
{
  TREE ptree;
  MYSQL_FTPARSER_PARAM *param;
  DBUG_ENTER("_mi_ft_parserecord");
  if (! (param= ftparser_call_initializer(info, keynr, 0)))
    DBUG_RETURN(NULL);
  memset(&ptree, 0, sizeof(ptree));
  param->flags= 0;
  if (_mi_ft_parse(&ptree, info, keynr, record, param, mem_root))
    DBUG_RETURN(NULL);

  DBUG_RETURN(ft_linearize(&ptree, mem_root));
}

static int _mi_ft_store(MI_INFO *info, uint keynr, uchar *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint key_length;
  DBUG_ENTER("_mi_ft_store");

  for (; wlist->pos; wlist++)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_write(info,keynr,(uchar*) keybuf,key_length))
      DBUG_RETURN(1);
   }
   DBUG_RETURN(0);
}

static int _mi_ft_erase(MI_INFO *info, uint keynr, uchar *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint key_length, err=0;
  DBUG_ENTER("_mi_ft_erase");

  for (; wlist->pos; wlist++)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length))
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

int _mi_ft_cmp(MI_INFO *info, uint keynr, const uchar *rec1, const uchar *rec2)
{
  FT_SEG_ITERATOR ftsi1, ftsi2;
  const CHARSET_INFO *cs= info->s->keyinfo[keynr].seg->charset;
  DBUG_ENTER("_mi_ft_cmp");
  _mi_ft_segiterator_init(info, keynr, rec1, &ftsi1);
  _mi_ft_segiterator_init(info, keynr, rec2, &ftsi2);

  while (_mi_ft_segiterator(&ftsi1) && _mi_ft_segiterator(&ftsi2))
  {
    if ((ftsi1.pos != ftsi2.pos) &&
        (!ftsi1.pos || !ftsi2.pos ||
         ha_compare_text(cs, (uchar*) ftsi1.pos,ftsi1.len,
                         (uchar*) ftsi2.pos,ftsi2.len,0,0)))
      DBUG_RETURN(THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT);
  }
  DBUG_RETURN(GEE_THEY_ARE_ABSOLUTELY_IDENTICAL);
}


/* update a document entry */

int _mi_ft_update(MI_INFO *info, uint keynr, uchar *keybuf,
                  const uchar *oldrec, const uchar *newrec, my_off_t pos)
{
  int error= -1;
  FT_WORD *oldlist,*newlist, *old_word, *new_word;
  const CHARSET_INFO *cs= info->s->keyinfo[keynr].seg->charset;
  uint key_length;
  int cmp, cmp2;
  DBUG_ENTER("_mi_ft_update");

  if (!(old_word=oldlist=_mi_ft_parserecord(info, keynr, oldrec,
                                            &info->ft_memroot)) ||
      !(new_word=newlist=_mi_ft_parserecord(info, keynr, newrec,
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
      key_length=_ft_make_key(info,keynr,keybuf,old_word,pos);
      if ((error=_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length)))
        goto err;
    }
    if (cmp > 0 || cmp2)
    {
      key_length=_ft_make_key(info,keynr,keybuf,new_word,pos);
      if ((error=_mi_ck_write(info,keynr,(uchar*) keybuf,key_length)))
        goto err;
    }
    if (cmp<=0) old_word++;
    if (cmp>=0) new_word++;
 }
 if (old_word->pos)
   error=_mi_ft_erase(info,keynr,keybuf,old_word,pos);
 else if (new_word->pos)
   error=_mi_ft_store(info,keynr,keybuf,new_word,pos);

err:
  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_RETURN(error);
}


/* adds a document to the collection */

int _mi_ft_add(MI_INFO *info, uint keynr, uchar *keybuf, const uchar *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  DBUG_ENTER("_mi_ft_add");
  DBUG_PRINT("enter",("keynr: %d",keynr));

  if ((wlist=_mi_ft_parserecord(info, keynr, record, &info->ft_memroot)))
    error=_mi_ft_store(info,keynr,keybuf,wlist,pos);

  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
}


/* removes a document from the collection */

int _mi_ft_del(MI_INFO *info, uint keynr, uchar *keybuf, const uchar *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  DBUG_ENTER("_mi_ft_del");
  DBUG_PRINT("enter",("keynr: %d",keynr));

  if ((wlist=_mi_ft_parserecord(info, keynr, record, &info->ft_memroot)))
    error=_mi_ft_erase(info,keynr,keybuf,wlist,pos);

  free_root(&info->ft_memroot, MYF(MY_MARK_BLOCKS_FREE));
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
}

uint _ft_make_key(MI_INFO *info, uint keynr, uchar *keybuf, FT_WORD *wptr,
		  my_off_t filepos)
{
  uchar buf[HA_FT_MAXBYTELEN+16];
  DBUG_ENTER("_ft_make_key");

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
  DBUG_RETURN(_mi_make_key(info,keynr,(uchar*) keybuf,buf,filepos));
}


/*
  convert key value to ft2
*/

uint _mi_ft_convert_to_ft2(MI_INFO *info, uint keynr, uchar *key)
{
  my_off_t root;
  DYNAMIC_ARRAY *da=info->ft1_to_ft2;
  MI_KEYDEF *keyinfo=&info->s->ft2_keyinfo;
  uchar *key_ptr= (uchar*) dynamic_array_ptr(da, 0), *end;
  uint length, key_length;
  DBUG_ENTER("_mi_ft_convert_to_ft2");

  /* we'll generate one pageful at once, and insert the rest one-by-one */
  /* calculating the length of this page ...*/
  length=(keyinfo->block_length-2) / keyinfo->keylength;
  set_if_smaller(length, da->elements);
  length=length * keyinfo->keylength;

  get_key_full_length_rdonly(key_length, key);
  while (_mi_ck_delete(info, keynr, key, key_length) == 0)
  {
    /*
      nothing to do here.
      _mi_ck_delete() will populate info->ft1_to_ft2 with deleted keys
     */
  }

  /* creating pageful of keys */
  mi_putint(info->buff,length+2,0);
  memcpy(info->buff+2, key_ptr, length);
  info->buff_used=info->page_changed=1;           /* info->buff is used */
  if ((root= _mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR ||
      _mi_write_keypage(info,keyinfo,root,DFLT_INIT_HITS,info->buff))
    DBUG_RETURN(-1);

  /* inserting the rest of key values */
  end= (uchar*) dynamic_array_ptr(da, da->elements);
  for (key_ptr+=length; key_ptr < end; key_ptr+=keyinfo->keylength)
    if(_mi_ck_real_write_btree(info, keyinfo, key_ptr, 0, &root, SEARCH_SAME))
      DBUG_RETURN(-1);

  /* now, writing the word key entry */
  ft_intXstore(key+key_length, - (int) da->elements);
  _mi_dpointer(info, key+key_length+HA_FT_WLEN, root);

  DBUG_RETURN(_mi_ck_real_write_btree(info,
                                     info->s->keyinfo+keynr,
                                     key, 0,
                                     &info->s->state.key_root[keynr],
                                     SEARCH_SAME));
}

