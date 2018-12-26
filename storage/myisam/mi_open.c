/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  open a isam-database

  Internal temporary tables
  -------------------------
  Since only single instance of internal temporary table is required by
  optimizer, such tables are not registered on myisam_open_list. In effect
  it means (a) THR_LOCK_myisam is not held while such table is being created,
  opened or closed; (b) no iteration through myisam_open_list while opening a
  table. This optimization gives nice scalability benefit in concurrent
  environment. MEMORY internal temporary tables are optimized similarly.
*/

#include "fulltext.h"
#include "sp_defs.h"
#include "rt_index.h"
#include <m_ctype.h>

#ifdef _WIN32
#include <fcntl.h>
#include <process.h>
#endif

static void setup_key_functions(MI_KEYDEF *keyinfo);
#define get_next_element(to,pos,size) { memcpy((char*) to,pos,(size_t) size); \
					pos+=size;}


#define disk_pos_assert(pos, end_pos) \
if (pos > end_pos)             \
{                              \
  set_my_errno(HA_ERR_CRASHED);\
  goto err;                    \
}


/******************************************************************************
** Return the shared struct if the table is already open.
** In MySQL the server will handle version issues.
******************************************************************************/

MI_INFO *test_if_reopen(char *filename)
{
  LIST *pos;

  for (pos=myisam_open_list ; pos ; pos=pos->next)
  {
    MI_INFO *info=(MI_INFO*) pos->data;
    MYISAM_SHARE *share=info->s;
    if (!strcmp(share->unique_file_name,filename) && share->last_version)
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

MI_INFO *mi_open_share(const char *name, MYISAM_SHARE *old_share, int mode,
                       uint open_flags)
{
  int lock_error,kfile,open_mode,save_errno, realpath_err;
  uint i,j,len,errpos,head_length,base_pos,offset,info_length,keys,
    key_parts,unique_key_parts,fulltext_keys,uniques;
  uint internal_table= open_flags & HA_OPEN_INTERNAL_TABLE;
  char name_buff[FN_REFLEN], org_name[FN_REFLEN], index_name[FN_REFLEN],
       data_name[FN_REFLEN];
  uchar *disk_cache, *disk_pos, *end_pos;
  MI_INFO info, *m_info;
  MYISAM_SHARE share_buff,*share;
  ulong rec_per_key_part[HA_MAX_POSSIBLE_KEY*MI_MAX_KEY_SEG];
  my_off_t key_root[HA_MAX_POSSIBLE_KEY],key_del[MI_MAX_KEY_BLOCK_SIZE];
  ulonglong max_key_file_length, max_data_file_length;
  ST_FILE_ID file_id= {0, 0};
  DBUG_ENTER("mi_open_share");

  m_info= NULL;
  kfile= -1;
  lock_error=1;
  errpos=0;
  head_length=sizeof(share_buff.state.header);
  memset(&info, 0, sizeof(info));

  realpath_err= my_realpath(name_buff,
                  fn_format(org_name,name,"",MI_NAME_IEXT,4),MYF(0));
  if (my_is_symlink(name_buff, &file_id))
  {
    if (realpath_err ||
       (*myisam_test_invalid_symlink)(name_buff) ||
       my_is_symlink(name_buff, &file_id))
    {
      set_my_errno(HA_WRONG_CREATE_OPTION);
      DBUG_RETURN (NULL);
    }
  }

  if (!internal_table)
  {
    mysql_mutex_lock(&THR_LOCK_myisam);
    if (!old_share && ! (open_flags & HA_OPEN_FROM_SQL_LAYER))
    {
      MI_INFO *old_info= test_if_reopen(name_buff);
      if (old_info)
        old_share= old_info->s;
    }
  }

  if (!old_share)
  {
    share= &share_buff;
    memset(&share_buff, 0, sizeof(share_buff));
    share_buff.state.rec_per_key_part=rec_per_key_part;
    share_buff.state.key_root=key_root;
    share_buff.state.key_del=key_del;
    share_buff.key_cache= multi_key_cache_search((uchar*) name_buff,
                                                 strlen(name_buff));

    DBUG_EXECUTE_IF("myisam_pretend_crashed_table_on_open",
                    if (strstr(name, "/t1"))
                    {
                      set_my_errno(HA_ERR_CRASHED);
                      goto err;
                    });
    DEBUG_SYNC_C("before_opening_indexfile");
    if ((kfile= mysql_file_open(mi_key_file_kfile,
                                name_buff,
                                (open_mode= O_RDWR) | O_SHARE | O_NOFOLLOW,
                                MYF(0))) < 0)
    {
      if ((errno != EROFS && errno != EACCES) ||
	  mode != O_RDONLY ||
          (kfile= mysql_file_open(mi_key_file_kfile,
                                  name_buff,
                                  (open_mode= O_RDONLY) | O_SHARE | O_NOFOLLOW,
                                  MYF(0))) < 0)
	goto err;
    }

    if (!my_is_same_file(kfile, &file_id))
    {
      mysql_file_close(kfile, MYF(0));
      set_my_errno(HA_WRONG_CREATE_OPTION);
      goto err;
    }

    share->mode=open_mode;
    errpos=1;
    if (mysql_file_read(kfile, share->state.header.file_version, head_length,
                        MYF(MY_NABP)))
    {
      set_my_errno(HA_ERR_NOT_A_TABLE);
      goto err;
    }
    if (memcmp((uchar*) share->state.header.file_version,
	       (uchar*) myisam_file_magic, 4))
    {
      DBUG_PRINT("error",("Wrong header in %s",name_buff));
      DBUG_DUMP("error_dump", share->state.header.file_version,
		head_length);
      set_my_errno(HA_ERR_NOT_A_TABLE);
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
      set_my_errno(HA_ERR_OLD_FILE);
      goto err;
    }
    if ((share->options & HA_OPTION_RELIES_ON_SQL_LAYER) &&
        ! (open_flags & HA_OPEN_FROM_SQL_LAYER))
    {
      DBUG_PRINT("error", ("table cannot be openned from non-sql layer"));
      set_my_errno(HA_ERR_UNSUPPORTED);
      goto err;
    }
    /* Don't call realpath() if the name can't be a link */
    if (!strcmp(name_buff, org_name) ||
        my_readlink(index_name, org_name, MYF(0)) == -1)
      (void) my_stpcpy(index_name, org_name);
    *strrchr(org_name, '.')= '\0';
    (void) fn_format(data_name,org_name,"",MI_NAME_DEXT,
                     MY_APPEND_EXT|MY_UNPACK_FILENAME|MY_RESOLVE_SYMLINKS);

    info_length=mi_uint2korr(share->state.header.header_length);
    base_pos=mi_uint2korr(share->state.header.base_pos);
    if (!(disk_cache= (uchar*) my_alloca(info_length+128)))
    {
      set_my_errno(ENOMEM);
      goto err;
    }
    end_pos=disk_cache+info_length;
    errpos=2;

    mysql_file_seek(kfile, 0L, MY_SEEK_SET, MYF(0));
    if (!(open_flags & HA_OPEN_TMP_TABLE))
    {
      if ((lock_error=my_lock(kfile,F_RDLCK,0L,F_TO_EOF,
			      MYF(open_flags & HA_OPEN_WAIT_IF_LOCKED ?
				  0 : MY_DONT_WAIT))) &&
	  !(open_flags & HA_OPEN_IGNORE_IF_LOCKED))
	goto err;
    }
    errpos=3;
    if (mysql_file_read(kfile, disk_cache, info_length, MYF(MY_NABP)))
    {
      set_my_errno(HA_ERR_CRASHED);
      goto err;
    }
    len=mi_uint2korr(share->state.header.state_info_length);
    keys=    (uint) share->state.header.keys;
    uniques= (uint) share->state.header.uniques;
    fulltext_keys= (uint) share->state.header.fulltext_keys;
    key_parts= mi_uint2korr(share->state.header.key_parts);
    unique_key_parts= mi_uint2korr(share->state.header.unique_key_parts);
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
			    len,MI_BASE_INFO_SIZE));
    }
    disk_pos= my_n_base_info_read(disk_cache + base_pos, &share->base);
    share->state.state_length=base_pos;

    if (!(open_flags & HA_OPEN_FOR_REPAIR) &&
	((share->state.changed & STATE_CRASHED) ||
	 ((open_flags & HA_OPEN_ABORT_IF_CRASHED) &&
	  (my_disable_locking && share->state.open_count))))
    {
      DBUG_PRINT("error",("Table is marked as crashed. open_flags: %u  "
                          "changed: %u  open_count: %u  !locking: %d",
                          open_flags, share->state.changed,
                          share->state.open_count, my_disable_locking));
      set_my_errno(((share->state.changed & STATE_CRASHED_ON_REPAIR) ?
                    HA_ERR_CRASHED_ON_REPAIR : HA_ERR_CRASHED_ON_USAGE));
      goto err;
    }

    /* sanity check */
    if (share->base.keystart > 65535 || 
        share->base.rec_reflength > 8 || share->base.key_reflength > 7) 
    {
      set_my_errno(HA_ERR_CRASHED);
      goto err;
    }

    key_parts+=fulltext_keys*FT_SEGS;
    if (share->base.max_key_length > MI_MAX_KEY_BUFF || keys > MI_MAX_KEY ||
	key_parts > MI_MAX_KEY * MI_MAX_KEY_SEG)
    {
      DBUG_PRINT("error",("Wrong key info:  Max_key_length: %d  keys: %d  key_parts: %d", share->base.max_key_length, keys, key_parts));
      set_my_errno(HA_ERR_UNSUPPORTED);
      goto err;
    }

    /* Correct max_file_length based on length of sizeof(off_t) */
    max_data_file_length=
      (share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
      (((ulonglong) 1 << (share->base.rec_reflength*8))-1) :
      (mi_safe_mul(share->base.pack_reclength,
		   (ulonglong) 1 << (share->base.rec_reflength*8))-1);
    max_key_file_length=
      mi_safe_mul(MI_MIN_KEY_BLOCK_LENGTH,
		  ((ulonglong) 1 << (share->base.key_reflength*8))-1);
#if SIZEOF_OFF_T == 4
    set_if_smaller(max_data_file_length, INT_MAX32);
    set_if_smaller(max_key_file_length, INT_MAX32);
#endif
    share->base.max_data_file_length=(my_off_t) max_data_file_length;
    share->base.max_key_file_length=(my_off_t) max_key_file_length;

    if (share->options & HA_OPTION_COMPRESS_RECORD)
      share->base.max_key_length+=2;	/* For safety */

    /* Add space for node pointer */
    share->base.max_key_length+= share->base.key_reflength;

    if (!my_multi_malloc(mi_key_memory_MYISAM_SHARE,
                         MY_WME,
			 &share,sizeof(*share),
			 &share->state.rec_per_key_part,sizeof(long)*key_parts,
			 &share->keyinfo,keys*sizeof(MI_KEYDEF),
			 &share->uniqueinfo,uniques*sizeof(MI_UNIQUEDEF),
			 &share->keyparts,
			 (key_parts+unique_key_parts+keys+uniques) *
			 sizeof(HA_KEYSEG),
			 &share->rec,
			 (share->base.fields+1)*sizeof(MI_COLUMNDEF),
			 &share->blobs,sizeof(MI_BLOB)*share->base.blobs,
			 &share->unique_file_name,strlen(name_buff)+1,
			 &share->index_file_name,strlen(index_name)+1,
			 &share->data_file_name,strlen(data_name)+1,
			 &share->state.key_root,keys*sizeof(my_off_t),
			 &share->state.key_del,
			 (share->state.header.max_block_size_index*sizeof(my_off_t)),
                         &share->key_root_lock, sizeof(mysql_rwlock_t)*keys,
                         &share->mmap_lock, sizeof(mysql_rwlock_t),
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
			     share->state.header.max_block_size_index));
    my_stpcpy(share->unique_file_name, name_buff);
    share->unique_name_length= strlen(name_buff);
    my_stpcpy(share->index_file_name,  index_name);
    my_stpcpy(share->data_file_name,   data_name);

    share->blocksize= MY_MIN(IO_SIZE, myisam_block_size);
    {
      HA_KEYSEG *pos=share->keyparts;
      uint32 ftkey_nr= 1;
      for (i=0 ; i < keys ; i++)
      {
        share->keyinfo[i].share= share;
	disk_pos=mi_keydef_read(disk_pos, &share->keyinfo[i]);
        disk_pos_assert(disk_pos + share->keyinfo[i].keysegs * HA_KEYSEG_SIZE,
 			end_pos);
        if (share->keyinfo[i].key_alg == HA_KEY_ALG_RTREE)
          share->have_rtree=1;
	set_if_smaller(share->blocksize,share->keyinfo[i].block_length);
	share->keyinfo[i].seg=pos;
	for (j=0 ; j < share->keyinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
          if (pos->flag & HA_BLOB_PART &&
              ! (share->options & (HA_OPTION_COMPRESS_RECORD |
                                   HA_OPTION_PACK_RECORD)))
          {
            set_my_errno(HA_ERR_CRASHED);
            goto err;
          }
	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language, MYF(MY_WME))))
	    {
	      set_my_errno(HA_ERR_UNKNOWN_CHARSET);
	      goto err;
	    }
	  }
	  else if (pos->type == HA_KEYTYPE_BINARY)
	    pos->charset= &my_charset_bin;
          if (!(share->keyinfo[i].flag & HA_SPATIAL) &&
              pos->start > share->base.reclength)
          {
            set_my_errno(HA_ERR_CRASHED);
            goto err;
          }
	}
	if (share->keyinfo[i].flag & HA_SPATIAL)
	{
	  uint sp_segs=SPDIMS*2;
	  share->keyinfo[i].seg=pos-sp_segs;
	  share->keyinfo[i].keysegs--;
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
            uint k;
            share->keyinfo[i].seg=pos;
            for (k=0; k < FT_SEGS; k++)
            {
              *pos= ft_keysegs[k];
              pos[0].language= pos[-1].language;
              if (!(pos[0].charset= pos[-1].charset))
              {
                set_my_errno(HA_ERR_CRASHED);
                goto err;
              }
              pos++;
            }
          }
          if (!share->ft2_keyinfo.seg)
          {
            memcpy(& share->ft2_keyinfo, & share->keyinfo[i], sizeof(MI_KEYDEF));
            share->ft2_keyinfo.keysegs=1;
            share->ft2_keyinfo.flag=0;
            share->ft2_keyinfo.keylength=
            share->ft2_keyinfo.minlength=
            share->ft2_keyinfo.maxlength=HA_FT_WLEN+share->base.rec_reflength;
            share->ft2_keyinfo.seg=pos-1;
            share->ft2_keyinfo.end=pos;
            setup_key_functions(& share->ft2_keyinfo);
          }
          share->keyinfo[i].ftkey_nr= ftkey_nr++;
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
	disk_pos=mi_uniquedef_read(disk_pos, &share->uniqueinfo[i]);
        disk_pos_assert(disk_pos + share->uniqueinfo[i].keysegs *
			HA_KEYSEG_SIZE, end_pos);
	share->uniqueinfo[i].seg=pos;
	for (j=0 ; j < share->uniqueinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language, MYF(MY_WME))))
	    {
	      set_my_errno(HA_ERR_UNKNOWN_CHARSET);
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
      share->ftkeys= ftkey_nr;
    }

    disk_pos_assert(disk_pos + share->base.fields *MI_COLUMNDEF_SIZE, end_pos);
    for (i=j=offset=0 ; i < share->base.fields ; i++)
    {
      disk_pos=mi_recinfo_read(disk_pos,&share->rec[i]);
      share->rec[i].pack_type=0;
      share->rec[i].huff_tree=0;
      share->rec[i].offset=offset;
      if (share->rec[i].type == (int) FIELD_BLOB)
      {
	share->blobs[j].pack_length=
	  share->rec[i].length-portable_sizeof_char_ptr;
	share->blobs[j].offset=offset;
	j++;
      }
      offset+=share->rec[i].length;
    }
    share->rec[i].type=(int) FIELD_LAST;	/* End marker */
    if (offset > share->base.reclength)
    {
      /* purecov: begin inspected */
      set_my_errno(HA_ERR_CRASHED);
      goto err;
      /* purecov: end */
    }

    if (! lock_error)
    {
      (void) my_lock(kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE));
      lock_error=1;			/* Database unlocked */
    }

    if (mi_open_datafile(&info, share, name, -1))
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
					(keys ? MI_INDEX_BLOCK_MARGIN *
					 share->blocksize * keys : 0));
    share->blocksize= MY_MIN(IO_SIZE, myisam_block_size);
    share->data_file_type=STATIC_RECORD;
    if (share->options & HA_OPTION_COMPRESS_RECORD)
    {
      share->data_file_type = COMPRESSED_RECORD;
      share->options|= HA_OPTION_READ_ONLY_DATA;
      info.s=share;
      if (_mi_read_pack_info(&info,
			     (pbool)
			     MY_TEST(!(share->options &
                                       (HA_OPTION_PACK_RECORD |
                                        HA_OPTION_TEMP_COMPRESS_RECORD)))))
	goto err;
    }
    else if (share->options & HA_OPTION_PACK_RECORD)
      share->data_file_type = DYNAMIC_RECORD;
    mi_setup_functions(share);
    share->is_log_table= FALSE;
    thr_lock_init(&share->lock);
    mysql_mutex_init(mi_key_mutex_MYISAM_SHARE_intern_lock,
                     &share->intern_lock, MY_MUTEX_INIT_FAST);
    for (i=0; i<keys; i++)
      mysql_rwlock_init(mi_key_rwlock_MYISAM_SHARE_key_root_lock,
                        &share->key_root_lock[i]);
    mysql_rwlock_init(mi_key_rwlock_MYISAM_SHARE_mmap_lock, &share->mmap_lock);
    if (myisam_concurrent_insert)
    {
      share->concurrent_insert=
	((share->options & (HA_OPTION_READ_ONLY_DATA | HA_OPTION_TMP_TABLE |
			   HA_OPTION_COMPRESS_RECORD |
			   HA_OPTION_TEMP_COMPRESS_RECORD)) ||
	 (open_flags & HA_OPEN_TMP_TABLE) ||
	 share->have_rtree) ? 0 : 1;
      if (share->concurrent_insert)
      {
	share->lock.get_status=mi_get_status;
	share->lock.copy_status=mi_copy_status;
	share->lock.update_status=mi_update_status;
        share->lock.restore_status= mi_restore_status;
	share->lock.check_status=mi_check_status;
      }
    }
    /*
      Memory mapping can only be requested after initializing intern_lock.
    */
    if (open_flags & HA_OPEN_MMAP)
    {
      info.s= share;
      mi_extra(&info, HA_EXTRA_MMAP, 0);
    }
  }
  else
  {
    share= old_share;
    if (mode == O_RDWR && share->mode == O_RDONLY)
    {
      set_my_errno(EACCES);				/* Can't open in write mode */
      goto err;
    }
    if (mi_open_datafile(&info, share, name, -1))
      goto err;
    errpos=5;
  }

  /* alloc and set up private structure parts */
  if (!my_multi_malloc(mi_key_memory_MI_INFO,
                       MY_WME,
		       &m_info,sizeof(MI_INFO),
		       &info.blobs,sizeof(MI_BLOB)*share->base.blobs,
		       &info.buff,(share->base.max_key_block_length*2+
				   share->base.max_key_length),
		       &info.lastkey,share->base.max_key_length*3+1,
                       &info.rnext_same_key, share->base.max_key_length,
		       &info.first_mbr_key, share->base.max_key_length,
		       &info.filename,strlen(name)+1,
		       &info.rtree_recursion_state,share->have_rtree ? 1024 : 0,
		       NullS))
    goto err;
  errpos=6;

  if (!share->have_rtree)
    info.rtree_recursion_state= NULL;

  my_stpcpy(info.filename,name);
  memcpy(info.blobs,share->blobs,sizeof(MI_BLOB)*share->base.blobs);
  info.lastkey2=info.lastkey+share->base.max_key_length;

  /*
    If only mi_rkey is called earlier, rnext_same_key should be set in
    mi_rnext_same.
  */
  info.set_rnext_same_key= FALSE;
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
  mysql_mutex_lock(&share->intern_lock);
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
      myisam_delay_key_write)
    share->delay_key_write=1;
  info.state= &share->state.state;	/* Change global values by default */
  mysql_mutex_unlock(&share->intern_lock);

  /* Allocate buffer for one record */

  /* prerequisites: memset(&info, 0) && info->s=share; are met. */
  if (!mi_alloc_rec_buff(&info, -1, &info.rec_buff))
    goto err;
  memset(info.rec_buff, 0, mi_get_rec_buff_len(&info, info.rec_buff));

  *m_info=info;
  thr_lock_data_init(&share->lock,&m_info->lock,(void*) m_info);

  if (!internal_table)
  {
    m_info->open_list.data= (void*) m_info;
    myisam_open_list= list_add(myisam_open_list, &m_info->open_list);
    mysql_mutex_unlock(&THR_LOCK_myisam);
  }

  memset(info.buff, 0, share->base.max_key_block_length * 2);

  if (myisam_log_file >= 0)
  {
    intern_filename(name_buff,share->index_file_name);
    _myisam_log(MI_LOG_OPEN, m_info, (uchar*) name_buff, strlen(name_buff));
  }
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno() ? my_errno() : HA_ERR_END_OF_FILE;
  if ((save_errno == HA_ERR_CRASHED) ||
      (save_errno == HA_ERR_CRASHED_ON_USAGE) ||
      (save_errno == HA_ERR_CRASHED_ON_REPAIR))
    mi_report_error(save_errno, name);
  switch (errpos) {
  case 6:
    my_free(m_info);
    /* fall through */
  case 5:
    (void) mysql_file_close(info.dfile, MYF(0));
    if (old_share)
      break;					/* Don't remove open table */
    /* fall through */
  case 4:
    my_free(share);
    /* fall through */
  case 3:
    if (! lock_error)
      (void) my_lock(kfile, F_UNLCK, 0L, F_TO_EOF, MYF(MY_SEEK_NOT_DONE));
    /* fall through */
  case 2:
    /* fall through */
  case 1:
    (void) mysql_file_close(kfile, MYF(0));
    /* fall through */
  case 0:
  default:
    break;
  }
  if (!internal_table)
    mysql_mutex_unlock(&THR_LOCK_myisam);
  set_my_errno(save_errno);
  DBUG_RETURN (NULL);
} /* mi_open_share */


uchar *mi_alloc_rec_buff(MI_INFO *info, ulong length, uchar **buf)
{
  uint extra;
  uint32 old_length= 0;

  if (! *buf || length > (old_length=mi_get_rec_buff_len(info, *buf)))
  {
    uchar *newptr = *buf;

    /* to simplify initial init of info->rec_buf in mi_open and mi_extra */
    if (length == (ulong) -1)
    {
      if (info->s->options & HA_OPTION_COMPRESS_RECORD)
        length= MY_MAX(info->s->base.pack_reclength, info->s->max_pack_length);
      else
        length= info->s->base.pack_reclength;
      length= MY_MAX(length, info->s->base.max_key_length);
      /* Avoid unnecessary realloc */
      if (newptr && length == old_length)
	return newptr;
    }

    extra= ((info->s->options & HA_OPTION_PACK_RECORD) ?
	    ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	    MI_REC_BUFF_OFFSET : 0);
    if (extra && newptr)
      newptr-= MI_REC_BUFF_OFFSET;
    if (!(newptr=(uchar*) my_realloc(mi_key_memory_record_buffer,
                                     (uchar*)newptr, length+extra+8,
                                     MYF(MY_ALLOW_ZERO_PTR))))
      return newptr;
    *((uint32 *) newptr)= (uint32) length;
    *buf= newptr+(extra ?  MI_REC_BUFF_OFFSET : 0);
  }
  return *buf;
}


ulonglong mi_safe_mul(ulonglong a, ulonglong b)
{
  ulonglong max_val= ~ (ulonglong) 0;		/* my_off_t is unsigned */

  if (!a || max_val / a < b)
    return max_val;
  return a*b;
}

	/* Set up functions in structs */

void mi_setup_functions(MYISAM_SHARE *share)
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

    /* add bits used to pack data to pack_reclength for faster allocation */
    share->base.pack_reclength+= share->base.pack_bits;
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
  share->file_read= mi_nommap_pread;
  share->file_write= mi_nommap_pwrite;
  if (!(share->options & HA_OPTION_CHECKSUM))
    share->calc_checksum=0;
  return;
}


static void setup_key_functions(MI_KEYDEF *keyinfo)
{
  if (keyinfo->key_alg == HA_KEY_ALG_RTREE)
  {
    keyinfo->ck_insert = rtree_insert;
    keyinfo->ck_delete = rtree_delete;
  }
  else
  {
    keyinfo->ck_insert = _mi_ck_write;
    keyinfo->ck_delete = _mi_ck_delete;
  }
  if (keyinfo->flag & HA_BINARY_PACK_KEY)
  {						/* Simple prefix compression */
    keyinfo->bin_search=_mi_seq_search;
    keyinfo->get_key=_mi_get_binary_pack_key;
    keyinfo->pack_key=_mi_calc_bin_pack_key_length;
    keyinfo->store_key=_mi_store_bin_pack_key;
  }
  else if (keyinfo->flag & HA_VAR_LENGTH_KEY)
  {
    keyinfo->get_key= _mi_get_pack_key;
    if (keyinfo->seg[0].flag & HA_PACK_KEY)
    {						/* Prefix compression */
      /*
        _mi_prefix_search() compares end-space against ASCII blank (' ').
        It cannot be used for character sets, that do not encode the
        blank character like ASCII does. UCS2 is an example. All
        character sets with a fixed width > 1 or a mimimum width > 1
        cannot represent blank like ASCII does. In these cases we have
        to use _mi_seq_search() for the search.
      */
      if (!keyinfo->seg->charset || use_strnxfrm(keyinfo->seg->charset) ||
          (keyinfo->seg->flag & HA_NULL_PART) ||
          (keyinfo->seg->charset->mbminlen > 1))
        keyinfo->bin_search=_mi_seq_search;
      else
        keyinfo->bin_search=_mi_prefix_search;
      keyinfo->pack_key=_mi_calc_var_pack_key_length;
      keyinfo->store_key=_mi_store_var_pack_key;
    }
    else
    {
      keyinfo->bin_search=_mi_seq_search;
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


/*
   Function to save and store the header in the index file (.MYI)
*/

uint mi_state_info_write(File file, MI_STATE_INFO *state, uint pWrite)
{
  uchar  buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];
  uchar *ptr=buff;
  uint	i, keys= (uint) state->header.keys,
	key_blocks=state->header.max_block_size_index;
  DBUG_ENTER("mi_state_info_write");

  memcpy(ptr, &state->header, sizeof(state->header));
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
    DBUG_RETURN(mysql_file_pwrite(file, buff, (size_t) (ptr-buff), 0L,
                                  MYF(MY_NABP | MY_THREADSAFE)) != 0);
  DBUG_RETURN(mysql_file_write(file, buff, (size_t) (ptr-buff),
                               MYF(MY_NABP)) != 0);
}


uchar *mi_state_info_read(uchar *ptr, MI_STATE_INFO *state)
{
  uint i,keys,key_parts,key_blocks;
  memcpy(&state->header, ptr, sizeof(state->header));
  ptr +=sizeof(state->header);
  keys=(uint) state->header.keys;
  key_parts=mi_uint2korr(state->header.key_parts);
  key_blocks=state->header.max_block_size_index;

  state->open_count = mi_uint2korr(ptr);	ptr +=2;
  state->changed= *ptr++;
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


uint mi_state_info_read_dsk(File file, MI_STATE_INFO *state, my_bool pRead)
{
  uchar	buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];

  if (!myisam_single_user)
  {
    if (pRead)
    {
      if (mysql_file_pread(file, buff, state->state_length, 0L, MYF(MY_NABP)))
	return 1;
    }
    else if (mysql_file_read(file, buff, state->state_length, MYF(MY_NABP)))
      return 1;
    mi_state_info_read(buff, state);
  }
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
  memset(ptr, 0, 13);					ptr +=13; /* extra */
  return mysql_file_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}


uchar *my_n_base_info_read(uchar *ptr, MI_BASE_INFO *base)
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

  ptr+=13;
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
  *ptr++ = keydef->key_alg;			/* Rtree or Btree */
  mi_int2store(ptr,keydef->flag);		ptr +=2;
  mi_int2store(ptr,keydef->block_length);	ptr +=2;
  mi_int2store(ptr,keydef->keylength);		ptr +=2;
  mi_int2store(ptr,keydef->minlength);		ptr +=2;
  mi_int2store(ptr,keydef->maxlength);		ptr +=2;
  return mysql_file_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

uchar *mi_keydef_read(uchar *ptr, MI_KEYDEF *keydef)
{
   keydef->keysegs	= (uint) *ptr++;
   keydef->key_alg	= *ptr++;		/* Rtree or Btree */

   keydef->flag		= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_length = mi_uint2korr(ptr);	ptr +=2;
   keydef->keylength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->minlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->maxlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_size_index= keydef->block_length/MI_MIN_KEY_BLOCK_LENGTH-1;
   keydef->underflow_block_length=keydef->block_length/3;
   keydef->version	= 0;			/* Not saved */
   keydef->parser       = &ft_default_parser;
   keydef->ftkey_nr     = 0;
   return ptr;
}

/***************************************************************************
**  mi_keyseg
***************************************************************************/

int mi_keyseg_write(File file, const HA_KEYSEG *keyseg)
{
  uchar buff[HA_KEYSEG_SIZE];
  uchar *ptr=buff;
  ulong pos;

  *ptr++= keyseg->type;
  *ptr++= keyseg->language & 0xFF; /* Collation ID, low byte */
  *ptr++= keyseg->null_bit;
  *ptr++= keyseg->bit_start;
  *ptr++= keyseg->language >> 8; /* Collation ID, high byte */
  *ptr++= keyseg->bit_length;
  mi_int2store(ptr,keyseg->flag);	ptr+=2;
  mi_int2store(ptr,keyseg->length);	ptr+=2;
  mi_int4store(ptr,keyseg->start);	ptr+=4;
  pos= keyseg->null_bit ? keyseg->null_pos : keyseg->bit_pos;
  mi_int4store(ptr, pos);
  ptr+=4;
  
  return mysql_file_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}


uchar *mi_keyseg_read(uchar *ptr, HA_KEYSEG *keyseg)
{
   keyseg->type		= *ptr++;
   keyseg->language	= *ptr++;
   keyseg->null_bit	= *ptr++;
   keyseg->bit_start	= *ptr++;
   keyseg->language	+= ((uint16) (*ptr++)) << 8;
   keyseg->bit_length   = *ptr++;
   keyseg->flag		= mi_uint2korr(ptr);  ptr +=2;
   keyseg->length	= mi_uint2korr(ptr);  ptr +=2;
   keyseg->start	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->null_pos	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->bit_end= 0;
   keyseg->charset=0;				/* Will be filled in later */
   if (keyseg->null_bit)
     /* We adjust bit_pos if null_bit is last in the byte */
     keyseg->bit_pos= (uint16)(keyseg->null_pos + (keyseg->null_bit == (1 << 7)));
   else
   {
     keyseg->bit_pos= (uint16)keyseg->null_pos;
     keyseg->null_pos= 0;
   }
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

  return mysql_file_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

uchar *mi_uniquedef_read(uchar *ptr, MI_UNIQUEDEF *def)
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
  return mysql_file_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

uchar *mi_recinfo_read(uchar *ptr, MI_COLUMNDEF *recinfo)
{
   recinfo->type=  mi_sint2korr(ptr);	ptr +=2;
   recinfo->length=mi_uint2korr(ptr);	ptr +=2;
   recinfo->null_bit= (uint8) *ptr++;
   recinfo->null_pos=mi_uint2korr(ptr); ptr +=2;
   return ptr;
}

/**************************************************************************
Open data file.
We can't use dup() here as the data file descriptors need to have different
active seek-positions.

The argument file_to_dup is here for the future if there would on some OS
exist a dup()-like call that would give us two different file descriptors.
*************************************************************************/

int mi_open_datafile(MI_INFO *info, MYISAM_SHARE *share, const char *org_name,
                     File file_to_dup MY_ATTRIBUTE((unused)))
{
  char *data_name= share->data_file_name;
  char real_data_name[FN_REFLEN];
  ST_FILE_ID file_id= {0, 0};

  if (org_name)
  {
    fn_format(real_data_name,org_name,"",MI_NAME_DEXT,4);
    if (my_is_symlink(real_data_name, &file_id))
    {
      if (my_realpath(real_data_name, real_data_name, MYF(0)) ||
          (*myisam_test_invalid_symlink)(real_data_name) ||
          my_is_symlink(real_data_name, &file_id))
      {
        set_my_errno(HA_WRONG_CREATE_OPTION);
        return 1;
      }
      data_name= real_data_name;
    }
  }
  DEBUG_SYNC_C("before_opening_datafile");
  info->dfile= mysql_file_open(mi_key_file_dfile,
                               data_name, share->mode | O_SHARE | O_NOFOLLOW,
                               MYF(MY_WME));
  if (info->dfile < 0)
    return 1;
  if (org_name && !my_is_same_file(info->dfile, &file_id))
  {
    mysql_file_close(info->dfile, MYF(0));
    set_my_errno(HA_WRONG_CREATE_OPTION);
    return 1;
  }
  return 0;
}


int mi_open_keyfile(MYISAM_SHARE *share)
{
  if ((share->kfile= mysql_file_open(mi_key_file_kfile,
                                     share->unique_file_name,
                                     share->mode | O_SHARE,
                                     MYF(MY_WME))) < 0)
    return 1;
  return 0;
}


/*
  Disable all indexes.

  SYNOPSIS
    mi_disable_indexes()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Disable all indexes.

  RETURN
    0  ok
*/

int mi_disable_indexes(MI_INFO *info)
{
  MYISAM_SHARE *share= info->s;

  mi_clear_all_keys_active(share->state.key_map);
  return 0;
}


/*
  Enable all indexes

  SYNOPSIS
    mi_enable_indexes()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Enable all indexes. The indexes might have been disabled
    by mi_disable_index() before.
    The function works only if both data and indexes are empty,
    otherwise a repair is required.
    To be sure, call handler::delete_all_rows() before.

  RETURN
    0  ok
    HA_ERR_CRASHED data or index is non-empty.
*/

int mi_enable_indexes(MI_INFO *info)
{
  int error= 0;
  MYISAM_SHARE *share= info->s;

  if (share->state.state.data_file_length ||
      (share->state.state.key_file_length != share->base.keystart))
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    error= HA_ERR_CRASHED;
  }
  else
    mi_set_all_keys_active(share->state.key_map, share->base.keys);
  return error;
}


/*
  Test if indexes are disabled.

  SYNOPSIS
    mi_indexes_are_disabled()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Test if indexes are disabled.

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
    2  non-unique indexes are disabled
*/

int mi_indexes_are_disabled(MI_INFO *info)
{
  MYISAM_SHARE *share= info->s;

  /*
    No keys or all are enabled. keys is the number of keys. Left shifted
    gives us only one bit set. When decreased by one, gives us all all bits
    up to this one set and it gets unset.
  */
  if (!share->base.keys ||
      (mi_is_all_keys_active(share->state.key_map, share->base.keys)))
    return 0;

  /* All are disabled */
  if (mi_is_any_key_active(share->state.key_map))
    return 1;

  /*
    We have keys. Some enabled, some disabled.
    Don't check for any non-unique disabled but return directly 2
  */
  return 2;
}

