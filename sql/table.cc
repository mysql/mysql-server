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


	/* Functions defined in this file */

static void frm_error(int error,TABLE *form,const char *name,int errortype);
static void fix_type_pointers(const char ***array, TYPELIB *point_to_type,
			      uint types, char **names);
static uint find_field(TABLE *form,uint start,uint length);


static byte* get_field_name(Field *buff,uint *length,
			    my_bool not_used __attribute__((unused)))
{
  *length= (uint) strlen(buff->field_name);
  return (byte*) buff->field_name;
}

	/* Open a .frm file */

int openfrm(const char *name, const char *alias, uint db_stat, uint prgflag,
	    uint ha_open_flags, TABLE *outparam)
{
  reg1 uint i;
  reg2 uchar *strpos;
  int	 j,error;
  uint	 rec_buff_length,n_length,int_length,records,key_parts,keys,
    interval_count,interval_parts,read_length,db_create_options;
  ulong  pos;
  char	 index_file[FN_REFLEN], *names,*keynames;
  uchar  head[288],*disk_buff,new_field_pack_flag;
  my_string record;
  const char **int_array;
  bool	 new_frm_ver,use_hash, null_field_first;
  File	 file;
  Field  **field_ptr,*reg_field;
  KEY	 *keyinfo;
  KEY_PART_INFO *key_part;
  uchar *null_pos;
  uint null_bit;
  SQL_CRYPT *crypted=0;
  DBUG_ENTER("openfrm");
  DBUG_PRINT("enter",("name: '%s'  form: %lx",name,outparam));

  bzero((char*) outparam,sizeof(*outparam));
  outparam->blob_ptr_size=sizeof(char*);
  disk_buff=NULL; record= NULL; keynames=NullS;
  outparam->db_stat = db_stat;
  error=1;

  init_sql_alloc(&outparam->mem_root,1024,0);
  MEM_ROOT *old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
  my_pthread_setspecific_ptr(THR_MALLOC,&outparam->mem_root);

  outparam->real_name=strdup_root(&outparam->mem_root,
				  name+dirname_length(name));
  *fn_ext(outparam->real_name)='\0';		// Remove extension
  outparam->table_name=my_strdup(alias,MYF(MY_WME));
  if (!outparam->real_name || !outparam->table_name)
    goto err_end;

  if ((file=my_open(fn_format(index_file,name,"",reg_ext,4),
		    O_RDONLY | O_SHARE,
		    MYF(0)))
      < 0)
  {
    goto err_end; /* purecov: inspected */
  }
  error=4;
  if (!(outparam->path= strdup_root(&outparam->mem_root,name)))
    goto err_not_open;
  *fn_ext(outparam->path)='\0';		// Remove extension

  if (my_read(file,(byte*) head,64,MYF(MY_NABP))) goto err_not_open;
  if (head[0] != (uchar) 254 || head[1] != 1 ||
      (head[2] != FRM_VER && head[2] != FRM_VER+1))
    goto err_not_open; /* purecov: inspected */
  new_field_pack_flag=head[27];
  new_frm_ver= (head[2] == FRM_VER+1);

  error=3;
  if (!(pos=get_form_pos(file,head,(TYPELIB*) 0)))
    goto err_not_open; /* purecov: inspected */
  *fn_ext(index_file)='\0';			// Remove .frm extension

  outparam->db_type=ha_checktype((enum db_type) (uint) *(head+3));
  outparam->db_create_options=db_create_options=uint2korr(head+30);
  outparam->db_options_in_use=outparam->db_create_options;
  null_field_first=0;
  if (!head[32])				// New frm file in 3.23
  {
    outparam->avg_row_length=uint4korr(head+34);
    outparam->row_type=(row_type) head[40];
    outparam->raid_type=   head[41];
    outparam->raid_chunks= head[42];
    outparam->raid_chunksize= uint4korr(head+43);
    null_field_first=1;
  }
  outparam->db_record_offset=1;
  if (db_create_options & HA_OPTION_LONG_BLOB_PTR)
    outparam->blob_ptr_size=portable_sizeof_char_ptr;
  outparam->db_low_byte_first=test(outparam->db_type == DB_TYPE_MYISAM ||
				   outparam->db_type == DB_TYPE_BERKELEY_DB ||
				   outparam->db_type == DB_TYPE_HEAP);

  error=4;
  outparam->max_rows=uint4korr(head+18);
  outparam->min_rows=uint4korr(head+22);

  /* Read keyinformation */
  VOID(my_seek(file,(ulong) uint2korr(head+6),MY_SEEK_SET,MYF(0)));
  if (read_string(file,(gptr*) &disk_buff,(uint) uint2korr(head+28)))
    goto err_not_open; /* purecov: inspected */
  outparam->keys=keys=disk_buff[0];
  outparam->keys_in_use= (((key_map) 1) << keys)- (key_map) 1;

  outparam->key_parts=key_parts=disk_buff[1];
  n_length=keys*sizeof(KEY)+key_parts*sizeof(KEY_PART_INFO);
  if (!(keyinfo = (KEY*) alloc_root(&outparam->mem_root,
				    n_length+uint2korr(disk_buff+4))))
    goto err_not_open; /* purecov: inspected */
  bzero((char*) keyinfo,n_length);
  outparam->key_info=keyinfo;
  outparam->max_key_length=0;
  key_part= (KEY_PART_INFO*) (keyinfo+keys);
  strpos=disk_buff+6;

  ulong *rec_per_key;
  if (!(rec_per_key= (ulong*) alloc_root(&outparam->mem_root,
					 sizeof(ulong*)*key_parts)))
    goto err_not_open;

  for (i=0 ; i < keys ; i++, keyinfo++)
  {
    uint null_parts=0;
    keyinfo->flags=	 ((uint) strpos[0]) ^ HA_NOSAME;
    keyinfo->key_length= (uint) uint2korr(strpos+1);
    keyinfo->key_parts=  (uint) strpos[3];  strpos+=4;
    keyinfo->key_part=	 key_part;
    keyinfo->rec_per_key= rec_per_key;
    for (j=keyinfo->key_parts ; j-- ; key_part++)
    {
      *rec_per_key++=0;
      key_part->fieldnr=	(uint16) (uint2korr(strpos) & FIELD_NR_MASK);
      key_part->offset= (uint) uint2korr(strpos+2)-1;
      key_part->key_type=	(uint) uint2korr(strpos+5);
      // key_part->field=	(Field*) 0;	// Will be fixed later
      if (new_frm_ver)
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
    keyinfo->key_length+=null_parts;
    set_if_bigger(outparam->max_key_length,keyinfo->key_length+
		  keyinfo->key_parts);
    if (keyinfo->flags & HA_NOSAME)
      set_if_bigger(outparam->max_unique_length,keyinfo->key_length);
  }

  (void) strmov(keynames= (char *) key_part,(char *) strpos);
  outparam->reclength = uint2korr((head+16));
  if (*(head+26) == 1)
    outparam->system=1;				/* one-record-database */
#ifdef HAVE_CRYPTED_FRM
  else if (*(head+26) == 2)
  {
    extern SQL_CRYPT *get_crypt_for_frm(void);
    my_pthread_setspecific_ptr(THR_MALLOC,old_root);
    crypted=get_crypt_for_frm();
    my_pthread_setspecific_ptr(THR_MALLOC,&outparam->mem_root);
    outparam->crypted=1;
  }
#endif

  /* Allocate handler */
  if (!(outparam->file= get_new_handler(outparam,outparam->db_type)))
    goto err_not_open;

  error=4;
  outparam->reginfo.lock_type= TL_UNLOCK;
  outparam->current_lock=F_UNLCK;
  if ((db_stat & HA_OPEN_KEYFILE) || (prgflag & DELAYED_OPEN)) records=2;
  else records=1;
  if (prgflag & (READ_ALL+EXTRA_RECORD)) records++;
  /* QQ: TODO, remove the +1 from below */
  rec_buff_length=ALIGN_SIZE(outparam->reclength+1+
			     outparam->file->extra_rec_buf_length());
  if (!(outparam->record[0]= (byte*)
	(record = (char *) alloc_root(&outparam->mem_root,
				      rec_buff_length * records))))
    goto err_not_open;				/* purecov: inspected */
  record[outparam->reclength]=0;		// For purify and ->c_ptr()
  outparam->rec_buff_length=rec_buff_length;
  if (my_pread(file,(byte*) record,(uint) outparam->reclength,
	       (ulong) (uint2korr(head+6)+uint2korr(head+14)),
	       MYF(MY_NABP)))
    goto err_not_open; /* purecov: inspected */
  for (i=0 ; i < records ; i++, record+=rec_buff_length)
  {
    outparam->record[i]=(byte*) record;
    if (i)
      memcpy(record,record-rec_buff_length,(uint) outparam->reclength);
  }

  if (records == 2)
  {						/* fix for select */
    outparam->record[2]=outparam->record[1];
    if (db_stat & HA_READ_ONLY)
      outparam->record[1]=outparam->record[0]; /* purecov: inspected */
  }

  VOID(my_seek(file,pos,MY_SEEK_SET,MYF(0)));
  if (my_read(file,(byte*) head,288,MYF(MY_NABP))) goto err_not_open;
  if (crypted)
  {
    crypted->decode((char*) head+256,288-256);
    if (sint2korr(head+284) != 0)		// Should be 0
      goto err_not_open;			// Wrong password
  }

  outparam->fields= uint2korr(head+258);
  pos=uint2korr(head+260);			/* Length of all screens */
  n_length=uint2korr(head+268);
  interval_count=uint2korr(head+270);
  interval_parts=uint2korr(head+272);
  int_length=uint2korr(head+274);
  outparam->null_fields=uint2korr(head+282);
  outparam->comment=strdup_root(&outparam->mem_root,
				(char*) head+47);

  DBUG_PRINT("form",("i_count: %d  i_parts: %d  index: %d  n_length: %d  int_length: %d", interval_count,interval_parts, outparam->keys,n_length,int_length));

  if (!(field_ptr = (Field **)
	alloc_root(&outparam->mem_root,
		   (uint) ((outparam->fields+1)*sizeof(Field*)+
			   interval_count*sizeof(TYPELIB)+
			   (outparam->fields+interval_parts+
			    keys+3)*sizeof(my_string)+
			   (n_length+int_length)))))
    goto err_not_open; /* purecov: inspected */

  outparam->field=field_ptr;
  read_length=((uint) (outparam->fields*11)+pos+
	       (uint) (n_length+int_length));
  if (read_string(file,(gptr*) &disk_buff,read_length))
    goto err_not_open; /* purecov: inspected */
  if (crypted)
  {
    crypted->decode((char*) disk_buff,read_length);
    delete crypted;
    crypted=0;
  }
  strpos= disk_buff+pos;

  outparam->intervals= (TYPELIB*) (field_ptr+outparam->fields+1);
  int_array= (const char **) (outparam->intervals+interval_count);
  names= (char*) (int_array+outparam->fields+interval_parts+keys+3);
  if (!interval_count)
    outparam->intervals=0;			// For better debugging
  memcpy((char*) names, strpos+(outparam->fields*11),
	 (uint) (n_length+int_length));

  fix_type_pointers(&int_array,&outparam->fieldnames,1,&names);
  fix_type_pointers(&int_array,outparam->intervals,interval_count,
		    &names);
  if (keynames)
    fix_type_pointers(&int_array,&outparam->keynames,1,&keynames);
  VOID(my_close(file,MYF(MY_WME)));
  file= -1;

  record=(char*) outparam->record[0]-1;		/* Fieldstart = 1 */
  if (null_field_first)
  {
    outparam->null_flags=null_pos=(uchar*) record+1;
    null_bit= (db_create_options & HA_OPTION_PACK_RECORD) ? 1 : 2;
    outparam->null_bytes=(outparam->null_fields+null_bit+6)/8;
  }
  else
  {
    outparam->null_bytes=(outparam->null_fields+7)/8;
    outparam->null_flags=null_pos=
      (uchar*) (record+1+outparam->reclength-outparam->null_bytes);
    null_bit=1;
  }

  use_hash= outparam->fields >= MAX_FIELDS_BEFORE_HASH;
  if (use_hash)
    use_hash= !hash_init(&outparam->name_hash,
			 outparam->fields,0,0,
			 (hash_get_key) get_field_name,0,
			 HASH_CASE_INSENSITIVE);

  for (i=0 ; i < outparam->fields; i++, strpos+= 11, field_ptr++)
  {
    uint pack_flag= uint2korr(strpos+6);
    uint interval_nr= (uint) strpos[10];

    *field_ptr=reg_field=
      make_field(record+uint2korr(strpos+4),
		 (uint32) strpos[3],		// field_length
		 null_pos,null_bit,
		 pack_flag,
		 (Field::utype) MTYP_TYPENR((uint) strpos[8]),
		 (interval_nr ?
		  outparam->intervals+interval_nr-1 :
		  (TYPELIB*) 0),
		 outparam->fieldnames.type_names[i],
		 outparam);
    if (!(reg_field->flags & NOT_NULL_FLAG))
    {
      if ((null_bit<<=1) == 256)
      {
	null_pos++;
	null_bit=1;
      }
    }
    if (reg_field->unireg_check == Field::NEXT_NUMBER)
    {
      if ((int) (outparam->next_number_index= (uint)
		 find_ref_key(outparam,reg_field,
			      &outparam->next_number_key_offset)) < 0)
	reg_field->unireg_check=Field::NONE;	/* purecov: inspected */
      else
      {
	outparam->found_next_number_field=reg_field;
	reg_field->flags|=AUTO_INCREMENT_FLAG;
      }
    }
    if (outparam->timestamp_field == reg_field)
      outparam->timestamp_field_offset=i;
    if (use_hash)
      (void) hash_insert(&outparam->name_hash,(byte*) *field_ptr); // Will never fail
  }
  *field_ptr=0;					// End marker

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    uint primary_key=(uint) (find_type((char*) "PRIMARY",&outparam->keynames,
				       3)-1);
    uint ha_option=outparam->file->option_flag();
    keyinfo=outparam->key_info;
    key_part=keyinfo->key_part;

    for (uint key=0 ; key < outparam->keys ; key++,keyinfo++)
    {
      uint usable_parts=0;
      keyinfo->name=(char*) outparam->keynames.type_names[key];
      if (primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
	/*
	  If the UNIQUE key don't have NULL columns, declare this as
	  a primary key.
	*/
	primary_key=key;
	for (i=0 ; i < keyinfo->key_parts ;i++)
	{
	  if (!key_part[i].fieldnr ||
	      outparam->field[key_part[i].fieldnr-1]->null_ptr)
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
	if (key_part->fieldnr > outparam->fields)
	  goto err_not_open; // sanity check
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
	  }
	  if (field->type() == FIELD_TYPE_BLOB ||
	      field->real_type() == FIELD_TYPE_VAR_STRING)
	  {
	    if (field->type() == FIELD_TYPE_BLOB)
	      key_part->key_part_flag|= HA_BLOB_PART;
	    keyinfo->extra_length+=HA_KEY_BLOB_LENGTH;
	    key_part->store_length+=HA_KEY_BLOB_LENGTH;
	  }
	  if (i == 0 && key != primary_key)
	    field->flags |=
	      ((keyinfo->flags & HA_NOSAME) &&
	       field->key_length() ==
	       keyinfo->key_length ? UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
	  if (i == 0)
	    field->key_start|= ((key_map) 1 << key);
	  if ((ha_option & HA_HAVE_KEY_READ_ONLY) &&
	      field->key_length() == key_part->length &&
	      field->type() != FIELD_TYPE_BLOB &&
	      (field->key_type() != HA_KEYTYPE_TEXT ||
	       (!(ha_option & HA_KEY_READ_WRONG_STR) &&
		!(keyinfo->flags & HA_FULLTEXT))))
	    field->part_of_key|= ((key_map) 1 << key);
	  if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
	      usable_parts == i)
	    usable_parts++;			// For FILESORT
	  field->flags|= PART_KEY_FLAG;
	  if (key == primary_key)
	  {
	    field->flags|= PRI_KEY_FLAG;
	    if (ha_option & HA_PRIMARY_KEY_IN_READ_INDEX)
	      field->part_of_key|= ((key_map) 1 << primary_key);
	  }
	  if (field->key_length() != key_part->length)
	  {
	    key_part->key_part_flag|= HA_PART_KEY;
	    if (field->type() != FIELD_TYPE_BLOB)
	    {					// Create a new field
	      field=key_part->field=field->new_field(outparam);
	      field->field_length=key_part->length;
	    }
	  }
	}
	else
	{					// Error: shorten key
	  keyinfo->key_parts=usable_parts;
	  keyinfo->flags=0;
	}
      }
      keyinfo->usable_key_parts=usable_parts; // Filesort
    }
    if (primary_key < MAX_KEY && 
	(outparam->keys_in_use & ((key_map) 1 << primary_key)))
    {
      outparam->primary_key=primary_key;
      if (outparam->file->option_flag() & HA_PRIMARY_KEY_IN_READ_INDEX)
	outparam->ref_primary_key= (key_map) 1 << primary_key;
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
      outparam->primary_key = MAX_KEY; // we do not have a primary key
  }
  else
    outparam->primary_key= MAX_KEY;
  x_free((gptr) disk_buff);
  disk_buff=0;
  if (new_field_pack_flag <= 1)
  {			/* Old file format with default null */
    uint null_length=(outparam->null_fields+7)/8;
    bfill(outparam->null_flags,null_length,255);
    bfill(outparam->null_flags+outparam->rec_buff_length,null_length,255);
    if (records > 2)
      bfill(outparam->null_flags+outparam->rec_buff_length*2,null_length,255);
  }

  if (outparam->blob_fields)
  {
    Field **ptr;
    Field_blob **save;

    if (!(outparam->blob_field=save=
	  (Field_blob**) alloc_root(&outparam->mem_root,
				    (uint) (outparam->blob_fields+1)*
				    sizeof(Field_blob*))))
      goto err_not_open;
    for (ptr=outparam->field ; *ptr ; ptr++)
    {
      if ((*ptr)->flags & BLOB_FLAG)
	(*save++)= (Field_blob*) *ptr;
    }
    *save=0;					// End marker
  }
  else
    outparam->blob_field=
      (Field_blob**) (outparam->field+outparam->fields); // Point at null ptr

  /* The table struct is now initialzed;  Open the table */
  error=2;
  if (db_stat)
  {
    int err;
    if ((err=(outparam->file->
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
      outparam->crashed=((err == HA_ERR_CRASHED_ON_USAGE) &&
			 outparam->file->auto_repair() &&
			 !(ha_open_flags & HA_OPEN_FOR_REPAIR));
      goto err_not_open; /* purecov: inspected */
    }
  }
  outparam->db_low_byte_first=outparam->file->low_byte_first();

  my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  opened_tables++;
#ifndef DBUG_OFF
  if (use_hash)
    (void) hash_check(&outparam->name_hash);
#endif
  DBUG_RETURN (0);

 err_not_open:
  x_free((gptr) disk_buff);
  if (file > 0)
    VOID(my_close(file,MYF(MY_WME)));

 err_end:					/* Here when no file */
  delete crypted;
  my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  frm_error(error,outparam,name,ME_ERROR+ME_WAITTANG);
  delete outparam->file;
  outparam->file=0;				// For easyer errorchecking
  outparam->db_stat=0;
  free_root(&outparam->mem_root,MYF(0));
  my_free(outparam->table_name,MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN (error);
} /* openfrm */


	/* close a .frm file and it's tables */

int closefrm(register TABLE *table)
{
  int error=0;
  DBUG_ENTER("closefrm");
  if (table->db_stat)
    error=table->file->close();
  if (table->table_name)
  {
    my_free(table->table_name,MYF(0));
    table->table_name=0;
  }
  if (table->fields)
  {
    for (Field **ptr=table->field ; *ptr ; ptr++)
      delete *ptr;
    table->fields=0;
  }
  delete table->file;
  table->file=0;				/* For easyer errorchecking */
  hash_free(&table->name_hash);
  free_root(&table->mem_root,MYF(0));
  DBUG_RETURN(error);
}


/* Deallocate temporary blob storage */

void free_blobs(register TABLE *table)
{
  for (Field_blob **ptr=table->blob_field ; *ptr ; ptr++)
    (*ptr)->free();
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
  VOID(my_chsize(file,newpos,MYF(MY_WME)));	/* Append file with '\0' */
  DBUG_RETURN(newpos);
} /* make_new_entry */


	/* error message when opening a form file */

static void frm_error(int error, TABLE *form, const char *name, myf errortype)
{
  int err_no;
  char buff[FN_REFLEN];
  const char *form_dev="",*datext;
  DBUG_ENTER("frm_error");

  switch (error) {
  case 1:
    if (my_errno == ENOENT)
    {
      char *db;
      uint length=dirname_part(buff,name);
      buff[length-1]=0;
      db=buff+dirname_length(buff);
      my_error(ER_NO_SUCH_TABLE,MYF(0),db,form->real_name);
    }
    else
      my_error(ER_FILE_NOT_FOUND,errortype,
	       fn_format(buff,name,form_dev,reg_ext,0),my_errno);
    break;
  case 2:
  {
    datext=form->file ? *form->file->bas_ext() : "";
    err_no= (my_errno == ENOENT) ? ER_FILE_NOT_FOUND : (my_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    my_error(err_no,errortype,
	     fn_format(buff,form->real_name,form_dev,datext,2),my_errno);
    break;
  }
  default:				/* Better wrong error than none */
  case 4:
    my_error(ER_NOT_FORM_FILE,errortype,
	     fn_format(buff,name,form_dev,reg_ext,0));
    break;
  }
  DBUG_VOID_RETURN;
} /* frm_error */


	/*
	** fix a str_type to a array type
	** typeparts sepearated with some char. differents types are separated
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
      ptr+=2;				/* Skipp end mark and last 0 */
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
  if (!(result->type_names=(const char **) sql_alloc(sizeof(char *)*
						     (result->count+1))))
    return 0;
  List_iterator<String> it(strings);
  String *tmp;
  for (uint i=0; (tmp=it++) ; i++)
    result->type_names[i]=tmp->ptr();
  result->type_names[result->count]=0;		// End marker
  return result;
}


	/*
	** Search after a field with given start & length
	** If an exact field isn't found, return longest field with starts
	** at right position.
	** Return 0 on error, else field number+1
	** This is needed because in some .frm fields 'fieldnr' was saved wrong
	*/

static uint find_field(TABLE *form,uint start,uint length)
{
  Field **field;
  uint i,pos;

  pos=0;

  for (field=form->field, i=1 ; i<= form->fields ; i++,field++)
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


	/* Check that the integer is in the internvall */

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


void append_unescaped(String *res,const char *pos)
{
  for ( ; *pos ; pos++)
  {
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
      res->append('\\');		/* This gives better readbility */
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
}

	/* Create a .frm file */

File create_frm(register my_string name, uint reclength, uchar *fileinfo,
		HA_CREATE_INFO *create_info, uint keys)
{
  register File file;
  uint key_length;
  ulong length;
  char fill[IO_SIZE];

#if SIZEOF_OFF_T > 4
  /* Fix this in MySQL 4.0;  The current limit is 4G rows (QQ) */
  if (create_info->max_rows > ~(ulong) 0)
    create_info->max_rows= ~(ulong) 0;
  if (create_info->min_rows > ~(ulong) 0)
    create_info->min_rows= ~(ulong) 0;
#endif

  if ((file=my_create(name,CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    bzero((char*) fileinfo,64);
    fileinfo[0]=(uchar) 254; fileinfo[1]= 1; fileinfo[2]= FRM_VER+1; // Header
    fileinfo[3]= (uchar) ha_checktype(create_info->db_type);
    fileinfo[4]=1;
    int2store(fileinfo+6,IO_SIZE);		/* Next block starts here */
    key_length=keys*(7+NAME_LEN+MAX_REF_PARTS*9)+16;
    length=(ulong) next_io_size((ulong) (IO_SIZE+key_length+reclength));
    int4store(fileinfo+10,length);
    int2store(fileinfo+14,key_length);
    int2store(fileinfo+16,reclength);
    int4store(fileinfo+18,create_info->max_rows);
    int4store(fileinfo+22,create_info->min_rows);
    fileinfo[27]=2;				// Use long pack-fields
    create_info->table_options|=HA_OPTION_LONG_BLOB_PTR; // Use portable blob pointers
    int2store(fileinfo+30,create_info->table_options);
    fileinfo[32]=0;				// No filename anymore
    int4store(fileinfo+34,create_info->avg_row_length);
    fileinfo[40]= (uchar) create_info->row_type;
    fileinfo[41]= (uchar) create_info->raid_type;
    fileinfo[42]= (uchar) create_info->raid_chunks;
    int4store(fileinfo+43,create_info->raid_chunksize);
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
  return (file);
} /* create_frm */


void update_create_info_from_table(HA_CREATE_INFO *create_info, TABLE *table)
{
  create_info->max_rows=table->max_rows;
  create_info->min_rows=table->min_rows;
  create_info->table_options=table->db_create_options;
  create_info->avg_row_length=table->avg_row_length;
  create_info->row_type=table->row_type;
  create_info->raid_type=table->raid_type;
  create_info->raid_chunks=table->raid_chunks;
  create_info->raid_chunksize=table->raid_chunksize;
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
  Alloc a value as a string and return it
  If field is empty, return NULL
*/

char *get_field(MEM_ROOT *mem, TABLE *table, uint fieldnr)
{
  Field *field=table->field[fieldnr];
  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff));
  field->val_str(&str,&str);
  uint length=str.length();
  if (!length)
    return NullS;
  char *to= (char*) alloc_root(mem,length+1);
  memcpy(to,str.ptr(),(uint) length);
  to[length]=0;
  return to;
}

bool check_db_name(const char *name)
{
  while (*name)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    if (use_mb(default_charset_info))
    {
      int len=my_ismbchar(default_charset_info, name, name+MBMAXLEN);
      if (len)
      {
        name += len;
        continue;
      }
    }
#endif
    if (*name == '/' || *name == FN_LIBCHAR)
      return 1;
    name++;
  }
  return 0;
}


/*
  Allow anything as a table name, as long as it doesn't contain an
  a '/', or a '.' character
  returns 1 on error
*/


bool check_table_name(const char *name, uint length)
{
  const char *end= name+length;

  while (name != end)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    if (use_mb(default_charset_info))
    {
      int len=my_ismbchar(default_charset_info, name, end);
      if (len)
      {
        name += len;
        continue;
      }
    }
#endif
    if (*name == '/' || *name == FN_LIBCHAR || *name == FN_EXTCHAR)
      return 1;
    name++;
  }
  return 0;
}

bool check_column_name(const char *name)
{
  while (*name)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    if (use_mb(default_charset_info))
    {
      int len=my_ismbchar(default_charset_info, name, name+MBMAXLEN);
      if (len)
      {
        name += len;
        continue;
      }
    }
#endif
    if (*name == NAMES_SEP_CHAR)
      return 1;
    name++;
  }
  return 0;
}

/*
** Get type of table from .frm file
*/

db_type get_table_type(const char *name)
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
      (head[2] != FRM_VER && head[2] != FRM_VER+1))
    DBUG_RETURN(DB_TYPE_UNKNOWN);
  DBUG_RETURN(ha_checktype((enum db_type) (uint) *(head+3)));
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<String>;
template class List_iterator<String>;
#endif
