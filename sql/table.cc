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


/* Some general useful functions */

#include "mysql_priv.h"
#include <errno.h>
#include <m_ctype.h>
#include "md5.h"

	/* Functions defined in this file */

static void frm_error(int error,TABLE *form,const char *name,
                      int errortype, int errarg);
static void fix_type_pointers(const char ***array, TYPELIB *point_to_type,
			      uint types, char **names);
static uint find_field(TABLE *form,uint start,uint length);


static byte* get_field_name(Field **buff,uint *length,
			    my_bool not_used __attribute__((unused)))
{
  *length= (uint) strlen((*buff)->field_name);
  return (byte*) (*buff)->field_name;
}

/*
  Open a .frm file 

  SYNOPSIS
    openfrm()

    name           path to table-file "db/name"
    alias          alias for table
    db_stat        open flags (for example HA_OPEN_KEYFILE|HA_OPEN_RNDFILE..)
                   can be 0 (example in ha_example_table)
    prgflag        READ_ALL etc..
    ha_open_flags  HA_OPEN_ABORT_IF_LOCKED etc..
    outparam       result table

  RETURN VALUES
   0	ok
   1	Error (see frm_error)
   2    Error (see frm_error)
   3    Wrong data in .frm file
   4    Error (see frm_error)
   5    Error (see frm_error: charset unavailable)
   6    Unknown .frm version
*/

int openfrm(THD *thd, const char *name, const char *alias, uint db_stat,
            uint prgflag, uint ha_open_flags, TABLE *outparam)
{
  reg1 uint i;
  reg2 uchar *strpos;
  int	 j,error, errarg= 0;
  uint	 rec_buff_length,n_length,int_length,records,key_parts,keys,
         interval_count,interval_parts,read_length,db_create_options;
  uint	 key_info_length, com_length;
  ulong  pos, record_offset;
  char	 index_file[FN_REFLEN], *names, *keynames, *comment_pos;
  uchar  head[288],*disk_buff,new_field_pack_flag;
  my_string record;
  const char **int_array;
  bool	 use_hash, null_field_first;
  bool   error_reported= FALSE;
  File	 file;
  Field  **field_ptr,*reg_field;
  KEY	 *keyinfo;
  KEY_PART_INFO *key_part;
  uchar *null_pos;
  uint null_bit_pos, new_frm_ver, field_pack_length;
  SQL_CRYPT *crypted=0;
  MEM_ROOT **root_ptr, *old_root;
  TABLE_SHARE *share;
  DBUG_ENTER("openfrm");
  DBUG_PRINT("enter",("name: '%s'  form: 0x%lx",name,outparam));

  error= 1;
  disk_buff= NULL;
  root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
  old_root= *root_ptr;

  bzero((char*) outparam,sizeof(*outparam));
  outparam->in_use= thd;
  outparam->s= share= &outparam->share_not_to_be_used;

  if ((file=my_open(fn_format(index_file, name, "", reg_ext,
			      MY_UNPACK_FILENAME),
		    O_RDONLY | O_SHARE,
		    MYF(0)))
      < 0)
    goto err;

  error= 4;
  if (my_read(file,(byte*) head,64,MYF(MY_NABP)))
    goto err;

  if (memcmp(head, "TYPE=", 5) == 0)
  {
    // new .frm
    my_close(file,MYF(MY_WME));

    if (db_stat & NO_ERR_ON_NEW_FRM)
      DBUG_RETURN(5);
    file= -1;
    // caller can't process new .frm
    goto err;
  }

  share->blob_ptr_size= sizeof(char*);
  outparam->db_stat= db_stat;
  init_sql_alloc(&outparam->mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  *root_ptr= &outparam->mem_root;

  share->table_name= strdup_root(&outparam->mem_root,
                                 name+dirname_length(name));
  share->path= strdup_root(&outparam->mem_root, name);
  outparam->alias= my_strdup(alias, MYF(MY_WME));
  if (!share->table_name || !share->path || !outparam->alias)
    goto err;
  *fn_ext(share->table_name)='\0';		// Remove extension
  *fn_ext(share->path)='\0';                    // Remove extension

  if (head[0] != (uchar) 254 || head[1] != 1)
    goto err;                                   /* purecov: inspected */
  if (head[2] != FRM_VER && head[2] != FRM_VER+1 &&
       ! (head[2] >= FRM_VER+3 && head[2] <= FRM_VER+4))
  {
    error= 6;
    goto err;                                   /* purecov: inspected */
  }
  new_field_pack_flag=head[27];
  new_frm_ver= (head[2] - FRM_VER);
  field_pack_length= new_frm_ver < 2 ? 11 : 17;

  error=3;
  if (!(pos=get_form_pos(file,head,(TYPELIB*) 0)))
    goto err;                                   /* purecov: inspected */
  *fn_ext(index_file)='\0';			// Remove .frm extension

  share->frm_version= head[2];
  /*
    Check if .frm file created by MySQL 5.0. In this case we want to
    display CHAR fields as CHAR and not as VARCHAR.
    We do it this way as we want to keep the old frm version to enable
    MySQL 4.1 to read these files.
  */
  if (share->frm_version == FRM_VER_TRUE_VARCHAR -1 && head[33] == 5)
    share->frm_version= FRM_VER_TRUE_VARCHAR;

  share->db_type= ha_checktype(thd,(enum db_type) (uint) *(head+3),0,0);
  share->db_create_options= db_create_options=uint2korr(head+30);
  share->db_options_in_use= share->db_create_options;
  share->mysql_version= uint4korr(head+51);
  null_field_first= 0;
  if (!head[32])				// New frm file in 3.23
  {
    share->avg_row_length= uint4korr(head+34);
    share-> row_type= (row_type) head[40];
    share->raid_type=   head[41];
    share->raid_chunks= head[42];
    share->raid_chunksize= uint4korr(head+43);
    share->table_charset= get_charset((uint) head[38],MYF(0));
    null_field_first= 1;
  }
  if (!share->table_charset)
  {
    /* unknown charset in head[38] or pre-3.23 frm */
    if (use_mb(default_charset_info))
    {
      /* Warn that we may be changing the size of character columns */
      sql_print_warning("'%s' had no or invalid character set, "
                        "and default character set is multi-byte, "
                        "so character column sizes may have changed",
                        name);
    }
    share->table_charset= default_charset_info;
  }
  share->db_record_offset= 1;
  if (db_create_options & HA_OPTION_LONG_BLOB_PTR)
    share->blob_ptr_size= portable_sizeof_char_ptr;
  /* Set temporarily a good value for db_low_byte_first */
  share->db_low_byte_first= test(share->db_type != DB_TYPE_ISAM);
  error=4;
  share->max_rows= uint4korr(head+18);
  share->min_rows= uint4korr(head+22);

  /* Read keyinformation */
  key_info_length= (uint) uint2korr(head+28);
  VOID(my_seek(file,(ulong) uint2korr(head+6),MY_SEEK_SET,MYF(0)));
  if (read_string(file,(gptr*) &disk_buff,key_info_length))
    goto err;                                   /* purecov: inspected */
  if (disk_buff[0] & 0x80)
  {
    share->keys=      keys=      (disk_buff[1] << 7) | (disk_buff[0] & 0x7f);
    share->key_parts= key_parts= uint2korr(disk_buff+2);
  }
  else
  {
    share->keys=      keys=      disk_buff[0];
    share->key_parts= key_parts= disk_buff[1];
  }
  share->keys_for_keyread.init(0);
  share->keys_in_use.init(keys);
  outparam->quick_keys.init();
  outparam->used_keys.init();
  outparam->keys_in_use_for_query.init();

  n_length=keys*sizeof(KEY)+key_parts*sizeof(KEY_PART_INFO);
  if (!(keyinfo = (KEY*) alloc_root(&outparam->mem_root,
				    n_length+uint2korr(disk_buff+4))))
    goto err;                                   /* purecov: inspected */
  bzero((char*) keyinfo,n_length);
  outparam->key_info=keyinfo;
  key_part= my_reinterpret_cast(KEY_PART_INFO*) (keyinfo+keys);
  strpos=disk_buff+6;

  ulong *rec_per_key;
  if (!(rec_per_key= (ulong*) alloc_root(&outparam->mem_root,
					 sizeof(ulong*)*key_parts)))
    goto err;

  for (i=0 ; i < keys ; i++, keyinfo++)
  {
    keyinfo->table= outparam;
    if (new_frm_ver >= 3)
    {
      keyinfo->flags=	   (uint) uint2korr(strpos) ^ HA_NOSAME;
      keyinfo->key_length= (uint) uint2korr(strpos+2);
      keyinfo->key_parts=  (uint) strpos[4];
      keyinfo->algorithm=  (enum ha_key_alg) strpos[5];
      strpos+=8;
    }
    else
    {
      keyinfo->flags=	 ((uint) strpos[0]) ^ HA_NOSAME;
      keyinfo->key_length= (uint) uint2korr(strpos+1);
      keyinfo->key_parts=  (uint) strpos[3];
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      strpos+=4;
    }

    keyinfo->key_part=	 key_part;
    keyinfo->rec_per_key= rec_per_key;
    for (j=keyinfo->key_parts ; j-- ; key_part++)
    {
      *rec_per_key++=0;
      key_part->fieldnr=	(uint16) (uint2korr(strpos) & FIELD_NR_MASK);
      key_part->offset= (uint) uint2korr(strpos+2)-1;
      key_part->key_type=	(uint) uint2korr(strpos+5);
      // key_part->field=	(Field*) 0;	// Will be fixed later
      if (new_frm_ver >= 1)
      {
	key_part->key_part_flag= *(strpos+4);
	key_part->length=	(uint) uint2korr(strpos+7);
	strpos+=9;
      }
      else
      {
	key_part->length=	*(strpos+4);
	key_part->key_part_flag=0;
	if (key_part->length > 128)
	{
	  key_part->length&=127;		/* purecov: inspected */
	  key_part->key_part_flag=HA_REVERSE_SORT; /* purecov: inspected */
	}
	strpos+=7;
      }
      key_part->store_length=key_part->length;
    }
  }
  keynames=(char*) key_part;
  strpos+= (strmov(keynames, (char *) strpos) - keynames)+1;

  share->reclength = uint2korr((head+16));
  if (*(head+26) == 1)
    share->system= 1;				/* one-record-database */
#ifdef HAVE_CRYPTED_FRM
  else if (*(head+26) == 2)
  {
    *root_ptr= old_root
    crypted=get_crypt_for_frm();
    *root_ptr= &outparam->mem_root;
    outparam->crypted=1;
  }
#endif

  record_offset= (ulong) (uint2korr(head+6)+
                          ((uint2korr(head+14) == 0xffff ?
                            uint4korr(head+47) : uint2korr(head+14))));
 
  if ((n_length= uint2korr(head+55)))
  {
    /* Read extra data segment */
    char *buff, *next_chunk, *buff_end;
    if (!(next_chunk= buff= my_malloc(n_length, MYF(MY_WME))))
      goto err;
    if (my_pread(file, (byte*)buff, n_length, record_offset + share->reclength,
                 MYF(MY_NABP)))
    {
      my_free(buff, MYF(0));
      goto err;
    }
    share->connect_string.length= uint2korr(buff);
    if (! (share->connect_string.str= strmake_root(&outparam->mem_root,
            next_chunk + 2, share->connect_string.length)))
    {
      my_free(buff, MYF(0));
      goto err;
    }
    next_chunk+= share->connect_string.length + 2;
    buff_end= buff + n_length;
    if (next_chunk + 2 < buff_end)
    {
      uint str_db_type_length= uint2korr(next_chunk);
      share->db_type= ha_resolve_by_name(next_chunk + 2, str_db_type_length);
      DBUG_PRINT("enter", ("Setting dbtype to: %d - %d - '%.*s'\n", share->db_type,
            str_db_type_length, str_db_type_length, next_chunk + 2));
      next_chunk+= str_db_type_length + 2;
    }
    my_free(buff, MYF(0));
  }
  /* Allocate handler */
  if (!(outparam->file= get_new_handler(outparam, share->db_type)))
    goto err;

  error=4;
  outparam->reginfo.lock_type= TL_UNLOCK;
  outparam->current_lock=F_UNLCK;
  if ((db_stat & HA_OPEN_KEYFILE) || (prgflag & DELAYED_OPEN))
    records=2;
  else
    records=1;
  if (prgflag & (READ_ALL+EXTRA_RECORD))
    records++;
  /* QQ: TODO, remove the +1 from below */
  rec_buff_length= ALIGN_SIZE(share->reclength + 1 +
                              outparam->file->extra_rec_buf_length());
  share->rec_buff_length= rec_buff_length;
  if (!(record= (char *) alloc_root(&outparam->mem_root,
                                    rec_buff_length * records)))
    goto err;                                   /* purecov: inspected */
  share->default_values= (byte *) record;

  if (my_pread(file,(byte*) record, (uint) share->reclength,
               record_offset, MYF(MY_NABP)))
    goto err; /* purecov: inspected */

  if (records == 1)
  {
    /* We are probably in hard repair, and the buffers should not be used */
    outparam->record[0]= outparam->record[1]= share->default_values;
  }
  else
  {
    outparam->record[0]= (byte *) record+ rec_buff_length;
    if (records > 2)
      outparam->record[1]= (byte *) record+ rec_buff_length*2;
    else
      outparam->record[1]= outparam->record[0];   // Safety
  }
 
#ifdef HAVE_purify
  /*
    We need this because when we read var-length rows, we are not updating
    bytes after end of varchar
  */
  if (records > 1)
  {
    memcpy(outparam->record[0], share->default_values, rec_buff_length);
    if (records > 2)
      memcpy(outparam->record[1], share->default_values, rec_buff_length);
  }
#endif
  VOID(my_seek(file,pos,MY_SEEK_SET,MYF(0)));
  if (my_read(file,(byte*) head,288,MYF(MY_NABP)))
    goto err;
#ifdef HAVE_CRYPTED_FRM
  if (crypted)
  {
    crypted->decode((char*) head+256,288-256);
    if (sint2korr(head+284) != 0)		// Should be 0
      goto err;                                 // Wrong password
  }
#endif

  share->fields= uint2korr(head+258);
  pos= uint2korr(head+260);			/* Length of all screens */
  n_length= uint2korr(head+268);
  interval_count= uint2korr(head+270);
  interval_parts= uint2korr(head+272);
  int_length= uint2korr(head+274);
  share->null_fields= uint2korr(head+282);
  com_length= uint2korr(head+284);
  share->comment= strdup_root(&outparam->mem_root, (char*) head+47);

  DBUG_PRINT("info",("i_count: %d  i_parts: %d  index: %d  n_length: %d  int_length: %d  com_length: %d", interval_count,interval_parts, share->keys,n_length,int_length, com_length));

  if (!(field_ptr = (Field **)
	alloc_root(&outparam->mem_root,
		   (uint) ((share->fields+1)*sizeof(Field*)+
			   interval_count*sizeof(TYPELIB)+
			   (share->fields+interval_parts+
			    keys+3)*sizeof(my_string)+
			   (n_length+int_length+com_length)))))
    goto err;                                   /* purecov: inspected */

  outparam->field=field_ptr;
  read_length=(uint) (share->fields * field_pack_length +
		      pos+ (uint) (n_length+int_length+com_length));
  if (read_string(file,(gptr*) &disk_buff,read_length))
    goto err;                                   /* purecov: inspected */
#ifdef HAVE_CRYPTED_FRM
  if (crypted)
  {
    crypted->decode((char*) disk_buff,read_length);
    delete crypted;
    crypted=0;
  }
#endif
  strpos= disk_buff+pos;

  share->intervals= (TYPELIB*) (field_ptr+share->fields+1);
  int_array= (const char **) (share->intervals+interval_count);
  names= (char*) (int_array+share->fields+interval_parts+keys+3);
  if (!interval_count)
    share->intervals= 0;			// For better debugging
  memcpy((char*) names, strpos+(share->fields*field_pack_length),
	 (uint) (n_length+int_length));
  comment_pos= names+(n_length+int_length);
  memcpy(comment_pos, disk_buff+read_length-com_length, com_length);

  fix_type_pointers(&int_array, &share->fieldnames, 1, &names);
  fix_type_pointers(&int_array, share->intervals, interval_count,
		    &names);

  {
    /* Set ENUM and SET lengths */
    TYPELIB *interval;
    for (interval= share->intervals;
         interval < share->intervals + interval_count;
         interval++)
    {
      uint count= (uint) (interval->count + 1) * sizeof(uint);
      if (!(interval->type_lengths= (uint *) alloc_root(&outparam->mem_root,
                                                        count)))
        goto err;
      for (count= 0; count < interval->count; count++)
        interval->type_lengths[count]= strlen(interval->type_names[count]);
      interval->type_lengths[count]= 0;
    }
  }

  if (keynames)
    fix_type_pointers(&int_array, &share->keynames, 1, &keynames);
  VOID(my_close(file,MYF(MY_WME)));
  file= -1;

  record= (char*) outparam->record[0]-1;	/* Fieldstart = 1 */
  if (null_field_first)
  {
    outparam->null_flags=null_pos=(uchar*) record+1;
    null_bit_pos= (db_create_options & HA_OPTION_PACK_RECORD) ? 0 : 1;
    /* null_bytes below is only correct under the condition that
       there are no bit fields.  Correct values is set below after the
       table struct is initialized */
    share->null_bytes= (share->null_fields + null_bit_pos + 7) / 8;
  }
  else
  {
    share->null_bytes= (share->null_fields+7)/8;
    outparam->null_flags= null_pos=
      (uchar*) (record+1+share->reclength-share->null_bytes);
    null_bit_pos= 0;
  }

  use_hash= share->fields >= MAX_FIELDS_BEFORE_HASH;
  if (use_hash)
    use_hash= !hash_init(&share->name_hash,
			 system_charset_info,
			 share->fields,0,0,
			 (hash_get_key) get_field_name,0,0);

  for (i=0 ; i < share->fields; i++, strpos+=field_pack_length, field_ptr++)
  {
    uint pack_flag, interval_nr, unireg_type, recpos, field_length;
    enum_field_types field_type;
    CHARSET_INFO *charset=NULL;
    Field::geometry_type geom_type= Field::GEOM_GEOMETRY;
    LEX_STRING comment;

    if (new_frm_ver >= 3)
    {
      /* new frm file in 4.1 */
      field_length= uint2korr(strpos+3);
      recpos=	    uint3korr(strpos+5);
      pack_flag=    uint2korr(strpos+8);
      unireg_type=  (uint) strpos[10];
      interval_nr=  (uint) strpos[12];
      uint comment_length=uint2korr(strpos+15);
      field_type=(enum_field_types) (uint) strpos[13];

      /* charset and geometry_type share the same byte in frm */
      if (field_type == FIELD_TYPE_GEOMETRY)
      {
#ifdef HAVE_SPATIAL
	geom_type= (Field::geometry_type) strpos[14];
	charset= &my_charset_bin;
#else
	error= 4;  // unsupported field type
	goto err;
#endif
      }
      else
      {
        if (!strpos[14])
          charset= &my_charset_bin;
        else if (!(charset=get_charset((uint) strpos[14], MYF(0))))
        {
          error= 5; // Unknown or unavailable charset
          errarg= (int) strpos[14];
          goto err;
        }
      }
      if (!comment_length)
      {
	comment.str= (char*) "";
	comment.length=0;
      }
      else
      {
	comment.str=    (char*) comment_pos;
	comment.length= comment_length;
	comment_pos+=   comment_length;
      }
    }
    else
    {
      field_length= (uint) strpos[3];
      recpos=	    uint2korr(strpos+4),
      pack_flag=    uint2korr(strpos+6);
      pack_flag&=   ~FIELDFLAG_NO_DEFAULT;     // Safety for old files
      unireg_type=  (uint) strpos[8];
      interval_nr=  (uint) strpos[10];

      /* old frm file */
      field_type= (enum_field_types) f_packtype(pack_flag);
      if (f_is_binary(pack_flag))
      {
        /*
          Try to choose the best 4.1 type:
          - for 4.0 "CHAR(N) BINARY" or "VARCHAR(N) BINARY" 
            try to find a binary collation for character set.
          - for other types (e.g. BLOB) just use my_charset_bin. 
        */
        if (!f_is_blob(pack_flag))
        {
          // 3.23 or 4.0 string
          if (!(charset= get_charset_by_csname(share->table_charset->csname,
                                               MY_CS_BINSORT, MYF(0))))
            charset= &my_charset_bin;
        }
        else
          charset= &my_charset_bin;
      }
      else
        charset= share->table_charset;
      bzero((char*) &comment, sizeof(comment));
    }

    if (interval_nr && charset->mbminlen > 1)
    {
      /* Unescape UCS2 intervals from HEX notation */
      TYPELIB *interval= share->intervals + interval_nr - 1;
      unhex_type2(interval);
    }
    
#ifndef TO_BE_DELETED_ON_PRODUCTION
    if (field_type == FIELD_TYPE_NEWDECIMAL && !share->mysql_version)
    {
      /*
        Fix pack length of old decimal values from 5.0.3 -> 5.0.4
        The difference is that in the old version we stored precision
        in the .frm table while we now store the display_length
      */
      uint decimals= f_decimals(pack_flag);
      field_length= my_decimal_precision_to_length(field_length,
                                                   decimals,
                                                   f_is_dec(pack_flag) == 0);
      sql_print_error("Found incompatible DECIMAL field '%s' in %s; Please do \"ALTER TABLE '%s' FORCE\" to fix it!", share->fieldnames.type_names[i], name, share->table_name);
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_CRASHED_ON_USAGE,
                          "Found incompatible DECIMAL field '%s' in %s; Please do \"ALTER TABLE '%s' FORCE\" to fix it!", share->fieldnames.type_names[i], name, share->table_name);
      share->crashed= 1;                        // Marker for CHECK TABLE
    }
#endif

    *field_ptr=reg_field=
      make_field(record+recpos,
		 (uint32) field_length,
		 null_pos, null_bit_pos,
		 pack_flag,
		 field_type,
		 charset,
		 geom_type,
		 (Field::utype) MTYP_TYPENR(unireg_type),
		 (interval_nr ?
		  share->intervals+interval_nr-1 :
		  (TYPELIB*) 0),
		 share->fieldnames.type_names[i],
		 outparam);
    if (!reg_field)				// Not supported field type
    {
      error= 4;
      goto err;			/* purecov: inspected */
    }

    reg_field->field_index= i;
    reg_field->comment=comment;
    if (field_type == FIELD_TYPE_BIT && !f_bit_as_char(pack_flag))
    {
      if ((null_bit_pos+= field_length & 7) > 7)
      {
        null_pos++;
        null_bit_pos-= 8;
      }
    }
    if (!(reg_field->flags & NOT_NULL_FLAG))
    {
      if (!(null_bit_pos= (null_bit_pos + 1) & 7))
        null_pos++;
    }
    if (f_no_default(pack_flag))
      reg_field->flags|= NO_DEFAULT_VALUE_FLAG;
    if (reg_field->unireg_check == Field::NEXT_NUMBER)
      outparam->found_next_number_field= reg_field;
    if (outparam->timestamp_field == reg_field)
      share->timestamp_field_offset= i;
    if (use_hash)
      (void) my_hash_insert(&share->name_hash,(byte*) field_ptr); // never fail
  }
  *field_ptr=0;					// End marker

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    uint primary_key=(uint) (find_type((char*) primary_key_name,
				       &share->keynames, 3) - 1);
    uint ha_option=outparam->file->table_flags();
    keyinfo=outparam->key_info;
    key_part=keyinfo->key_part;

    for (uint key=0 ; key < share->keys ; key++,keyinfo++)
    {
      uint usable_parts=0;
      keyinfo->name=(char*) share->keynames.type_names[key];
      /* Fix fulltext keys for old .frm files */
      if (outparam->key_info[key].flags & HA_FULLTEXT)
	outparam->key_info[key].algorithm= HA_KEY_ALG_FULLTEXT;

      if (primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
	/*
	  If the UNIQUE key doesn't have NULL columns and is not a part key
	  declare this as a primary key.
	*/
	primary_key=key;
	for (i=0 ; i < keyinfo->key_parts ;i++)
	{
	  uint fieldnr= key_part[i].fieldnr;
	  if (!fieldnr ||
	      outparam->field[fieldnr-1]->null_ptr ||
	      outparam->field[fieldnr-1]->key_length() !=
	      key_part[i].length)
	  {
	    primary_key=MAX_KEY;		// Can't be used
	    break;
	  }
	}
      }

      for (i=0 ; i < keyinfo->key_parts ; key_part++,i++)
      {
	if (new_field_pack_flag <= 1)
	  key_part->fieldnr=(uint16) find_field(outparam,
						(uint) key_part->offset,
						(uint) key_part->length);
#ifdef EXTRA_DEBUG
	if (key_part->fieldnr > share->fields)
	  goto err;                             // sanity check
#endif
	if (key_part->fieldnr)
	{					// Should always be true !
	  Field *field=key_part->field=outparam->field[key_part->fieldnr-1];
	  if (field->null_ptr)
	  {
	    key_part->null_offset=(uint) ((byte*) field->null_ptr -
					  outparam->record[0]);
	    key_part->null_bit= field->null_bit;
	    key_part->store_length+=HA_KEY_NULL_LENGTH;
	    keyinfo->flags|=HA_NULL_PART_KEY;
	    keyinfo->extra_length+= HA_KEY_NULL_LENGTH;
	    keyinfo->key_length+= HA_KEY_NULL_LENGTH;
	  }
	  if (field->type() == FIELD_TYPE_BLOB ||
	      field->real_type() == MYSQL_TYPE_VARCHAR)
	  {
	    if (field->type() == FIELD_TYPE_BLOB)
	      key_part->key_part_flag|= HA_BLOB_PART;
            else
              key_part->key_part_flag|= HA_VAR_LENGTH_PART;
	    keyinfo->extra_length+=HA_KEY_BLOB_LENGTH;
	    key_part->store_length+=HA_KEY_BLOB_LENGTH;
	    keyinfo->key_length+= HA_KEY_BLOB_LENGTH;
	    /*
	      Mark that there may be many matching values for one key
	      combination ('a', 'a ', 'a  '...)
	    */
	    if (!(field->flags & BINARY_FLAG))
	      keyinfo->flags|= HA_END_SPACE_KEY;
	  }
	  if (field->type() == MYSQL_TYPE_BIT)
            key_part->key_part_flag|= HA_BIT_PART;

	  if (i == 0 && key != primary_key)
	    field->flags |= ((keyinfo->flags & HA_NOSAME) &&
                             (keyinfo->key_parts == 1)) ?
                            UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG;
	  if (i == 0)
	    field->key_start.set_bit(key);
	  if (field->key_length() == key_part->length &&
	      !(field->flags & BLOB_FLAG))
	  {
            if (outparam->file->index_flags(key, i, 0) & HA_KEYREAD_ONLY)
            {
              share->keys_for_keyread.set_bit(key);
	      field->part_of_key.set_bit(key);
            }
	    if (outparam->file->index_flags(key, i, 1) & HA_READ_ORDER)
	      field->part_of_sortkey.set_bit(key);
	  }
	  if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
	      usable_parts == i)
	    usable_parts++;			// For FILESORT
	  field->flags|= PART_KEY_FLAG;
	  if (key == primary_key)
	  {
	    field->flags|= PRI_KEY_FLAG;
	    /*
	      If this field is part of the primary key and all keys contains
	      the primary key, then we can use any key to find this column
	    */
	    if (ha_option & HA_PRIMARY_KEY_IN_READ_INDEX)
	      field->part_of_key= share->keys_in_use;
	  }
	  if (field->key_length() != key_part->length)
	  {
#ifndef TO_BE_DELETED_ON_PRODUCTION
            if (field->type() == FIELD_TYPE_NEWDECIMAL)
            {
              /*
                Fix a fatal error in decimal key handling that causes crashes
                on Innodb. We fix it by reducing the key length so that
                InnoDB never gets a too big key when searching.
                This allows the end user to do an ALTER TABLE to fix the
                error.
              */
	      keyinfo->key_length-= (key_part->length - field->key_length());
	      key_part->store_length-= (uint16)(key_part->length -
                                                field->key_length());
              key_part->length= (uint16)field->key_length();
              sql_print_error("Found wrong key definition in %s; Please do \"ALTER TABLE '%s' FORCE \" to fix it!", name, share->table_name);
              push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                                  ER_CRASHED_ON_USAGE,
                                  "Found wrong key definition in %s; Please do \"ALTER TABLE '%s' FORCE\" to fix it!", name, share->table_name);

              share->crashed= 1;                // Marker for CHECK TABLE
	      goto to_be_deleted;
            }
#endif
	    key_part->key_part_flag|= HA_PART_KEY_SEG;
	    if (!(field->flags & BLOB_FLAG))
	    {					// Create a new field
	      field=key_part->field=field->new_field(&outparam->mem_root,
						     outparam);
	      field->field_length=key_part->length;
	    }
	  }

	to_be_deleted:

	  /*
	    If the field can be NULL, don't optimize away the test
	    key_part_column = expression from the WHERE clause
	    as we need to test for NULL = NULL.
	  */
	  if (field->real_maybe_null())
	    key_part->key_part_flag|= HA_PART_KEY_SEG;
	}
	else
	{					// Error: shorten key
	  keyinfo->key_parts=usable_parts;
	  keyinfo->flags=0;
	}
      }
      keyinfo->usable_key_parts=usable_parts; // Filesort

      set_if_bigger(share->max_key_length,keyinfo->key_length+
                    keyinfo->key_parts);
      share->total_key_length+= keyinfo->key_length;
      /*
        MERGE tables do not have unique indexes. But every key could be
        an unique index on the underlying MyISAM table. (Bug #10400)
      */
      if ((keyinfo->flags & HA_NOSAME) ||
          (ha_option & HA_ANY_INDEX_MAY_BE_UNIQUE))
        set_if_bigger(share->max_unique_length,keyinfo->key_length);
    }
    if (primary_key < MAX_KEY &&
	(share->keys_in_use.is_set(primary_key)))
    {
      share->primary_key= primary_key;
      /*
	If we are using an integer as the primary key then allow the user to
	refer to it as '_rowid'
      */
      if (outparam->key_info[primary_key].key_parts == 1)
      {
	Field *field= outparam->key_info[primary_key].key_part[0].field;
	if (field && field->result_type() == INT_RESULT)
	  outparam->rowid_field=field;
      }
    }
    else
      share->primary_key = MAX_KEY; // we do not have a primary key
  }
  else
    share->primary_key= MAX_KEY;
  x_free((gptr) disk_buff);
  disk_buff=0;
  if (new_field_pack_flag <= 1)
  {
    /* Old file format with default as not null */
    uint null_length= (share->null_fields+7)/8;
    bfill(share->default_values + (outparam->null_flags - (uchar*) record),
          null_length, 255);
  }

  if ((reg_field=outparam->found_next_number_field))
  {
    if ((int) (share->next_number_index= (uint)
	       find_ref_key(outparam,reg_field,
			    &share->next_number_key_offset)) < 0)
    {
      reg_field->unireg_check=Field::NONE;	/* purecov: inspected */
      outparam->found_next_number_field=0;
    }
    else
      reg_field->flags|=AUTO_INCREMENT_FLAG;
  }

  if (share->blob_fields)
  {
    Field **ptr;
    uint i, *save;

    /* Store offsets to blob fields to find them fast */
    if (!(share->blob_field= save=
	  (uint*) alloc_root(&outparam->mem_root,
                             (uint) (share->blob_fields* sizeof(uint)))))
      goto err;
    for (i=0, ptr= outparam->field ; *ptr ; ptr++, i++)
    {
      if ((*ptr)->flags & BLOB_FLAG)
	(*save++)= i;
    }
  }

  /* the correct null_bytes can now be set, since bitfields have been taken into account */
  share->null_bytes= null_pos - (uchar*) outparam->null_flags + (null_bit_pos + 7) / 8;
  share->last_null_bit_pos= null_bit_pos;

  /* The table struct is now initialized;  Open the table */
  error=2;
  if (db_stat)
  {
    int ha_err;
    unpack_filename(index_file,index_file);
    if ((ha_err= (outparam->file->
                  ha_open(index_file,
                          (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
                          (db_stat & HA_OPEN_TEMPORARY ? HA_OPEN_TMP_TABLE :
                           ((db_stat & HA_WAIT_IF_LOCKED) ||
                            (specialflag & SPECIAL_WAIT_IF_LOCKED)) ?
                           HA_OPEN_WAIT_IF_LOCKED :
                           (db_stat & (HA_ABORT_IF_LOCKED | HA_GET_INFO)) ?
                          HA_OPEN_ABORT_IF_LOCKED :
                           HA_OPEN_IGNORE_IF_LOCKED) | ha_open_flags))))
    {
      /* Set a flag if the table is crashed and it can be auto. repaired */
      share->crashed= ((ha_err == HA_ERR_CRASHED_ON_USAGE) &&
                       outparam->file->auto_repair() &&
                       !(ha_open_flags & HA_OPEN_FOR_REPAIR));

      if (ha_err == HA_ERR_NO_SUCH_TABLE)
      {
	/* The table did not exists in storage engine, use same error message
	   as if the .frm file didn't exist */
	error= 1;
	my_errno= ENOENT;
      }
      else
      {
        outparam->file->print_error(ha_err, MYF(0));
        error_reported= TRUE;
      }
      goto err;                                 /* purecov: inspected */
    }
  }
  share->db_low_byte_first= outparam->file->low_byte_first();

  *root_ptr= old_root;
  thd->status_var.opened_tables++;
#ifndef DBUG_OFF
  if (use_hash)
    (void) hash_check(&share->name_hash);
#endif
  DBUG_RETURN (0);

 err:
  x_free((gptr) disk_buff);
  if (file > 0)
    VOID(my_close(file,MYF(MY_WME)));

  delete crypted;
  *root_ptr= old_root;
  if (! error_reported)
    frm_error(error,outparam,name,ME_ERROR+ME_WAITTANG, errarg);
  delete outparam->file;
  outparam->file=0;				// For easier errorchecking
  outparam->db_stat=0;
  hash_free(&share->name_hash);
  free_root(&outparam->mem_root, MYF(0));       // Safe to call on bzero'd root
  my_free((char*) outparam->alias, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN (error);
} /* openfrm */


	/* close a .frm file and it's tables */

int closefrm(register TABLE *table)
{
  int error=0;
  DBUG_ENTER("closefrm");
  if (table->db_stat)
    error=table->file->close();
  my_free((char*) table->alias, MYF(MY_ALLOW_ZERO_PTR));
  table->alias= 0;
  if (table->field)
  {
    for (Field **ptr=table->field ; *ptr ; ptr++)
      delete *ptr;
    table->field= 0;
  }
  delete table->file;
  table->file= 0;				/* For easier errorchecking */
  hash_free(&table->s->name_hash);
  free_root(&table->mem_root, MYF(0));
  DBUG_RETURN(error);
}


/* Deallocate temporary blob storage */

void free_blobs(register TABLE *table)
{
  uint *ptr, *end;
  for (ptr= table->s->blob_field, end=ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
    ((Field_blob*) table->field[*ptr])->free();
}


	/* Find where a form starts */
	/* if formname is NullS then only formnames is read */

ulong get_form_pos(File file, uchar *head, TYPELIB *save_names)
{
  uint a_length,names,length;
  uchar *pos,*buf;
  ulong ret_value=0;
  DBUG_ENTER("get_form_pos");

  names=uint2korr(head+8);
  a_length=(names+2)*sizeof(my_string);		/* Room for two extra */

  if (!save_names)
    a_length=0;
  else
    save_names->type_names=0;			/* Clear if error */

  if (names)
  {
    length=uint2korr(head+4);
    VOID(my_seek(file,64L,MY_SEEK_SET,MYF(0)));
    if (!(buf= (uchar*) my_malloc((uint) length+a_length+names*4,
				  MYF(MY_WME))) ||
	my_read(file,(byte*) buf+a_length,(uint) (length+names*4),
		MYF(MY_NABP)))
    {						/* purecov: inspected */
      x_free((gptr) buf);			/* purecov: inspected */
      DBUG_RETURN(0L);				/* purecov: inspected */
    }
    pos= buf+a_length+length;
    ret_value=uint4korr(pos);
  }
  if (! save_names)
    my_free((gptr) buf,MYF(0));
  else if (!names)
    bzero((char*) save_names,sizeof(save_names));
  else
  {
    char *str;
    str=(char *) (buf+a_length);
    fix_type_pointers((const char ***) &buf,save_names,1,&str);
  }
  DBUG_RETURN(ret_value);
}


	/* Read string from a file with malloc */

int read_string(File file, gptr *to, uint length)
{
  DBUG_ENTER("read_string");

  x_free((gptr) *to);
  if (!(*to= (gptr) my_malloc(length+1,MYF(MY_WME))) ||
      my_read(file,(byte*) *to,length,MYF(MY_NABP)))
  {
    x_free((gptr) *to); /* purecov: inspected */
    *to= 0; /* purecov: inspected */
    DBUG_RETURN(1); /* purecov: inspected */
  }
  *((char*) *to+length)= '\0';
  DBUG_RETURN (0);
} /* read_string */


	/* Add a new form to a form file */

ulong make_new_entry(File file, uchar *fileinfo, TYPELIB *formnames,
		     const char *newname)
{
  uint i,bufflength,maxlength,n_length,length,names;
  ulong endpos,newpos;
  char buff[IO_SIZE];
  uchar *pos;
  DBUG_ENTER("make_new_entry");

  length=(uint) strlen(newname)+1;
  n_length=uint2korr(fileinfo+4);
  maxlength=uint2korr(fileinfo+6);
  names=uint2korr(fileinfo+8);
  newpos=uint4korr(fileinfo+10);

  if (64+length+n_length+(names+1)*4 > maxlength)
  {						/* Expand file */
    newpos+=IO_SIZE;
    int4store(fileinfo+10,newpos);
    endpos=(ulong) my_seek(file,0L,MY_SEEK_END,MYF(0));/* Copy from file-end */
    bufflength= (uint) (endpos & (IO_SIZE-1));	/* IO_SIZE is a power of 2 */

    while (endpos > maxlength)
    {
      VOID(my_seek(file,(ulong) (endpos-bufflength),MY_SEEK_SET,MYF(0)));
      if (my_read(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
      VOID(my_seek(file,(ulong) (endpos-bufflength+IO_SIZE),MY_SEEK_SET,
		   MYF(0)));
      if ((my_write(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME))))
	DBUG_RETURN(0);
      endpos-=bufflength; bufflength=IO_SIZE;
    }
    bzero(buff,IO_SIZE);			/* Null new block */
    VOID(my_seek(file,(ulong) maxlength,MY_SEEK_SET,MYF(0)));
    if (my_write(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
    maxlength+=IO_SIZE;				/* Fix old ref */
    int2store(fileinfo+6,maxlength);
    for (i=names, pos= (uchar*) *formnames->type_names+n_length-1; i-- ;
	 pos+=4)
    {
      endpos=uint4korr(pos)+IO_SIZE;
      int4store(pos,endpos);
    }
  }

  if (n_length == 1 )
  {						/* First name */
    length++;
    VOID(strxmov(buff,"/",newname,"/",NullS));
  }
  else
    VOID(strxmov(buff,newname,"/",NullS)); /* purecov: inspected */
  VOID(my_seek(file,63L+(ulong) n_length,MY_SEEK_SET,MYF(0)));
  if (my_write(file,(byte*) buff,(uint) length+1,MYF(MY_NABP+MY_WME)) ||
      (names && my_write(file,(byte*) (*formnames->type_names+n_length-1),
			 names*4, MYF(MY_NABP+MY_WME))) ||
      my_write(file,(byte*) fileinfo+10,(uint) 4,MYF(MY_NABP+MY_WME)))
    DBUG_RETURN(0L); /* purecov: inspected */

  int2store(fileinfo+8,names+1);
  int2store(fileinfo+4,n_length+length);
  VOID(my_chsize(file, newpos, 0, MYF(MY_WME)));/* Append file with '\0' */
  DBUG_RETURN(newpos);
} /* make_new_entry */


	/* error message when opening a form file */

static void frm_error(int error, TABLE *form, const char *name,
                      myf errortype, int errarg)
{
  int err_no;
  char buff[FN_REFLEN];
  const char *form_dev="",*datext;
  const char *real_name= (char*) name+dirname_length(name);
  DBUG_ENTER("frm_error");

  switch (error) {
  case 1:
    if (my_errno == ENOENT)
    {
      char *db;
      uint length=dirname_part(buff,name);
      buff[length-1]=0;
      db=buff+dirname_length(buff);
      my_error(ER_NO_SUCH_TABLE, MYF(0), db, real_name);
    }
    else
      my_error(ER_FILE_NOT_FOUND, errortype,
               fn_format(buff, name, form_dev, reg_ext, 0), my_errno);
    break;
  case 2:
  {
    datext= form->file ? *form->file->bas_ext() : "";
    datext= datext==NullS ? "" : datext;
    err_no= (my_errno == ENOENT) ? ER_FILE_NOT_FOUND : (my_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    my_error(err_no,errortype,
	     fn_format(buff,real_name,form_dev,datext,2),my_errno);
    break;
  }
  case 5:
  {
    const char *csname= get_charset_name((uint) errarg);
    char tmp[10];
    if (!csname || csname[0] =='?')
    {
      my_snprintf(tmp, sizeof(tmp), "#%d", errarg);
      csname= tmp;
    }
    my_printf_error(ER_UNKNOWN_COLLATION,
                    "Unknown collation '%s' in table '%-.64s' definition", 
                    MYF(0), csname, real_name);
    break;
  }
  case 6:
    my_printf_error(ER_NOT_FORM_FILE,
                    "Table '%-.64s' was created with a different version "
                    "of MySQL and cannot be read",
                    MYF(0), name);
    break;
  default:				/* Better wrong error than none */
  case 4:
    my_error(ER_NOT_FORM_FILE, errortype,
             fn_format(buff, name, form_dev, reg_ext, 0));
    break;
  }
  DBUG_VOID_RETURN;
} /* frm_error */


	/*
	** fix a str_type to a array type
	** typeparts separated with some char. differents types are separated
	** with a '\0'
	*/

static void
fix_type_pointers(const char ***array, TYPELIB *point_to_type, uint types,
		  char **names)
{
  char *type_name, *ptr;
  char chr;

  ptr= *names;
  while (types--)
  {
    point_to_type->name=0;
    point_to_type->type_names= *array;

    if ((chr= *ptr))			/* Test if empty type */
    {
      while ((type_name=strchr(ptr+1,chr)) != NullS)
      {
	*((*array)++) = ptr+1;
	*type_name= '\0';		/* End string */
	ptr=type_name;
      }
      ptr+=2;				/* Skip end mark and last 0 */
    }
    else
      ptr++;
    point_to_type->count= (uint) (*array - point_to_type->type_names);
    point_to_type++;
    *((*array)++)= NullS;		/* End of type */
  }
  *names=ptr;				/* Update end */
  return;
} /* fix_type_pointers */


TYPELIB *typelib(List<String> &strings)
{
  TYPELIB *result=(TYPELIB*) sql_alloc(sizeof(TYPELIB));
  if (!result)
    return 0;
  result->count=strings.elements;
  result->name="";
  uint nbytes= (sizeof(char*) + sizeof(uint)) * (result->count + 1);
  if (!(result->type_names= (const char**) sql_alloc(nbytes)))
    return 0;
  result->type_lengths= (uint*) (result->type_names + result->count + 1);
  List_iterator<String> it(strings);
  String *tmp;
  for (uint i=0; (tmp=it++) ; i++)
  {
    result->type_names[i]= tmp->ptr();
    result->type_lengths[i]= tmp->length();
  }
  result->type_names[result->count]= 0;		// End marker
  result->type_lengths[result->count]= 0;
  return result;
}


/*
 Search after a field with given start & length
 If an exact field isn't found, return longest field with starts
 at right position.
 
 NOTES
   This is needed because in some .frm fields 'fieldnr' was saved wrong

 RETURN
   0  error
   #  field number +1
*/

static uint find_field(TABLE *form,uint start,uint length)
{
  Field **field;
  uint i, pos, fields;

  pos=0;
  fields= form->s->fields;
  for (field=form->field, i=1 ; i<= fields ; i++,field++)
  {
    if ((*field)->offset() == start)
    {
      if ((*field)->key_length() == length)
	return (i);
      if (!pos || form->field[pos-1]->pack_length() <
	  (*field)->pack_length())
	pos=i;
    }
  }
  return (pos);
}


	/* Check that the integer is in the internal */

int set_zone(register int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */

	/* Adjust number to next larger disk buffer */

ulong next_io_size(register ulong pos)
{
  reg2 ulong offset;
  if ((offset= pos & (IO_SIZE-1)))
    return pos-offset+IO_SIZE;
  return pos;
} /* next_io_size */


/*
  Store an SQL quoted string.

  SYNOPSIS  
    append_unescaped()
    res		result String
    pos		string to be quoted
    length	it's length

  NOTE
    This function works correctly with utf8 or single-byte charset strings.
    May fail with some multibyte charsets though.
*/

void append_unescaped(String *res, const char *pos, uint length)
{
  const char *end= pos+length;
  res->append('\'');

  for (; pos != end ; pos++)
  {
#if defined(USE_MB) && MYSQL_VERSION_ID < 40100
    uint mblen;
    if (use_mb(default_charset_info) &&
        (mblen= my_ismbchar(default_charset_info, pos, end)))
    {
      res->append(pos, mblen);
      pos+= mblen;
      continue;
    }
#endif

    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->append('\\');
      res->append('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->append('\\');
      res->append('n');
      break;
    case '\r':
      res->append('\\');		/* This gives better readability */
      res->append('r');
      break;
    case '\\':
      res->append('\\');		/* Because of the sql syntax */
      res->append('\\');
      break;
    case '\'':
      res->append('\'');		/* Because of the sql syntax */
      res->append('\'');
      break;
    default:
      res->append(*pos);
      break;
    }
  }
  res->append('\'');
}

	/* Create a .frm file */

File create_frm(THD *thd, my_string name, const char *db,
                const char *table, uint reclength, uchar *fileinfo,
		HA_CREATE_INFO *create_info, uint keys)
{
  register File file;
  ulong length;
  char fill[IO_SIZE];
  int create_flags= O_RDWR | O_TRUNC;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= O_EXCL | O_NOFOLLOW;

#if SIZEOF_OFF_T > 4
  /* Fix this when we have new .frm files;  Current limit is 4G rows (QQ) */
  if (create_info->max_rows > ~(ulong) 0)
    create_info->max_rows= ~(ulong) 0;
  if (create_info->min_rows > ~(ulong) 0)
    create_info->min_rows= ~(ulong) 0;
#endif
  /*
    Ensure that raid_chunks can't be larger than 255, as this would cause
    problems with drop database
  */
  set_if_smaller(create_info->raid_chunks, 255);

  if ((file= my_create(name, CREATE_MODE, create_flags, MYF(0))) >= 0)
  {
    uint key_length, tmp_key_length;
    uint tmp;
    bzero((char*) fileinfo,64);
    /* header */
    fileinfo[0]=(uchar) 254;
    fileinfo[1]= 1;
    fileinfo[2]= FRM_VER+3+ test(create_info->varchar);

    fileinfo[3]= (uchar) ha_checktype(thd,create_info->db_type,0,0);
    fileinfo[4]=1;
    int2store(fileinfo+6,IO_SIZE);		/* Next block starts here */
    key_length=keys*(7+NAME_LEN+MAX_REF_PARTS*9)+16;
    length= next_io_size((ulong) (IO_SIZE+key_length+reclength+
                                  create_info->extra_size));
    int4store(fileinfo+10,length);
    tmp_key_length= (key_length < 0xffff) ? key_length : 0xffff;
    int2store(fileinfo+14,tmp_key_length);
    int2store(fileinfo+16,reclength);
    int4store(fileinfo+18,create_info->max_rows);
    int4store(fileinfo+22,create_info->min_rows);
    fileinfo[27]=2;				// Use long pack-fields
    create_info->table_options|=HA_OPTION_LONG_BLOB_PTR; // Use portable blob pointers
    int2store(fileinfo+30,create_info->table_options);
    fileinfo[32]=0;				// No filename anymore
    fileinfo[33]=5;                             // Mark for 5.0 frm file
    int4store(fileinfo+34,create_info->avg_row_length);
    fileinfo[38]= (create_info->default_table_charset ?
		   create_info->default_table_charset->number : 0);
    fileinfo[40]= (uchar) create_info->row_type;
    fileinfo[41]= (uchar) create_info->raid_type;
    fileinfo[42]= (uchar) create_info->raid_chunks;
    int4store(fileinfo+43,create_info->raid_chunksize);
    int4store(fileinfo+47, key_length);
    tmp= MYSQL_VERSION_ID;          // Store to avoid warning from int4store
    int4store(fileinfo+51, tmp);
    int2store(fileinfo+55, create_info->extra_size);
    bzero(fill,IO_SIZE);
    for (; length > IO_SIZE ; length-= IO_SIZE)
    {
      if (my_write(file,(byte*) fill,IO_SIZE,MYF(MY_WME | MY_NABP)))
      {
	VOID(my_close(file,MYF(0)));
	VOID(my_delete(name,MYF(0)));
	return(-1);
      }
    }
  }
  else
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_TABLE,MYF(0),table,my_errno);
  }
  return (file);
} /* create_frm */


void update_create_info_from_table(HA_CREATE_INFO *create_info, TABLE *table)
{
  TABLE_SHARE *share= table->s;
  DBUG_ENTER("update_create_info_from_table");

  create_info->max_rows= share->max_rows;
  create_info->min_rows= share->min_rows;
  create_info->table_options= share->db_create_options;
  create_info->avg_row_length= share->avg_row_length;
  create_info->row_type= share->row_type;
  create_info->raid_type= share->raid_type;
  create_info->raid_chunks= share->raid_chunks;
  create_info->raid_chunksize= share->raid_chunksize;
  create_info->default_table_charset= share->table_charset;
  create_info->table_charset= 0;

  DBUG_VOID_RETURN;
}

int
rename_file_ext(const char * from,const char * to,const char * ext)
{
  char from_b[FN_REFLEN],to_b[FN_REFLEN];
  VOID(strxmov(from_b,from,ext,NullS));
  VOID(strxmov(to_b,to,ext,NullS));
  return (my_rename(from_b,to_b,MYF(MY_WME)));
}


/*
  Allocate string field in MEM_ROOT and return it as String

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string
    res         result String

  RETURN VALUES
    1   string is empty
    0	all ok
*/

bool get_field(MEM_ROOT *mem, Field *field, String *res)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  uint length;

  field->val_str(&str);
  if (!(length= str.length()))
  {
    res->length(0);
    return 1;
  }
  if (!(to= strmake_root(mem, str.ptr(), length)))
    length= 0;                                  // Safety fix
  res->set(to, length, ((Field_str*)field)->charset());
  return 0;
}


/*
  Allocate string field in MEM_ROOT and return it as NULL-terminated string

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string

  RETURN VALUES
    NullS  string is empty
    #      pointer to NULL-terminated string value of field
*/

char *get_field(MEM_ROOT *mem, Field *field)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  uint length;

  field->val_str(&str);
  length= str.length();
  if (!length || !(to= (char*) alloc_root(mem,length+1)))
    return NullS;
  memcpy(to,str.ptr(),(uint) length);
  to[length]=0;
  return to;
}


/*
  Check if database name is valid

  SYNPOSIS
    check_db_name()
    name		Name of database

  NOTES
    If lower_case_table_names is set then database is converted to lower case

  RETURN
    0	ok
    1   error
*/

bool check_db_name(char *name)
{
  char *start=name;
  /* Used to catch empty names and names with end space */
  bool last_char_is_space= TRUE;

  if (lower_case_table_names && name != any_db)
    my_casedn_str(files_charset_info, name);

  while (*name)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    last_char_is_space= my_isspace(default_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, 
                          name+system_charset_info->mbmaxlen);
      if (len)
      {
        name += len;
        continue;
      }
    }
#else
    last_char_is_space= *name==' ';
#endif
    if (*name == '/' || *name == '\\' || *name == FN_LIBCHAR ||
	*name == FN_EXTCHAR)
      return 1;
    name++;
  }
  return last_char_is_space || (uint) (name - start) > NAME_LEN;
}


/*
  Allow anything as a table name, as long as it doesn't contain an
  a '/', or a '.' character
  or ' ' at the end
  returns 1 on error
*/


bool check_table_name(const char *name, uint length)
{
  const char *end= name+length;
  if (!length || length > NAME_LEN)
    return 1;
#if defined(USE_MB) && defined(USE_MB_IDENT)
  bool last_char_is_space= FALSE;
#else
  if (name[length-1]==' ')
    return 1;
#endif

  while (name != end)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    last_char_is_space= my_isspace(default_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, end);
      if (len)
      {
        name += len;
        continue;
      }
    }
#endif
    if (*name == '/' || *name == '\\' || *name == FN_EXTCHAR)
      return 1;
    name++;
  }
#if defined(USE_MB) && defined(USE_MB_IDENT)
  return last_char_is_space;
#else
  return 0;
#endif
}


bool check_column_name(const char *name)
{
  const char *start= name;
  bool last_char_is_space= TRUE;
  
  while (*name)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    last_char_is_space= my_isspace(default_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, 
                          name+system_charset_info->mbmaxlen);
      if (len)
      {
        name += len;
        continue;
      }
    }
#else
    last_char_is_space= *name==' ';
#endif
    if (*name == NAMES_SEP_CHAR)
      return 1;
    name++;
  }
  /* Error if empty or too long column name */
  return last_char_is_space || (uint) (name - start) > NAME_LEN;
}

/*
** Get type of table from .frm file
*/

db_type get_table_type(THD *thd, const char *name)
{
  File	 file;
  uchar head[4];
  int error;
  DBUG_ENTER("get_table_type");
  DBUG_PRINT("enter",("name: '%s'",name));

  if ((file=my_open(name,O_RDONLY, MYF(0))) < 0)
    DBUG_RETURN(DB_TYPE_UNKNOWN);
  error=my_read(file,(byte*) head,4,MYF(MY_NABP));
  my_close(file,MYF(0));
  if (error || head[0] != (uchar) 254 || head[1] != 1 ||
      (head[2] != FRM_VER && head[2] != FRM_VER+1 &&
       (head[2] < FRM_VER+3 || head[2] > FRM_VER+4)))
    DBUG_RETURN(DB_TYPE_UNKNOWN);
  DBUG_RETURN(ha_checktype(thd,(enum db_type) (uint) *(head+3),0,0));
}

/*
  Create Item_field for each column in the table.

  SYNPOSIS
    st_table::fill_item_list()
      item_list          a pointer to an empty list used to store items

  DESCRIPTION
    Create Item_field object for each column in the table and
    initialize it with the corresponding Field. New items are
    created in the current THD memory root.

  RETURN VALUE
    0                    success
    1                    out of memory
*/

bool st_table::fill_item_list(List<Item> *item_list) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item= new Item_field(*ptr);
    if (!item || item_list->push_back(item))
      return TRUE;
  }
  return FALSE;
}

/*
  Reset an existing list of Item_field items to point to the
  Fields of this table.

  SYNPOSIS
    st_table::fill_item_list()
      item_list          a non-empty list with Item_fields

  DESCRIPTION
    This is a counterpart of fill_item_list used to redirect
    Item_fields to the fields of a newly created table.
    The caller must ensure that number of items in the item_list
    is the same as the number of columns in the table.
*/

void st_table::reset_item_list(List<Item> *item_list) const
{
  List_iterator_fast<Item> it(*item_list);
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item_field= (Item_field*) it++;
    DBUG_ASSERT(item_field != 0);
    item_field->reset_field(*ptr);
  }
}

/*
  calculate md5 of query

  SYNOPSIS
    st_table_list::calc_md5()
    buffer	buffer for md5 writing
*/

void  st_table_list::calc_md5(char *buffer)
{
  my_MD5_CTX context;
  uchar digest[16];
  my_MD5Init(&context);
  my_MD5Update(&context,(uchar *) query.str, query.length);
  my_MD5Final(digest, &context);
  sprintf((char *) buffer,
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2], digest[3],
	    digest[4], digest[5], digest[6], digest[7],
	    digest[8], digest[9], digest[10], digest[11],
	    digest[12], digest[13], digest[14], digest[15]);
}


/*
  set ancestor TABLE for table place holder of VIEW

  DESCRIPTION
    Replace all views that only uses one table with the table itself.
    This allows us to treat the view as a simple table and even update
    it

  SYNOPSIS
    st_table_list::set_ancestor()
*/

void st_table_list::set_ancestor()
{
  TABLE_LIST *tbl;

  if ((tbl= ancestor))
  {
    /* This is a view. Process all tables of view */
    DBUG_ASSERT(view);
    do
    {
      if (tbl->ancestor)                        // This is a view
      {
        /*
          This is the only case where set_ancestor is called on an object
          that may not be a view (in which case ancestor is 0)
        */
        tbl->ancestor->set_ancestor();
      }
    } while ((tbl= tbl->next_local));

    if (!multitable_view)
    {
      table= ancestor->table;
      schema_table= ancestor->schema_table;
    }
  }
}


/*
  setup fields of placeholder of merged VIEW

  SYNOPSIS
    st_table_list::setup_ancestor()
    thd		    - thread handler

  NOTES
    ancestor is list of tables and views used by view (underlying tables/views)

  DESCRIPTION
    It is:
    - preparing translation table for view columns
    If there are underlying view(s) procedure first will be called for them.

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool st_table_list::setup_ancestor(THD *thd)
{
  DBUG_ENTER("st_table_list::setup_ancestor");
  if (!field_translation)
  {
    Field_translator *transl;
    SELECT_LEX *select= &view->select_lex;
    Item *item;
    TABLE_LIST *tbl;
    List_iterator_fast<Item> it(select->item_list);
    uint field_count= 0;

    if (check_stack_overrun(thd, STACK_MIN_SIZE, (char *)&field_count))
    {
      DBUG_RETURN(TRUE);
    }

    for (tbl= ancestor; tbl; tbl= tbl->next_local)
    {
      if (tbl->ancestor &&
          tbl->setup_ancestor(thd))
      {
        DBUG_RETURN(TRUE);
      }
    }

    /* Create view fields translation table */

    if (!(transl=
          (Field_translator*)(thd->stmt_arena->
                              alloc(select->item_list.elements *
                                    sizeof(Field_translator)))))
    {
      DBUG_RETURN(TRUE);
    }

    while ((item= it++))
    {
      transl[field_count].name= item->name;
      transl[field_count++].item= item;
    }
    field_translation= transl;
    field_translation_end= transl + field_count;
    /* TODO: use hash for big number of fields */

    /* full text function moving to current select */
    if (view->select_lex.ftfunc_list->elements)
    {
      Item_func_match *ifm;
      SELECT_LEX *current_select= thd->lex->current_select;
      List_iterator_fast<Item_func_match>
        li(*(view->select_lex.ftfunc_list));
      while ((ifm= li++))
        current_select->ftfunc_list->push_front(ifm);
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Prepare where expression of view

  SYNOPSIS
    st_table_list::prep_where()
    thd             - thread handler
    conds           - condition of this JOIN
    no_where_clause - do not build WHERE or ON outer qwery do not need it
                      (it is INSERT), we do not need conds if this flag is set

  NOTE: have to be called befor CHECK OPTION preparation, because it makes
  fix_fields for view WHERE clause

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool st_table_list::prep_where(THD *thd, Item **conds,
                               bool no_where_clause)
{
  DBUG_ENTER("st_table_list::prep_where");

  for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
  {
    if (tbl->view && tbl->prep_where(thd, conds, no_where_clause))
    {
      DBUG_RETURN(TRUE);
    }
  }

  if (where)
  {
    if (!where->fixed && where->fix_fields(thd, &where))
    {
      DBUG_RETURN(TRUE);
    }

    /*
      check that it is not VIEW in which we insert with INSERT SELECT
      (in this case we can't add view WHERE condition to main SELECT_LEX)
    */
    if (!no_where_clause && !where_processed)
    {
      TABLE_LIST *tbl= this;
      Query_arena *arena= thd->stmt_arena, backup;
      arena= thd->activate_stmt_arena_if_needed(&backup);  // For easier test

      /* Go up to join tree and try to find left join */
      for (; tbl; tbl= tbl->embedding)
      {
        if (tbl->outer_join)
        {
          /*
            Store WHERE condition to ON expression for outer join, because
            we can't use WHERE to correctly execute left joins on VIEWs and
            this expression will not be moved to WHERE condition (i.e. will
            be clean correctly for PS/SP)
          */
          tbl->on_expr= and_conds(tbl->on_expr, where);
          break;
        }
      }
      if (tbl == 0)
        *conds= and_conds(*conds, where);
      if (arena)
        thd->restore_active_arena(arena, &backup);
      where_processed= TRUE;
    }
  }

  DBUG_RETURN(FALSE);
}


/*
  Prepare check option expression of table

  SYNOPSIS
    st_table_list::prep_check_option()
    thd             - thread handler
    check_opt_type  - WITH CHECK OPTION type (VIEW_CHECK_NONE,
                      VIEW_CHECK_LOCAL, VIEW_CHECK_CASCADED)
                      we use this parameter instead of direct check of
                      effective_with_check to change type of underlying
                      views to VIEW_CHECK_CASCADED if outer view have
                      such option and prevent processing of underlying
                      view check options if outer view have just
                      VIEW_CHECK_LOCAL option.

  NOTE
    This method build check options for every call
    (usual execution or every SP/PS call)
    This method have to be called after WHERE preparation
    (st_table_list::prep_where)

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool st_table_list::prep_check_option(THD *thd, uint8 check_opt_type)
{
  DBUG_ENTER("st_table_list::prep_check_option");

  for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
  {
    /* see comment of check_opt_type parameter */
    if (tbl->view &&
        tbl->prep_check_option(thd,
                               ((check_opt_type == VIEW_CHECK_CASCADED) ?
                                VIEW_CHECK_CASCADED :
                                VIEW_CHECK_NONE)))
    {
      DBUG_RETURN(TRUE);
    }
  }

  if (check_opt_type)
  {
    Item *item= 0;
    if (where)
    {
      DBUG_ASSERT(where->fixed);
      item= where->copy_andor_structure(thd);
    }
    if (check_opt_type == VIEW_CHECK_CASCADED)
    {
      for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
      {
        if (tbl->check_option)
          item= and_conds(item, tbl->check_option);
      }
    }
    if (item)
      thd->change_item_tree(&check_option, item);
  }

  if (check_option)
  {
    const char *save_where= thd->where;
    thd->where= "check option";
    if (!check_option->fixed &&
        check_option->fix_fields(thd, &check_option) ||
        check_option->check_cols(1))
    {
      DBUG_RETURN(TRUE);
    }
    thd->where= save_where;
  }
  DBUG_RETURN(FALSE);
}


/*
  Hide errors which show view underlying table information

  SYNOPSIS
    st_table_list::hide_view_error()
    thd     thread handler

*/

void st_table_list::hide_view_error(THD *thd)
{
  /* Hide "Unknown column" or "Unknown function" error */
  if (thd->net.last_errno == ER_BAD_FIELD_ERROR ||
      thd->net.last_errno == ER_SP_DOES_NOT_EXIST)
  {
    thd->clear_error();
    my_error(ER_VIEW_INVALID, MYF(0), view_db.str, view_name.str);
  }
  else if (thd->net.last_errno == ER_NO_DEFAULT_FOR_FIELD)
  {
    thd->clear_error();
    // TODO: make correct error message
    my_error(ER_NO_DEFAULT_FOR_VIEW_FIELD, MYF(0), view_db.str, view_name.str);
  }
}


/*
  Find underlying base tables (TABLE_LIST) which represent given
  table_to_find (TABLE)

  SYNOPSIS
    st_table_list::find_underlying_table()
    table_to_find table to find

  RETURN
    0  table is not found
    found table reference
*/

st_table_list *st_table_list::find_underlying_table(TABLE *table_to_find)
{
  /* is this real table and table which we are looking for? */
  if (table == table_to_find && ancestor == 0)
    return this;

  for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
  {
    TABLE_LIST *result;
    if ((result= tbl->find_underlying_table(table_to_find)))
      return result;
  }
  return 0;
}

/*
  cleunup items belonged to view fields translation table

  SYNOPSIS
    st_table_list::cleanup_items()
*/

void st_table_list::cleanup_items()
{
  if (!field_translation)
    return;

  for (Field_translator *transl= field_translation;
       transl < field_translation_end;
       transl++)
    transl->item->walk(&Item::cleanup_processor, 0);
}


/*
  check CHECK OPTION condition

  SYNOPSIS
    check_option()
    ignore_failure ignore check option fail

  RETURN
    VIEW_CHECK_OK     OK
    VIEW_CHECK_ERROR  FAILED
    VIEW_CHECK_SKIP   FAILED, but continue
*/

int st_table_list::view_check_option(THD *thd, bool ignore_failure)
{
  if (check_option && check_option->val_int() == 0)
  {
    TABLE_LIST *view= top_table();
    if (ignore_failure)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_VIEW_CHECK_FAILED, ER(ER_VIEW_CHECK_FAILED),
                          view->view_db.str, view->view_name.str);
      return(VIEW_CHECK_SKIP);
    }
    else
    {
      my_error(ER_VIEW_CHECK_FAILED, MYF(0), view->view_db.str, view->view_name.str);
      return(VIEW_CHECK_ERROR);
    }
  }
  return(VIEW_CHECK_OK);
}


/*
  Find table in underlying tables by mask and check that only this
  table belong to given mask

  SYNOPSIS
    st_table_list::check_single_table()
    table	reference on variable where to store found table
		(should be 0 on call, to find table, or point to table for
		unique test)
    map         bit mask of tables
    view        view for which we are looking table

  RETURN
    FALSE table not found or found only one
    TRUE  found several tables
*/

bool st_table_list::check_single_table(st_table_list **table, table_map map,
                                       st_table_list *view)
{
  for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
  {
    if (tbl->table)
    {
      if (tbl->table->map & map)
      {
	if (*table)
	  return TRUE;
        *table= tbl;
        tbl->check_option= view->check_option;
      }
    }
    else if (tbl->check_single_table(table, map, view))
      return TRUE;
  }
  return FALSE;
}


/*
  Set insert_values buffer

  SYNOPSIS
    set_insert_values()
    mem_root   memory pool for allocating

  RETURN
    FALSE - OK
    TRUE  - out of memory
*/

bool st_table_list::set_insert_values(MEM_ROOT *mem_root)
{
  if (table)
  {
    if (!table->insert_values &&
        !(table->insert_values= (byte *)alloc_root(mem_root,
                                                   table->s->rec_buff_length)))
      return TRUE;
  }
  else
  {
    DBUG_ASSERT(view && ancestor);
    for (TABLE_LIST *tbl= ancestor; tbl; tbl= tbl->next_local)
      if (tbl->set_insert_values(mem_root))
        return TRUE;
  }
  return FALSE;
}


/*
  Test if this is a leaf with respect to name resolution.

  SYNOPSIS
    st_table_list::is_leaf_for_name_resolution()

  DESCRIPTION
    A table reference is a leaf with respect to name resolution if
    it is either a leaf node in a nested join tree (table, view,
    schema table, subquery), or an inner node that represents a
    NATURAL/USING join, or a nested join with materialized join
    columns.

  RETURN
    TRUE if a leaf, FALSE otherwise.
*/
bool st_table_list::is_leaf_for_name_resolution()
{
  return (view || is_natural_join || is_join_columns_complete ||
          !nested_join);
}


/*
  Retrieve the first (left-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    st_table_list::first_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the left-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The left-most child of a nested table reference is the last element
    in the list of children because the children are inserted in
    reverse order.

  RETURN
    If 'this' is a nested table reference - the left-most child of
      the tree rooted in 'this',
    else return 'this'
*/

TABLE_LIST *st_table_list::first_leaf_for_name_resolution()
{
  TABLE_LIST *cur_table_ref;
  NESTED_JOIN *cur_nested_join;
  LINT_INIT(cur_table_ref);

  if (is_leaf_for_name_resolution())
    return this;
  DBUG_ASSERT(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    List_iterator_fast<TABLE_LIST> it(cur_nested_join->join_list);
    cur_table_ref= it++;
    /*
      If the current nested join is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the first operand is
      already at the front of the list. Otherwise the first operand
      is in the end of the list of join operands.
    */
    if (!(cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      TABLE_LIST *next;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*
  Retrieve the last (right-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    st_table_list::last_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the right-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The right-most child of a nested table reference is the first
    element in the list of children because the children are inserted
    in reverse order.

  RETURN
    - If 'this' is a nested table reference - the right-most child of
      the tree rooted in 'this',
    - else - 'this'
*/

TABLE_LIST *st_table_list::last_leaf_for_name_resolution()
{
  TABLE_LIST *cur_table_ref= this;
  NESTED_JOIN *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  DBUG_ASSERT(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    cur_table_ref= cur_nested_join->join_list.head();
    /*
      If the current nested is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the last operand is in the
      end of the list.
    */
    if ((cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      List_iterator_fast<TABLE_LIST> it(cur_nested_join->join_list);
      TABLE_LIST *next;
      cur_table_ref= it++;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


Natural_join_column::Natural_join_column(Field_translator *field_param,
                                         TABLE_LIST *tab)
{
  DBUG_ASSERT(tab->field_translation);
  view_field= field_param;
  table_field= NULL;
  table_ref= tab;
  is_common= FALSE;
}


Natural_join_column::Natural_join_column(Field *field_param,
                                         TABLE_LIST *tab)
{
  DBUG_ASSERT(tab->table == field_param->table);
  table_field= field_param;
  view_field= NULL;
  table_ref= tab;
  is_common= FALSE;
}


const char *Natural_join_column::name()
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    return view_field->name;
  }

  return table_field->field_name;
}


Item *Natural_join_column::create_item(THD *thd)
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    return create_view_field(thd, table_ref, &view_field->item,
                             view_field->name);
  }
  return new Item_field(thd, &thd->lex->current_select->context, table_field);
}


Field *Natural_join_column::field()
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    return NULL;
  }
  return table_field;
}


const char *Natural_join_column::table_name()
{
  return table_ref->alias;
}


const char *Natural_join_column::db_name()
{
  if (view_field)
    return table_ref->view_db.str;

  DBUG_ASSERT(!strcmp(table_ref->db,
                      table_ref->table->s->db));
  return table_ref->db;
}


GRANT_INFO *Natural_join_column::grant()
{
  if (view_field)
    return &(table_ref->grant);
  return &(table_ref->table->grant);
}



#ifndef NO_EMBEDDED_ACCESS_CHECKS

/*
  Check the access rights for the current join column.
  columns.

  SYNOPSIS
    Natural_join_column::check_grants()

  DESCRIPTION
    Check the access rights to a column from a natural join in a generic
    way that hides the heterogeneity of the column representation - whether
    it is a view or a stored table colum.

  RETURN
    FALSE  The column can be accessed
    TRUE   There are no access rights to all equivalent columns
*/

bool
Natural_join_column::check_grants(THD *thd, const char *name, uint length)
{
  GRANT_INFO *grant;
  const char *db_name;
  const char *table_name;

  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    grant= &(table_ref->grant);
    db_name= table_ref->view_db.str;
    table_name= table_ref->view_name.str;
  }
  else
  {
    DBUG_ASSERT(table_field && view_field == NULL);
    grant= &(table_ref->table->grant);
    db_name= table_ref->table->s->db;
    table_name= table_ref->table->s->table_name;
  }

  return check_grant_column(thd, grant, db_name, table_name, name, length);
}
#endif


void Field_iterator_view::set(TABLE_LIST *table)
{
  DBUG_ASSERT(table->field_translation);
  view= table;
  ptr= table->field_translation;
  array_end= table->field_translation_end;
}


const char *Field_iterator_table::name()
{
  return (*ptr)->field_name;
}


Item *Field_iterator_table::create_item(THD *thd)
{
  return new Item_field(thd, &thd->lex->current_select->context, *ptr);
}


const char *Field_iterator_view::name()
{
  return ptr->name;
}


Item *Field_iterator_view::create_item(THD *thd)
{
  return create_view_field(thd, view, &ptr->item, ptr->name);
}

Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
                        const char *name)
{
  bool save_wrapper= thd->lex->select_lex.no_wrap_view_item;
  Item *field= *field_ref;
  DBUG_ENTER("create_view_field");

  if (view->schema_table_reformed)
  {
    /*
      In case of SHOW command (schema_table_reformed set) all items are
      fixed
    */
    DBUG_ASSERT(field && field->fixed);
    DBUG_RETURN(field);
  }

  DBUG_ASSERT(field);
  thd->lex->current_select->no_wrap_view_item= TRUE;
  if (!field->fixed)
  {
    if (field->fix_fields(thd, field_ref))
    {
      thd->lex->current_select->no_wrap_view_item= save_wrapper;
      DBUG_RETURN(0);
    }
    field= *field_ref;
  }
  thd->lex->current_select->no_wrap_view_item= save_wrapper;
  if (thd->lex->current_select->no_wrap_view_item)
  {
    DBUG_RETURN(field);
  }
  Item *item= new Item_direct_view_ref(&view->view->select_lex.context,
                                       field_ref, view->alias,
                                       name);
  DBUG_RETURN(item);
}


void Field_iterator_natural_join::set(TABLE_LIST *table_ref)
{
  DBUG_ASSERT(table_ref->join_columns);
  delete column_ref_it;

  /*
    TODO: try not to allocate new iterator every time. If we have to,
    then check for out of memory condition.
  */
  column_ref_it= new List_iterator_fast<Natural_join_column>
                     (*(table_ref->join_columns));
  cur_column_ref= (*column_ref_it)++;
}


void Field_iterator_natural_join::next()
{
  cur_column_ref= (*column_ref_it)++;
  DBUG_ASSERT(!cur_column_ref || ! cur_column_ref->table_field ||
              cur_column_ref->table_ref->table ==
              cur_column_ref->table_field->table);
}


void Field_iterator_table_ref::set_field_iterator()
{
  DBUG_ENTER("Field_iterator_table_ref::set_field_iterator");
  /*
    If the table reference we are iterating over is a natural join, or it is
    an operand of a natural join, and TABLE_LIST::join_columns contains all
    the columns of the join operand, then we pick the columns from
    TABLE_LIST::join_columns, instead of the  orginial container of the
    columns of the join operator.
  */
  if (table_ref->is_join_columns_complete)
  {
    /* Necesary, but insufficient conditions. */
    DBUG_ASSERT(table_ref->is_natural_join ||
                table_ref->nested_join ||
                table_ref->join_columns &&
                /* This is a merge view. */
                ((table_ref->field_translation &&
                  table_ref->join_columns->elements ==
                  (ulong)(table_ref->field_translation_end -
                          table_ref->field_translation)) ||
                 /* This is stored table or a tmptable view. */
                 (!table_ref->field_translation &&
                  table_ref->join_columns->elements ==
                  table_ref->table->s->fields)));
    field_it= &natural_join_it;
    DBUG_PRINT("info",("field_it for '%s' is Field_iterator_natural_join",
                       table_ref->alias));
  }
  /* This is a merge view, so use field_translation. */
  else if (table_ref->field_translation)
  {
    DBUG_ASSERT(table_ref->view &&
                table_ref->effective_algorithm == VIEW_ALGORITHM_MERGE);
    field_it= &view_field_it;
    DBUG_PRINT("info", ("field_it for '%s' is Field_iterator_view",
                        table_ref->alias));
  }
  /* This is a base table or stored view. */
  else
  {
    DBUG_ASSERT(table_ref->table || table_ref->view);
    field_it= &table_field_it;
    DBUG_PRINT("info", ("field_it for '%s' is Field_iterator_table",
                        table_ref->alias));
  }
  field_it->set(table_ref);
  DBUG_VOID_RETURN;
}


void Field_iterator_table_ref::set(TABLE_LIST *table)
{
  DBUG_ASSERT(table);
  first_leaf= table->first_leaf_for_name_resolution();
  last_leaf=  table->last_leaf_for_name_resolution();
  DBUG_ASSERT(first_leaf && last_leaf);
  table_ref= first_leaf;
  set_field_iterator();
}


void Field_iterator_table_ref::next()
{
  /* Move to the next field in the current table reference. */
  field_it->next();
  /*
    If all fields of the current table reference are exhausted, move to
    the next leaf table reference.
  */
  if (field_it->end_of_fields() && table_ref != last_leaf)
  {
    table_ref= table_ref->next_name_resolution_table;
    DBUG_ASSERT(table_ref);
    set_field_iterator();
  }
}


const char *Field_iterator_table_ref::table_name()
{
  if (table_ref->view)
    return table_ref->view_name.str;
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->table_name();

  DBUG_ASSERT(!strcmp(table_ref->table_name,
                      table_ref->table->s->table_name));
  return table_ref->table_name;
}


const char *Field_iterator_table_ref::db_name()
{
  if (table_ref->view)
    return table_ref->view_db.str;
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->db_name();

  DBUG_ASSERT(!strcmp(table_ref->db, table_ref->table->s->db));
  return table_ref->db;
}


GRANT_INFO *Field_iterator_table_ref::grant()
{
  if (table_ref->view)
    return &(table_ref->grant);
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->grant();
  return &(table_ref->table->grant);
}


/*
  Create new or return existing column reference to a column of a
  natural/using join.

  SYNOPSIS
    Field_iterator_table_ref::get_or_create_column_ref()
    thd         [in]  pointer to current thread
    is_created  [out] set to TRUE if the column was created,
                      FALSE if we return an already created colum

  DESCRIPTION
    TODO

  RETURN
    #     Pointer to a column of a natural join (or its operand)
    NULL  No memory to allocate the column
*/

Natural_join_column *
Field_iterator_table_ref::get_or_create_column_ref(THD *thd, bool *is_created)
{
  Natural_join_column *nj_col;

  *is_created= TRUE;
  if (field_it == &table_field_it)
  {
    /* The field belongs to a stored table. */
    Field *field= table_field_it.field();
    nj_col= new Natural_join_column(field, table_ref);
  }
  else if (field_it == &view_field_it)
  {
    /* The field belongs to a merge view or information schema table. */
    Field_translator *translated_field= view_field_it.field_translator();
    nj_col= new Natural_join_column(translated_field, table_ref);
  }
  else
  {
    /*
      The field belongs to a NATURAL join, therefore the column reference was
      already created via one of the two constructor calls above. In this case
      we just return the already created column reference.
    */
    *is_created= FALSE;
    nj_col= natural_join_it.column_ref();
    DBUG_ASSERT(nj_col);
  }
  DBUG_ASSERT(!nj_col->table_field ||
              nj_col->table_ref->table == nj_col->table_field->table);
  return nj_col;
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<String>;
template class List_iterator<String>;
#endif
