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

/* open a isam-database */

#include "fulltext.h"
#include <m_ctype.h>

#if defined(MSDOS) || defined(__WIN__)
#ifdef __WIN__
#include <fcntl.h>
#else
#include <process.h>			/* Prototype for getpid */
#endif
#endif
#ifdef VMS
#include "static.c"
#endif

static void setup_functions(MYISAM_SHARE *info);
static void setup_key_functions(MI_KEYDEF *keyinfo);
#define get_next_element(to,pos,size) { memcpy((char*) to,pos,(size_t) size); \
					pos+=size;}


/******************************************************************************
** Return the shared struct if the table is already open.
** In MySQL the server will handle version issues.
******************************************************************************/

static MI_INFO *test_if_reopen(char *filename)
{
  LIST *pos;

  for (pos=myisam_open_list ; pos ; pos=pos->next)
  {
    MI_INFO *info=(MI_INFO*) pos->data;
    MYISAM_SHARE *share=info->s;
    if (!strcmp(share->filename,filename) && share->last_version)
      return info;
  }
  return 0;
}


/******************************************************************************
  open a MyISAM database.
  See my_base.h for the handle_locking argument
  if handle_locking and HA_OPEN_ABORT_IF_CRASHED then abort if the table
  is marked crashed or if we are not using locking and the table doesn't
  have an open count of 0.
******************************************************************************/

MI_INFO *mi_open(const char *name, int mode, uint open_flags)
{
  int lock_error,kfile,open_mode,save_errno;
  uint i,j,len,errpos,head_length,base_pos,offset,info_length,extra,keys,
    key_parts,unique_key_parts,tmp_length,uniques;
  char name_buff[FN_REFLEN],*disk_cache,*disk_pos;
  MI_INFO info,*m_info,*old_info;
  MYISAM_SHARE share_buff,*share;
  ulong rec_per_key_part[MI_MAX_POSSIBLE_KEY*MI_MAX_KEY_SEG];
  my_off_t key_root[MI_MAX_POSSIBLE_KEY],key_del[MI_MAX_KEY_BLOCK_SIZE];
  ulonglong max_key_file_length, max_data_file_length;
  DBUG_ENTER("mi_open");

  LINT_INIT(m_info);
  kfile= -1;
  lock_error=1;
  errpos=0;
  head_length=sizeof(share_buff.state.header);
  bzero((byte*) &info,sizeof(info));

  VOID(fn_format(name_buff,name,"",MI_NAME_IEXT,4+16+32));
  pthread_mutex_lock(&THR_LOCK_myisam);
  if (!(old_info=test_if_reopen(name_buff)))
  {
    share= &share_buff;
    bzero((gptr) &share_buff,sizeof(share_buff));
    share_buff.state.rec_per_key_part=rec_per_key_part;
    share_buff.state.key_root=key_root;
    share_buff.state.key_del=key_del;

    if ((kfile=my_open(name_buff,(open_mode=O_RDWR) | O_SHARE,MYF(0))) < 0)
    {
      if ((errno != EROFS && errno != EACCES) ||
	  mode != O_RDONLY ||
	  (kfile=my_open(name_buff,(open_mode=O_RDONLY) | O_SHARE,MYF(0))) < 0)
	goto err;
    }
    errpos=1;
    if (my_read(kfile,(char*) share->state.header.file_version,head_length,
		MYF(MY_NABP)))
      goto err;

    if (memcmp((byte*) share->state.header.file_version,
	       (byte*) myisam_file_magic, 4))
    {
      DBUG_PRINT("error",("Wrong header in %s",name_buff));
      DBUG_DUMP("error_dump",(char*) share->state.header.file_version,
		head_length);
      my_errno=HA_ERR_CRASHED;
      goto err;
    }
    share->options= mi_uint2korr(share->state.header.options);
    if (share->options &
	~(HA_OPTION_PACK_RECORD | HA_OPTION_PACK_KEYS |
	  HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA |
	  HA_OPTION_TEMP_COMPRESS_RECORD | HA_OPTION_CHECKSUM |
	  HA_OPTION_TMP_TABLE | HA_OPTION_DELAY_KEY_WRITE))
    {
      DBUG_PRINT("error",("wrong options: 0x%lx",
			  share->options));
      my_errno=HA_ERR_OLD_FILE;
      goto err;
    }
    info_length=mi_uint2korr(share->state.header.header_length);
    base_pos=mi_uint2korr(share->state.header.base_pos);
    if (!(disk_cache=(char*) my_alloca(info_length)))
    {
      my_errno=ENOMEM;
      goto err;
    }
    errpos=2;

    VOID(my_seek(kfile,0L,MY_SEEK_SET,MYF(0)));
    if (!(open_flags & HA_OPEN_TMP_TABLE))
    {
      if ((lock_error=my_lock(kfile,F_RDLCK,0L,F_TO_EOF,
			      MYF(open_flags & HA_OPEN_WAIT_IF_LOCKED ?
				  0 : MY_DONT_WAIT))) &&
	  !(open_flags & HA_OPEN_IGNORE_IF_LOCKED))
	goto err;
    }
    errpos=3;
    if (my_read(kfile,disk_cache,info_length,MYF(MY_NABP)))
      goto err;
    len=mi_uint2korr(share->state.header.state_info_length);
    keys=    (uint) share->state.header.keys;
    uniques= (uint) share->state.header.uniques;
    key_parts= mi_uint2korr(share->state.header.key_parts);
    unique_key_parts= mi_uint2korr(share->state.header.unique_key_parts);
    tmp_length=(MI_STATE_INFO_SIZE + keys * MI_STATE_KEY_SIZE +
		key_parts*MI_STATE_KEYSEG_SIZE +
		share->state.header.max_block_size*MI_STATE_KEYBLOCK_SIZE);
    if (len != MI_STATE_INFO_SIZE)
    {
      DBUG_PRINT("warning",
		 ("saved_state_info_length: %d  state_info_length: %d",
		  len,MI_STATE_INFO_SIZE));
    }
    share->state_diff_length=len-MI_STATE_INFO_SIZE;

    mi_state_info_read(disk_cache, &share->state);
    len= mi_uint2korr(share->state.header.base_info_length);
    if (len != MI_BASE_INFO_SIZE)
    {
      DBUG_PRINT("warning",("saved_base_info_length: %d  base_info_length: %d",
			    len,MI_BASE_INFO_SIZE))
    }
    disk_pos=my_n_base_info_read(disk_cache+base_pos, &share->base);
    share->state.state_length=base_pos;

    if ((open_flags & HA_OPEN_ABORT_IF_CRASHED) &&
	((share->state.changed & STATE_CRASHED) ||
	 (my_disable_locking && share->state.open_count)))
    {
      DBUG_PRINT("error",("Table is marked as crashed"));
      my_errno=((share->state.changed & STATE_CRASHED_ON_REPAIR) ?
		HA_ERR_CRASHED_ON_REPAIR : HA_ERR_CRASHED);
      goto err;
    }
    /* Correct max_file_length based on length of sizeof_t */
    max_data_file_length=
      (share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
      (((ulonglong) 1 << (share->base.rec_reflength*8))-1) :
      (mi_safe_mul(share->base.reclength,
		   (ulonglong) 1 << (share->base.rec_reflength*8))-1);
    max_key_file_length=
      mi_safe_mul(MI_KEY_BLOCK_LENGTH,
		  ((ulonglong) 1 << (share->base.key_reflength*8))-1);
#if SIZEOF_OFF_T == 4
    set_if_smaller(max_data_file_length, INT_MAX32);
    set_if_smaller(max_key_file_length, INT_MAX32);
#endif
#if USE_RAID && SYSTEM_SIZEOF_OFF_T == 4
    set_if_smaller(max_key_file_length, INT_MAX32);
    if (!share->base.raid_type)
    {
      set_if_smaller(max_data_file_length, INT_MAX32);
    }
    else
    {
      set_if_smaller(max_data_file_length,
		     (ulonglong) share->base.raid_chunks << 31);
    }
#elif !defined(USE_RAID)
    if (share->base.raid_type)
    {
      DBUG_PRINT("error",("Table uses RAID but we don't have RAID support"));
      my_errno=HA_ERR_UNSUPPORTED;
      goto err;
    }
#endif
    share->base.max_data_file_length=(my_off_t) max_data_file_length;
    share->base.max_key_file_length=(my_off_t) max_key_file_length;

    if (share->base.max_key_length > MI_MAX_KEY_BUFF || keys > MI_MAX_KEY ||
	key_parts >= MI_MAX_KEY * MI_MAX_KEY_SEG)
    {
      DBUG_PRINT("error",("Wrong key info:  Max_key_length: %d  keys: %d  key_parts: %d", share->base.max_key_length, keys, key_parts));
      my_errno=HA_ERR_UNSUPPORTED;
      goto err;
    }
    if (share->options & HA_OPTION_COMPRESS_RECORD)
      share->base.max_key_length+=2;	/* For safety */

    if (!my_multi_malloc(MY_WME,
			 &share,sizeof(*share),
			 &share->state.rec_per_key_part,sizeof(long)*key_parts,
			 &share->keyinfo,keys*sizeof(MI_KEYDEF),
			 &share->uniqueinfo,uniques*sizeof(MI_UNIQUEDEF),
			 &share->keyparts,
			 (key_parts+unique_key_parts+keys+uniques) *
			 sizeof(MI_KEYSEG),
			 &share->rec,
			 (share->base.fields+1)*sizeof(MI_COLUMNDEF),
			 &share->blobs,sizeof(MI_BLOB)*share->base.blobs,
			 &share->filename,strlen(name_buff)+1,
			 &share->state.key_root,keys*sizeof(my_off_t),
			 &share->state.key_del,
			 (share->state.header.max_block_size*sizeof(my_off_t)),
#ifdef THREAD
			 &share->key_root_lock,sizeof(rw_lock_t)*keys,
#endif
			 NullS))
      goto err;
    errpos=4;
    *share=share_buff;
    memcpy((char*) share->state.rec_per_key_part,
	   (char*) rec_per_key_part, sizeof(long)*key_parts);
    memcpy((char*) share->state.key_root,
	   (char*) key_root, sizeof(my_off_t)*keys);
    memcpy((char*) share->state.key_del,
	   (char*) key_del, (sizeof(my_off_t) *
			     share->state.header.max_block_size));
    strmov(share->filename,name_buff);

    share->blocksize=min(IO_SIZE,myisam_block_size);
    {
      MI_KEYSEG *pos=share->keyparts;
      for (i=0 ; i < keys ; i++)
      {
	disk_pos=mi_keydef_read(disk_pos, &share->keyinfo[i]);
	set_if_smaller(share->blocksize,share->keyinfo[i].block_length);
	share->keyinfo[i].seg=pos;
	for (j=0 ; j < share->keyinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
	  if (pos->type == HA_KEYTYPE_TEXT || pos->type == HA_KEYTYPE_VARTEXT)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language, MYF(MY_WME))))
	    {
	      my_errno=HA_ERR_UNKNOWN_CHARSET;
	      goto err;
	    }
	  }
	}
	if (share->keyinfo[i].flag & HA_FULLTEXT)		    /* SerG */
	{
	  share->keyinfo[i].seg=pos-FT_SEGS;			    /* SerG */
	  share->fulltext_index=1;
	}
	share->keyinfo[i].end=pos;
	pos->type=HA_KEYTYPE_END;			/* End */
	pos->length=share->base.rec_reflength;
	pos->null_bit=0;
	pos++;
      }
      for (i=0 ; i < uniques ; i++)
      {
	disk_pos=mi_uniquedef_read(disk_pos, &share->uniqueinfo[i]);
	share->uniqueinfo[i].seg=pos;
	for (j=0 ; j < share->uniqueinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
	  if (pos->type == HA_KEYTYPE_TEXT || pos->type == HA_KEYTYPE_VARTEXT)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language, MYF(MY_WME))))
	    {
	      my_errno=HA_ERR_UNKNOWN_CHARSET;
	      goto err;
	    }
	  }
	}
	share->uniqueinfo[i].end=pos;
	pos->type=HA_KEYTYPE_END;			/* End */
	pos->null_bit=0;
	pos++;
      }
    }
    for (i=0 ; i < keys ; i++)
      setup_key_functions(share->keyinfo+i);

    for (i=j=offset=0 ; i < share->base.fields ; i++)
    {
      disk_pos=mi_recinfo_read(disk_pos,&share->rec[i]);
      share->rec[i].pack_type=0;
      share->rec[i].huff_tree=0;
      share->rec[i].offset=offset;
      if (share->rec[i].type == (int) FIELD_BLOB)
      {
	share->blobs[j].pack_length=
	  share->rec[i].length-mi_portable_sizeof_char_ptr;;
	share->blobs[j].offset=offset;
	j++;
      }
      offset+=share->rec[i].length;
    }
    share->rec[i].type=(int) FIELD_LAST;	/* End marker */

    if (! lock_error)
    {
      VOID(my_lock(kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE)));
      lock_error=1;			/* Database unlocked */
    }

#ifdef USE_RAID
    if (share->base.raid_type)
    {
      if ((info.dfile=my_raid_open(fn_format(name_buff,name,"",MI_NAME_DEXT,
					     2+4),
				   mode | O_SHARE,
				   share->base.raid_type,
				   share->base.raid_chunks,
				   share->base.raid_chunksize,
				   MYF(MY_WME | MY_RAID))) < 0)
      goto err;
    }
    else
#endif
      if ((info.dfile=my_open(fn_format(name_buff,name,"",MI_NAME_DEXT,2+4),
			      mode | O_SHARE,
			      MYF(MY_WME))) < 0)
	goto err;
    errpos=5;

    share->kfile=kfile;
    share->mode=open_mode;
    share->this_process=(ulong) getpid();
    share->rnd= (int)	 share->this_process;	/* rnd-counter for splits */
#ifndef DBUG_OFF
    share->rnd=0;				/* To make things repeatable */
#endif
    share->last_process= share->state.process;
    share->base.key_parts=key_parts;
    share->base.all_key_parts=key_parts+unique_key_parts;
    if (!(share->last_version=share->state.version))
      share->last_version=1;			/* Safety */
    share->rec_reflength=share->base.rec_reflength; /* May be changed */
    share->base.margin_key_file_length=(share->base.max_key_file_length -
					(keys ? MI_INDEX_BLOCK_MARGIN *
					 share->blocksize * keys : 0));
    share->blocksize=min(IO_SIZE,myisam_block_size);

    share->data_file_type=STATIC_RECORD;
    if (share->options & HA_OPTION_COMPRESS_RECORD)
    {
      share->data_file_type = COMPRESSED_RECORD;
      share->options|= HA_OPTION_READ_ONLY_DATA;
      info.s=share;
      if (_mi_read_pack_info(&info,
			     (pbool)
			     test(!(share->options &
				    (HA_OPTION_PACK_RECORD |
				     HA_OPTION_TEMP_COMPRESS_RECORD)))))
	goto err;
    }
    else if (share->options & HA_OPTION_PACK_RECORD)
      share->data_file_type = DYNAMIC_RECORD;
    my_afree((gptr) disk_cache);
    setup_functions(share);
#ifdef THREAD
    thr_lock_init(&share->lock);
    VOID(pthread_mutex_init(&share->intern_lock,NULL));
    for (i=0; i<keys; i++)
      VOID(my_rwlock_init(&share->key_root_lock[i], NULL));
    if (!thr_lock_inited)
    {
      /* Probably a single threaded program; Don't use concurrent inserts */
      myisam_concurrent_insert=0;
    }
    else if (myisam_concurrent_insert)
    {
      share->concurrent_insert=
	((share->options & (HA_OPTION_READ_ONLY_DATA | HA_OPTION_TMP_TABLE |
			   HA_OPTION_COMPRESS_RECORD |
			   HA_OPTION_TEMP_COMPRESS_RECORD)) ||
	 (open_flags & HA_OPEN_TMP_TABLE)) ? 0 : 1;
      if (share->concurrent_insert)
      {
	share->lock.get_status=mi_get_status;
	share->lock.copy_status=mi_copy_status;
	share->lock.update_status=mi_update_status;
	share->lock.check_status=mi_check_status;
      }
    }
#endif
  }
  else
  {
    share= old_info->s;
    if (mode == O_RDWR && share->mode == O_RDONLY)
    {
      my_errno=EACCES;				/* Can't open in write mode */
      goto err;
    }
#ifdef USE_RAID
    if (share->base.raid_type)
    {
      if ((info.dfile=my_raid_open(fn_format(name_buff,old_info->filename,"",
					     MI_NAME_DEXT, 2+4),
				   mode | O_SHARE,
				   share->base.raid_type,
				   share->base.raid_chunks,
				   share->base.raid_chunksize,
				   MYF(MY_WME | MY_RAID))) < 0)
      goto err;
    }
    else
#endif
      if ((info.dfile=my_open(fn_format(name_buff,old_info->filename,"",
					MI_NAME_DEXT,2+4),
			      mode | O_SHARE,MYF(MY_WME))) < 0)
	{
	  my_errno=errno;
	  goto err;
	}
    errpos=5;
  }

  /* alloc and set up private structure parts */
  if (!my_multi_malloc(MY_WME,
		       &m_info,sizeof(MI_INFO),
		       &info.blobs,sizeof(MI_BLOB)*share->base.blobs,
		       &info.buff,(share->base.max_key_block_length*2+
				   share->base.max_key_length),
		       &info.lastkey,share->base.max_key_length*3+1,
		       &info.filename,strlen(name)+1,
		       NullS))
    goto err;
  errpos=6;

  strmov(info.filename,name);
  memcpy(info.blobs,share->blobs,sizeof(MI_BLOB)*share->base.blobs);
  info.lastkey2=info.lastkey+share->base.max_key_length;

  info.s=share;
  info.lastpos= HA_OFFSET_ERROR;
  info.update= (short) (HA_STATE_NEXT_FOUND+HA_STATE_PREV_FOUND);
  info.opt_flag=READ_CHECK_USED;
  info.this_unique= (ulong) info.dfile; /* Uniq number in process */
  if (share->data_file_type == COMPRESSED_RECORD)
    info.this_unique= share->state.unique;
  info.this_loop=0;			/* Update counter */
  info.last_unique= share->state.unique;
  if (mode == O_RDONLY)
    share->options|=HA_OPTION_READ_ONLY_DATA;
  info.lock_type=F_UNLCK;
  info.quick_mode=0;
  info.errkey= -1;
  info.page_changed=1;
  pthread_mutex_lock(&share->intern_lock);
  info.read_record=share->read_record;
  share->reopen++;
  share->write_flag=MYF(MY_NABP | MY_WAIT_IF_FULL);
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    info.lock_type=F_RDLCK;
    share->r_locks++;
  }
  if ((open_flags & HA_OPEN_TMP_TABLE) ||
      (share->options & HA_OPTION_TMP_TABLE))
  {
    share->temporary=share->delay_key_write=1;
    share->write_flag=MYF(MY_NABP);
    share->w_locks++;			/* We don't have to update status */
    info.lock_type=F_WRLCK;
  }
  if (((open_flags & HA_OPEN_DELAY_KEY_WRITE) ||
      (share->options & HA_OPTION_DELAY_KEY_WRITE)) &&
      myisam_delay_key_write)
    share->delay_key_write=1;
  info.state= &share->state.state;	/* Change global values by default */
  pthread_mutex_unlock(&share->intern_lock);

  /* Allocate buffer for one record */

  extra=0;
  if (share->options & HA_OPTION_PACK_RECORD)
    extra=ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
      MI_DYN_DELETE_BLOCK_HEADER;

  tmp_length=max(share->base.pack_reclength,share->base.max_key_length);
  info.alloced_rec_buff_length=tmp_length;
  if (!(info.rec_alloc=(byte*) my_malloc(tmp_length+extra+8,
					 MYF(MY_WME | MY_ZEROFILL))))
    goto err;
  if (extra)
    info.rec_buff=info.rec_alloc+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER);
  else
    info.rec_buff=info.rec_alloc;

  *m_info=info;
#ifdef THREAD
  thr_lock_data_init(&share->lock,&m_info->lock,(void*) m_info);
#endif
  m_info->open_list.data=(void*) m_info;
  myisam_open_list=list_add(myisam_open_list,&m_info->open_list);

  pthread_mutex_unlock(&THR_LOCK_myisam);
  if (myisam_log_file >= 0)
  {
    intern_filename(name_buff,share->filename);
    _myisam_log(MI_LOG_OPEN,m_info,name_buff,(uint) strlen(name_buff));
  }
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  switch (errpos) {
  case 6:
    my_free((gptr) m_info,MYF(0));
    /* fall through */
  case 5:
    VOID(my_close(info.dfile,MYF(0)));
    if (old_info)
      break;					/* Don't remove open table */
    /* fall through */
  case 4:
    my_free((gptr) share,MYF(0));
    /* fall through */
  case 3:
    if (! lock_error)
      VOID(my_lock(kfile, F_UNLCK, 0L, F_TO_EOF, MYF(MY_SEEK_NOT_DONE)));
    /* fall through */
  case 2:
    my_afree((gptr) disk_cache);
    /* fall through */
  case 1:
    VOID(my_close(kfile,MYF(0)));
    /* fall through */
  case 0:
  default:
    break;
  }
  pthread_mutex_unlock(&THR_LOCK_myisam);
  my_errno=save_errno;
  DBUG_RETURN (NULL);
} /* mi_open */


ulonglong mi_safe_mul(ulonglong a, ulonglong b)
{
  ulonglong max_val= ~ (ulonglong) 0;		/* my_off_t is unsigned */

  if (!a || max_val / a < b)
    return max_val;
  return a*b;
}

	/* Set up functions in structs */

static void setup_functions(register MYISAM_SHARE *share)
{
  if (share->options & HA_OPTION_COMPRESS_RECORD)
  {
    share->read_record=_mi_read_pack_record;
    share->read_rnd=_mi_read_rnd_pack_record;
    if (!(share->options & HA_OPTION_TEMP_COMPRESS_RECORD))
      share->calc_checksum=0;				/* No checksum */
    else if (share->options & HA_OPTION_PACK_RECORD)
      share->calc_checksum= mi_checksum;
    else
      share->calc_checksum= mi_static_checksum;
  }
  else if (share->options & HA_OPTION_PACK_RECORD)
  {
    share->read_record=_mi_read_dynamic_record;
    share->read_rnd=_mi_read_rnd_dynamic_record;
    share->delete_record=_mi_delete_dynamic_record;
    share->compare_record=_mi_cmp_dynamic_record;
    share->compare_unique=_mi_cmp_dynamic_unique;
    share->calc_checksum= mi_checksum;

    if (share->base.blobs)
    {
      share->update_record=_mi_update_blob_record;
      share->write_record=_mi_write_blob_record;
    }
    else
    {
      share->write_record=_mi_write_dynamic_record;
      share->update_record=_mi_update_dynamic_record;
    }
  }
  else
  {
    share->read_record=_mi_read_static_record;
    share->read_rnd=_mi_read_rnd_static_record;
    share->delete_record=_mi_delete_static_record;
    share->compare_record=_mi_cmp_static_record;
    share->update_record=_mi_update_static_record;
    share->write_record=_mi_write_static_record;
    share->compare_unique=_mi_cmp_static_unique;
    share->calc_checksum= mi_static_checksum;
  }
  if (!(share->options & HA_OPTION_CHECKSUM))
    share->calc_checksum=0;
  return;
}


static void setup_key_functions(register MI_KEYDEF *keyinfo)
{
  if (keyinfo->flag & HA_BINARY_PACK_KEY)
  {						/* Simple prefix compression */
    keyinfo->bin_search=_mi_seq_search;
    keyinfo->get_key=_mi_get_binary_pack_key;
    keyinfo->pack_key=_mi_calc_bin_pack_key_length;
    keyinfo->store_key=_mi_store_bin_pack_key;
  }
  else if (keyinfo->flag & HA_VAR_LENGTH_KEY)
  {
    keyinfo->bin_search=_mi_seq_search;
    keyinfo->get_key= _mi_get_pack_key;
    if (keyinfo->seg[0].flag & HA_PACK_KEY)
    {						/* Prefix compression */
      keyinfo->pack_key=_mi_calc_var_pack_key_length;
      keyinfo->store_key=_mi_store_var_pack_key;
    }
    else
    {
      keyinfo->pack_key=_mi_calc_var_key_length; /* Variable length key */
      keyinfo->store_key=_mi_store_static_key;
    }
  }
  else
  {
    keyinfo->bin_search=_mi_bin_search;
    keyinfo->get_key=_mi_get_static_key;
    keyinfo->pack_key=_mi_calc_static_key_length;
    keyinfo->store_key=_mi_store_static_key;
  }
  return;
}


/***************************************************************************
** Function to save and store the header in the index file (.MSI)
***************************************************************************/

uint mi_state_info_write(File file, MI_STATE_INFO *state, uint pWrite)
{
  uchar  buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];
  uchar *ptr=buff;
  uint	i, keys= (uint) state->header.keys,
	key_blocks=state->header.max_block_size;

  memcpy_fixed(ptr,&state->header,sizeof(state->header));
  ptr+=sizeof(state->header);

  /* open_count must be first because of _mi_mark_file_changed ! */
  mi_int2store(ptr,state->open_count);		ptr +=2;
  *ptr++= (uchar)state->changed; *ptr++= state->sortkey;
  mi_rowstore(ptr,state->state.records);	ptr +=8;
  mi_rowstore(ptr,state->state.del);		ptr +=8;
  mi_rowstore(ptr,state->split);		ptr +=8;
  mi_sizestore(ptr,state->dellink);		ptr +=8;
  mi_sizestore(ptr,state->state.key_file_length);	ptr +=8;
  mi_sizestore(ptr,state->state.data_file_length);	ptr +=8;
  mi_sizestore(ptr,state->state.empty);		ptr +=8;
  mi_sizestore(ptr,state->state.key_empty);	ptr +=8;
  mi_int8store(ptr,state->auto_increment);	ptr +=8;
  mi_int8store(ptr,(ulonglong) state->checksum);ptr +=8;
  mi_int4store(ptr,state->process);		ptr +=4;
  mi_int4store(ptr,state->unique);		ptr +=4;
  mi_int4store(ptr,state->status);		ptr +=4;
  *ptr++=0; *ptr++=0;  *ptr++=0; *ptr++=0;	/* extra */

  ptr+=state->state_diff_length;

  for (i=0; i < keys; i++)
  {
    mi_sizestore(ptr,state->key_root[i]);	ptr +=8;
  }
  for (i=0; i < key_blocks; i++)
  {
    mi_sizestore(ptr,state->key_del[i]);	ptr +=8;
  }
  if (pWrite & 2)				/* From isamchk */
  {
    uint key_parts= mi_uint2korr(state->header.key_parts);
    mi_int4store(ptr,state->sec_index_changed); ptr +=4;
    mi_int4store(ptr,state->sec_index_used);	ptr +=4;
    mi_int4store(ptr,state->version);		ptr +=4;
    mi_int8store(ptr,state->key_map);		ptr +=8;
    mi_int8store(ptr,(ulonglong) state->create_time);	ptr +=8;
    mi_int8store(ptr,(ulonglong) state->recover_time);	ptr +=8;
    mi_int8store(ptr,(ulonglong) state->check_time);	ptr +=8;
    mi_sizestore(ptr,state->rec_per_key_rows);	ptr+=8;
    for (i=0 ; i < key_parts ; i++)
    {
      mi_int4store(ptr,state->rec_per_key_part[i]);  ptr+=4;
    }
  }

  if (pWrite & 1)
     return my_pwrite(file,(char*) buff, (uint) (ptr-buff), 0L,
		      MYF(MY_NABP | MY_THREADSAFE));
  else
    return my_write(file,  (char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}


char *mi_state_info_read(char *ptr, MI_STATE_INFO *state)
{
  uint i,keys,key_parts,key_blocks;
  memcpy_fixed(&state->header,ptr, sizeof(state->header));
  ptr +=sizeof(state->header);
  keys=(uint) state->header.keys;
  key_parts=mi_uint2korr(state->header.key_parts);
  key_blocks=state->header.max_block_size;

  state->open_count = mi_uint2korr(ptr);	ptr +=2;
  state->changed= (bool) *ptr++;
  state->sortkey = (uint) *ptr++;
  state->state.records= mi_rowkorr(ptr);	ptr +=8;
  state->state.del = mi_rowkorr(ptr);		ptr +=8;
  state->split	= mi_rowkorr(ptr);		ptr +=8;
  state->dellink= mi_sizekorr(ptr);		ptr +=8;
  state->state.key_file_length = mi_sizekorr(ptr);	ptr +=8;
  state->state.data_file_length= mi_sizekorr(ptr);	ptr +=8;
  state->state.empty	= mi_sizekorr(ptr);	ptr +=8;
  state->state.key_empty= mi_sizekorr(ptr);	ptr +=8;
  state->auto_increment=mi_uint8korr(ptr);	ptr +=8;
  state->checksum=(ha_checksum) mi_uint8korr(ptr);	ptr +=8;
  state->process= mi_uint4korr(ptr);		ptr +=4;
  state->unique = mi_uint4korr(ptr);		ptr +=4;
  state->status = mi_uint4korr(ptr);		ptr +=4;
						ptr +=4; /* extra */
  for (i=0; i < keys; i++)
  {
    state->key_root[i]= mi_sizekorr(ptr);	ptr +=8;
  }
  for (i=0; i < key_blocks; i++)
  {
    state->key_del[i] = mi_sizekorr(ptr);	ptr +=8;
  }
  ptr+= state->state_diff_length;
  state->sec_index_changed = mi_uint4korr(ptr); ptr +=4;
  state->sec_index_used =    mi_uint4korr(ptr); ptr +=4;
  state->version     = mi_uint4korr(ptr);	ptr +=4;
  state->key_map     = mi_uint8korr(ptr);	ptr +=8;
  state->create_time = (time_t) mi_sizekorr(ptr);	ptr +=8;
  state->recover_time =(time_t) mi_sizekorr(ptr);	ptr +=8;
  state->check_time =  (time_t) mi_sizekorr(ptr);	ptr +=8;
  state->rec_per_key_rows=mi_sizekorr(ptr);	ptr +=8;
  for (i=0 ; i < key_parts ; i++)
  {
    state->rec_per_key_part[i]= mi_uint4korr(ptr); ptr+=4;
  }
  return ptr;
}


uint mi_state_info_read_dsk(File file, MI_STATE_INFO *state, my_bool pRead)
{
  char	buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];

  if (pRead)
  {
    if (my_pread(file, buff, state->state_length,0L, MYF(MY_NABP)))
      return (MY_FILE_ERROR);
  }
  else if (my_read(file, buff, state->state_length,MYF(MY_NABP)))
    return (MY_FILE_ERROR);
  mi_state_info_read(buff, state);
  return 0;
}


/****************************************************************************
**  store and read of MI_BASE_INFO
****************************************************************************/

uint mi_base_info_write(File file, MI_BASE_INFO *base)
{
  uchar buff[MI_BASE_INFO_SIZE], *ptr=buff;

  mi_sizestore(ptr,base->keystart);			ptr +=8;
  mi_sizestore(ptr,base->max_data_file_length);		ptr +=8;
  mi_sizestore(ptr,base->max_key_file_length);		ptr +=8;
  mi_rowstore(ptr,base->records);			ptr +=8;
  mi_rowstore(ptr,base->reloc);				ptr +=8;
  mi_int4store(ptr,base->mean_row_length);		ptr +=4;
  mi_int4store(ptr,base->reclength);			ptr +=4;
  mi_int4store(ptr,base->pack_reclength);		ptr +=4;
  mi_int4store(ptr,base->min_pack_length);		ptr +=4;
  mi_int4store(ptr,base->max_pack_length);		ptr +=4;
  mi_int4store(ptr,base->min_block_length);		ptr +=4;
  mi_int4store(ptr,base->fields);			ptr +=4;
  mi_int4store(ptr,base->pack_fields);			ptr +=4;
  *ptr++=base->rec_reflength;
  *ptr++=base->key_reflength;
  *ptr++=base->keys;
  *ptr++=base->auto_key;
  mi_int2store(ptr,base->pack_bits);			ptr +=2;
  mi_int2store(ptr,base->blobs);			ptr +=2;
  mi_int2store(ptr,base->max_key_block_length);		ptr +=2;
  mi_int2store(ptr,base->max_key_length);		ptr +=2;
  mi_int2store(ptr,base->extra_alloc_bytes);		ptr +=2;
  *ptr++= base->extra_alloc_procent;
  *ptr++= base->raid_type;
  mi_int2store(ptr,base->raid_chunks);			ptr +=2;
  mi_int4store(ptr,base->raid_chunksize);		ptr +=4;
  bzero(ptr,6);						ptr +=6; /* extra */
  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}


char *my_n_base_info_read(char *ptr, MI_BASE_INFO *base)
{
  base->keystart = mi_sizekorr(ptr);			ptr +=8;
  base->max_data_file_length = mi_sizekorr(ptr);	ptr +=8;
  base->max_key_file_length = mi_sizekorr(ptr);		ptr +=8;
  base->records =  (ha_rows) mi_sizekorr(ptr);		ptr +=8;
  base->reloc = (ha_rows) mi_sizekorr(ptr);		ptr +=8;
  base->mean_row_length = mi_uint4korr(ptr);		ptr +=4;
  base->reclength = mi_uint4korr(ptr);			ptr +=4;
  base->pack_reclength = mi_uint4korr(ptr);		ptr +=4;
  base->min_pack_length = mi_uint4korr(ptr);		ptr +=4;
  base->max_pack_length = mi_uint4korr(ptr);		ptr +=4;
  base->min_block_length = mi_uint4korr(ptr);		ptr +=4;
  base->fields = mi_uint4korr(ptr);			ptr +=4;
  base->pack_fields = mi_uint4korr(ptr);		ptr +=4;

  base->rec_reflength = *ptr++;
  base->key_reflength = *ptr++;
  base->keys=		*ptr++;
  base->auto_key=	*ptr++;
  base->pack_bits = mi_uint2korr(ptr);			ptr +=2;
  base->blobs = mi_uint2korr(ptr);			ptr +=2;
  base->max_key_block_length= mi_uint2korr(ptr);	ptr +=2;
  base->max_key_length = mi_uint2korr(ptr);		ptr +=2;
  base->extra_alloc_bytes = mi_uint2korr(ptr);		ptr +=2;
  base->extra_alloc_procent = *ptr++;
  base->raid_type= *ptr++;
  base->raid_chunks= mi_uint2korr(ptr);			ptr +=2;
  base->raid_chunksize= mi_uint4korr(ptr);		ptr +=4;
  /* TO BE REMOVED: Fix for old RAID files */
  if (base->raid_type == 0)
  {
    base->raid_chunks=0;
    base->raid_chunksize=0;
  }

  ptr+=6;
  return ptr;
}

/*--------------------------------------------------------------------------
  mi_keydef
---------------------------------------------------------------------------*/

uint mi_keydef_write(File file, MI_KEYDEF *keydef)
{
  uchar buff[MI_KEYDEF_SIZE];
  uchar *ptr=buff;

  *ptr++ = (uchar) keydef->keysegs;
  *ptr++ = 0;					/* not used */
  mi_int2store(ptr,keydef->flag);		ptr +=2;
  mi_int2store(ptr,keydef->block_length);	ptr +=2;
  mi_int2store(ptr,keydef->keylength);		ptr +=2;
  mi_int2store(ptr,keydef->minlength);		ptr +=2;
  mi_int2store(ptr,keydef->maxlength);		ptr +=2;
  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *mi_keydef_read(char *ptr, MI_KEYDEF *keydef)
{
   keydef->keysegs	= (uint) *ptr++;
   ptr++;
   keydef->flag		= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_length = mi_uint2korr(ptr);	ptr +=2;
   keydef->keylength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->minlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->maxlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_size	= keydef->block_length/MI_KEY_BLOCK_LENGTH-1;
   keydef->underflow_block_length=keydef->block_length/3;
   keydef->version	= 0;			/* Not saved */
   return ptr;
}

/***************************************************************************
**  mi_keyseg
***************************************************************************/

int mi_keyseg_write(File file, const MI_KEYSEG *keyseg)
{
  uchar buff[MI_KEYSEG_SIZE];
  uchar *ptr=buff;

  *ptr++ =keyseg->type;
  *ptr++ =keyseg->language;
  *ptr++ =keyseg->null_bit;
  *ptr++ =keyseg->bit_start;
  *ptr++ =keyseg->bit_end;
  *ptr++ =0;					/* Not used */
  mi_int2store(ptr,keyseg->flag);	ptr+=2;
  mi_int2store(ptr,keyseg->length);	ptr+=2;
  mi_int4store(ptr,keyseg->start);	ptr+=4;
  mi_int4store(ptr,keyseg->null_pos);	ptr+=4;

  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}


char *mi_keyseg_read(char *ptr, MI_KEYSEG *keyseg)
{
   keyseg->type		= *ptr++;
   keyseg->language	= *ptr++;
   keyseg->null_bit	= *ptr++;
   keyseg->bit_start	= *ptr++;
   keyseg->bit_end	= *ptr++;
   ptr++;
   keyseg->flag		= mi_uint2korr(ptr);  ptr +=2;
   keyseg->length	= mi_uint2korr(ptr);  ptr +=2;
   keyseg->start	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->null_pos	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->charset=0;				/* Will be filled in later */
   return ptr;
}

/*--------------------------------------------------------------------------
  mi_uniquedef
---------------------------------------------------------------------------*/

uint mi_uniquedef_write(File file, MI_UNIQUEDEF *def)
{
  uchar buff[MI_UNIQUEDEF_SIZE];
  uchar *ptr=buff;

  mi_int2store(ptr,def->keysegs);		ptr+=2;
  *ptr++=  (uchar) def->key;
  *ptr++ = (uchar) def->null_are_equal;

  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *mi_uniquedef_read(char *ptr, MI_UNIQUEDEF *def)
{
   def->keysegs = mi_uint2korr(ptr);
   def->key	= ptr[2];
   def->null_are_equal=ptr[3];
   return ptr+4;				/* 1 extra byte */
}

/***************************************************************************
**  MI_COLUMNDEF
***************************************************************************/

uint mi_recinfo_write(File file, MI_COLUMNDEF *recinfo)
{
  uchar buff[MI_COLUMNDEF_SIZE];
  uchar *ptr=buff;

  mi_int2store(ptr,recinfo->type);	ptr +=2;
  mi_int2store(ptr,recinfo->length);	ptr +=2;
  *ptr++ = recinfo->null_bit;
  mi_int2store(ptr,recinfo->null_pos);	ptr+= 2;
  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *mi_recinfo_read(char *ptr, MI_COLUMNDEF *recinfo)
{
   recinfo->type=  mi_sint2korr(ptr);	ptr +=2;
   recinfo->length=mi_uint2korr(ptr);	ptr +=2;
   recinfo->null_bit= (uint8) *ptr++;
   recinfo->null_pos=mi_uint2korr(ptr); ptr +=2;
   return ptr;
}
