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

/* functions to work with full-text indices */

#include "ftdefs.h"
#include <math.h>

/**************************************************************
   This is to make ft-code to ignore keyseg.length at all     *
   and to index the whole VARCHAR/BLOB instead...             */
#undef set_if_smaller
#define set_if_smaller(A,B)                          /* no op */
/**************************************************************/

void _mi_ft_segiterator_init(MI_INFO *info, uint keynr, const byte *record,
			     FT_SEG_ITERATOR *ftsi)
{
  ftsi->num=info->s->keyinfo[keynr].keysegs-FT_SEGS;
  ftsi->seg=info->s->keyinfo[keynr].seg;
  ftsi->rec=record;
}

void _mi_ft_segiterator_dummy_init(const byte *record, uint len,
				   FT_SEG_ITERATOR *ftsi)
{
  ftsi->num=1;
  ftsi->seg=0;
  ftsi->pos=record;
  ftsi->len=len;
}

/*
  This function breaks convention "return 0 in success"
  but it's easier to use like this

     while(_mi_ft_segiterator())

  so "1" means "OK", "0" means "EOF"
*/

uint _mi_ft_segiterator(register FT_SEG_ITERATOR *ftsi)
{
  if (!ftsi->num) return 0; else ftsi->num--;
  if (!ftsi->seg) return 1; else ftsi->seg--;

  if (ftsi->seg->null_bit &&
      (ftsi->rec[ftsi->seg->null_pos] & ftsi->seg->null_bit))
  {
      ftsi->pos=0;
      return 1;
  }
  ftsi->pos= ftsi->rec+ftsi->seg->start;
  if (ftsi->seg->flag & HA_VAR_LENGTH)
  {
    ftsi->len=uint2korr(ftsi->pos);
    ftsi->pos+=2;				 /* Skip VARCHAR length */
    set_if_smaller(ftsi->len,ftsi->seg->length);
    return 1;
  }
  if (ftsi->seg->flag & HA_BLOB_PART)
  {
    ftsi->len=_mi_calc_blob_length(ftsi->seg->bit_start,ftsi->pos);
    memcpy_fixed((char*) &ftsi->pos, ftsi->pos+ftsi->seg->bit_start,
		 sizeof(char*));
    set_if_smaller(ftsi->len,ftsi->seg->length);
    return 1;
  }
  ftsi->len=ftsi->seg->length;
  return 1;
}


/* parses a document i.e. calls ft_parse for every keyseg */

uint _mi_ft_parse(TREE *parsed, MI_INFO *info, uint keynr, const byte *record)
{
  FT_SEG_ITERATOR ftsi;
  _mi_ft_segiterator_init(info, keynr, record, &ftsi);

  ft_parse_init(parsed, info->s->keyinfo[keynr].seg->charset);
  while (_mi_ft_segiterator(&ftsi))
  {
    if (ftsi.pos)
      if (ft_parse(parsed, (byte *)ftsi.pos, ftsi.len))
        return 1;
  }
  return 0;
}

FT_WORD * _mi_ft_parserecord(MI_INFO *info, uint keynr,
			     byte *keybuf __attribute__((unused)),
			     const byte *record)
{
  TREE ptree;

  bzero((char*) &ptree, sizeof(ptree));
  if (_mi_ft_parse(&ptree, info, keynr, record))
    return NULL;

  return ft_linearize(/*info, keynr, keybuf, */ &ptree);
}

static int _mi_ft_store(MI_INFO *info, uint keynr, byte *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint key_length;

  for (; wlist->pos; wlist++)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_write(info,keynr,(uchar*) keybuf,key_length))
      return 1;
   }
   return 0;
}

static int _mi_ft_erase(MI_INFO *info, uint keynr, byte *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint key_length, err=0;

  for (; wlist->pos; wlist++)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length))
      err=1;
   }
   return err;
}

/*
  Compares an appropriate parts of two WORD_KEY keys directly out of records
  returns 1 if they are different
*/

#define THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT 1
#define GEE_THEY_ARE_ABSOLUTELY_IDENTICAL	 0

int _mi_ft_cmp(MI_INFO *info, uint keynr, const byte *rec1, const byte *rec2)
{
  FT_SEG_ITERATOR ftsi1, ftsi2;
  CHARSET_INFO *cs=info->s->keyinfo[keynr].seg->charset;
  _mi_ft_segiterator_init(info, keynr, rec1, &ftsi1);
  _mi_ft_segiterator_init(info, keynr, rec2, &ftsi2);

  while (_mi_ft_segiterator(&ftsi1) && _mi_ft_segiterator(&ftsi2))
  {
    if ((ftsi1.pos != ftsi2.pos) &&
        (!ftsi1.pos || !ftsi2.pos ||
          _mi_compare_text(cs, (uchar*) ftsi1.pos,ftsi1.len,
                               (uchar*) ftsi2.pos,ftsi2.len,0)))
      return THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT;
  }
  return GEE_THEY_ARE_ABSOLUTELY_IDENTICAL;
}


/* update a document entry */

int _mi_ft_update(MI_INFO *info, uint keynr, byte *keybuf,
                  const byte *oldrec, const byte *newrec, my_off_t pos)
{
  int error= -1;
  FT_WORD *oldlist,*newlist, *old_word, *new_word;
  CHARSET_INFO *cs=info->s->keyinfo[keynr].seg->charset;
  uint key_length;
  int cmp, cmp2;

  if (!(old_word=oldlist=_mi_ft_parserecord(info, keynr, keybuf, oldrec)))
    goto err0;
  if (!(new_word=newlist=_mi_ft_parserecord(info, keynr, keybuf, newrec)))
    goto err1;

  error=0;
  while(old_word->pos && new_word->pos)
  {
    cmp=_mi_compare_text(cs, (uchar*) old_word->pos,old_word->len,
                             (uchar*) new_word->pos,new_word->len,0);
    cmp2= cmp ? 0 : (fabs(old_word->weight - new_word->weight) > 1.e-5);

    if (cmp < 0 || cmp2)
    {
      key_length=_ft_make_key(info,keynr,keybuf,old_word,pos);
      if ((error=_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length)))
        goto err2;
    }
    if (cmp > 0 || cmp2)
    {
      key_length=_ft_make_key(info,keynr,keybuf,new_word,pos);
      if ((error=_mi_ck_write(info,keynr,(uchar*) keybuf,key_length)))
        goto err2;
    }
    if (cmp<=0) old_word++;
    if (cmp>=0) new_word++;
 }
 if (old_word->pos)
   error=_mi_ft_erase(info,keynr,keybuf,old_word,pos);
 else if (new_word->pos)
   error=_mi_ft_store(info,keynr,keybuf,new_word,pos);

err2:
    my_free((char*) newlist,MYF(0));
err1:
    my_free((char*) oldlist,MYF(0));
err0:
  return error;
}


/* adds a document to the collection */

int _mi_ft_add(MI_INFO *info, uint keynr, byte *keybuf, const byte *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;

  if ((wlist=_mi_ft_parserecord(info, keynr, keybuf, record)))
  {
    error=_mi_ft_store(info,keynr,keybuf,wlist,pos);
    my_free((char*) wlist,MYF(0));
  }
  return error;
}


/* removes a document from the collection */

int _mi_ft_del(MI_INFO *info, uint keynr, byte *keybuf, const byte *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  if ((wlist=_mi_ft_parserecord(info, keynr, keybuf, record)))
  {
    error=_mi_ft_erase(info,keynr,keybuf,wlist,pos);
    my_free((char*) wlist,MYF(0));
  }
  return error;
}

uint _ft_make_key(MI_INFO *info, uint keynr, byte *keybuf, FT_WORD *wptr,
		  my_off_t filepos)
{
  byte buf[HA_FT_MAXLEN+16];

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float weight=(float) ((filepos==HA_OFFSET_ERROR) ? 0 : wptr->weight);
  mi_float4store(buf,weight);
#else
#error
#endif

#ifdef EVAL_RUN
  *(buf+HA_FT_WLEN)=wptr->cnt;
  int2store(buf+HA_FT_WLEN+1,wptr->len);
  memcpy(buf+HA_FT_WLEN+3,wptr->pos,wptr->len);
#else /* EVAL_RUN */
  int2store(buf+HA_FT_WLEN,wptr->len);
  memcpy(buf+HA_FT_WLEN+2,wptr->pos,wptr->len);
#endif /* EVAL_RUN */
  return _mi_make_key(info,keynr,(uchar*) keybuf,buf,filepos);
}
