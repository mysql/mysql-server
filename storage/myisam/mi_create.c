/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Create a MyISAM table */

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>

#include "my_bit.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "storage/myisam/ftdefs.h"
#include "storage/myisam/sp_defs.h"

#ifdef _WIN32
#include <fcntl.h>
#include <process.h>
#endif
#include "m_ctype.h"

/*
  Old options is used when recreating database, from myisamchk
*/

int mi_create(const char *name,uint keys,MI_KEYDEF *keydefs,
	      uint columns, MI_COLUMNDEF *recinfo,
	      uint uniques, MI_UNIQUEDEF *uniquedefs,
	      MI_CREATE_INFO *ci,uint flags)
{
  uint i,j;
  File dfile= 0, file= 0;
  int errpos,save_errno, create_mode= O_RDWR | O_TRUNC;
  myf create_flag;
  uint fields,length,max_key_length,packed,pointer,real_length_diff,
       key_length,info_length,key_segs,options,min_key_length_skip,
       base_pos,long_varchar_count,varchar_length,
       max_key_block_length,unique_key_parts,fulltext_keys,offset;
  uint aligned_key_start, block_length;
  uint internal_table= flags & HA_CREATE_INTERNAL_TABLE;
  ulong reclength, real_reclength,min_pack_length;
  char filename[FN_REFLEN],linkname[FN_REFLEN], *linkname_ptr;
  ulong pack_reclength;
  ulonglong tot_length,max_rows, tmp;
  enum en_fieldtype type;
  MYISAM_SHARE share;
  MI_KEYDEF *keydef,tmp_keydef;
  MI_UNIQUEDEF *uniquedef;
  HA_KEYSEG *keyseg,tmp_keyseg;
  MI_COLUMNDEF *rec;
  ulong *rec_per_key_part;
  my_off_t key_root[HA_MAX_POSSIBLE_KEY],key_del[MI_MAX_KEY_BLOCK_SIZE];
  MI_CREATE_INFO tmp_create_info;
  DBUG_ENTER("mi_create");
  DBUG_PRINT("enter", ("keys: %u  columns: %u  uniques: %u  flags: %u",
                      keys, columns, uniques, flags));

  if (!ci)
  {
    memset(&tmp_create_info, 0, sizeof(tmp_create_info));
    ci=&tmp_create_info;
  }

  if (keys + uniques > MI_MAX_KEY || columns == 0)
  {
    set_my_errno(HA_WRONG_CREATE_OPTION);
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  errpos=0;
  options=0;
  memset(&share, 0, sizeof(share));

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
    ci->reloc_rows=ci->max_rows;		/* Check if wrong parameter */

  if (!(rec_per_key_part=
	(ulong*) my_malloc(mi_key_memory_MYISAM_SHARE,
                           (keys + uniques)*MI_MAX_KEY_SEG*sizeof(long),
			   MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(my_errno());

	/* Start by checking fields and field-types used */

  reclength=varchar_length=long_varchar_count=packed=
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
	  if (rec->length == 4+portable_sizeof_char_ptr)
	    pack_reclength= INT_MAX32;
	  else
	    pack_reclength+=(1 << ((rec->length-portable_sizeof_char_ptr)*8)); /* Max blob length */
	}
      }
      else if (type == FIELD_SKIP_PRESPACE ||
	       type == FIELD_SKIP_ENDSPACE)
      {
	if (pack_reclength != INT_MAX32)
	  pack_reclength+= rec->length > 255 ? 2 : 1;
	min_pack_length++;
      }
      else if (type == FIELD_VARCHAR)
      {
	varchar_length+= rec->length-1;          /* Used for min_pack_length */
	packed--;
	pack_reclength++;
        min_pack_length++;
        /* We must test for 257 as length includes pack-length */
        if (rec->length >= 257)
	{
	  long_varchar_count++;
	  pack_reclength+= 2;			/* May be packed on 3 bytes */
	}
      }
      else if (type != FIELD_SKIP_ZERO)
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
      if (rec->type == (int) FIELD_SKIP_ZERO && rec->length == 1)
      {
        /*
          NOTE1: here we change a field type FIELD_SKIP_ZERO ->
          FIELD_NORMAL
        */
	rec->type=(int) FIELD_NORMAL;
	packed--;
	min_pack_length++;
	break;
      }
    }
  }

  if (packed || (flags & HA_PACK_RECORD))
    options|=HA_OPTION_PACK_RECORD;	/* Must use packed records */
  /* We can't use checksum with static length rows */
  if (!(options & HA_OPTION_PACK_RECORD))
    options&= ~HA_OPTION_CHECKSUM;
  if (!(options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    min_pack_length+= varchar_length;
  if (flags & HA_CREATE_TMP_TABLE)
  {
    options|= HA_OPTION_TMP_TABLE;
    create_mode|= O_EXCL | O_NOFOLLOW;
  }
  if (flags & HA_CREATE_CHECKSUM || (options & HA_OPTION_CHECKSUM))
  {
    options|= HA_OPTION_CHECKSUM;
    min_pack_length++;
  }
  if (flags & HA_CREATE_DELAY_KEY_WRITE)
    options|= HA_OPTION_DELAY_KEY_WRITE;
  if (flags & HA_CREATE_RELIES_ON_SQL_LAYER)
    options|= HA_OPTION_RELIES_ON_SQL_LAYER;

  packed=(packed+7)/8;
  if (pack_reclength != INT_MAX32)
    pack_reclength+= reclength+packed +
      test_all_bits(options, HA_OPTION_CHECKSUM | HA_OPTION_PACK_RECORD);
  min_pack_length+=packed;

  if (!ci->data_file_length && ci->max_rows)
  {
    if (pack_reclength == INT_MAX32 ||
             (~(ulonglong) 0)/ci->max_rows < (ulonglong) pack_reclength)
      ci->data_file_length= ~(ulonglong) 0;
    else
      ci->data_file_length=(ulonglong) ci->max_rows*pack_reclength;
  }
  else if (!ci->max_rows)
    ci->max_rows=(ha_rows) (ci->data_file_length/(min_pack_length +
					 ((options & HA_OPTION_PACK_RECORD) ?
					  3 : 0)));

  if (options & (HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD))
    pointer=mi_get_pointer_length(ci->data_file_length,myisam_data_pointer_size);
  else
    pointer=mi_get_pointer_length(ci->max_rows,myisam_data_pointer_size);
  if (!(max_rows=(ulonglong) ci->max_rows))
    max_rows= ((((ulonglong) 1 << (pointer*8)) -1) / min_pack_length);


  real_reclength=reclength;
  if (!(options & (HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD)))
  {
    if (reclength <= pointer)
      reclength=pointer+1;		/* reserve place for delete link */
  }
  else
    reclength+= long_varchar_count;	/* We need space for varchar! */

  max_key_length=0; tot_length=0 ; key_segs=0;
  fulltext_keys=0;
  max_key_block_length=0;
  share.state.rec_per_key_part=rec_per_key_part;
  share.state.key_root=key_root;
  share.state.key_del=key_del;
  if (uniques)
  {
    max_key_block_length= myisam_block_size;
    max_key_length=	  MI_UNIQUE_HASH_LENGTH + pointer;
  }

  for (i=0, keydef=keydefs ; i < keys ; i++ , keydef++)
  {

    share.state.key_root[i]= HA_OFFSET_ERROR;
    min_key_length_skip=length=real_length_diff=0;
    key_length=pointer;
    if (keydef->flag & HA_SPATIAL)
    {
      /* BAR TODO to support 3D and more dimensions in the future */
      uint sp_segs=SPDIMS*2;
      keydef->flag=HA_SPATIAL;

      if (flags & HA_DONT_TOUCH_DATA)
      {
        /*
           called by myisamchk - i.e. table structure was taken from
           MYI file and SPATIAL key *does have* additional sp_segs keysegs.
           keydef->seg here points right at the GEOMETRY segment,
           so we only need to decrease keydef->keysegs.
           (see recreate_table() in mi_check.c)
        */
        keydef->keysegs-=sp_segs-1;
      }

      for (j=0, keyseg=keydef->seg ; (int) j < keydef->keysegs ;
	   j++, keyseg++)
      {
        if (keyseg->type != HA_KEYTYPE_BINARY &&
	    keyseg->type != HA_KEYTYPE_VARBINARY1 &&
            keyseg->type != HA_KEYTYPE_VARBINARY2)
        {
          set_my_errno(HA_WRONG_CREATE_OPTION);
          goto err_no_lock;
        }
      }
      keydef->keysegs+=sp_segs;
      key_length+=SPLEN*sp_segs;
      length++;                              /* At least one length byte */
      min_key_length_skip+=SPLEN*2*SPDIMS;
    }
    else if (keydef->flag & HA_FULLTEXT)
    {
      keydef->flag=HA_FULLTEXT | HA_PACK_KEY | HA_VAR_LENGTH_KEY;
      options|=HA_OPTION_PACK_KEYS;             /* Using packed keys */

      for (j=0, keyseg=keydef->seg ; (int) j < keydef->keysegs ;
	   j++, keyseg++)
      {
        if (keyseg->type != HA_KEYTYPE_TEXT &&
	    keyseg->type != HA_KEYTYPE_VARTEXT1 &&
            keyseg->type != HA_KEYTYPE_VARTEXT2)
        {
          set_my_errno(HA_WRONG_CREATE_OPTION);
          goto err_no_lock;
        }
        if (!(keyseg->flag & HA_BLOB_PART) &&
	    (keyseg->type == HA_KEYTYPE_VARTEXT1 ||
             keyseg->type == HA_KEYTYPE_VARTEXT2))
        {
          /* Make a flag that this is a VARCHAR */
          keyseg->flag|= HA_VAR_LENGTH_PART;
          /* Store in bit_start number of bytes used to pack the length */
          keyseg->bit_start= ((keyseg->type == HA_KEYTYPE_VARTEXT1)?
                              1 : 2);
        }
      }

      fulltext_keys++;
      key_length+= HA_FT_MAXBYTELEN+HA_FT_WLEN;
      length++;                              /* At least one length byte */
      min_key_length_skip+=HA_FT_MAXBYTELEN;
      real_length_diff=HA_FT_MAXBYTELEN-FT_MAX_WORD_LEN_FOR_SORT;
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

	/* Only use HA_PACK_KEY when first segment is a variable length key */
	if (!(keydef->seg[0].flag & (HA_SPACE_PACK | HA_BLOB_PART |
				     HA_VAR_LENGTH_PART)))
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

      if (keydef->flag & HA_AUTO_KEY && ci->with_auto_increment)
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
          break;
        case HA_KEYTYPE_VARTEXT1:
        case HA_KEYTYPE_VARTEXT2:
        case HA_KEYTYPE_VARBINARY1:
        case HA_KEYTYPE_VARBINARY2:
          if (!(keyseg->flag & HA_BLOB_PART))
          {
            /* Make a flag that this is a VARCHAR */
            keyseg->flag|= HA_VAR_LENGTH_PART;
            /* Store in bit_start number of bytes used to pack the length */
            keyseg->bit_start= ((keyseg->type == HA_KEYTYPE_VARTEXT1 ||
                                 keyseg->type == HA_KEYTYPE_VARBINARY1) ?
                                1 : 2);
          }
          break;
	default:
	  break;
	}
	if (keyseg->flag & HA_SPACE_PACK)
	{
          DBUG_ASSERT(!(keyseg->flag & HA_VAR_LENGTH_PART));
	  keydef->flag |= HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY;
	  options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	  length++;				/* At least one length byte */
	  min_key_length_skip+=keyseg->length;
	  if (keyseg->length >= 255)
	  {					/* prefix may be 3 bytes */
	    min_key_length_skip+=2;
	    length+=2;
	  }
	}
	if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
	{
          DBUG_ASSERT(!test_all_bits(keyseg->flag,
                                    (HA_VAR_LENGTH_PART | HA_BLOB_PART)));
	  keydef->flag|=HA_VAR_LENGTH_KEY;
	  length++;				/* At least one length byte */
	  options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	  min_key_length_skip+=keyseg->length;
	  if (keyseg->length >= 255)
	  {					/* prefix may be 3 bytes */
	    min_key_length_skip+=2;
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
      set_my_errno(HA_WRONG_CREATE_OPTION);
      goto err_no_lock;
    }
    /*
      key_segs may be 0 in the case when we only want to be able to
      add on row into the table. This can happen with some DISTINCT queries
      in MySQL
    */
    if ((keydef->flag & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME &&
	key_segs)
      share.state.rec_per_key_part[key_segs-1]=1L;
    length+=key_length;
    /* Get block length for key, if defined by user */
    block_length= (keydef->block_length ? 
                   my_round_up_to_next_power(keydef->block_length) :
                   myisam_block_size);
    block_length= MY_MAX(block_length, MI_MIN_KEY_BLOCK_LENGTH);
    block_length= MY_MIN(block_length, MI_MAX_KEY_BLOCK_LENGTH);

    keydef->block_length= (uint16) MI_BLOCK_SIZE(length-real_length_diff,
                                                 pointer,MI_MAX_KEYPTR_SIZE,
                                                 block_length);
    if (keydef->block_length > MI_MAX_KEY_BLOCK_LENGTH ||
        length >= MI_MAX_KEY_BUFF)
    {
      set_my_errno(HA_WRONG_CREATE_OPTION);
      goto err_no_lock;
    }
    set_if_bigger(max_key_block_length,keydef->block_length);
    keydef->keylength= (uint16) key_length;
    keydef->minlength= (uint16) (length-min_key_length_skip);
    keydef->maxlength= (uint16) length;

    if (length > max_key_length)
      max_key_length= length;
    tot_length+= (max_rows/(ulong) (((uint) keydef->block_length-5)/
				    (length*2)))*
      (ulong) keydef->block_length;
  }
  for (i=max_key_block_length/MI_MIN_KEY_BLOCK_LENGTH ; i-- ; )
    key_del[i]=HA_OFFSET_ERROR;

  unique_key_parts=0;
  for (i=0, uniquedef=uniquedefs ; i < uniques ; i++ , uniquedef++)
  {
    uniquedef->key=keys+i;
    unique_key_parts+=uniquedef->keysegs;
    share.state.key_root[keys+i]= HA_OFFSET_ERROR;
    tot_length+= (max_rows/(ulong) (((uint) myisam_block_size-5)/
                         ((MI_UNIQUE_HASH_LENGTH + pointer)*2)))*
                         (ulong) myisam_block_size;
  }
  keys+=uniques;				/* Each unique has 1 key */
  key_segs+=uniques;				/* Each unique has 1 key seg */

  base_pos=(MI_STATE_INFO_SIZE + keys * MI_STATE_KEY_SIZE +
	    max_key_block_length/MI_MIN_KEY_BLOCK_LENGTH*
	    MI_STATE_KEYBLOCK_SIZE+
	    key_segs*MI_STATE_KEYSEG_SIZE);
  info_length=base_pos+(uint) (MI_BASE_INFO_SIZE+
			       keys * MI_KEYDEF_SIZE+
			       uniques * MI_UNIQUEDEF_SIZE +
			       (key_segs + unique_key_parts)*HA_KEYSEG_SIZE+
			       columns*MI_COLUMNDEF_SIZE);
  DBUG_PRINT("info", ("info_length: %u", info_length));
  /* There are only 16 bits for the total header length. */
  if (info_length > 65535)
  {
    my_printf_error(0, "MyISAM table '%s' has too many columns and/or "
                    "indexes and/or unique constraints.",
                    MYF(0), name + dirname_length(name));
    set_my_errno(HA_WRONG_CREATE_OPTION);
    goto err_no_lock;
  }

  memmove(share.state.header.file_version, (uchar*) myisam_file_magic, 4);
  ci->old_options=options| (ci->old_options & HA_OPTION_TEMP_COMPRESS_RECORD ?
			HA_OPTION_COMPRESS_RECORD |
			HA_OPTION_TEMP_COMPRESS_RECORD: 0);
  mi_int2store(share.state.header.options,ci->old_options);
  mi_int2store(share.state.header.header_length,info_length);
  mi_int2store(share.state.header.state_info_length,MI_STATE_INFO_SIZE);
  mi_int2store(share.state.header.base_info_length,MI_BASE_INFO_SIZE);
  mi_int2store(share.state.header.base_pos,base_pos);
  share.state.header.language= (ci->language ?
				ci->language : default_charset_info->number);
  share.state.header.max_block_size_index= max_key_block_length/MI_MIN_KEY_BLOCK_LENGTH;

  share.state.dellink = HA_OFFSET_ERROR;
  share.state.process=	(ulong) getpid();
  share.state.unique=	(ulong) 0;
  share.state.update_count=(ulong) 0;
  share.state.version=	(ulong) time((time_t*) 0);
  share.state.sortkey=  (ushort) ~0;
  share.state.auto_increment=ci->auto_increment;
  share.options=options;
  share.base.rec_reflength=pointer;
  /* Get estimate for index file length (this may be wrong for FT keys) */
  tmp= (tot_length + max_key_block_length * keys *
	MI_INDEX_BLOCK_MARGIN) / MI_MIN_KEY_BLOCK_LENGTH;
  /*
    use maximum of key_file_length we calculated and key_file_length value we
    got from MYI file header (see also myisampack.c:save_state)
  */
  share.base.key_reflength=
    mi_get_pointer_length(MY_MAX(ci->key_file_length, tmp), 3);
  share.base.keys= share.state.header.keys= keys;
  share.state.header.uniques= uniques;
  share.state.header.fulltext_keys= fulltext_keys;
  mi_int2store(share.state.header.key_parts,key_segs);
  mi_int2store(share.state.header.unique_key_parts,unique_key_parts);

  mi_set_all_keys_active(share.state.key_map, keys);
  aligned_key_start= my_round_up_to_next_power(max_key_block_length ?
                                               max_key_block_length :
                                               myisam_block_size);

  share.base.keystart= share.state.state.key_file_length=
    MY_ALIGN(info_length, aligned_key_start);
  share.base.max_key_block_length=max_key_block_length;
  share.base.max_key_length=ALIGN_SIZE(max_key_length+4);
  share.base.records=ci->max_rows;
  share.base.reloc=  ci->reloc_rows;
  share.base.reclength=real_reclength;
  share.base.pack_reclength=reclength+ (options & HA_OPTION_CHECKSUM ? 1 : 0);
  share.base.max_pack_length=pack_reclength;
  share.base.min_pack_length=min_pack_length;
  share.base.pack_bits=packed;
  share.base.fields=fields;
  share.base.pack_fields=packed;

  /* max_data_file_length and max_key_file_length are recalculated on open */
  if (options & HA_OPTION_TMP_TABLE)
    share.base.max_data_file_length=(my_off_t) ci->data_file_length;

  share.base.min_block_length=
    (share.base.pack_reclength+3 < MI_EXTEND_BLOCK_LENGTH &&
     ! share.base.blobs) ?
    MY_MAX(share.base.pack_reclength, MI_MIN_BLOCK_LENGTH) :
    MI_EXTEND_BLOCK_LENGTH;
  if (! (flags & HA_DONT_TOUCH_DATA))
    share.state.create_time= (long) time((time_t*) 0);

  if (!internal_table)
    mysql_mutex_lock(&THR_LOCK_myisam);

  /*
    NOTE: For test_if_reopen() we need a real path name. Hence we need
    MY_RETURN_REAL_PATH for every fn_format(filename, ...).
  */
  if (ci->index_file_name)
  {
    char *iext= strrchr(ci->index_file_name, '.');
    int have_iext= iext && !strcmp(iext, MI_NAME_IEXT);
    if (options & HA_OPTION_TMP_TABLE)
    {
      char *path;
      /* chop off the table name, tempory tables use generated name */
      if ((path= strrchr(ci->index_file_name, FN_LIBCHAR)))
        *path= '\0';
      fn_format(filename, name, ci->index_file_name, MI_NAME_IEXT,
                MY_REPLACE_DIR | MY_UNPACK_FILENAME |
                MY_RETURN_REAL_PATH | MY_APPEND_EXT);
    }
    else
    {
      fn_format(filename, ci->index_file_name, "", MI_NAME_IEXT,
                MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH |
                (have_iext ? MY_REPLACE_EXT : MY_APPEND_EXT));
    }
    fn_format(linkname, name, "", MI_NAME_IEXT,
              MY_UNPACK_FILENAME|MY_APPEND_EXT);
    linkname_ptr=linkname;
    /*
      Don't create the table if the link or file exists to ensure that one
      doesn't accidently destroy another table.
    */
    create_flag=0;
  }
  else
  {
    char *iext= strrchr(name, '.');
    int have_iext= iext && !strcmp(iext, MI_NAME_IEXT);
    fn_format(filename, name, "", MI_NAME_IEXT,
              MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH |
              (have_iext ? MY_REPLACE_EXT : MY_APPEND_EXT));
    linkname_ptr=0;
    /* Replace the current file */
    create_flag=(flags & HA_CREATE_KEEP_FILES) ? 0 : MY_DELETE_OLD;
  }

  /*
    If a MRG_MyISAM table is in use, the mapped MyISAM tables are open,
    but no entry is made in the table cache for them.
    A TRUNCATE command checks for the table in the cache only and could
    be fooled to believe, the table is not open.
    Pull the emergency brake in this situation. (Bug #8306)

    NOTE: The filename is compared against unique_file_name of every
    open table. Hence we need a real path here.
  */
  if (!internal_table && test_if_reopen(filename))
  {
    my_printf_error(0, "MyISAM table '%s' is in use "
                    "(most likely by a MERGE table). Try FLUSH TABLES.",
                    MYF(0), name + dirname_length(name));
    set_my_errno(HA_ERR_TABLE_EXIST);
    goto err;
  }

  if ((file= mysql_file_create_with_symlink(mi_key_file_kfile,
                                            linkname_ptr, filename, 0,
                                            create_mode,
                                            MYF(MY_WME | create_flag))) < 0)
    goto err;
  errpos=1;

  if (!(flags & HA_DONT_TOUCH_DATA))
  {
    {
      if (ci->data_file_name)
      {
        char *dext= strrchr(ci->data_file_name, '.');
        int have_dext= dext && !strcmp(dext, MI_NAME_DEXT);

        if (options & HA_OPTION_TMP_TABLE)
        {
          char *path;
          /* chop off the table name, tempory tables use generated name */
          if ((path= strrchr(ci->data_file_name, FN_LIBCHAR)))
            *path= '\0';
          fn_format(filename, name, ci->data_file_name, MI_NAME_DEXT,
                    MY_REPLACE_DIR | MY_UNPACK_FILENAME | MY_APPEND_EXT);
        }
        else
        {
          fn_format(filename, ci->data_file_name, "", MI_NAME_DEXT,
                    MY_UNPACK_FILENAME |
                    (have_dext ? MY_REPLACE_EXT : MY_APPEND_EXT));
        }

	fn_format(linkname, name, "",MI_NAME_DEXT,
	          MY_UNPACK_FILENAME | MY_APPEND_EXT);
	linkname_ptr=linkname;
	create_flag=0;
      }
      else
      {
	fn_format(filename,name,"", MI_NAME_DEXT,
	          MY_UNPACK_FILENAME | MY_APPEND_EXT);
	linkname_ptr=0;
        create_flag=(flags & HA_CREATE_KEEP_FILES) ? 0 : MY_DELETE_OLD;
      }
      if ((dfile=
           mysql_file_create_with_symlink(mi_key_file_dfile,
                                          linkname_ptr, filename, 0,
                                          create_mode,
                                          MYF(MY_WME | create_flag))) < 0)
	goto err;
    }
    errpos=3;
  }

  DBUG_PRINT("info", ("write state info and base info"));
  if (mi_state_info_write(file, &share.state, 2) ||
      mi_base_info_write(file, &share.base))
    goto err;
#ifndef DBUG_OFF
  if ((uint) mysql_file_tell(file, MYF(0)) != base_pos + MI_BASE_INFO_SIZE)
  {
    uint pos=(uint) mysql_file_tell(file, MYF(0));
    DBUG_PRINT("warning",("base_length: %d  != used_length: %d",
			  base_pos+ MI_BASE_INFO_SIZE, pos));
  }
#endif

  /* Write key and keyseg definitions */
  DBUG_PRINT("info", ("write key and keyseg definitions"));
  for (i=0 ; i < share.base.keys - uniques; i++)
  {
    uint sp_segs=(keydefs[i].flag & HA_SPATIAL) ? 2*SPDIMS : 0;

    if (mi_keydef_write(file, &keydefs[i]))
      goto err;
    for (j=0 ; j < keydefs[i].keysegs-sp_segs ; j++)
      if (mi_keyseg_write(file, &keydefs[i].seg[j]))
       goto err;
    for (j=0 ; j < sp_segs ; j++)
    {
      HA_KEYSEG sseg;
      sseg.type=SPTYPE;
      sseg.language= 7;                         /* Binary */
      sseg.null_bit=0;
      sseg.bit_start=0;
      sseg.bit_end=0;
      sseg.bit_length= 0;
      sseg.bit_pos= 0;
      sseg.length=SPLEN;
      sseg.null_pos=0;
      sseg.start=j*SPLEN;
      sseg.flag= HA_SWAP_KEY;
      if (mi_keyseg_write(file, &sseg))
        goto err;
    }
  }
  /* Create extra keys for unique definitions */
  offset= real_reclength - uniques * MI_UNIQUE_HASH_LENGTH;
  memset(&tmp_keydef, 0, sizeof(tmp_keydef));
  memset(&tmp_keyseg, 0, sizeof(tmp_keyseg));
  for (i=0; i < uniques ; i++)
  {
    tmp_keydef.keysegs=1;
    tmp_keydef.flag=		HA_UNIQUE_CHECK;
    tmp_keydef.block_length=	(uint16)myisam_block_size;
    tmp_keydef.keylength=	MI_UNIQUE_HASH_LENGTH + pointer;
    tmp_keydef.minlength=tmp_keydef.maxlength=tmp_keydef.keylength;
    tmp_keyseg.type=		MI_UNIQUE_HASH_TYPE;
    tmp_keyseg.length=		MI_UNIQUE_HASH_LENGTH;
    tmp_keyseg.start=		offset;
    offset+=			MI_UNIQUE_HASH_LENGTH;
    if (mi_keydef_write(file,&tmp_keydef) ||
	mi_keyseg_write(file,(&tmp_keyseg)))
      goto err;
  }

  /* Save unique definition */
  DBUG_PRINT("info", ("write unique definitions"));
  for (i=0 ; i < share.state.header.uniques ; i++)
  {
    HA_KEYSEG *keyseg_end;
    keyseg= uniquedefs[i].seg;
    if (mi_uniquedef_write(file, &uniquedefs[i]))
      goto err;
    for (keyseg= uniquedefs[i].seg, keyseg_end= keyseg+ uniquedefs[i].keysegs;
         keyseg < keyseg_end;
         keyseg++)
    {
      switch (keyseg->type) {
      case HA_KEYTYPE_VARTEXT1:
      case HA_KEYTYPE_VARTEXT2:
      case HA_KEYTYPE_VARBINARY1:
      case HA_KEYTYPE_VARBINARY2:
        if (!(keyseg->flag & HA_BLOB_PART))
        {
          keyseg->flag|= HA_VAR_LENGTH_PART;
          keyseg->bit_start= ((keyseg->type == HA_KEYTYPE_VARTEXT1 ||
                               keyseg->type == HA_KEYTYPE_VARBINARY1) ?
                              1 : 2);
        }
        break;
      default:
        break;
      }
      if (mi_keyseg_write(file, keyseg))
	goto err;
    }
  }
  DBUG_PRINT("info", ("write field definitions"));
  for (i=0 ; i < share.base.fields ; i++)
    if (mi_recinfo_write(file, &recinfo[i]))
      goto err;

#ifndef DBUG_OFF
  if ((uint) mysql_file_tell(file, MYF(0)) != info_length)
  {
    uint pos= (uint) mysql_file_tell(file, MYF(0));
    DBUG_PRINT("warning",("info_length: %d  != used_length: %d",
			  info_length, pos));
  }
#endif

	/* Enlarge files */
  DBUG_PRINT("info", ("enlarge to keystart: %lu", (ulong) share.base.keystart));
  if (mysql_file_chsize(file, (ulong) share.base.keystart, 0, MYF(0)))
    goto err;

  if (! (flags & HA_DONT_TOUCH_DATA))
  {
    errpos=2;
    if (mysql_file_close(dfile, MYF(0)))
      goto err;
  }
  errpos=0;
  if (!internal_table)
    mysql_mutex_unlock(&THR_LOCK_myisam);
  if (mysql_file_close(file, MYF(0)))
    goto err_no_lock;
  my_free(rec_per_key_part);
  DBUG_RETURN(0);

err:
  if (!internal_table)
    mysql_mutex_unlock(&THR_LOCK_myisam);

err_no_lock:
  save_errno=my_errno();
  switch (errpos) {
  case 3:
    (void) mysql_file_close(dfile, MYF(0));
    /* fall through */
  case 2:
  if (! (flags & HA_DONT_TOUCH_DATA))
    mysql_file_delete_with_symlink(mi_key_file_dfile,
                                   fn_format(filename, name, "", MI_NAME_DEXT,
                                             MY_UNPACK_FILENAME | MY_APPEND_EXT),
                                   MYF(0));
    /* fall through */
  case 1:
    (void) mysql_file_close(file, MYF(0));
    if (! (flags & HA_DONT_TOUCH_DATA))
      mysql_file_delete_with_symlink(mi_key_file_kfile,
                                     fn_format(filename, name, "", MI_NAME_IEXT,
                                               MY_UNPACK_FILENAME | MY_APPEND_EXT),
                                     MYF(0));
  }
  my_free(rec_per_key_part);
  set_my_errno(save_errno);
  DBUG_RETURN(save_errno);		/* return the fatal errno */
}


uint mi_get_pointer_length(ulonglong file_length, uint def)
{
  DBUG_ASSERT(def >= 2 && def <= 7);
  if (file_length)				/* If not default */
  {
    if (file_length >= 1ULL << 48)
      def=7;
    else if (file_length >= 1ULL << 40)
      def=6;
    else if (file_length >= 1ULL << 32)
      def=5;
    else if (file_length >= 1ULL << 24)
      def=4;
    else if (file_length >= 1ULL << 16)
      def=3;
    else
      def=2;
  }
  return def;
}
