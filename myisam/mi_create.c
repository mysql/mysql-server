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

/* Create a MyISAM table */

#include "fulltext.h"
#if defined(MSDOS) || defined(__WIN__)
#ifdef __WIN__
#include <fcntl.h>
#else
#include <process.h>			/* Prototype for getpid */
#endif
#endif
#include <m_ctype.h>

	/*
	** Old options is used when recreating database, from isamchk
	*/

int mi_create(const char *name,uint keys,MI_KEYDEF *keydefs,
	      uint columns, MI_COLUMNDEF *recinfo,
	      uint uniques, MI_UNIQUEDEF *uniquedefs,
	      MI_CREATE_INFO *ci,uint flags)
{
  register uint i,j;
  File dfile,file;
  int errpos,save_errno;
  uint fields,length,max_key_length,packed,pointer,
       key_length,info_length,key_segs,options,min_key_length_skipp,
       base_pos,varchar_count,long_varchar_count,varchar_length,
       max_key_block_length,unique_key_parts,offset;
  ulong reclength, real_reclength,min_pack_length;
  char buff[max(FN_REFLEN,2048)];
  ulong pack_reclength;
  ulonglong tot_length,max_rows;
  enum en_fieldtype type;
  MYISAM_SHARE share;
  MI_KEYDEF *keydef,tmp_keydef;
  MI_UNIQUEDEF *uniquedef;
  MI_KEYSEG *keyseg,tmp_keyseg;
  MI_COLUMNDEF *rec;
  ulong rec_per_key_part[MI_MAX_POSSIBLE_KEY*MI_MAX_KEY_SEG];
  my_off_t key_root[MI_MAX_POSSIBLE_KEY],key_del[MI_MAX_KEY_BLOCK_SIZE];
  MI_CREATE_INFO tmp_create_info;
  DBUG_ENTER("mi_create");

  if (!ci)
  {
    bzero((char*) &tmp_create_info,sizeof(tmp_create_info));
    ci=&tmp_create_info;
  }

  if (keys + uniques > MI_MAX_KEY)
  {
    DBUG_RETURN(my_errno=HA_WRONG_CREATE_OPTION);
  }
  LINT_INIT(dfile);
  LINT_INIT(file);
  pthread_mutex_lock(&THR_LOCK_myisam);
  errpos=0;
  options=0;
  bzero((byte*) &share,sizeof(share));

  if (flags & HA_DONT_TOUCH_DATA)
  {
    if (!(ci->old_options & HA_OPTION_TEMP_COMPRESS_RECORD))
      options=ci->old_options &
	(HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD |
	 HA_OPTION_READ_ONLY_DATA | HA_OPTION_CHECKSUM |
	 HA_OPTION_TMP_TABLE | HA_OPTION_DELAY_KEY_WRITE);
    else
      options=ci->old_options &
	(HA_OPTION_CHECKSUM | HA_OPTION_TMP_TABLE | HA_OPTION_DELAY_KEY_WRITE);
  }

  if (ci->reloc_rows > ci->max_rows)
    ci->reloc_rows=ci->max_rows;			/* Check if wrong parameter */

	/* Start by checking fields and field-types used */

  reclength=varchar_count=varchar_length=long_varchar_count=packed=
    min_pack_length=pack_reclength=0;
  for (rec=recinfo, fields=0 ;
       fields != columns ;
       rec++,fields++)
  {
    reclength+=rec->length;
    if ((type=(enum en_fieldtype) rec->type) != FIELD_NORMAL &&
	type != FIELD_CHECK)
    {
      packed++;
      if (type == FIELD_BLOB)
      {
	share.base.blobs++;
	if (pack_reclength != INT_MAX32)
	{
	  if (rec->length == 4+mi_portable_sizeof_char_ptr)
	    pack_reclength= INT_MAX32;
	  else
	    pack_reclength+=(1 << ((rec->length-mi_portable_sizeof_char_ptr)*8)); /* Max blob length */
	}
      }
      else if (type == FIELD_SKIPP_PRESPACE ||
	       type == FIELD_SKIPP_ENDSPACE)
      {
	if (pack_reclength != INT_MAX32)
	  pack_reclength+= rec->length > 255 ? 2 : 1;
	min_pack_length++;
      }
      else if (type == FIELD_VARCHAR)
      {
	varchar_count++;
	varchar_length+=rec->length-2;
	packed--;
	pack_reclength+=1;
	if (test(rec->length > 257))
	{					/* May be packed on 3 bytes */
	  long_varchar_count++;
	  pack_reclength+=2;
	}
      }
      else if (type != FIELD_SKIPP_ZERO)
      {
	min_pack_length+=rec->length;
	packed--;				/* Not a pack record type */
      }
    }
    else					/* FIELD_NORMAL */
      min_pack_length+=rec->length;
  }
  if ((packed & 7) == 1)
  {				/* Bad packing, try to remove a zero-field */
    while (rec != recinfo)
    {
      rec--;
      if (rec->type == (int) FIELD_SKIPP_ZERO && rec->length == 1)
      {
	rec->type=(int) FIELD_NORMAL;
	packed--;
	min_pack_length++;
	break;
      }
    }
  }

  if (packed || (flags & HA_PACK_RECORD))
    options|=HA_OPTION_PACK_RECORD;	/* Must use packed records */
  if (options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD))
    min_pack_length+=varchar_count;		/* Min length to pack */
  else
  {
    min_pack_length+=varchar_length+2*varchar_count;
  }
  if (flags & HA_CREATE_TMP_TABLE)
    options|= HA_OPTION_TMP_TABLE;
  if (flags & HA_CREATE_CHECKSUM || (options & HA_OPTION_CHECKSUM))
  {
    options|= HA_OPTION_CHECKSUM;
    min_pack_length++;
  }
  if (flags & HA_CREATE_DELAY_KEY_WRITE)
    options|= HA_OPTION_DELAY_KEY_WRITE;

  packed=(packed+7)/8;
  if (pack_reclength != INT_MAX32)
    pack_reclength+= reclength+packed +
      test(test_all_bits(options, HA_OPTION_CHECKSUM | HA_PACK_RECORD));
  min_pack_length+=packed;

  if (!ci->data_file_length)
  {
    if (ci->max_rows == 0 || pack_reclength == INT_MAX32)
      ci->data_file_length= INT_MAX32-1;		/* Should be enough */
    else if ((~(ulonglong) 0)/ci->max_rows < (ulonglong) pack_reclength)
      ci->data_file_length= ~(ulonglong) 0;
    else
      ci->data_file_length=(ulonglong) ci->max_rows*pack_reclength;
  }
  else if (!ci->max_rows)
    ci->max_rows=(ha_rows) (ci->data_file_length/(min_pack_length +
					 ((options & HA_OPTION_PACK_RECORD) ?
					  3 : 0)));

  if (options & (HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD))
    pointer=mi_get_pointer_length(ci->data_file_length,4);
  else
    pointer=mi_get_pointer_length(ci->max_rows,4);
  if (!(max_rows=(ulonglong) ci->max_rows))
    max_rows= ((((ulonglong) 1 << (pointer*8)) -1) / min_pack_length);


  real_reclength=reclength;
  if (!(options & (HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD)))
  {
    if (reclength <= pointer)
      reclength=pointer+1;		/* reserve place for delete link */
  }
  else
    reclength+=long_varchar_count;	/* We need space for this! */

  max_key_length=0; tot_length=0 ; key_segs=0;
  max_key_block_length=0;
  share.state.rec_per_key_part=rec_per_key_part;
  bzero((char*) rec_per_key_part,sizeof(rec_per_key_part));
  share.state.key_root=key_root;
  share.state.key_del=key_del;
  if (uniques)
  {
    max_key_block_length= MI_KEY_BLOCK_LENGTH;
    max_key_length= MI_UNIQUE_HASH_LENGTH;
  }

  for (i=0, keydef=keydefs ; i < keys ; i++ , keydef++)
  {
    share.state.key_root[i]= HA_OFFSET_ERROR;

    min_key_length_skipp=length=0;
    key_length=pointer;

    if (keydef->flag & HA_FULLTEXT)                                 /* SerG */
    {
      keydef->flag=HA_FULLTEXT | HA_PACK_KEY | HA_VAR_LENGTH_KEY;
      options|=HA_OPTION_PACK_KEYS;             /* Using packed keys */

      if (flags & HA_DONT_TOUCH_DATA)
      {
        /* called by myisamchk - i.e. table structure was taken from
           MYI file and FULLTEXT key *do has* additional FT_SEGS keysegs.
           We'd better delete them now
        */
        keydef->keysegs-=FT_SEGS;
      }

      for (j=0, keyseg=keydef->seg ; (int) j < keydef->keysegs ;
	   j++, keyseg++)
      {
        if (keyseg->type != HA_KEYTYPE_TEXT &&
	    keyseg->type != HA_KEYTYPE_VARTEXT)
        {
          my_errno=HA_WRONG_CREATE_OPTION;
          goto err;
        }
      }
      keydef->keysegs+=FT_SEGS;

      key_length+= HA_FT_MAXLEN+HA_FT_WLEN;
#ifdef EVAL_RUN
      key_length++;
#endif

      length++;                              /* At least one length byte */
      min_key_length_skipp+=HA_FT_MAXLEN;
#if HA_FT_MAXLEN >= 255
      min_key_length_skipp+=2;                  /* prefix may be 3 bytes */
      length+=2;
#endif
    }
    else
    {
    /* Test if prefix compression */
    if (keydef->flag & HA_PACK_KEY)
    {
      /* Can't use space_compression on number keys */
      if ((keydef->seg[0].flag & HA_SPACE_PACK) &&
	  keydef->seg[0].type == (int) HA_KEYTYPE_NUM)
	keydef->seg[0].flag&= ~HA_SPACE_PACK;

      /* Only use HA_PACK_KEY if the first segment is a variable length key */
      if (!(keydef->seg[0].flag & (HA_SPACE_PACK | HA_BLOB_PART |
				   HA_VAR_LENGTH)))
      {
	/* pack relative to previous key */
	keydef->flag&= ~HA_PACK_KEY;
	keydef->flag|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
      }
      else
      {
	keydef->seg[0].flag|=HA_PACK_KEY;	/* for easyer intern test */
	keydef->flag|=HA_VAR_LENGTH_KEY;
	options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
      }
    }
    if (keydef->flag & HA_BINARY_PACK_KEY)
      options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */

    if (keydef->flag & HA_AUTO_KEY)
      share.base.auto_key=i+1;
    for (j=0, keyseg=keydef->seg ; j < keydef->keysegs ; j++, keyseg++)
    {
      /* numbers are stored with high by first to make compression easier */
      switch (keyseg->type) {
      case HA_KEYTYPE_SHORT_INT:
      case HA_KEYTYPE_LONG_INT:
      case HA_KEYTYPE_FLOAT:
      case HA_KEYTYPE_DOUBLE:
      case HA_KEYTYPE_USHORT_INT:
      case HA_KEYTYPE_ULONG_INT:
      case HA_KEYTYPE_LONGLONG:
      case HA_KEYTYPE_ULONGLONG:
      case HA_KEYTYPE_INT24:
      case HA_KEYTYPE_UINT24:
      case HA_KEYTYPE_INT8:
	keyseg->flag|= HA_SWAP_KEY;
	/* fall through */
      default:
	break;
      }
      if (keyseg->flag & HA_SPACE_PACK)
      {
	keydef->flag |= HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY;
	options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	length++;				/* At least one length byte */
	min_key_length_skipp+=keyseg->length;
	if (keyseg->length >= 255)
	{					/* prefix may be 3 bytes */
	  min_key_length_skipp+=2;
	  length+=2;
	}
      }
      if (keyseg->flag & (HA_VAR_LENGTH | HA_BLOB_PART))
      {
	keydef->flag|=HA_VAR_LENGTH_KEY;
	length++;				/* At least one length byte */
	options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	min_key_length_skipp+=keyseg->length;
	if (keyseg->length >= 255)
	{					/* prefix may be 3 bytes */
	  min_key_length_skipp+=2;
	  length+=2;
	}
      }
      key_length+= keyseg->length;
      if (keyseg->null_bit)
      {
	key_length++;
	options|=HA_OPTION_PACK_KEYS;
	keyseg->flag|=HA_NULL_PART;
	keydef->flag|=HA_VAR_LENGTH_KEY | HA_NULL_PART_KEY;
        }
      }
    } /* if HA_FULLTEXT */
    key_segs+=keydef->keysegs;
    if (keydef->keysegs > MI_MAX_KEY_SEG)
    {
      my_errno=HA_WRONG_CREATE_OPTION;
      goto err;
    }
    if ((keydef->flag & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
      share.state.rec_per_key_part[key_segs-1]=1L;
    length+=key_length;
    keydef->block_length= MI_BLOCK_SIZE(length,pointer,MI_MAX_KEYPTR_SIZE);
    if (keydef->block_length/MI_KEY_BLOCK_LENGTH > MI_MAX_KEY_BLOCK_SIZE)
    {
      my_errno=HA_WRONG_CREATE_OPTION;
      goto err;
    }
    set_if_bigger(max_key_block_length,keydef->block_length);
    keydef->keylength= (uint16) key_length;
    keydef->minlength= (uint16) (length-min_key_length_skipp);
    keydef->maxlength= (uint16) length;

    if (length > max_key_length)
      max_key_length= length;
    tot_length+= (max_rows/(ulong) (((uint) keydef->block_length-5)/
				    (length*2)))*
      (ulong) keydef->block_length;
  }
  for (i=max_key_block_length/MI_KEY_BLOCK_LENGTH ; i-- ; )
    key_del[i]=HA_OFFSET_ERROR;

  unique_key_parts=0;
  offset=reclength-uniques*MI_UNIQUE_HASH_LENGTH;
  for (i=0, uniquedef=uniquedefs ; i < uniques ; i++ , uniquedef++)
  {
    uniquedef->key=keys+i;
    unique_key_parts+=uniquedef->keysegs;
    share.state.key_root[keys+i]= HA_OFFSET_ERROR;
  }
  keys+=uniques;				/* Each unique has 1 key */
  key_segs+=uniques;				/* Each unique has 1 key seg */

  base_pos=(MI_STATE_INFO_SIZE + keys * MI_STATE_KEY_SIZE +
	    max_key_block_length/MI_KEY_BLOCK_LENGTH*MI_STATE_KEYBLOCK_SIZE+
	    key_segs*MI_STATE_KEYSEG_SIZE);
  info_length=base_pos+(uint) (MI_BASE_INFO_SIZE+
			       keys * MI_KEYDEF_SIZE+
			       uniques * MI_UNIQUEDEF_SIZE +
			       (key_segs + unique_key_parts)*MI_KEYSEG_SIZE+
			       columns*MI_COLUMNDEF_SIZE);

  bmove(share.state.header.file_version,(byte*) myisam_file_magic,4);
  ci->old_options=options| (ci->old_options & HA_OPTION_TEMP_COMPRESS_RECORD ?
			HA_OPTION_COMPRESS_RECORD |
			HA_OPTION_TEMP_COMPRESS_RECORD: 0);
  mi_int2store(share.state.header.options,ci->old_options);
  mi_int2store(share.state.header.header_length,info_length);
  mi_int2store(share.state.header.state_info_length,MI_STATE_INFO_SIZE);
  mi_int2store(share.state.header.base_info_length,MI_BASE_INFO_SIZE);
  mi_int2store(share.state.header.base_pos,base_pos);
  share.state.header.language= (ci->language ?
				ci->language : MY_CHARSET_CURRENT);
  share.state.header.max_block_size=max_key_block_length/MI_KEY_BLOCK_LENGTH;

  share.state.dellink = HA_OFFSET_ERROR;
  share.state.process=	(ulong) getpid();
  share.state.unique=	(ulong) 0;
  share.state.version=	(ulong) time((time_t*) 0);
  share.state.sortkey=  (ushort) ~0;
  share.state.auto_increment=ci->auto_increment;
  share.options=options;
  share.base.rec_reflength=pointer;
  share.base.key_reflength=
    mi_get_pointer_length((tot_length + max_key_block_length * keys *
			   MI_INDEX_BLOCK_MARGIN) / MI_KEY_BLOCK_LENGTH,
			  3);
  share.base.keys= share.state.header.keys = keys;
  share.state.header.uniques= uniques;
  mi_int2store(share.state.header.key_parts,key_segs);
  mi_int2store(share.state.header.unique_key_parts,unique_key_parts);

  share.state.key_map = ((ulonglong) 1 << keys)-1;
  share.base.keystart = share.state.state.key_file_length=
    MY_ALIGN(info_length, myisam_block_size);
  share.base.max_key_block_length=max_key_block_length;
  share.base.max_key_length=ALIGN_SIZE(max_key_length+4);
  share.base.records=ci->max_rows;
  share.base.reloc=  ci->reloc_rows;
  share.base.reclength=real_reclength;
  share.base.pack_reclength=reclength+ test(options & HA_OPTION_CHECKSUM);;
  share.base.max_pack_length=pack_reclength;
  share.base.min_pack_length=min_pack_length;
  share.base.pack_bits=packed;
  share.base.fields=fields;
  share.base.pack_fields=packed;
#ifdef USE_RAID
  share.base.raid_type=ci->raid_type;
  share.base.raid_chunks=ci->raid_chunks;
  share.base.raid_chunksize=ci->raid_chunksize;
#endif

  /* max_data_file_length and max_key_file_length are recalculated on open */
  if (options & HA_OPTION_TMP_TABLE)
    share.base.max_data_file_length=(my_off_t) ci->data_file_length;

  share.base.min_block_length=
    (share.base.pack_reclength+3 < MI_EXTEND_BLOCK_LENGTH &&
     ! share.base.blobs) ?
    max(share.base.pack_reclength,MI_MIN_BLOCK_LENGTH) :
    MI_EXTEND_BLOCK_LENGTH;
  if (! (flags & HA_DONT_TOUCH_DATA))
    share.state.create_time= (long) time((time_t*) 0);

  if ((file = my_create(fn_format(buff,name,"",MI_NAME_IEXT,4),0,
       O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    goto err;
  errpos=1;
  VOID(fn_format(buff,name,"",MI_NAME_DEXT,2+4));
  if (!(flags & HA_DONT_TOUCH_DATA))
  {
#ifdef USE_RAID
    if (share.base.raid_type)
    {
      if ((dfile=my_raid_create(buff,0,O_RDWR | O_TRUNC,
				share.base.raid_type,
				share.base.raid_chunks,
				share.base.raid_chunksize,
				MYF(MY_WME | MY_RAID))) < 0)
	goto err;
    }
    else
#endif
    if ((dfile = my_create(buff,0,O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
      goto err;

    errpos=3;
  }

  if (mi_state_info_write(file, &share.state, 2) ||
      mi_base_info_write(file, &share.base))
    goto err;
#ifndef DBUG_OFF
  if ((uint) my_tell(file,MYF(0)) != base_pos+ MI_BASE_INFO_SIZE)
  {
    uint pos=(uint) my_tell(file,MYF(0));
    DBUG_PRINT("warning",("base_length: %d  != used_length: %d",
			  base_pos+ MI_BASE_INFO_SIZE, pos));
  }
#endif

  /* Write key and keyseg definitions */
  for (i=0 ; i < share.base.keys - uniques; i++)
  {
    uint ft_segs=(keydefs[i].flag & HA_FULLTEXT) ? FT_SEGS : 0;    /* SerG */

    if (mi_keydef_write(file, &keydefs[i]))
      goto err;
    for (j=0 ; j < keydefs[i].keysegs-ft_segs ; j++)
      if (mi_keyseg_write(file, &keydefs[i].seg[j]))
	goto err;
    for (j=0 ; j < ft_segs ; j++)                                   /* SerG */
    {
      MI_KEYSEG seg=ft_keysegs[j];
      seg.language= keydefs[i].seg[0].language;
      if (mi_keyseg_write(file, &seg))
        goto err;
    }
  }
  /* Create extra keys for unique definitions */
  offset=reclength-uniques*MI_UNIQUE_HASH_LENGTH;
  bzero((char*) &tmp_keydef,sizeof(tmp_keydef));
  bzero((char*) &tmp_keyseg,sizeof(tmp_keyseg));
  for (i=0; i < uniques ; i++)
  {
    tmp_keydef.keysegs=1;
    tmp_keydef.flag=		HA_UNIQUE_CHECK;
    tmp_keydef.block_length=	MI_KEY_BLOCK_LENGTH;
    tmp_keydef.keylength=	MI_UNIQUE_HASH_LENGTH + pointer;
    tmp_keydef.minlength=tmp_keydef.maxlength=tmp_keydef.keylength;
    tmp_keyseg.type= 		MI_UNIQUE_HASH_TYPE;
    tmp_keyseg.length= 		MI_UNIQUE_HASH_LENGTH;
    tmp_keyseg.start=		offset;
    offset+=			MI_UNIQUE_HASH_LENGTH;
    if (mi_keydef_write(file,&tmp_keydef) ||
	mi_keyseg_write(file,(&tmp_keyseg)))
      goto err;
  }

  /* Save unique definition */
  for (i=0 ; i < share.state.header.uniques ; i++)
  {
    if (mi_uniquedef_write(file, &uniquedefs[i]))
      goto err;
    for (j=0 ; j < uniquedefs[i].keysegs ; j++)
    {
      if (mi_keyseg_write(file, &uniquedefs[i].seg[j]))
	goto err;
    }
  }
  for (i=0 ; i < share.base.fields ; i++)
    if (mi_recinfo_write(file, &recinfo[i]))
      goto err;

#ifndef DBUG_OFF
  if ((uint) my_tell(file,MYF(0)) != info_length)
  {
    uint pos= (uint) my_tell(file,MYF(0));
    DBUG_PRINT("warning",("info_length: %d  != used_length: %d",
			  info_length, pos));
  }
#endif

	/* Enlarge files */
  if (my_chsize(file,(ulong) share.base.keystart,MYF(0)))
    goto err;

  if (! (flags & HA_DONT_TOUCH_DATA))
  {
#ifdef USE_RELOC
    if (my_chsize(dfile,share.base.min_pack_length*ci->reloc_rows,MYF(0)))
      goto err;
#endif
    errpos=2;
    if (my_close(dfile,MYF(0)))
      goto err;
  }
  errpos=0;
  pthread_mutex_unlock(&THR_LOCK_myisam);
  if (my_close(file,MYF(0)))
    goto err;
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&THR_LOCK_myisam);
  save_errno=my_errno;
  switch (errpos) {
  case 3:
    VOID(my_close(dfile,MYF(0)));
    /* fall through */
  case 2:
  if (! (flags & HA_DONT_TOUCH_DATA))
  {
    /* QQ: Tõnu should add a call to my_raid_delete() here */
    VOID(fn_format(buff,name,"",MI_NAME_DEXT,2+4));
    my_delete(buff,MYF(0));
  }
    /* fall through */
  case 1:
    VOID(my_close(file,MYF(0)));
    if (! (flags & HA_DONT_TOUCH_DATA))
    {
      VOID(fn_format(buff,name,"",MI_NAME_IEXT,2+4));
      my_delete(buff,MYF(0));
    }
  }
  DBUG_RETURN(my_errno=save_errno);		/* return the fatal errno */
}


uint mi_get_pointer_length(ulonglong file_length, uint def)
{
  if (file_length)				/* If not default */
  {
    if (file_length >= (longlong) 1 << 56)
      def=8;
    if (file_length >= (longlong) 1 << 48)
      def=7;
    if (file_length >= (longlong) 1 << 40)
      def=6;
    else if (file_length >= (longlong) 1 << 32)
      def=5;
    else if (file_length >= (1L << 24))
      def=4;
    else if (file_length >= (1L << 16))
      def=3;
    else
      def=2;
  }
  return def;
}
