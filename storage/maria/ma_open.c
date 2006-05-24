/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "ma_fulltext.h"
#include "ma_sp_defs.h"
#include "ma_rt_index.h"
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

static void setup_key_functions(MARIA_KEYDEF *keyinfo);
#define get_next_element(to,pos,size) { memcpy((char*) to,pos,(size_t) size); \
					pos+=size;}


#define disk_pos_assert(pos, end_pos) \
if (pos > end_pos)             \
{                              \
  my_errno=HA_ERR_CRASHED;     \
  goto err;                    \
}


/******************************************************************************
** Return the shared struct if the table is already open.
** In MySQL the server will handle version issues.
******************************************************************************/

MARIA_HA *_ma_test_if_reopen(char *filename)
{
  LIST *pos;

  for (pos=maria_open_list ; pos ; pos=pos->next)
  {
    MARIA_HA *info=(MARIA_HA*) pos->data;
    MARIA_SHARE *share=info->s;
    if (!strcmp(share->unique_file_name,filename) && share->last_version)
      return info;
  }
  return 0;
}


/******************************************************************************
  open a MARIA database.
  See my_base.h for the handle_locking argument
  if handle_locking and HA_OPEN_ABORT_IF_CRASHED then abort if the table
  is marked crashed or if we are not using locking and the table doesn't
  have an open count of 0.
******************************************************************************/

MARIA_HA *maria_open(const char *name, int mode, uint open_flags)
{
  int kfile,open_mode,save_errno,have_rtree=0;
  uint i,j,len,errpos,head_length,base_pos,offset,info_length,keys,
    key_parts,unique_key_parts,fulltext_keys,uniques;
  char name_buff[FN_REFLEN], org_name[FN_REFLEN], index_name[FN_REFLEN],
       data_name[FN_REFLEN];
  char *disk_cache, *disk_pos, *end_pos;
  MARIA_HA info,*m_info,*old_info;
  MARIA_SHARE share_buff,*share;
  ulong rec_per_key_part[HA_MAX_POSSIBLE_KEY*HA_MAX_KEY_SEG];
  my_off_t key_root[HA_MAX_POSSIBLE_KEY],key_del[MARIA_MAX_KEY_BLOCK_SIZE];
  ulonglong max_key_file_length, max_data_file_length;
  DBUG_ENTER("maria_open");

  LINT_INIT(m_info);
  kfile= -1;
  errpos=0;
  head_length=sizeof(share_buff.state.header);
  bzero((byte*) &info,sizeof(info));

  my_realpath(name_buff, fn_format(org_name,name,"",MARIA_NAME_IEXT,
                                   MY_UNPACK_FILENAME|MY_APPEND_EXT),MYF(0));
  pthread_mutex_lock(&THR_LOCK_maria);
  if (!(old_info=_ma_test_if_reopen(name_buff)))
  {
    share= &share_buff;
    bzero((gptr) &share_buff,sizeof(share_buff));
    share_buff.state.rec_per_key_part=rec_per_key_part;
    share_buff.state.key_root=key_root;
    share_buff.state.key_del=key_del;
    share_buff.key_cache= multi_key_cache_search(name_buff, strlen(name_buff),
                                                 maria_key_cache);

    DBUG_EXECUTE_IF("maria_pretend_crashed_table_on_open",
                    if (strstr(name, "/t1"))
                    {
                      my_errno= HA_ERR_CRASHED;
                      goto err;
                    });
    if ((kfile=my_open(name_buff,(open_mode=O_RDWR) | O_SHARE,MYF(0))) < 0)
    {
      if ((errno != EROFS && errno != EACCES) ||
	  mode != O_RDONLY ||
	  (kfile=my_open(name_buff,(open_mode=O_RDONLY) | O_SHARE,MYF(0))) < 0)
	goto err;
    }
    share->mode=open_mode;
    errpos=1;
    if (my_read(kfile,(char*) share->state.header.file_version,head_length,
		MYF(MY_NABP)))
    {
      my_errno= HA_ERR_NOT_A_TABLE;
      goto err;
    }
    if (memcmp((byte*) share->state.header.file_version,
	       (byte*) maria_file_magic, 4))
    {
      DBUG_PRINT("error",("Wrong header in %s",name_buff));
      DBUG_DUMP("error_dump",(char*) share->state.header.file_version,
		head_length);
      my_errno=HA_ERR_NOT_A_TABLE;
      goto err;
    }
    share->options= mi_uint2korr(share->state.header.options);
    if (share->options &
	~(HA_OPTION_PACK_RECORD | HA_OPTION_PACK_KEYS |
	  HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA |
	  HA_OPTION_TEMP_COMPRESS_RECORD | HA_OPTION_CHECKSUM |
          HA_OPTION_TMP_TABLE | HA_OPTION_DELAY_KEY_WRITE |
          HA_OPTION_RELIES_ON_SQL_LAYER))
    {
      DBUG_PRINT("error",("wrong options: 0x%lx", share->options));
      my_errno=HA_ERR_OLD_FILE;
      goto err;
    }
    if ((share->options & HA_OPTION_RELIES_ON_SQL_LAYER) &&
        ! (open_flags & HA_OPEN_FROM_SQL_LAYER))
    {
      DBUG_PRINT("error", ("table cannot be openned from non-sql layer"));
      my_errno= HA_ERR_UNSUPPORTED;
      goto err;
    }
    /* Don't call realpath() if the name can't be a link */
    if (!strcmp(name_buff, org_name) ||
        my_readlink(index_name, org_name, MYF(0)) == -1)
      (void) strmov(index_name, org_name);
    *strrchr(org_name, '.')= '\0';
    (void) fn_format(data_name,org_name,"",MARIA_NAME_DEXT,
                     MY_APPEND_EXT|MY_UNPACK_FILENAME|MY_RESOLVE_SYMLINKS);

    info_length=mi_uint2korr(share->state.header.header_length);
    base_pos=mi_uint2korr(share->state.header.base_pos);
    if (!(disk_cache=(char*) my_alloca(info_length+128)))
    {
      my_errno=ENOMEM;
      goto err;
    }
    end_pos=disk_cache+info_length;
    errpos=2;

    VOID(my_seek(kfile,0L,MY_SEEK_SET,MYF(0)));
    errpos=3;
    if (my_read(kfile,disk_cache,info_length,MYF(MY_NABP)))
    {
      my_errno=HA_ERR_CRASHED;
      goto err;
    }
    len=mi_uint2korr(share->state.header.state_info_length);
    keys=    (uint) share->state.header.keys;
    uniques= (uint) share->state.header.uniques;
    fulltext_keys= (uint) share->state.header.fulltext_keys;
    key_parts= mi_uint2korr(share->state.header.key_parts);
    unique_key_parts= mi_uint2korr(share->state.header.unique_key_parts);
    if (len != MARIA_STATE_INFO_SIZE)
    {
      DBUG_PRINT("warning",
		 ("saved_state_info_length: %d  state_info_length: %d",
		  len,MARIA_STATE_INFO_SIZE));
    }
    share->state_diff_length=len-MARIA_STATE_INFO_SIZE;

    _ma_state_info_read((uchar*) disk_cache, &share->state);
    len= mi_uint2korr(share->state.header.base_info_length);
    if (len != MARIA_BASE_INFO_SIZE)
    {
      DBUG_PRINT("warning",("saved_base_info_length: %d  base_info_length: %d",
			    len,MARIA_BASE_INFO_SIZE));
    }
    disk_pos= (char*)
      _ma_n_base_info_read((uchar*) disk_cache + base_pos, &share->base);
    share->state.state_length=base_pos;

    if (!(open_flags & HA_OPEN_FOR_REPAIR) &&
	((share->state.changed & STATE_CRASHED) ||
	 ((open_flags & HA_OPEN_ABORT_IF_CRASHED) &&
	  (my_disable_locking && share->state.open_count))))
    {
      DBUG_PRINT("error",("Table is marked as crashed"));
      my_errno=((share->state.changed & STATE_CRASHED_ON_REPAIR) ?
		HA_ERR_CRASHED_ON_REPAIR : HA_ERR_CRASHED_ON_USAGE);
      goto err;
    }

    /* sanity check */
    if (share->base.keystart > 65535 || share->base.rec_reflength > 8)
    {
      my_errno=HA_ERR_CRASHED;
      goto err;
    }

    key_parts+=fulltext_keys*FT_SEGS;
    if (share->base.max_key_length > HA_MAX_KEY_BUFF || keys > MARIA_MAX_KEY ||
	key_parts >= MARIA_MAX_KEY * HA_MAX_KEY_SEG)
    {
      DBUG_PRINT("error",("Wrong key info:  Max_key_length: %d  keys: %d  key_parts: %d", share->base.max_key_length, keys, key_parts));
      my_errno=HA_ERR_UNSUPPORTED;
      goto err;
    }

    /* Correct max_file_length based on length of sizeof(off_t) */
    max_data_file_length=
      (share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
      (((ulonglong) 1 << (share->base.rec_reflength*8))-1) :
      (_ma_safe_mul(share->base.pack_reclength,
		   (ulonglong) 1 << (share->base.rec_reflength*8))-1);
    max_key_file_length=
      _ma_safe_mul(MARIA_MIN_KEY_BLOCK_LENGTH,
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

    if (share->options & HA_OPTION_COMPRESS_RECORD)
      share->base.max_key_length+=2;	/* For safety */

    if (!my_multi_malloc(MY_WME,
			 &share,sizeof(*share),
			 &share->state.rec_per_key_part,sizeof(long)*key_parts,
			 &share->keyinfo,keys*sizeof(MARIA_KEYDEF),
			 &share->uniqueinfo,uniques*sizeof(MARIA_UNIQUEDEF),
			 &share->keyparts,
			 (key_parts+unique_key_parts+keys+uniques) *
			 sizeof(HA_KEYSEG),
			 &share->rec,
			 (share->base.fields+1)*sizeof(MARIA_COLUMNDEF),
			 &share->blobs,sizeof(MARIA_BLOB)*share->base.blobs,
			 &share->unique_file_name,strlen(name_buff)+1,
			 &share->index_file_name,strlen(index_name)+1,
			 &share->data_file_name,strlen(data_name)+1,
			 &share->state.key_root,keys*sizeof(my_off_t),
			 &share->state.key_del,
			 (share->state.header.max_block_size*sizeof(my_off_t)),
#ifdef THREAD
			 &share->key_root_lock,sizeof(rw_lock_t)*keys,
#endif
			 &share->mmap_lock,sizeof(rw_lock_t),
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
    strmov(share->unique_file_name, name_buff);
    share->unique_name_length= strlen(name_buff);
    strmov(share->index_file_name,  index_name);
    strmov(share->data_file_name,   data_name);

    share->blocksize=min(IO_SIZE,maria_block_size);
    {
      HA_KEYSEG *pos=share->keyparts;
      for (i=0 ; i < keys ; i++)
      {
        share->keyinfo[i].share= share;
	disk_pos=_ma_keydef_read(disk_pos, &share->keyinfo[i]);
        disk_pos_assert(disk_pos + share->keyinfo[i].keysegs * HA_KEYSEG_SIZE,
 			end_pos);
        if (share->keyinfo[i].key_alg == HA_KEY_ALG_RTREE)
          have_rtree=1;
	set_if_smaller(share->blocksize,share->keyinfo[i].block_length);
	share->keyinfo[i].seg=pos;
	for (j=0 ; j < share->keyinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=_ma_keyseg_read(disk_pos, pos);

	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
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
	if (share->keyinfo[i].flag & HA_SPATIAL)
	{
#ifdef HAVE_SPATIAL
	  uint sp_segs=SPDIMS*2;
	  share->keyinfo[i].seg=pos-sp_segs;
	  share->keyinfo[i].keysegs--;
#else
	  my_errno=HA_ERR_UNSUPPORTED;
	  goto err;
#endif
	}
        else if (share->keyinfo[i].flag & HA_FULLTEXT)
	{
          if (!fulltext_keys)
          { /* 4.0 compatibility code, to be removed in 5.0 */
            share->keyinfo[i].seg=pos-FT_SEGS;
            share->keyinfo[i].keysegs-=FT_SEGS;
          }
          else
          {
            uint j;
            share->keyinfo[i].seg=pos;
            for (j=0; j < FT_SEGS; j++)
            {
              *pos= ft_keysegs[j];
              pos[0].language= pos[-1].language;
              if (!(pos[0].charset= pos[-1].charset))
              {
                my_errno=HA_ERR_CRASHED;
                goto err;
              }
              pos++;
            }
          }
          if (!share->ft2_keyinfo.seg)
          {
            memcpy(& share->ft2_keyinfo, & share->keyinfo[i], sizeof(MARIA_KEYDEF));
            share->ft2_keyinfo.keysegs=1;
            share->ft2_keyinfo.flag=0;
            share->ft2_keyinfo.keylength=
            share->ft2_keyinfo.minlength=
            share->ft2_keyinfo.maxlength=HA_FT_WLEN+share->base.rec_reflength;
            share->ft2_keyinfo.seg=pos-1;
            share->ft2_keyinfo.end=pos;
            setup_key_functions(& share->ft2_keyinfo);
          }
	}
        setup_key_functions(share->keyinfo+i);
	share->keyinfo[i].end=pos;
	pos->type=HA_KEYTYPE_END;			/* End */
	pos->length=share->base.rec_reflength;
	pos->null_bit=0;
	pos->flag=0;					/* For purify */
	pos++;
      }
      for (i=0 ; i < uniques ; i++)
      {
	disk_pos=_ma_uniquedef_read(disk_pos, &share->uniqueinfo[i]);
        disk_pos_assert(disk_pos + share->uniqueinfo[i].keysegs *
			HA_KEYSEG_SIZE, end_pos);
	share->uniqueinfo[i].seg=pos;
	for (j=0 ; j < share->uniqueinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=_ma_keyseg_read(disk_pos, pos);
	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
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
	pos->flag=0;
	pos++;
      }
      share->ftparsers= 0;
    }

    disk_pos_assert(disk_pos + share->base.fields *MARIA_COLUMNDEF_SIZE, end_pos);
    for (i=j=offset=0 ; i < share->base.fields ; i++)
    {
      disk_pos=_ma_recinfo_read(disk_pos,&share->rec[i]);
      share->rec[i].pack_type=0;
      share->rec[i].huff_tree=0;
      share->rec[i].offset=offset;
      if (share->rec[i].type == (int) FIELD_BLOB)
      {
	share->blobs[j].pack_length=
	  share->rec[i].length-maria_portable_sizeof_char_ptr;;
	share->blobs[j].offset=offset;
	j++;
      }
      offset+=share->rec[i].length;
    }
    share->rec[i].type=(int) FIELD_LAST;	/* End marker */

    if (_ma_open_datafile(&info, share, -1))
      goto err;
    errpos=5;

    share->kfile=kfile;
    share->this_process=(ulong) getpid();
    share->last_process= share->state.process;
    share->base.key_parts=key_parts;
    share->base.all_key_parts=key_parts+unique_key_parts;
    if (!(share->last_version=share->state.version))
      share->last_version=1;			/* Safety */
    share->rec_reflength=share->base.rec_reflength; /* May be changed */
    share->base.margin_key_file_length=(share->base.max_key_file_length -
					(keys ? MARIA_INDEX_BLOCK_MARGIN *
					 share->blocksize * keys : 0));
    share->blocksize=min(IO_SIZE,maria_block_size);
    share->data_file_type=STATIC_RECORD;
    if (share->options & HA_OPTION_COMPRESS_RECORD)
    {
      share->data_file_type = COMPRESSED_RECORD;
      share->options|= HA_OPTION_READ_ONLY_DATA;
      info.s=share;
      if (_ma_read_pack_info(&info,
			     (pbool)
			     test(!(share->options &
				    (HA_OPTION_PACK_RECORD |
				     HA_OPTION_TEMP_COMPRESS_RECORD)))))
	goto err;
    }
    else if (share->options & HA_OPTION_PACK_RECORD)
      share->data_file_type = DYNAMIC_RECORD;
    my_afree((gptr) disk_cache);
    _ma_setup_functions(share);
#ifdef THREAD
    thr_lock_init(&share->lock);
    VOID(pthread_mutex_init(&share->intern_lock,MY_MUTEX_INIT_FAST));
    for (i=0; i<keys; i++)
      VOID(my_rwlock_init(&share->key_root_lock[i], NULL));
    VOID(my_rwlock_init(&share->mmap_lock, NULL));
    if (!thr_lock_inited)
    {
      /* Probably a single threaded program; Don't use concurrent inserts */
      maria_concurrent_insert=0;
    }
    else if (maria_concurrent_insert)
    {
      share->concurrent_insert=
	((share->options & (HA_OPTION_READ_ONLY_DATA | HA_OPTION_TMP_TABLE |
			   HA_OPTION_COMPRESS_RECORD |
			   HA_OPTION_TEMP_COMPRESS_RECORD)) ||
	 (open_flags & HA_OPEN_TMP_TABLE) ||
	 have_rtree) ? 0 : 1;
      if (share->concurrent_insert)
      {
	share->lock.get_status=_ma_get_status;
	share->lock.copy_status=_ma_copy_status;
	share->lock.update_status=_ma_update_status;
	share->lock.check_status=_ma_check_status;
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
    if (_ma_open_datafile(&info, share, old_info->dfile))
      goto err;
    errpos=5;
    have_rtree= old_info->maria_rtree_recursion_state != NULL;
  }

  /* alloc and set up private structure parts */
  if (!my_multi_malloc(MY_WME,
		       &m_info,sizeof(MARIA_HA),
		       &info.blobs,sizeof(MARIA_BLOB)*share->base.blobs,
		       &info.buff,(share->base.max_key_block_length*2+
				   share->base.max_key_length),
		       &info.lastkey,share->base.max_key_length*3+1,
		       &info.first_mbr_key, share->base.max_key_length,
		       &info.filename,strlen(name)+1,
		       &info.maria_rtree_recursion_state,have_rtree ? 1024 : 0,
		       NullS))
    goto err;
  errpos=6;

  if (!have_rtree)
    info.maria_rtree_recursion_state= NULL;

  strmov(info.filename,name);
  memcpy(info.blobs,share->blobs,sizeof(MARIA_BLOB)*share->base.blobs);
  info.lastkey2=info.lastkey+share->base.max_key_length;

  info.s=share;
  info.lastpos= HA_OFFSET_ERROR;
  info.update= (short) (HA_STATE_NEXT_FOUND+HA_STATE_PREV_FOUND);
  info.opt_flag=READ_CHECK_USED;
  info.this_unique= (ulong) info.dfile; /* Uniq number in process */
  if (share->data_file_type == COMPRESSED_RECORD)
    info.this_unique= share->state.unique;
  info.this_loop=0;				/* Update counter */
  info.last_unique= share->state.unique;
  info.last_loop=   share->state.update_count;
  if (mode == O_RDONLY)
    share->options|=HA_OPTION_READ_ONLY_DATA;
  info.lock_type=F_UNLCK;
  info.quick_mode=0;
  info.bulk_insert=0;
  info.ft1_to_ft2=0;
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
    share->tot_locks++;
  }
  if ((open_flags & HA_OPEN_TMP_TABLE) ||
      (share->options & HA_OPTION_TMP_TABLE))
  {
    share->temporary=share->delay_key_write=1;
    share->write_flag=MYF(MY_NABP);
    share->w_locks++;			/* We don't have to update status */
    share->tot_locks++;
    info.lock_type=F_WRLCK;
  }
  if (((open_flags & HA_OPEN_DELAY_KEY_WRITE) ||
      (share->options & HA_OPTION_DELAY_KEY_WRITE)) &&
      maria_delay_key_write)
    share->delay_key_write=1;
  info.state= &share->state.state;	/* Change global values by default */
  pthread_mutex_unlock(&share->intern_lock);

  /* Allocate buffer for one record */

  /* prerequisites: bzero(info) && info->s=share; are met. */
  if (!_ma_alloc_rec_buff(&info, -1, &info.rec_buff))
    goto err;
  bzero(info.rec_buff, _ma_get_rec_buff_len(&info, info.rec_buff));

  *m_info=info;
#ifdef THREAD
  thr_lock_data_init(&share->lock,&m_info->lock,(void*) m_info);
#endif
  m_info->open_list.data=(void*) m_info;
  maria_open_list=list_add(maria_open_list,&m_info->open_list);

  pthread_mutex_unlock(&THR_LOCK_maria);
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  if ((save_errno == HA_ERR_CRASHED) ||
      (save_errno == HA_ERR_CRASHED_ON_USAGE) ||
      (save_errno == HA_ERR_CRASHED_ON_REPAIR))
    _ma_report_error(save_errno, name);
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
  pthread_mutex_unlock(&THR_LOCK_maria);
  my_errno=save_errno;
  DBUG_RETURN (NULL);
} /* maria_open */


byte *_ma_alloc_rec_buff(MARIA_HA *info, ulong length, byte **buf)
{
  uint extra;
  uint32 old_length;
  LINT_INIT(old_length);

  if (! *buf || length > (old_length=_ma_get_rec_buff_len(info, *buf)))
  {
    byte *newptr = *buf;

    /* to simplify initial init of info->rec_buf in maria_open and maria_extra */
    if (length == (ulong) -1)
    {
      length= max(info->s->base.pack_reclength,
                  info->s->base.max_key_length);
      /* Avoid unnecessary realloc */
      if (newptr && length == old_length)
	return newptr;
    }

    extra= ((info->s->options & HA_OPTION_PACK_RECORD) ?
	    ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER)+MARIA_SPLIT_LENGTH+
	    MARIA_REC_BUFF_OFFSET : 0);
    if (extra && newptr)
      newptr-= MARIA_REC_BUFF_OFFSET;
    if (!(newptr=(byte*) my_realloc((gptr)newptr, length+extra+8,
                                     MYF(MY_ALLOW_ZERO_PTR))))
      return newptr;
    *((uint32 *) newptr)= (uint32) length;
    *buf= newptr+(extra ?  MARIA_REC_BUFF_OFFSET : 0);
  }
  return *buf;
}


ulonglong _ma_safe_mul(ulonglong a, ulonglong b)
{
  ulonglong max_val= ~ (ulonglong) 0;		/* my_off_t is unsigned */

  if (!a || max_val / a < b)
    return max_val;
  return a*b;
}

	/* Set up functions in structs */

void _ma_setup_functions(register MARIA_SHARE *share)
{
  if (share->options & HA_OPTION_COMPRESS_RECORD)
  {
    share->read_record= _ma_read_pack_record;
    share->read_rnd= _ma_read_rnd_pack_record;
    if (!(share->options & HA_OPTION_TEMP_COMPRESS_RECORD))
      share->calc_checksum=0;				/* No checksum */
    else if (share->options & HA_OPTION_PACK_RECORD)
      share->calc_checksum= _ma_checksum;
    else
      share->calc_checksum= _ma_static_checksum;
  }
  else if (share->options & HA_OPTION_PACK_RECORD)
  {
    share->read_record= _ma_read_dynamic_record;
    share->read_rnd= _ma_read_rnd_dynamic_record;
    share->delete_record= _ma_delete_dynamic_record;
    share->compare_record= _ma_cmp_dynamic_record;
    share->compare_unique= _ma_cmp_dynamic_unique;
    share->calc_checksum= _ma_checksum;

    /* add bits used to pack data to pack_reclength for faster allocation */
    share->base.pack_reclength+= share->base.pack_bits;
    if (share->base.blobs)
    {
      share->update_record= _ma_update_blob_record;
      share->write_record= _ma_write_blob_record;
    }
    else
    {
      share->write_record= _ma_write_dynamic_record;
      share->update_record= _ma_update_dynamic_record;
    }
  }
  else
  {
    share->read_record= _ma_read_static_record;
    share->read_rnd= _ma_read_rnd_static_record;
    share->delete_record= _ma_delete_static_record;
    share->compare_record= _ma_cmp_static_record;
    share->update_record= _ma_update_static_record;
    share->write_record= _ma_write_static_record;
    share->compare_unique= _ma_cmp_static_unique;
    share->calc_checksum= _ma_static_checksum;
  }
  share->file_read= _ma_nommap_pread;
  share->file_write= _ma_nommap_pwrite;
  if (!(share->options & HA_OPTION_CHECKSUM))
    share->calc_checksum=0;
  return;
}


static void setup_key_functions(register MARIA_KEYDEF *keyinfo)
{
  if (keyinfo->key_alg == HA_KEY_ALG_RTREE)
  {
#ifdef HAVE_RTREE_KEYS
    keyinfo->ck_insert = maria_rtree_insert;
    keyinfo->ck_delete = maria_rtree_delete;
#else
    DBUG_ASSERT(0); /* maria_open should check it never happens */
#endif
  }
  else
  {
    keyinfo->ck_insert = _ma_ck_write;
    keyinfo->ck_delete = _ma_ck_delete;
  }
  if (keyinfo->flag & HA_BINARY_PACK_KEY)
  {						/* Simple prefix compression */
    keyinfo->bin_search= _ma_seq_search;
    keyinfo->get_key= _ma_get_binary_pack_key;
    keyinfo->pack_key= _ma_calc_bin_pack_key_length;
    keyinfo->store_key= _ma_store_bin_pack_key;
  }
  else if (keyinfo->flag & HA_VAR_LENGTH_KEY)
  {
    keyinfo->get_key= _ma_get_pack_key;
    if (keyinfo->seg[0].flag & HA_PACK_KEY)
    {						/* Prefix compression */
      if (!keyinfo->seg->charset || use_strnxfrm(keyinfo->seg->charset) ||
          (keyinfo->seg->flag & HA_NULL_PART))
        keyinfo->bin_search= _ma_seq_search;
      else
        keyinfo->bin_search= _ma_prefix_search;
      keyinfo->pack_key= _ma_calc_var_pack_key_length;
      keyinfo->store_key= _ma_store_var_pack_key;
    }
    else
    {
      keyinfo->bin_search= _ma_seq_search;
      keyinfo->pack_key= _ma_calc_var_key_length; /* Variable length key */
      keyinfo->store_key= _ma_store_static_key;
    }
  }
  else
  {
    keyinfo->bin_search= _ma_bin_search;
    keyinfo->get_key= _ma_get_static_key;
    keyinfo->pack_key= _ma_calc_static_key_length;
    keyinfo->store_key= _ma_store_static_key;
  }
  return;
}


/*
   Function to save and store the header in the index file (.MYI)
*/

uint _ma_state_info_write(File file, MARIA_STATE_INFO *state, uint pWrite)
{
  uchar  buff[MARIA_STATE_INFO_SIZE + MARIA_STATE_EXTRA_SIZE];
  uchar *ptr=buff;
  uint	i, keys= (uint) state->header.keys,
	key_blocks=state->header.max_block_size;
  DBUG_ENTER("_ma_state_info_write");

  memcpy_fixed(ptr,&state->header,sizeof(state->header));
  ptr+=sizeof(state->header);

  /* open_count must be first because of _ma_mark_file_changed ! */
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
  mi_int8store(ptr,(ulonglong) state->state.checksum);ptr +=8;
  mi_int4store(ptr,state->process);		ptr +=4;
  mi_int4store(ptr,state->unique);		ptr +=4;
  mi_int4store(ptr,state->status);		ptr +=4;
  mi_int4store(ptr,state->update_count);	ptr +=4;

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
    DBUG_RETURN(my_pwrite(file,(char*) buff, (uint) (ptr-buff), 0L,
			  MYF(MY_NABP | MY_THREADSAFE)));
  DBUG_RETURN(my_write(file,  (char*) buff, (uint) (ptr-buff),
		       MYF(MY_NABP)));
}


uchar *_ma_state_info_read(uchar *ptr, MARIA_STATE_INFO *state)
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
  state->state.checksum=(ha_checksum) mi_uint8korr(ptr);	ptr +=8;
  state->process= mi_uint4korr(ptr);		ptr +=4;
  state->unique = mi_uint4korr(ptr);		ptr +=4;
  state->status = mi_uint4korr(ptr);		ptr +=4;
  state->update_count=mi_uint4korr(ptr);	ptr +=4;

  ptr+= state->state_diff_length;

  for (i=0; i < keys; i++)
  {
    state->key_root[i]= mi_sizekorr(ptr);	ptr +=8;
  }
  for (i=0; i < key_blocks; i++)
  {
    state->key_del[i] = mi_sizekorr(ptr);	ptr +=8;
  }
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


uint _ma_state_info_read_dsk(File file, MARIA_STATE_INFO *state, my_bool pRead)
{
  char	buff[MARIA_STATE_INFO_SIZE + MARIA_STATE_EXTRA_SIZE];

  if (!maria_single_user)
  {
    if (pRead)
    {
      if (my_pread(file, buff, state->state_length,0L, MYF(MY_NABP)))
	return (MY_FILE_ERROR);
    }
    else if (my_read(file, buff, state->state_length,MYF(MY_NABP)))
      return (MY_FILE_ERROR);
    _ma_state_info_read((uchar*) buff, state);
  }
  return 0;
}


/****************************************************************************
**  store and read of MARIA_BASE_INFO
****************************************************************************/

uint _ma_base_info_write(File file, MARIA_BASE_INFO *base)
{
  uchar buff[MARIA_BASE_INFO_SIZE], *ptr=buff;

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


uchar *_ma_n_base_info_read(uchar *ptr, MARIA_BASE_INFO *base)
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
  maria_keydef
---------------------------------------------------------------------------*/

uint _ma_keydef_write(File file, MARIA_KEYDEF *keydef)
{
  uchar buff[MARIA_KEYDEF_SIZE];
  uchar *ptr=buff;

  *ptr++ = (uchar) keydef->keysegs;
  *ptr++ = keydef->key_alg;			/* Rtree or Btree */
  mi_int2store(ptr,keydef->flag);		ptr +=2;
  mi_int2store(ptr,keydef->block_length);	ptr +=2;
  mi_int2store(ptr,keydef->keylength);		ptr +=2;
  mi_int2store(ptr,keydef->minlength);		ptr +=2;
  mi_int2store(ptr,keydef->maxlength);		ptr +=2;
  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *_ma_keydef_read(char *ptr, MARIA_KEYDEF *keydef)
{
   keydef->keysegs	= (uint) *ptr++;
   keydef->key_alg	= *ptr++;		/* Rtree or Btree */

   keydef->flag		= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_length = mi_uint2korr(ptr);	ptr +=2;
   keydef->keylength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->minlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->maxlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_size	= keydef->block_length/MARIA_MIN_KEY_BLOCK_LENGTH-1;
   keydef->underflow_block_length=keydef->block_length/3;
   keydef->version	= 0;			/* Not saved */
   keydef->parser       = &ft_default_parser;
   keydef->ftparser_nr  = 0;
   return ptr;
}

/***************************************************************************
**  maria_keyseg
***************************************************************************/

int _ma_keyseg_write(File file, const HA_KEYSEG *keyseg)
{
  uchar buff[HA_KEYSEG_SIZE];
  uchar *ptr=buff;
  ulong pos;

  *ptr++= keyseg->type;
  *ptr++= keyseg->language;
  *ptr++= keyseg->null_bit;
  *ptr++= keyseg->bit_start;
  *ptr++= keyseg->bit_end;
  *ptr++= keyseg->bit_length;
  mi_int2store(ptr,keyseg->flag);	ptr+=2;
  mi_int2store(ptr,keyseg->length);	ptr+=2;
  mi_int4store(ptr,keyseg->start);	ptr+=4;
  pos= keyseg->null_bit ? keyseg->null_pos : keyseg->bit_pos;
  mi_int4store(ptr, pos);
  ptr+=4;

  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}


char *_ma_keyseg_read(char *ptr, HA_KEYSEG *keyseg)
{
   keyseg->type		= *ptr++;
   keyseg->language	= *ptr++;
   keyseg->null_bit	= *ptr++;
   keyseg->bit_start	= *ptr++;
   keyseg->bit_end	= *ptr++;
   keyseg->bit_length   = *ptr++;
   keyseg->flag		= mi_uint2korr(ptr);  ptr +=2;
   keyseg->length	= mi_uint2korr(ptr);  ptr +=2;
   keyseg->start	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->null_pos	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->charset=0;				/* Will be filled in later */
   if (keyseg->null_bit)
     keyseg->bit_pos= (uint16)(keyseg->null_pos + (keyseg->null_bit == 7));
   else
   {
     keyseg->bit_pos= (uint16)keyseg->null_pos;
     keyseg->null_pos= 0;
   }
   return ptr;
}

/*--------------------------------------------------------------------------
  maria_uniquedef
---------------------------------------------------------------------------*/

uint _ma_uniquedef_write(File file, MARIA_UNIQUEDEF *def)
{
  uchar buff[MARIA_UNIQUEDEF_SIZE];
  uchar *ptr=buff;

  mi_int2store(ptr,def->keysegs);		ptr+=2;
  *ptr++=  (uchar) def->key;
  *ptr++ = (uchar) def->null_are_equal;

  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *_ma_uniquedef_read(char *ptr, MARIA_UNIQUEDEF *def)
{
   def->keysegs = mi_uint2korr(ptr);
   def->key	= ptr[2];
   def->null_are_equal=ptr[3];
   return ptr+4;				/* 1 extra byte */
}

/***************************************************************************
**  MARIA_COLUMNDEF
***************************************************************************/

uint _ma_recinfo_write(File file, MARIA_COLUMNDEF *recinfo)
{
  uchar buff[MARIA_COLUMNDEF_SIZE];
  uchar *ptr=buff;

  mi_int2store(ptr,recinfo->type);	ptr +=2;
  mi_int2store(ptr,recinfo->length);	ptr +=2;
  *ptr++ = recinfo->null_bit;
  mi_int2store(ptr,recinfo->null_pos);	ptr+= 2;
  return my_write(file,(char*) buff, (uint) (ptr-buff), MYF(MY_NABP));
}

char *_ma_recinfo_read(char *ptr, MARIA_COLUMNDEF *recinfo)
{
   recinfo->type=  mi_sint2korr(ptr);	ptr +=2;
   recinfo->length=mi_uint2korr(ptr);	ptr +=2;
   recinfo->null_bit= (uint8) *ptr++;
   recinfo->null_pos=mi_uint2korr(ptr); ptr +=2;
   return ptr;
}

/**************************************************************************
Open data file with or without RAID
We can't use dup() here as the data file descriptors need to have different
active seek-positions.

The argument file_to_dup is here for the future if there would on some OS
exist a dup()-like call that would give us two different file descriptors.
*************************************************************************/

int _ma_open_datafile(MARIA_HA *info, MARIA_SHARE *share, File file_to_dup __attribute__((unused)))
{
#ifdef USE_RAID
  if (share->base.raid_type)
  {
    info->dfile=my_raid_open(share->data_file_name,
			     share->mode | O_SHARE,
			     share->base.raid_type,
			     share->base.raid_chunks,
			     share->base.raid_chunksize,
			     MYF(MY_WME | MY_RAID));
  }
  else
#endif
    info->dfile=my_open(share->data_file_name, share->mode | O_SHARE,
			MYF(MY_WME));
  return info->dfile >= 0 ? 0 : 1;
}


int _ma_open_keyfile(MARIA_SHARE *share)
{
  if ((share->kfile=my_open(share->unique_file_name, share->mode | O_SHARE,
			    MYF(MY_WME))) < 0)
    return 1;
  return 0;
}


/*
  Disable all indexes.

  SYNOPSIS
    maria_disable_indexes()
    info        A pointer to the MARIA storage engine MARIA_HA struct.

  DESCRIPTION
    Disable all indexes.

  RETURN
    0  ok
*/

int maria_disable_indexes(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;

  maria_clear_all_keys_active(share->state.key_map);
  return 0;
}


/*
  Enable all indexes

  SYNOPSIS
    maria_enable_indexes()
    info        A pointer to the MARIA storage engine MARIA_HA struct.

  DESCRIPTION
    Enable all indexes. The indexes might have been disabled
    by maria_disable_index() before.
    The function works only if both data and indexes are empty,
    otherwise a repair is required.
    To be sure, call handler::delete_all_rows() before.

  RETURN
    0  ok
    HA_ERR_CRASHED data or index is non-empty.
*/

int maria_enable_indexes(MARIA_HA *info)
{
  int error= 0;
  MARIA_SHARE *share= info->s;

  if (share->state.state.data_file_length ||
      (share->state.state.key_file_length != share->base.keystart))
  {
    maria_print_error(info->s, HA_ERR_CRASHED);
    error= HA_ERR_CRASHED;
  }
  else
    maria_set_all_keys_active(share->state.key_map, share->base.keys);
  return error;
}


/*
  Test if indexes are disabled.

  SYNOPSIS
    maria_indexes_are_disabled()
    info        A pointer to the MARIA storage engine MARIA_HA struct.

  DESCRIPTION
    Test if indexes are disabled.

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int maria_indexes_are_disabled(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;

  return (! maria_is_any_key_active(share->state.key_map) && share->base.keys);
}
