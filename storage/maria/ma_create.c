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

/* Create a MARIA table */

#include "ma_ftdefs.h"
#include "ma_sp_defs.h"
#include <my_bit.h>
#include "ma_blockrec.h"
#include "trnman_public.h"

#if defined(MSDOS) || defined(__WIN__)
#ifdef __WIN__
#include <fcntl.h>
#else
#include <process.h>			/* Prototype for getpid */
#endif
#endif
#include <m_ctype.h>

static int compare_columns(MARIA_COLUMNDEF **a, MARIA_COLUMNDEF **b);

/*
  Old options is used when recreating database, from maria_chk
*/

int maria_create(const char *name, enum data_file_type datafile_type,
                 uint keys,MARIA_KEYDEF *keydefs,
                 uint columns, MARIA_COLUMNDEF *columndef,
                 uint uniques, MARIA_UNIQUEDEF *uniquedefs,
                 MARIA_CREATE_INFO *ci,uint flags)
{
  register uint i,j;
  File dfile,file;
  int errpos,save_errno, create_mode= O_RDWR | O_TRUNC, res;
  myf create_flag;
  uint length,max_key_length,packed,pack_bytes,pointer,real_length_diff,
       key_length,info_length,key_segs,options,min_key_length,
       base_pos,long_varchar_count,varchar_length,
       unique_key_parts,fulltext_keys,offset, not_block_record_extra_length;
  uint max_field_lengths, extra_header_size, column_nr;
  ulong reclength, real_reclength,min_pack_length;
  char filename[FN_REFLEN], linkname[FN_REFLEN], *linkname_ptr;
  ulong pack_reclength;
  ulonglong tot_length,max_rows, tmp;
  enum en_fieldtype type;
  enum data_file_type org_datafile_type= datafile_type;
  MARIA_SHARE share;
  MARIA_KEYDEF *keydef,tmp_keydef;
  MARIA_UNIQUEDEF *uniquedef;
  HA_KEYSEG *keyseg,tmp_keyseg;
  MARIA_COLUMNDEF *column, *end_column;
  double *rec_per_key_part;
  ulong  *nulls_per_key_part;
  uint16 *column_array;
  my_off_t key_root[HA_MAX_POSSIBLE_KEY], kfile_size_before_extension;
  MARIA_CREATE_INFO tmp_create_info;
  my_bool tmp_table= FALSE; /* cache for presence of HA_OPTION_TMP_TABLE */
  my_bool forced_packed;
  myf     sync_dir=  0;
  uchar   *log_data= NULL;
  DBUG_ENTER("maria_create");
  DBUG_PRINT("enter", ("keys: %u  columns: %u  uniques: %u  flags: %u",
                      keys, columns, uniques, flags));

  DBUG_ASSERT(maria_inited);
  LINT_INIT(dfile);
  LINT_INIT(file);

  if (!ci)
  {
    bzero((char*) &tmp_create_info,sizeof(tmp_create_info));
    ci=&tmp_create_info;
  }

  if (keys + uniques > MARIA_MAX_KEY)
  {
    DBUG_RETURN(my_errno=HA_WRONG_CREATE_OPTION);
  }
  errpos=0;
  options=0;
  bzero((uchar*) &share,sizeof(share));

  if (flags & HA_DONT_TOUCH_DATA)
  {
    /* We come here from recreate table */
    org_datafile_type= ci->org_data_file_type;
    if (!(ci->old_options & HA_OPTION_TEMP_COMPRESS_RECORD))
      options= (ci->old_options &
                (HA_OPTION_COMPRESS_RECORD | HA_OPTION_PACK_RECORD |
                 HA_OPTION_READ_ONLY_DATA | HA_OPTION_CHECKSUM |
                 HA_OPTION_TMP_TABLE | HA_OPTION_DELAY_KEY_WRITE |
                 HA_OPTION_LONG_BLOB_PTR | HA_OPTION_PAGE_CHECKSUM));
    else
    {
      /* Uncompressing rows */
      options= (ci->old_options &
                (HA_OPTION_CHECKSUM | HA_OPTION_TMP_TABLE |
                 HA_OPTION_DELAY_KEY_WRITE | HA_OPTION_LONG_BLOB_PTR |
                 HA_OPTION_PAGE_CHECKSUM));
    }
  }
  else
  {
    /* Transactional tables must be of type BLOCK_RECORD */
    if (ci->transactional)
      datafile_type= BLOCK_RECORD;
  }

  if (ci->reloc_rows > ci->max_rows)
    ci->reloc_rows=ci->max_rows;		/* Check if wrong parameter */

  if (!(rec_per_key_part=
	(double*) my_malloc((keys + uniques)*HA_MAX_KEY_SEG*sizeof(double) +
                            (keys + uniques)*HA_MAX_KEY_SEG*sizeof(ulong) +
                            sizeof(uint16) * columns,
                            MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(my_errno);
  nulls_per_key_part= (ulong*) (rec_per_key_part +
                                (keys + uniques) * HA_MAX_KEY_SEG);
  column_array= (uint16*) (nulls_per_key_part +
                           (keys + uniques) * HA_MAX_KEY_SEG);


  /* Start by checking fields and field-types used */
  varchar_length=long_varchar_count=packed= not_block_record_extra_length=
    pack_reclength= max_field_lengths= 0;
  reclength= min_pack_length= ci->null_bytes;
  forced_packed= 0;
  column_nr= 0;

  for (column= columndef, end_column= column + columns ;
       column != end_column ;
       column++)
  {
    /* Fill in not used struct parts */
    column->column_nr= column_nr++;
    column->offset= reclength;
    column->empty_pos= 0;
    column->empty_bit= 0;
    column->fill_length= column->length;
    if (column->null_bit)
      options|= HA_OPTION_NULL_FIELDS;

    reclength+= column->length;
    type= column->type;
    if (datafile_type == BLOCK_RECORD)
    {
      if (type == FIELD_SKIP_PRESPACE)
        type= column->type= FIELD_NORMAL; /* SKIP_PRESPACE not supported */
      if (type == FIELD_NORMAL &&
          column->length > FULL_PAGE_SIZE(maria_block_size))
      {
        /* FIELD_NORMAL can't be split over many blocks, convert to a CHAR */
        type= column->type= FIELD_SKIP_ENDSPACE;
      }
    }

    if (type != FIELD_NORMAL && type != FIELD_CHECK)
    {
      column->empty_pos= packed/8;
      column->empty_bit= (1 << (packed & 7));
      if (type == FIELD_BLOB)
      {
        forced_packed= 1;
        packed++;
	share.base.blobs++;
	if (pack_reclength != INT_MAX32)
	{
	  if (column->length == 4+portable_sizeof_char_ptr)
	    pack_reclength= INT_MAX32;
	  else
          {
            /* Add max possible blob length */
	    pack_reclength+= (1 << ((column->length-
                                     portable_sizeof_char_ptr)*8));
          }
	}
        max_field_lengths+= (column->length - portable_sizeof_char_ptr);
      }
      else if (type == FIELD_SKIP_PRESPACE ||
	       type == FIELD_SKIP_ENDSPACE)
      {
        forced_packed= 1;
        max_field_lengths+= column->length > 255 ? 2 : 1;
        not_block_record_extra_length++;
        packed++;
      }
      else if (type == FIELD_VARCHAR)
      {
	varchar_length+= column->length-1; /* Used for min_pack_length */
	pack_reclength++;
        not_block_record_extra_length++;
        max_field_lengths++;
        packed++;
        column->fill_length= 1;
        options|= HA_OPTION_NULL_FIELDS;        /* Use ma_checksum() */

        /* We must test for 257 as length includes pack-length */
        if (test(column->length >= 257))
	{
	  long_varchar_count++;
          max_field_lengths++;
          column->fill_length= 2;
	}
      }
      else if (type == FIELD_SKIP_ZERO)
        packed++;
      else
      {
        if (!column->null_bit)
          min_pack_length+= column->length;
        else
        {
          /* Only BLOCK_RECORD skips NULL fields for all field values */
          not_block_record_extra_length+= column->length;
        }
        column->empty_pos= 0;
        column->empty_bit= 0;
      }
    }
    else					/* FIELD_NORMAL */
    {
      if (!column->null_bit)
      {
        min_pack_length+= column->length;
        share.base.fixed_not_null_fields++;
        share.base.fixed_not_null_fields_length+= column->length;
      }
      else
        not_block_record_extra_length+= column->length;
    }
  }

  if (datafile_type == STATIC_RECORD && forced_packed)
  {
    /* Can't use fixed length records, revert to block records */
    datafile_type= BLOCK_RECORD;
  }

  if (datafile_type == DYNAMIC_RECORD)
    options|= HA_OPTION_PACK_RECORD;	/* Must use packed records */

  if (datafile_type == STATIC_RECORD)
  {
    /* We can't use checksum with static length rows */
    flags&= ~HA_CREATE_CHECKSUM;
    options&= ~HA_OPTION_CHECKSUM;
    min_pack_length= reclength;
    packed= 0;
  }
  else if (datafile_type != BLOCK_RECORD)
    min_pack_length+= not_block_record_extra_length;
  else
    min_pack_length+= 5;                        /* Min row overhead */

  if (flags & HA_CREATE_TMP_TABLE)
  {
    options|= HA_OPTION_TMP_TABLE;
    tmp_table= TRUE;
    create_mode|= O_NOFOLLOW;
    /* "CREATE TEMPORARY" tables are not crash-safe (dropped at restart) */
    ci->transactional= FALSE;
    flags&= ~HA_CREATE_PAGE_CHECKSUM;
  }
  share.base.null_bytes= ci->null_bytes;
  share.base.original_null_bytes= ci->null_bytes;
  share.base.born_transactional= ci->transactional;
  share.base.max_field_lengths= max_field_lengths;
  share.base.field_offsets= 0;                  /* for future */

  if (flags & HA_CREATE_CHECKSUM || (options & HA_OPTION_CHECKSUM))
  {
    options|= HA_OPTION_CHECKSUM;
    min_pack_length++;
    pack_reclength++;
  }
  if (pack_reclength < INT_MAX32)
    pack_reclength+= max_field_lengths + long_varchar_count;
  else
    pack_reclength= INT_MAX32;

  if (flags & HA_CREATE_DELAY_KEY_WRITE)
    options|= HA_OPTION_DELAY_KEY_WRITE;
  if (flags & HA_CREATE_RELIES_ON_SQL_LAYER)
    options|= HA_OPTION_RELIES_ON_SQL_LAYER;
  if (flags & HA_CREATE_PAGE_CHECKSUM)
    options|= HA_OPTION_PAGE_CHECKSUM;

  pack_bytes= (packed + 7) / 8;
  if (pack_reclength != INT_MAX32)
    pack_reclength+= reclength+pack_bytes +
      test(test_all_bits(options, HA_OPTION_CHECKSUM | HA_OPTION_PACK_RECORD));
  min_pack_length+= pack_bytes;
  /* Calculate min possible row length for rows-in-block */
  extra_header_size= MAX_FIXED_HEADER_SIZE;
  if (ci->transactional)
  {
    extra_header_size= TRANS_MAX_FIXED_HEADER_SIZE;
    DBUG_PRINT("info",("creating a transactional table"));
  }
  share.base.min_block_length= (extra_header_size + share.base.null_bytes +
                                pack_bytes);
  if (!ci->data_file_length && ci->max_rows)
  {
    if (pack_reclength == INT_MAX32 ||
             (~(ulonglong) 0)/ci->max_rows < (ulonglong) pack_reclength)
      ci->data_file_length= ~(ulonglong) 0;
    else
      ci->data_file_length=(ulonglong) ci->max_rows*pack_reclength;
  }
  else if (!ci->max_rows)
  {
    if (datafile_type == BLOCK_RECORD)
    {
      uint rows_per_page= ((maria_block_size - PAGE_OVERHEAD_SIZE) /
                           (min_pack_length + extra_header_size +
                            DIR_ENTRY_SIZE));
      ulonglong data_file_length= ci->data_file_length;
      if (!data_file_length)
        data_file_length= ((((ulonglong) 1 << ((BLOCK_RECORD_POINTER_SIZE-1) *
                                               8)) -1) * maria_block_size);
      if (rows_per_page > 0)
      {
        set_if_smaller(rows_per_page, MAX_ROWS_PER_PAGE);
        ci->max_rows= data_file_length / maria_block_size * rows_per_page;
      }
      else
        ci->max_rows= data_file_length / (min_pack_length +
                                          extra_header_size +
                                          DIR_ENTRY_SIZE);
    }
    else
      ci->max_rows=(ha_rows) (ci->data_file_length/(min_pack_length +
                                                    ((options &
                                                      HA_OPTION_PACK_RECORD) ?
                                                     3 : 0)));
  }
  max_rows= (ulonglong) ci->max_rows;
  if (datafile_type == BLOCK_RECORD)
  {
    /*
      The + 1 is for record position withing page
      The / 2 is because we need one bit for knowing if there is transid's
      after the row pointer
    */
    pointer= maria_get_pointer_length((ci->data_file_length /
                                       (maria_block_size * 2)), 3) + 1;
    set_if_smaller(pointer, BLOCK_RECORD_POINTER_SIZE);

    if (!max_rows)
      max_rows= (((((ulonglong) 1 << ((pointer-1)*8)) -1) * maria_block_size) /
                 min_pack_length / 2);
                                      }
  else
  {
    if (datafile_type != STATIC_RECORD)
      pointer= maria_get_pointer_length(ci->data_file_length,
                                        maria_data_pointer_size);
    else
      pointer= maria_get_pointer_length(ci->max_rows, maria_data_pointer_size);
    if (!max_rows)
      max_rows= ((((ulonglong) 1 << (pointer*8)) -1) / min_pack_length);
  }

  real_reclength=reclength;
  if (datafile_type == STATIC_RECORD)
  {
    if (reclength <= pointer)
      reclength=pointer+1;		/* reserve place for delete link */
  }
  else
    reclength+= long_varchar_count;	/* We need space for varchar! */

  max_key_length=0; tot_length=0 ; key_segs=0;
  fulltext_keys=0;
  share.state.rec_per_key_part=   rec_per_key_part;
  share.state.nulls_per_key_part= nulls_per_key_part;
  share.state.key_root=key_root;
  share.state.key_del= HA_OFFSET_ERROR;
  if (uniques)
    max_key_length= MARIA_UNIQUE_HASH_LENGTH + pointer;

  for (i=0, keydef=keydefs ; i < keys ; i++ , keydef++)
  {
    share.state.key_root[i]= HA_OFFSET_ERROR;
    length= real_length_diff= 0;
    min_key_length= key_length= pointer;

    if (keydef->key_alg == HA_KEY_ALG_RTREE)
      keydef->flag|= HA_RTREE_INDEX;            /* For easier tests */

    if (keydef->flag & HA_SPATIAL)
    {
#ifdef HAVE_SPATIAL
      /* BAR TODO to support 3D and more dimensions in the future */
      uint sp_segs=SPDIMS*2;
      keydef->flag=HA_SPATIAL;

      if (flags & HA_DONT_TOUCH_DATA)
      {
        /*
          Called by maria_chk - i.e. table structure was taken from
          MYI file and SPATIAL key *does have* additional sp_segs keysegs.
          keydef->seg here points right at the GEOMETRY segment,
          so we only need to decrease keydef->keysegs.
          (see maria_recreate_table() in _ma_check.c)
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
          my_errno=HA_WRONG_CREATE_OPTION;
          goto err_no_lock;
        }
      }
      keydef->keysegs+=sp_segs;
      key_length+=SPLEN*sp_segs;
      length++;                              /* At least one length uchar */
      min_key_length++;
#else
      my_errno= HA_ERR_UNSUPPORTED;
      goto err_no_lock;
#endif /*HAVE_SPATIAL*/
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
          my_errno=HA_WRONG_CREATE_OPTION;
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
      length++;                              /* At least one length uchar */
      min_key_length+= 1 + HA_FT_WLEN;
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
          DBUG_ASSERT(!(keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)));
	  keydef->flag |= HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY;
	  options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	  length++;				/* At least one length uchar */
          if (!keyseg->null_bit)
            min_key_length++;
          key_length+= keyseg->length;
	  if (keyseg->length >= 255)
	  {
            /* prefix may be 3 bytes */
	    length+= 2;
	  }
	}
	else if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
	{
          DBUG_ASSERT(!test_all_bits(keyseg->flag,
                                    (HA_VAR_LENGTH_PART | HA_BLOB_PART)));
	  keydef->flag|=HA_VAR_LENGTH_KEY;
	  length++;				/* At least one length uchar */
          if (!keyseg->null_bit)
            min_key_length++;
	  options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
          key_length+= keyseg->length;
	  if (keyseg->length >= 255)
	  {
            /* prefix may be 3 bytes */
	    length+= 2;
	  }
	}
        else
        {
          key_length+= keyseg->length;
          if (!keyseg->null_bit)
            min_key_length+= keyseg->length;
        }
	if (keyseg->null_bit)
	{
	  key_length++;
          /* min key part is 1 byte */
          min_key_length++;
	  options|=HA_OPTION_PACK_KEYS;
	  keyseg->flag|=HA_NULL_PART;
	  keydef->flag|=HA_VAR_LENGTH_KEY | HA_NULL_PART_KEY;
	}
      }
    } /* if HA_FULLTEXT */
    key_segs+=keydef->keysegs;
    if (keydef->keysegs > HA_MAX_KEY_SEG)
    {
      my_errno=HA_WRONG_CREATE_OPTION;
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
    /*
      A key can't be longer than than half a index block (as we have
      to be able to put at least 2 keys on an index block for the key
      algorithms to work).
    */
    if (length > maria_max_key_length())
    {
      my_errno=HA_WRONG_CREATE_OPTION;
      goto err_no_lock;
    }
    keydef->block_length= (uint16) maria_block_size;
    keydef->keylength= (uint16) key_length;
    keydef->minlength= (uint16) min_key_length;
    keydef->maxlength= (uint16) length;

    if (length > max_key_length)
      max_key_length= length;
    tot_length+= ((max_rows/(ulong) (((uint) maria_block_size -
                                      MAX_KEYPAGE_HEADER_SIZE -
                                      KEYPAGE_CHECKSUM_SIZE)/
                                     (length*2))) *
                  maria_block_size);
  }

  unique_key_parts=0;
  for (i=0, uniquedef=uniquedefs ; i < uniques ; i++ , uniquedef++)
  {
    uniquedef->key=keys+i;
    unique_key_parts+=uniquedef->keysegs;
    share.state.key_root[keys+i]= HA_OFFSET_ERROR;
    tot_length+= (max_rows/(ulong) (((uint) maria_block_size -
                                     MAX_KEYPAGE_HEADER_SIZE -
                                     KEYPAGE_CHECKSUM_SIZE) /
                         ((MARIA_UNIQUE_HASH_LENGTH + pointer)*2)))*
                         (ulong) maria_block_size;
  }
  keys+=uniques;				/* Each unique has 1 key */
  key_segs+=uniques;				/* Each unique has 1 key seg */

  base_pos=(MARIA_STATE_INFO_SIZE + keys * MARIA_STATE_KEY_SIZE +
	    key_segs * MARIA_STATE_KEYSEG_SIZE);
  info_length= base_pos+(uint) (MARIA_BASE_INFO_SIZE+
                                keys * MARIA_KEYDEF_SIZE+
                                uniques * MARIA_UNIQUEDEF_SIZE +
                                (key_segs + unique_key_parts)*HA_KEYSEG_SIZE+
                                columns*(MARIA_COLUMNDEF_SIZE + 2));

 DBUG_PRINT("info", ("info_length: %u", info_length));
  /* There are only 16 bits for the total header length. */
  if (info_length > 65535)
  {
    my_printf_error(HA_WRONG_CREATE_OPTION,
                    "Maria table '%s' has too many columns and/or "
                    "indexes and/or unique constraints.",
                    MYF(0), name + dirname_length(name));
    my_errno= HA_WRONG_CREATE_OPTION;
    goto err_no_lock;
  }

  bmove(share.state.header.file_version, maria_file_magic, 4);
  ci->old_options=options | (ci->old_options & HA_OPTION_TEMP_COMPRESS_RECORD ?
                             HA_OPTION_COMPRESS_RECORD |
                             HA_OPTION_TEMP_COMPRESS_RECORD: 0);
  mi_int2store(share.state.header.options,ci->old_options);
  mi_int2store(share.state.header.header_length,info_length);
  mi_int2store(share.state.header.state_info_length,MARIA_STATE_INFO_SIZE);
  mi_int2store(share.state.header.base_info_length,MARIA_BASE_INFO_SIZE);
  mi_int2store(share.state.header.base_pos,base_pos);
  share.state.header.data_file_type= share.data_file_type= datafile_type;
  share.state.header.org_data_file_type= org_datafile_type;
  share.state.header.language= (ci->language ?
				ci->language : default_charset_info->number);

  share.state.dellink = HA_OFFSET_ERROR;
  share.state.first_bitmap_with_space= 0;
#ifdef EXTERNAL_LOCKING
  share.state.process=	(ulong) getpid();
#endif
  share.state.version=	(ulong) time((time_t*) 0);
  share.state.sortkey=  (ushort) ~0;
  share.state.auto_increment=ci->auto_increment;
  share.options=options;
  share.base.rec_reflength=pointer;
  share.base.block_size= maria_block_size;

  /*
    Get estimate for index file length (this may be wrong for FT keys)
    This is used for pointers to other key pages.
  */
  tmp= (tot_length + maria_block_size * keys *
	MARIA_INDEX_BLOCK_MARGIN) / maria_block_size;

  /*
    use maximum of key_file_length we calculated and key_file_length value we
    got from MAI file header (see also mariapack.c:save_state)
  */
  share.base.key_reflength=
    maria_get_pointer_length(max(ci->key_file_length,tmp),3);
  share.base.keys= share.state.header.keys= keys;
  share.state.header.uniques= uniques;
  share.state.header.fulltext_keys= fulltext_keys;
  mi_int2store(share.state.header.key_parts,key_segs);
  mi_int2store(share.state.header.unique_key_parts,unique_key_parts);

  maria_set_all_keys_active(share.state.key_map, keys);

  share.base.keystart = share.state.state.key_file_length=
    MY_ALIGN(info_length, maria_block_size);
  share.base.max_key_block_length= maria_block_size;
  share.base.max_key_length=ALIGN_SIZE(max_key_length+4);
  share.base.records=ci->max_rows;
  share.base.reloc=  ci->reloc_rows;
  share.base.reclength=real_reclength;
  share.base.pack_reclength=reclength+ test(options & HA_OPTION_CHECKSUM);
  share.base.max_pack_length=pack_reclength;
  share.base.min_pack_length=min_pack_length;
  share.base.pack_bytes= pack_bytes;
  share.base.fields= columns;
  share.base.pack_fields= packed;

  if (share.data_file_type == BLOCK_RECORD)
  {
    /*
      we are going to create a first bitmap page, set data_file_length
      to reflect this, before the state goes to disk
    */
    share.state.state.data_file_length= maria_block_size;
    /* Add length of packed fields + length */
    share.base.pack_reclength+= share.base.max_field_lengths+3;

    /* Adjust max_pack_length, to be used if we have short rows */
    if (share.base.max_pack_length < maria_block_size)
    {
      share.base.max_pack_length+= FLAG_SIZE;
      if (ci->transactional)
        share.base.max_pack_length+= TRANSID_SIZE * 2;
    }
  }

  /* max_data_file_length and max_key_file_length are recalculated on open */
  if (tmp_table)
    share.base.max_data_file_length= (my_off_t) ci->data_file_length;
  else if (ci->transactional && translog_status == TRANSLOG_OK &&
           !maria_in_recovery)
  {
    /*
      we have checked translog_inited above, because maria_chk may call us
      (via maria_recreate_table()) and it does not have a log.
    */
    sync_dir= MY_SYNC_DIR;
    /*
      If crash between _ma_state_info_write_sub() and
      _ma_update_state__lsns_sub(), table should be ignored by Recovery (or
      old REDOs would fail), so we cannot let LSNs be 0:
    */
    share.state.skip_redo_lsn= share.state.is_of_horizon=
      share.state.create_rename_lsn= LSN_MAX;
  }

  if (datafile_type == DYNAMIC_RECORD)
  {
    share.base.min_block_length=
      (share.base.pack_reclength+3 < MARIA_EXTEND_BLOCK_LENGTH &&
       ! share.base.blobs) ?
      max(share.base.pack_reclength,MARIA_MIN_BLOCK_LENGTH) :
      MARIA_EXTEND_BLOCK_LENGTH;
  }
  else if (datafile_type == STATIC_RECORD)
    share.base.min_block_length= share.base.pack_reclength;

  if (! (flags & HA_DONT_TOUCH_DATA))
    share.state.create_time= time((time_t*) 0);

  pthread_mutex_lock(&THR_LOCK_maria);

  /*
    NOTE: For test_if_reopen() we need a real path name. Hence we need
    MY_RETURN_REAL_PATH for every fn_format(filename, ...).
  */
  if (ci->index_file_name)
  {
    char *iext= strrchr(ci->index_file_name, '.');
    int have_iext= iext && !strcmp(iext, MARIA_NAME_IEXT);
    if (tmp_table)
    {
      char *path;
      /* chop off the table name, tempory tables use generated name */
      if ((path= strrchr(ci->index_file_name, FN_LIBCHAR)))
        *path= '\0';
      fn_format(filename, name, ci->index_file_name, MARIA_NAME_IEXT,
                MY_REPLACE_DIR | MY_UNPACK_FILENAME |
                MY_RETURN_REAL_PATH | MY_APPEND_EXT);
    }
    else
    {
      fn_format(filename, ci->index_file_name, "", MARIA_NAME_IEXT,
                MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH |
                (have_iext ? MY_REPLACE_EXT : MY_APPEND_EXT));
    }
    fn_format(linkname, name, "", MARIA_NAME_IEXT,
              MY_UNPACK_FILENAME|MY_APPEND_EXT);
    linkname_ptr= linkname;
    /*
      Don't create the table if the link or file exists to ensure that one
      doesn't accidently destroy another table.
      Don't sync dir now if the data file has the same path.
    */
    create_flag=
      (ci->data_file_name &&
       !strcmp(ci->index_file_name, ci->data_file_name)) ? 0 : sync_dir;
  }
  else
  {
    char *iext= strrchr(name, '.');
    int have_iext= iext && !strcmp(iext, MARIA_NAME_IEXT);
    fn_format(filename, name, "", MARIA_NAME_IEXT,
              MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH |
              (have_iext ? MY_REPLACE_EXT : MY_APPEND_EXT));
    linkname_ptr= NullS;
    /*
      Replace the current file.
      Don't sync dir now if the data file has the same path.
    */
    create_flag=  (flags & HA_CREATE_KEEP_FILES) ? 0 : MY_DELETE_OLD;
    create_flag|= (!ci->data_file_name ? 0 : sync_dir);
  }

  /*
    If a MRG_MARIA table is in use, the mapped MARIA tables are open,
    but no entry is made in the table cache for them.
    A TRUNCATE command checks for the table in the cache only and could
    be fooled to believe, the table is not open.
    Pull the emergency brake in this situation. (Bug #8306)


    NOTE: The filename is compared against unique_file_name of every
    open table. Hence we need a real path here.
  */
  if (_ma_test_if_reopen(filename))
  {
    my_printf_error(0, "MARIA table '%s' is in use "
                    "(most likely by a MERGE table). Try FLUSH TABLES.",
                    MYF(0), name + dirname_length(name));
    my_errno= HA_ERR_TABLE_EXIST;
    goto err;
  }

  if ((file= my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
				    MYF(MY_WME|create_flag))) < 0)
    goto err;
  errpos=1;

  DBUG_PRINT("info", ("write state info and base info"));
  if (_ma_state_info_write_sub(file, &share.state,
                               MA_STATE_INFO_WRITE_FULL_INFO) ||
      _ma_base_info_write(file, &share.base))
    goto err;
  DBUG_PRINT("info", ("base_pos: %d  base_info_size: %d",
                      base_pos, MARIA_BASE_INFO_SIZE));
  DBUG_ASSERT(my_tell(file,MYF(0)) == base_pos+ MARIA_BASE_INFO_SIZE);

  /* Write key and keyseg definitions */
  DBUG_PRINT("info", ("write key and keyseg definitions"));
  for (i=0 ; i < share.base.keys - uniques; i++)
  {
    uint sp_segs=(keydefs[i].flag & HA_SPATIAL) ? 2*SPDIMS : 0;

    if (_ma_keydef_write(file, &keydefs[i]))
      goto err;
    for (j=0 ; j < keydefs[i].keysegs-sp_segs ; j++)
      if (_ma_keyseg_write(file, &keydefs[i].seg[j]))
       goto err;
#ifdef HAVE_SPATIAL
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
      if (_ma_keyseg_write(file, &sseg))
        goto err;
    }
#endif
  }
  /* Create extra keys for unique definitions */
  offset= real_reclength - uniques*MARIA_UNIQUE_HASH_LENGTH;
  bzero((char*) &tmp_keydef,sizeof(tmp_keydef));
  bzero((char*) &tmp_keyseg,sizeof(tmp_keyseg));
  for (i=0; i < uniques ; i++)
  {
    tmp_keydef.keysegs=1;
    tmp_keydef.flag=		HA_UNIQUE_CHECK;
    tmp_keydef.block_length=	(uint16) maria_block_size;
    tmp_keydef.keylength=	MARIA_UNIQUE_HASH_LENGTH + pointer;
    tmp_keydef.minlength=tmp_keydef.maxlength=tmp_keydef.keylength;
    tmp_keyseg.type=		MARIA_UNIQUE_HASH_TYPE;
    tmp_keyseg.length=		MARIA_UNIQUE_HASH_LENGTH;
    tmp_keyseg.start=		offset;
    offset+=			MARIA_UNIQUE_HASH_LENGTH;
    if (_ma_keydef_write(file,&tmp_keydef) ||
	_ma_keyseg_write(file,(&tmp_keyseg)))
      goto err;
  }

  /* Save unique definition */
  DBUG_PRINT("info", ("write unique definitions"));
  for (i=0 ; i < share.state.header.uniques ; i++)
  {
    HA_KEYSEG *keyseg_end;
    keyseg= uniquedefs[i].seg;
    if (_ma_uniquedef_write(file, &uniquedefs[i]))
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
        DBUG_ASSERT((keyseg->flag & HA_VAR_LENGTH_PART) == 0);
        break;
      }
      if (_ma_keyseg_write(file, keyseg))
	goto err;
    }
  }
  DBUG_PRINT("info", ("write field definitions"));
  if (datafile_type == BLOCK_RECORD)
  {
    /* Store columns in a more efficent order */
    MARIA_COLUMNDEF **col_order, **pos;
    if (!(col_order= (MARIA_COLUMNDEF**) my_malloc(share.base.fields *
                                                   sizeof(MARIA_COLUMNDEF*),
                                                   MYF(MY_WME))))
      goto err;
    for (column= columndef, pos= col_order ;
         column != end_column ;
         column++, pos++)
      *pos= column;
    qsort(col_order, share.base.fields, sizeof(*col_order),
          (qsort_cmp) compare_columns);
    for (i=0 ; i < share.base.fields ; i++)
    {
      column_array[col_order[i]->column_nr]= i;
      if (_ma_columndef_write(file, col_order[i]))
      {
        my_free(col_order, MYF(0));
        goto err;
      }
    }
    my_free(col_order, MYF(0));
  }
  else
  {
    for (i=0 ; i < share.base.fields ; i++)
    {
      column_array[i]= (uint16) i;
      if (_ma_columndef_write(file, &columndef[i]))
        goto err;
    }
  }
  if (_ma_column_nr_write(file, column_array, columns))
    goto err;

  if ((kfile_size_before_extension= my_tell(file,MYF(0))) == MY_FILEPOS_ERROR)
    goto err;
#ifndef DBUG_OFF
  if (kfile_size_before_extension != info_length)
    DBUG_PRINT("warning",("info_length: %u  != used_length: %u",
			  info_length, (uint)kfile_size_before_extension));
#endif

  if (sync_dir)
  {
    /*
      we log the first bytes and then the size to which we extend; this is
      not log 1 KB of mostly zeroes if this is a small table.
    */
    char empty_string[]= "";
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
    translog_size_t total_rec_length= 0;
    uint k;
    LSN lsn;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= 1 + 2 + 2 +
      (uint) kfile_size_before_extension;
    /* we are needing maybe 64 kB, so don't use the stack */
    log_data= my_malloc(log_array[TRANSLOG_INTERNAL_PARTS + 1].length, MYF(0));
    if ((log_data == NULL) ||
        my_pread(file, 1 + 2 + 2 + log_data,
                 (size_t) kfile_size_before_extension, 0, MYF(MY_NABP)))
      goto err;
    /*
      remember if the data file was created or not, to know if Recovery can
      do it or not, in the future
    */
    log_data[0]= test(flags & HA_DONT_TOUCH_DATA);
    int2store(log_data + 1, kfile_size_before_extension);
    int2store(log_data + 1 + 2, share.base.keystart);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str= (uchar *)name;
    /* we store the end-zero, for Recovery to just pass it to my_create() */
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= strlen(name) + 1;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str= log_data;
    /* symlink description is also needed for re-creation by Recovery: */
    {
      const char *s= ci->data_file_name ? ci->data_file_name : empty_string;
      log_array[TRANSLOG_INTERNAL_PARTS + 2].str= (uchar*)s;
      log_array[TRANSLOG_INTERNAL_PARTS + 2].length= strlen(s) + 1;
      s= ci->index_file_name ? ci->index_file_name : empty_string;
      log_array[TRANSLOG_INTERNAL_PARTS + 3].str= (uchar*)s;
      log_array[TRANSLOG_INTERNAL_PARTS + 3].length= strlen(s) + 1;
    }
    for (k= TRANSLOG_INTERNAL_PARTS;
         k < (sizeof(log_array)/sizeof(log_array[0])); k++)
      total_rec_length+= (translog_size_t) log_array[k].length;
    /**
       For this record to be of any use for Recovery, we need the upper
       MySQL layer to be crash-safe, which it is not now (that would require
       work using the ddl_log of sql/sql_table.cc); when it is, we should
       reconsider the moment of writing this log record (before or after op,
       under THR_LOCK_maria or not...), how to use it in Recovery.
       For now this record can serve when we apply logs to a backup,
       so we sync it. This happens before the data file is created. If the
       data file was created before, and we crashed before writing the log
       record, at restart the table may be used, so we would not have a
       trustable history in the log (impossible to apply this log to a
       backup). The way we do it, if we crash before writing the log record
       then there is no data file and the table cannot be used.
       @todo Note that in case of TRUNCATE TABLE we also come here; for
       Recovery to be able to finish TRUNCATE TABLE, instead of leaving a
       half-truncated table, we should log the record at start of
       maria_create(); for that we shouldn't write to the index file but to a
       buffer (DYNAMIC_STRING), put the buffer into the record, then put the
       buffer into the index file (so, change _ma_keydef_write() etc). That
       would also enable Recovery to finish a CREATE TABLE. The final result
       would be that we would be able to finish what the SQL layer has asked
       for: it would be atomic.
       When in CREATE/TRUNCATE (or DROP or RENAME or REPAIR) we have not
       called external_lock(), so have no TRN. It does not matter, as all
       these operations are non-transactional and sync their files.
    */
    if (unlikely(translog_write_record(&lsn,
                                       LOGREC_REDO_CREATE_TABLE,
                                       &dummy_transaction_object, NULL,
                                       total_rec_length,
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, NULL, NULL) ||
                 translog_flush(lsn)))
      goto err;
    share.kfile.file= file;
    DBUG_EXECUTE_IF("maria_flush_whole_log",
                    {
                      DBUG_PRINT("maria_flush_whole_log", ("now"));
                      translog_flush(translog_get_horizon());
                    });
    DBUG_EXECUTE_IF("maria_crash_create_table",
                    {
                      DBUG_PRINT("maria_crash_create_table", ("now"));
                      DBUG_ABORT();
                    });
    /*
      store LSN into file, needed for Recovery to not be confused if a
      DROP+CREATE happened (applying REDOs to the wrong table).
    */
    if (_ma_update_state_lsns_sub(&share, lsn, trnman_get_min_safe_trid(),
                                  FALSE, TRUE))
      goto err;
    my_free(log_data, MYF(0));
  }

  if (!(flags & HA_DONT_TOUCH_DATA))
  {
    if (ci->data_file_name)
    {
      char *dext= strrchr(ci->data_file_name, '.');
      int have_dext= dext && !strcmp(dext, MARIA_NAME_DEXT);

      if (tmp_table)
      {
        char *path;
        /* chop off the table name, tempory tables use generated name */
        if ((path= strrchr(ci->data_file_name, FN_LIBCHAR)))
          *path= '\0';
        fn_format(filename, name, ci->data_file_name, MARIA_NAME_DEXT,
                  MY_REPLACE_DIR | MY_UNPACK_FILENAME | MY_APPEND_EXT);
      }
      else
      {
        fn_format(filename, ci->data_file_name, "", MARIA_NAME_DEXT,
                  MY_UNPACK_FILENAME |
                  (have_dext ? MY_REPLACE_EXT : MY_APPEND_EXT));
      }
      fn_format(linkname, name, "",MARIA_NAME_DEXT,
                MY_UNPACK_FILENAME | MY_APPEND_EXT);
      linkname_ptr= linkname;
      create_flag=0;
    }
    else
    {
      fn_format(filename,name,"", MARIA_NAME_DEXT,
                MY_UNPACK_FILENAME | MY_APPEND_EXT);
      linkname_ptr= NullS;
      create_flag= (flags & HA_CREATE_KEEP_FILES) ? 0 : MY_DELETE_OLD;
    }
    if ((dfile=
         my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
                                MYF(MY_WME | create_flag | sync_dir))) < 0)
      goto err;
    errpos=3;

    if (_ma_initialize_data_file(&share, dfile))
      goto err;
  }

	/* Enlarge files */
  DBUG_PRINT("info", ("enlarge to keystart: %lu",
                      (ulong) share.base.keystart));
  if (my_chsize(file,(ulong) share.base.keystart,0,MYF(0)))
    goto err;

  if (sync_dir && my_sync(file, MYF(0)))
    goto err;

  if (! (flags & HA_DONT_TOUCH_DATA))
  {
#ifdef USE_RELOC
    if (my_chsize(dfile,share.base.min_pack_length*ci->reloc_rows,0,MYF(0)))
      goto err;
#endif
    if (sync_dir && my_sync(dfile, MYF(0)))
      goto err;
    if (my_close(dfile,MYF(0)))
      goto err;
  }
  pthread_mutex_unlock(&THR_LOCK_maria);
  res= 0;
  my_free((char*) rec_per_key_part,MYF(0));
  errpos=0;
  if (my_close(file,MYF(0)))
    res= my_errno;
  DBUG_RETURN(res);

err:
  pthread_mutex_unlock(&THR_LOCK_maria);

err_no_lock:
  save_errno=my_errno;
  switch (errpos) {
  case 3:
    VOID(my_close(dfile,MYF(0)));
    /* fall through */
  case 2:
  if (! (flags & HA_DONT_TOUCH_DATA))
    my_delete_with_symlink(fn_format(filename,name,"",MARIA_NAME_DEXT,
                                     MY_UNPACK_FILENAME | MY_APPEND_EXT),
			   sync_dir);
    /* fall through */
  case 1:
    VOID(my_close(file,MYF(0)));
    if (! (flags & HA_DONT_TOUCH_DATA))
      my_delete_with_symlink(fn_format(filename,name,"",MARIA_NAME_IEXT,
                                       MY_UNPACK_FILENAME | MY_APPEND_EXT),
			     sync_dir);
  }
  my_free(log_data, MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) rec_per_key_part, MYF(0));
  DBUG_RETURN(my_errno=save_errno);		/* return the fatal errno */
}


uint maria_get_pointer_length(ulonglong file_length, uint def)
{
  DBUG_ASSERT(def >= 2 && def <= 7);
  if (file_length)				/* If not default */
  {
#ifdef NOT_YET_READY_FOR_8_BYTE_POINTERS
    if (file_length >= (ULL(1) << 56))
      def=8;
    else
#endif
      if (file_length >= (ULL(1) << 48))
      def=7;
    else if (file_length >= (ULL(1) << 40))
      def=6;
    else if (file_length >= (ULL(1) << 32))
      def=5;
    else if (file_length >= (ULL(1) << 24))
      def=4;
    else if (file_length >= (ULL(1) << 16))
      def=3;
    else
      def=2;
  }
  return def;
}


/*
  Sort columns for records-in-block

  IMPLEMENTATION
   Sort columns in following order:

   Fixed size, not null columns
   Fixed length, null fields
   Numbers (zero fill fields)
   Variable length fields (CHAR, VARCHAR) according to length
   Blobs

   For same kind of fields, keep fields in original order
*/

static inline int sign(long a)
{
  return a < 0 ? -1 : (a > 0 ? 1 : 0);
}


static int compare_columns(MARIA_COLUMNDEF **a_ptr, MARIA_COLUMNDEF **b_ptr)
{
  MARIA_COLUMNDEF *a= *a_ptr, *b= *b_ptr;
  enum en_fieldtype a_type, b_type;

  a_type= (a->type == FIELD_CHECK) ? FIELD_NORMAL : a->type;
  b_type= (b->type == FIELD_CHECK) ? FIELD_NORMAL : b->type;

  if (a_type == FIELD_NORMAL && !a->null_bit)
  {
    if (b_type != FIELD_NORMAL || b->null_bit)
      return -1;
    return sign((long) a->offset - (long) b->offset);
  }
  if (b_type == FIELD_NORMAL && !b->null_bit)
    return 1;
  if (a_type == b_type)
    return sign((long) a->offset - (long) b->offset);
  if (a_type == FIELD_NORMAL)
    return -1;
  if (b_type == FIELD_NORMAL)
    return 1;
  if (a_type == FIELD_SKIP_ZERO)
    return -1;
  if (b_type == FIELD_SKIP_ZERO)
    return 1;
  if (a->type != FIELD_BLOB && b->type != FIELD_BLOB)
    if (a->length != b->length)
      return sign((long) a->length - (long) b->length);
  if (a_type == FIELD_BLOB)
    return 1;
  if (b_type == FIELD_BLOB)
    return -1;
  return sign((long) a->offset - (long) b->offset);
}


/**
   @brief Initialize data file

   @note
   In BLOCK_RECORD, a freshly created datafile is one page long; while in
   other formats it is 0-byte long.
 */

int _ma_initialize_data_file(MARIA_SHARE *share, File dfile)
{
  if (share->data_file_type == BLOCK_RECORD)
  {
    share->bitmap.block_size= share->base.block_size;
    share->bitmap.file.file = dfile;
    return _ma_bitmap_create_first(share);
  }
  return 0;
}


/**
   @brief Writes create_rename_lsn, skip_redo_lsn and is_of_horizon to disk,
   can force.

   This is for special cases where:
   - we don't want to write the full state to disk (so, not call
   _ma_state_info_write()) because some parts of the state may be
   currently inconsistent, or because it would be overkill
   - we must sync these LSNs immediately for correctness.
   It acquires intern_lock to protect the LSNs and state write.

   @param  share           table's share
   @param  lsn		   LSN to write to log files
   @param  create_trid     Trid to be used as state.create_trid
   @param  do_sync         if the write should be forced to disk
   @param  update_create_rename_lsn if this LSN should be updated or not

   @return Operation status
     @retval 0      ok
     @retval 1      error (disk problem)
*/

int _ma_update_state_lsns(MARIA_SHARE *share, LSN lsn, TrID create_trid,
                          my_bool do_sync, my_bool update_create_rename_lsn)
{
  int res;
  pthread_mutex_lock(&share->intern_lock);
  res= _ma_update_state_lsns_sub(share, lsn, create_trid, do_sync,
                                 update_create_rename_lsn);
  pthread_mutex_unlock(&share->intern_lock);
  return res;
}


/**
   @brief Writes create_rename_lsn, skip_redo_lsn and is_of_horizon to disk,
   can force.

   Shortcut of _ma_update_state_lsns() when we know that intern_lock is not
   needed (when creating a table or opening it for the first time).

   @param  share           table's share
   @param  lsn             LSN to write to state; if LSN_IMPOSSIBLE, write
                           a LOGREC_IMPORTED_TABLE and use its LSN as lsn.
   @param  create_trid     Trid to be used as state.create_trid
   @param  do_sync         if the write should be forced to disk
   @param  update_create_rename_lsn if this LSN should be updated or not

   @return Operation status
     @retval 0      ok
     @retval 1      error (disk problem)
*/

#if (_MSC_VER == 1310)
/*
 Visual Studio 2003 compiler produces internal compiler error
 in this function. Disable optimizations to workaround.
*/
#pragma optimize("",off)
#endif
int _ma_update_state_lsns_sub(MARIA_SHARE *share, LSN lsn, TrID create_trid,
                              my_bool do_sync,
                              my_bool update_create_rename_lsn)
{
  uchar buf[LSN_STORE_SIZE * 3], *ptr;
  uchar trid_buff[8];
  File file= share->kfile.file;
  DBUG_ASSERT(file >= 0);

  if (lsn == LSN_IMPOSSIBLE)
  {
    int res;
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    /* table name is logged only for information */
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=
      (uchar *)(share->open_file_name.str);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length=
      share->open_file_name.length + 1;
    if ((res= translog_write_record(&lsn, LOGREC_IMPORTED_TABLE,
                                    &dummy_transaction_object, NULL,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length,
                                    sizeof(log_array)/sizeof(log_array[0]),
                                    log_array, NULL, NULL)))
      return res;
  }

  for (ptr= buf; ptr < (buf + sizeof(buf)); ptr+= LSN_STORE_SIZE)
    lsn_store(ptr, lsn);
  share->state.skip_redo_lsn= share->state.is_of_horizon= lsn;
  share->state.create_trid= create_trid;
  mi_int8store(trid_buff, create_trid);

  /*
    Update create_rename_lsn if update was requested or if the old one had an
    impossible value.
  */
  if (update_create_rename_lsn ||
      (share->state.create_rename_lsn > lsn && lsn != LSN_IMPOSSIBLE))
  {
    share->state.create_rename_lsn= lsn;
    if (share->id != 0)
    {
      /*
        If OP is the operation which is calling us, if table is later written,
        we could see in the log:
        FILE_ID ... REDO_OP ... REDO_INSERT.
        (that can happen in real life at least with OP=REPAIR).
        As FILE_ID will be ignored by Recovery because it is <
        create_rename_lsn, REDO_INSERT would be ignored too, wrongly.
        To avoid that, we force a LOGREC_FILE_ID to be logged at next write:
      */
      translog_deassign_id_from_share(share);
    }
  }
  else
    lsn_store(buf, share->state.create_rename_lsn);
  return (my_pwrite(file, buf, sizeof(buf),
                    sizeof(share->state.header) +
                    MARIA_FILE_CREATE_RENAME_LSN_OFFSET, MYF(MY_NABP)) ||
          my_pwrite(file, trid_buff, sizeof(trid_buff),
                    sizeof(share->state.header) +
                    MARIA_FILE_CREATE_TRID_OFFSET, MYF(MY_NABP)) ||
          (do_sync && my_sync(file, MYF(0))));
}
#if (_MSC_VER == 1310)
#pragma optimize("",on)
#endif /*VS2003 compiler bug workaround*/
