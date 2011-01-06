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

/* Describe, check and repair of MARIA tables */

/*
  About checksum calculation.

  There are two types of checksums. Table checksum and row checksum.

  Row checksum is an additional uchar at the end of dynamic length
  records. It must be calculated if the table is configured for them.
  Otherwise they must not be used. The variable
  MYISAM_SHARE::calc_checksum determines if row checksums are used.
  MI_INFO::checksum is used as temporary storage during row handling.
  For parallel repair we must assure that only one thread can use this
  variable. There is no problem on the write side as this is done by one
  thread only. But when checking a record after read this could go
  wrong. But since all threads read through a common read buffer, it is
  sufficient if only one thread checks it.

  Table checksum is an eight uchar value in the header of the index file.
  It can be calculated even if row checksums are not used. The variable
  MI_CHECK::glob_crc is calculated over all records.
  MI_SORT_PARAM::calc_checksum determines if this should be done. This
  variable is not part of MI_CHECK because it must be set per thread for
  parallel repair. The global glob_crc must be changed by one thread
  only. And it is sufficient to calculate the checksum once only.
*/

#include "ma_ftdefs.h"
#include "ma_rt_index.h"
#include "ma_blockrec.h"
#include "trnman.h"
#include "ma_key_recover.h"

#include <stdarg.h>
#include <my_getopt.h>
#ifdef HAVE_SYS_VADVISE_H
#include <sys/vadvise.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/* Functions defined in this file */

static int check_k_link(HA_CHECK *param, MARIA_HA *info, my_off_t next_link);
static int chk_index(HA_CHECK *param, MARIA_HA *info, MARIA_KEYDEF *keyinfo,
		     MARIA_PAGE *page, ha_rows *keys,
		     ha_checksum *key_checksum, uint level);
static uint isam_key_length(MARIA_HA *info,MARIA_KEYDEF *keyinfo);
static ha_checksum calc_checksum(ha_rows count);
static int writekeys(MARIA_SORT_PARAM *sort_param);
static int sort_one_index(HA_CHECK *param, MARIA_HA *info,
                          MARIA_KEYDEF *keyinfo,
			  my_off_t pagepos, File new_file);
static int sort_key_read(MARIA_SORT_PARAM *sort_param, uchar *key);
static int sort_maria_ft_key_read(MARIA_SORT_PARAM *sort_param, uchar *key);
static int sort_get_next_record(MARIA_SORT_PARAM *sort_param);
static int sort_key_cmp(MARIA_SORT_PARAM *sort_param, const void *a,
                        const void *b);
static int sort_maria_ft_key_write(MARIA_SORT_PARAM *sort_param,
                                   const uchar *a);
static int sort_key_write(MARIA_SORT_PARAM *sort_param, const uchar *a);
static my_off_t get_record_for_key(MARIA_KEYDEF *keyinfo, const uchar *key);
static int sort_insert_key(MARIA_SORT_PARAM  *sort_param,
                           reg1 SORT_KEY_BLOCKS *key_block,
			   const uchar *key, my_off_t prev_block);
static int sort_delete_record(MARIA_SORT_PARAM *sort_param);
/*static int _ma_flush_pending_blocks(HA_CHECK *param);*/
static SORT_KEY_BLOCKS	*alloc_key_blocks(HA_CHECK *param, uint blocks,
					  uint buffer_length);
static ha_checksum maria_byte_checksum(const uchar *buf, uint length);
static void set_data_file_type(MARIA_SORT_INFO *sort_info, MARIA_SHARE *share);
static void restore_data_file_type(MARIA_SHARE *share);
static void change_data_file_descriptor(MARIA_HA *info, File new_file);
static void unuse_data_file_descriptor(MARIA_HA *info);
static int _ma_safe_scan_block_record(MARIA_SORT_INFO *sort_info,
                                      MARIA_HA *info, uchar *record);
static void copy_data_file_state(MARIA_STATE_INFO *to,
                                 MARIA_STATE_INFO *from);
static void report_keypage_fault(HA_CHECK *param, MARIA_HA *info,
                                 my_off_t position);
static my_bool create_new_data_handle(MARIA_SORT_PARAM *param, File new_file);
static my_bool _ma_flush_table_files_before_swap(HA_CHECK *param,
                                                 MARIA_HA *info);
static TrID max_trid_in_system(void);
static void _ma_check_print_not_visible_error(HA_CHECK *param, TrID used_trid);
void retry_if_quick(MARIA_SORT_PARAM *param, int error);
static void print_bitmap_description(MARIA_SHARE *share,
                                     pgcache_page_no_t page,
                                     uchar *buff);


/* Initialize check param with default values */

void maria_chk_init(HA_CHECK *param)
{
  bzero((uchar*) param,sizeof(*param));
  param->opt_follow_links=1;
  param->keys_in_use= ~(ulonglong) 0;
  param->search_after_block=HA_OFFSET_ERROR;
  param->auto_increment_value= 0;
  param->use_buffers=USE_BUFFER_INIT;
  param->read_buffer_length=READ_BUFFER_INIT;
  param->write_buffer_length=READ_BUFFER_INIT;
  param->sort_buffer_length=SORT_BUFFER_INIT;
  param->sort_key_blocks=BUFFERS_WHEN_SORTING;
  param->tmpfile_createflag=O_RDWR | O_TRUNC | O_EXCL;
  param->myf_rw=MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL);
  param->start_check_pos=0;
  param->max_record_length= LONGLONG_MAX;
  param->pagecache_block_size= KEY_CACHE_BLOCK_SIZE;
  param->stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;
}


/* Initialize check param and maria handler for check of table */

void maria_chk_init_for_check(HA_CHECK *param, MARIA_HA *info)
{
  param->not_visible_rows_found= 0;
  param->max_found_trid= 0;

  /*
    Set up transaction handler so that we can see all rows. When rows is read
    we will check the found id against param->max_tried
  */
  if (param->max_trid == 0)
  {
    if (!ma_control_file_inited())
      param->max_trid= 0;      /* Give warning for first trid found */
    else
      param->max_trid= max_trid_in_system();
  }
  maria_ignore_trids(info);
}


	/* Check the status flags for the table */

int maria_chk_status(HA_CHECK *param, MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;

  if (maria_is_crashed_on_repair(info))
    _ma_check_print_warning(param,
			   "Table is marked as crashed and last repair failed");
  else if (maria_in_repair(info))
    _ma_check_print_warning(param,
                            "Last repair was aborted before finishing");
  else if (maria_is_crashed(info))
    _ma_check_print_warning(param,
			   "Table is marked as crashed");
  if (share->state.open_count != (uint) (share->global_changed ? 1 : 0))
  {
    /* Don't count this as a real warning, as check can correct this ! */
    uint save=param->warning_printed;
    _ma_check_print_warning(param,
			   share->state.open_count==1 ?
			   "%d client is using or hasn't closed the table properly" :
			   "%d clients are using or haven't closed the table properly",
			   share->state.open_count);
    /* If this will be fixed by the check, forget the warning */
    if (param->testflag & T_UPDATE_STATE)
      param->warning_printed=save;
  }
  return 0;
}

/*
  Check delete links in row data
*/

int maria_chk_del(HA_CHECK *param, register MARIA_HA *info,
                  ulonglong test_flag)
{
  MARIA_SHARE *share= info->s;
  reg2 ha_rows i;
  uint delete_link_length;
  my_off_t empty,next_link,old_link;
  char buff[22],buff2[22];
  DBUG_ENTER("maria_chk_del");

  LINT_INIT(old_link);

  param->record_checksum=0;

  if (share->data_file_type == BLOCK_RECORD)
    DBUG_RETURN(0);                             /* No delete links here */

  delete_link_length=((share->options & HA_OPTION_PACK_RECORD) ? 20 :
		      share->rec_reflength+1);

  if (!(test_flag & T_SILENT))
    puts("- check record delete-chain");

  next_link=share->state.dellink;
  if (share->state.state.del == 0)
  {
    if (test_flag & T_VERBOSE)
    {
      puts("No recordlinks");
    }
  }
  else
  {
    if (test_flag & T_VERBOSE)
      printf("Recordlinks:    ");
    empty=0;
    for (i= share->state.state.del ; i > 0L && next_link != HA_OFFSET_ERROR ; i--)
    {
      if (_ma_killed_ptr(param))
        DBUG_RETURN(1);
      if (test_flag & T_VERBOSE)
	printf(" %9s",llstr(next_link,buff));
      if (next_link >= share->state.state.data_file_length)
	goto wrong;
      if (my_pread(info->dfile.file, (uchar*) buff, delete_link_length,
		   next_link,MYF(MY_NABP)))
      {
	if (test_flag & T_VERBOSE) puts("");
	_ma_check_print_error(param,"Can't read delete-link at filepos: %s",
		    llstr(next_link,buff));
	DBUG_RETURN(1);
      }
      if (*buff != '\0')
      {
	if (test_flag & T_VERBOSE) puts("");
	_ma_check_print_error(param,"Record at pos: %s is not remove-marked",
		    llstr(next_link,buff));
	goto wrong;
      }
      if (share->options & HA_OPTION_PACK_RECORD)
      {
	my_off_t prev_link=mi_sizekorr(buff+12);
	if (empty && prev_link != old_link)
	{
	  if (test_flag & T_VERBOSE) puts("");
	  _ma_check_print_error(param,"Deleted block at %s doesn't point back at previous delete link",llstr(next_link,buff2));
	  goto wrong;
	}
	old_link=next_link;
	next_link=mi_sizekorr(buff+4);
	empty+=mi_uint3korr(buff+1);
      }
      else
      {
	param->record_checksum+=(ha_checksum) next_link;
	next_link= _ma_rec_pos(share, (uchar *) buff + 1);
	empty+=share->base.pack_reclength;
      }
    }
    if (share->state.state.del && (test_flag & T_VERBOSE))
      puts("\n");
    if (empty != share->state.state.empty)
    {
      _ma_check_print_warning(param,
			     "Found %s deleted space in delete link chain. Should be %s",
			     llstr(empty,buff2),
			     llstr(share->state.state.empty,buff));
    }
    if (next_link != HA_OFFSET_ERROR)
    {
      _ma_check_print_error(param,
			   "Found more than the expected %s deleted rows in delete link chain",
			   llstr(share->state.state.del, buff));
      goto wrong;
    }
    if (i != 0)
    {
      _ma_check_print_error(param,
			   "Found %s deleted rows in delete link chain. Should be %s",
			   llstr(share->state.state.del - i, buff2),
			   llstr(share->state.state.del, buff));
      goto wrong;
    }
  }
  DBUG_RETURN(0);

wrong:
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  if (test_flag & T_VERBOSE)
    puts("");
  _ma_check_print_error(param,"record delete-link-chain corrupted");
  DBUG_RETURN(1);
} /* maria_chk_del */


/* Check delete links in index file */

static int check_k_link(HA_CHECK *param, register MARIA_HA *info,
                        my_off_t next_link)
{
  MARIA_SHARE *share= info->s;
  uint block_size= share->block_size;
  ha_rows records;
  char llbuff[21], llbuff2[21];
  uchar *buff;
  DBUG_ENTER("check_k_link");

  if (next_link == HA_OFFSET_ERROR)
    DBUG_RETURN(0);                             /* Avoid printing empty line */

  records= (ha_rows) (share->state.state.key_file_length / block_size);
  while (next_link != HA_OFFSET_ERROR && records > 0)
  {
    if (_ma_killed_ptr(param))
      DBUG_RETURN(1);
    if (param->testflag & T_VERBOSE)
      printf("%16s",llstr(next_link,llbuff));

    /* Key blocks must lay within the key file length entirely. */
    if (next_link + block_size > share->state.state.key_file_length)
    {
      /* purecov: begin tested */
      _ma_check_print_error(param, "Invalid key block position: %s  "
                            "key block size: %u  file_length: %s",
                            llstr(next_link, llbuff), block_size,
                            llstr(share->state.state.key_file_length, llbuff2));
      DBUG_RETURN(1);
      /* purecov: end */
    }

    /* Key blocks must be aligned at block_size */
    if (next_link & (block_size -1))
    {
      /* purecov: begin tested */
      _ma_check_print_error(param, "Mis-aligned key block: %s  "
                            "minimum key block length: %u",
                            llstr(next_link, llbuff),
                            block_size);
      DBUG_RETURN(1);
      /* purecov: end */
    }

    DBUG_ASSERT(share->pagecache->block_size == block_size);
    if (!(buff= pagecache_read(share->pagecache,
                               &share->kfile,
                               (pgcache_page_no_t) (next_link / block_size),
                               DFLT_INIT_HITS,
                               info->buff, PAGECACHE_READ_UNKNOWN_PAGE,
                               PAGECACHE_LOCK_LEFT_UNLOCKED, 0)))
    {
      /* purecov: begin tested */
      _ma_check_print_error(param, "key cache read error for block: %s",
                            llstr(next_link,llbuff));
      DBUG_RETURN(1);
      /* purecov: end */
    }
    if (_ma_get_keynr(info->s, buff) != MARIA_DELETE_KEY_NR)
      _ma_check_print_error(param, "Page at %s is not delete marked",
                            llstr(next_link, llbuff));

    next_link= mi_sizekorr(buff + share->keypage_header);
    records--;
    param->key_file_blocks+=block_size;
  }
  if (param->testflag & T_VERBOSE)
  {
    if (next_link != HA_OFFSET_ERROR)
      printf("%16s\n",llstr(next_link,llbuff));
    else
      puts("");
  }
  DBUG_RETURN (next_link != HA_OFFSET_ERROR);
} /* check_k_link */


	/* Check sizes of files */

int maria_chk_size(HA_CHECK *param, register MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  int error;
  register my_off_t skr,size;
  char buff[22],buff2[22];
  DBUG_ENTER("maria_chk_size");

  if (!(param->testflag & T_SILENT))
    puts("- check file-size");

  /*
    The following is needed if called externally (not from maria_chk).
    To get a correct physical size we need to flush them.
  */
  if ((error= _ma_flush_table_files(info,
                                    MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                    FLUSH_FORCE_WRITE, FLUSH_FORCE_WRITE)))
    _ma_check_print_error(param, "Failed to flush data or index file");

  size= my_seek(share->kfile.file, 0L, MY_SEEK_END, MYF(MY_THREADSAFE));
  if ((skr=(my_off_t) share->state.state.key_file_length) != size)
  {
    /* Don't give error if file generated by mariapack */
    if (skr > size && maria_is_any_key_active(share->state.key_map))
    {
      error=1;
      _ma_check_print_error(param,
			   "Size of indexfile is: %-8s        Should be: %s",
			   llstr(size,buff), llstr(skr,buff2));
    }
    else if (!(param->testflag & T_VERY_SILENT))
      _ma_check_print_warning(param,
			     "Size of indexfile is: %-8s      Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      ! (share->options & HA_OPTION_COMPRESS_RECORD) &&
      ulonglong2double(share->state.state.key_file_length) >
      ulonglong2double(share->base.margin_key_file_length)*0.9)
    _ma_check_print_warning(param,"Keyfile is almost full, %10s of %10s used",
			   llstr(share->state.state.key_file_length,buff),
			   llstr(share->base.max_key_file_length-1,buff));

  size= my_seek(info->dfile.file, 0L, MY_SEEK_END, MYF(0));
  skr=(my_off_t) share->state.state.data_file_length;
  if (share->options & HA_OPTION_COMPRESS_RECORD)
    skr+= MEMMAP_EXTRA_MARGIN;
#ifdef USE_RELOC
  if (share->data_file_type == STATIC_RECORD &&
      skr < (my_off_t) share->base.reloc*share->base.min_pack_length)
    skr=(my_off_t) share->base.reloc*share->base.min_pack_length;
#endif
  if (skr != size)
  {
    if (skr > size && skr != size + MEMMAP_EXTRA_MARGIN)
    {
      share->state.state.data_file_length=size;	/* Skip other errors */
      error=1;
      _ma_check_print_error(param,"Size of datafile is: %-9s         Should be: %s",
		    llstr(size,buff), llstr(skr,buff2));
      param->testflag|=T_RETRY_WITHOUT_QUICK;
    }
    else
    {
      _ma_check_print_warning(param,
			     "Size of datafile is: %-9s       Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
    }
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      !(share->options & HA_OPTION_COMPRESS_RECORD) &&
      ulonglong2double(share->state.state.data_file_length) >
      (ulonglong2double(share->base.max_data_file_length)*0.9))
    _ma_check_print_warning(param, "Datafile is almost full, %10s of %10s used",
			   llstr(share->state.state.data_file_length,buff),
			   llstr(share->base.max_data_file_length-1,buff2));
  DBUG_RETURN(error);
} /* maria_chk_size */


/* Check keys */

int maria_chk_key(HA_CHECK *param, register MARIA_HA *info)
{
  uint key,found_keys=0,full_text_keys=0,result=0;
  ha_rows keys;
  ha_checksum old_record_checksum,init_checksum;
  my_off_t all_keydata,all_totaldata,key_totlength,length;
  double  *rec_per_key_part;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  char buff[22],buff2[22];
  MARIA_PAGE page;
  DBUG_ENTER("maria_chk_key");

  if (!(param->testflag & T_SILENT))
    puts("- check key delete-chain");

  param->key_file_blocks=share->base.keystart;
  if (check_k_link(param, info, share->state.key_del))
  {
    if (param->testflag & T_VERBOSE) puts("");
    _ma_check_print_error(param,"key delete-link-chain corrupted");
    DBUG_RETURN(-1);
  }

  if (!(param->testflag & T_SILENT))
    puts("- check index reference");

  all_keydata=all_totaldata=key_totlength=0;
  init_checksum=param->record_checksum;
  old_record_checksum=0;
  if (share->data_file_type == STATIC_RECORD)
    old_record_checksum= (calc_checksum(share->state.state.records +
                                        share->state.state.del-1) *
                          share->base.pack_reclength);
  rec_per_key_part= param->new_rec_per_key_part;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       rec_per_key_part+=keyinfo->keysegs, key++, keyinfo++)
  {
    param->key_crc[key]=0;
    if (! maria_is_key_active(share->state.key_map, key))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part +
		      (uint) (rec_per_key_part - param->new_rec_per_key_part)),
	     keyinfo->keysegs*sizeof(*rec_per_key_part));
      continue;
    }
    found_keys++;

    param->record_checksum=init_checksum;

    bzero((char*) &param->unique_count,sizeof(param->unique_count));
    bzero((char*) &param->notnull_count,sizeof(param->notnull_count));

    if ((!(param->testflag & T_SILENT)))
      printf ("- check data record references index: %d\n",key+1);
    if (keyinfo->flag & (HA_FULLTEXT | HA_SPATIAL))
      full_text_keys++;
    if (share->state.key_root[key] == HA_OFFSET_ERROR)
    {
      if (share->state.state.records != 0 && !(keyinfo->flag & HA_FULLTEXT))
        _ma_check_print_error(param, "Key tree %u is empty", key + 1);
      goto do_stat;
    }
    if (_ma_fetch_keypage(&page, info, keyinfo, share->state.key_root[key],
                          PAGECACHE_LOCK_LEFT_UNLOCKED, DFLT_INIT_HITS,
                          info->buff, 0))
    {
      report_keypage_fault(param, info, share->state.key_root[key]);
      if (!(param->testflag & T_INFO))
	DBUG_RETURN(-1);
      result= -1;
      continue;
    }
    param->key_file_blocks+=keyinfo->block_length;
    keys=0;
    param->keydata=param->totaldata=0;
    param->key_blocks=0;
    param->max_level=0;
    if (chk_index(param, info,keyinfo, &page, &keys, param->key_crc+key,1))
      DBUG_RETURN(-1);
    if (!(keyinfo->flag & (HA_FULLTEXT | HA_SPATIAL | HA_RTREE_INDEX)))
    {
      if (keys != share->state.state.records)
      {
	_ma_check_print_error(param,"Found %s keys of %s",llstr(keys,buff),
		    llstr(share->state.state.records,buff2));
	if (!(param->testflag & T_INFO))
	DBUG_RETURN(-1);
	result= -1;
	continue;
      }
      if ((found_keys - full_text_keys == 1 &&
           !(share->data_file_type == STATIC_RECORD)) ||
          (param->testflag & T_DONT_CHECK_CHECKSUM))
	old_record_checksum= param->record_checksum;
      else if (old_record_checksum != param->record_checksum)
      {
	if (key)
	  _ma_check_print_error(param,
                                "Key %u doesn't point at same records as "
                                "key 1",
		      key+1);
	else
	  _ma_check_print_error(param,"Key 1 doesn't point at all records");
	if (!(param->testflag & T_INFO))
	  DBUG_RETURN(-1);
	result= -1;
	continue;
      }
    }
    if ((uint) share->base.auto_key -1 == key)
    {
      /* Check that auto_increment key is bigger than max key value */
      ulonglong auto_increment;
      const HA_KEYSEG *keyseg= share->keyinfo[share->base.auto_key-1].seg;
      info->lastinx=key;
      _ma_read_key_record(info, info->rec_buff, 0);
      auto_increment=
        ma_retrieve_auto_increment(info->rec_buff + keyseg->start,
                                   keyseg->type);
      if (auto_increment > share->state.auto_increment)
      {
	_ma_check_print_warning(param, "Auto-increment value: %s is smaller "
                                "than max used value: %s",
                                llstr(share->state.auto_increment,buff2),
                                llstr(auto_increment, buff));
      }
      if (param->testflag & T_AUTO_INC)
      {
        set_if_bigger(share->state.auto_increment,
                      auto_increment);
        set_if_bigger(share->state.auto_increment,
                      param->auto_increment_value);
      }

      /* Check that there isn't a row with auto_increment = 0 in the table */
      maria_extra(info,HA_EXTRA_KEYREAD,0);
      bzero(info->lastkey_buff, keyinfo->seg->length);
      if (!maria_rkey(info, info->rec_buff, key,
                      info->lastkey_buff,
                      (key_part_map) 1, HA_READ_KEY_EXACT))
      {
	/* Don't count this as a real warning, as maria_chk can't correct it */
	uint save=param->warning_printed;
	_ma_check_print_warning(param, "Found row where the auto_increment "
                                "column has the value 0");
	param->warning_printed=save;
      }
      maria_extra(info,HA_EXTRA_NO_KEYREAD,0);
    }

    length=(my_off_t) isam_key_length(info,keyinfo)*keys + param->key_blocks*2;
    if (param->testflag & T_INFO && param->totaldata != 0L && keys != 0L)
      printf("Key: %2d:  Keyblocks used: %3d%%  Packed: %4d%%  Max levels: %2d\n",
	     key+1,
	     (int) (my_off_t2double(param->keydata)*100.0/my_off_t2double(param->totaldata)),
	     (int) ((my_off_t2double(length) - my_off_t2double(param->keydata))*100.0/
		    my_off_t2double(length)),
	     param->max_level);
    all_keydata+=param->keydata; all_totaldata+=param->totaldata; key_totlength+=length;

do_stat:
    if (param->testflag & T_STATISTICS)
      maria_update_key_parts(keyinfo, rec_per_key_part, param->unique_count,
                       param->stats_method == MI_STATS_METHOD_IGNORE_NULLS?
                       param->notnull_count: NULL,
                       (ulonglong)share->state.state.records);
  }
  if (param->testflag & T_INFO)
  {
    if (all_totaldata != 0L && found_keys > 0)
      printf("Total:    Keyblocks used: %3d%%  Packed: %4d%%\n\n",
	     (int) (my_off_t2double(all_keydata)*100.0/
		    my_off_t2double(all_totaldata)),
	     (int) ((my_off_t2double(key_totlength) -
		     my_off_t2double(all_keydata))*100.0/
		     my_off_t2double(key_totlength)));
    else if (all_totaldata != 0L && maria_is_any_key_active(share->state.key_map))
      puts("");
  }
  if (param->key_file_blocks != share->state.state.key_file_length &&
      share->state.key_map == ~(ulonglong) 0)
    _ma_check_print_warning(param, "Some data are unreferenced in keyfile");
  if (found_keys != full_text_keys)
    param->record_checksum=old_record_checksum-init_checksum;	/* Remove delete links */
  else
    param->record_checksum=0;
  DBUG_RETURN(result);
} /* maria_chk_key */



static int chk_index_down(HA_CHECK *param, MARIA_HA *info,
                          MARIA_KEYDEF *keyinfo,
                          my_off_t page, uchar *buff, ha_rows *keys,
                          ha_checksum *key_checksum, uint level)
{
  char llbuff[22],llbuff2[22];
  MARIA_SHARE *share= info->s;
  MARIA_PAGE ma_page;
  DBUG_ENTER("chk_index_down");

  /* Key blocks must lay within the key file length entirely. */
  if (page + keyinfo->block_length > share->state.state.key_file_length)
  {
    /* purecov: begin tested */
    /* Give it a chance to fit in the real file size. */
    my_off_t max_length= my_seek(info->s->kfile.file, 0L, MY_SEEK_END,
                                 MYF(MY_THREADSAFE));
    _ma_check_print_error(param, "Invalid key block position: %s  "
                          "key block size: %u  file_length: %s",
                          llstr(page, llbuff), keyinfo->block_length,
                          llstr(share->state.state.key_file_length, llbuff2));
    if (page + keyinfo->block_length > max_length)
      goto err;
    /* Fix the remembered key file length. */
    share->state.state.key_file_length= (max_length &
                                          ~ (my_off_t) (keyinfo->block_length -
                                                        1));
    /* purecov: end */
  }

  /* Key blocks must be aligned at block length */
  if (page & (info->s->block_size -1))
  {
    /* purecov: begin tested */
    _ma_check_print_error(param, "Mis-aligned key block: %s  "
                          "key block length: %u",
                          llstr(page, llbuff), info->s->block_size);
    goto err;
    /* purecov: end */
  }

  if (_ma_fetch_keypage(&ma_page, info, keyinfo, page,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        DFLT_INIT_HITS, buff, 0))
  {
    report_keypage_fault(param, info, page);
    goto err;
  }
  param->key_file_blocks+=keyinfo->block_length;
  if (chk_index(param, info, keyinfo, &ma_page, keys, key_checksum,level))
    goto err;

  DBUG_RETURN(0);

  /* purecov: begin tested */
err:
  DBUG_RETURN(1);
  /* purecov: end */
}


/*
  "Ignore NULLs" statistics collection method: process first index tuple.

  SYNOPSIS
    maria_collect_stats_nonulls_first()
      keyseg   IN     Array of key part descriptions
      notnull  INOUT  Array, notnull[i] = (number of {keypart1...keypart_i}
                                           tuples that don't contain NULLs)
      key      IN     Key values tuple

  DESCRIPTION
    Process the first index tuple - find out which prefix tuples don't
    contain NULLs, and update the array of notnull counters accordingly.
*/

static
void maria_collect_stats_nonulls_first(HA_KEYSEG *keyseg, ulonglong *notnull,
                                       const uchar *key)
{
  uint first_null, kp;
  first_null= ha_find_null(keyseg, key) - keyseg;
  /*
    All prefix tuples that don't include keypart_{first_null} are not-null
    tuples (and all others aren't), increment counters for them.
  */
  for (kp= 0; kp < first_null; kp++)
    notnull[kp]++;
}


/*
  "Ignore NULLs" statistics collection method: process next index tuple.

  SYNOPSIS
    maria_collect_stats_nonulls_next()
      keyseg   IN     Array of key part descriptions
      notnull  INOUT  Array, notnull[i] = (number of {keypart1...keypart_i}
                                           tuples that don't contain NULLs)
      prev_key IN     Previous key values tuple
      last_key IN     Next key values tuple

  DESCRIPTION
    Process the next index tuple:
    1. Find out which prefix tuples of last_key don't contain NULLs, and
       update the array of notnull counters accordingly.
    2. Find the first keypart number where the prev_key and last_key tuples
       are different(A), or last_key has NULL value(B), and return it, so the
       caller can count number of unique tuples for each key prefix. We don't
       need (B) to be counted, and that is compensated back in
       maria_update_key_parts().

  RETURN
    1 + number of first keypart where values differ or last_key tuple has NULL
*/

static
int maria_collect_stats_nonulls_next(HA_KEYSEG *keyseg, ulonglong *notnull,
                                     const uchar *prev_key,
                                     const uchar *last_key)
{
  uint diffs[2];
  uint first_null_seg, kp;
  HA_KEYSEG *seg;

  /*
     Find the first keypart where values are different or either of them is
     NULL. We get results in diffs array:
     diffs[0]= 1 + number of first different keypart
     diffs[1]=offset: (last_key + diffs[1]) points to first value in
                      last_key that is NULL or different from corresponding
                      value in prev_key.
  */
  ha_key_cmp(keyseg, prev_key, last_key, USE_WHOLE_KEY,
             SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL, diffs);
  seg= keyseg + diffs[0] - 1;

  /* Find first NULL in last_key */
  first_null_seg= ha_find_null(seg, last_key + diffs[1]) - keyseg;
  for (kp= 0; kp < first_null_seg; kp++)
    notnull[kp]++;

  /*
    Return 1+ number of first key part where values differ. Don't care if
    these were NULLs and not .... We compensate for that in
    maria_update_key_parts.
  */
  return diffs[0];
}


/* Check if index is ok */

static int chk_index(HA_CHECK *param, MARIA_HA *info, MARIA_KEYDEF *keyinfo,
		     MARIA_PAGE *anc_page, ha_rows *keys,
		     ha_checksum *key_checksum, uint level)
{
  int flag;
  uint comp_flag, page_flag, nod_flag;
  uchar *temp_buff, *keypos, *old_keypos, *endpos;
  my_off_t next_page,record;
  MARIA_SHARE *share= info->s;
  char llbuff[22];
  uint diff_pos[2];
  uchar tmp_key_buff[MARIA_MAX_KEY_BUFF];
  MARIA_KEY tmp_key;
  DBUG_ENTER("chk_index");
  DBUG_DUMP("buff", anc_page->buff, anc_page->size);

  /* TODO: implement appropriate check for RTree keys */
  if (keyinfo->flag & (HA_SPATIAL | HA_RTREE_INDEX))
    DBUG_RETURN(0);

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    _ma_check_print_error(param,"Not enough memory for keyblock");
    DBUG_RETURN(-1);
  }

  if (keyinfo->flag & HA_NOSAME)
  {
    /* Not real duplicates */
    comp_flag=SEARCH_FIND | SEARCH_UPDATE | SEARCH_INSERT;
  }
  else
    comp_flag=SEARCH_SAME;			/* Keys in positionorder */

  page_flag=  anc_page->flag;
  nod_flag=   anc_page->node;
  old_keypos= anc_page->buff + share->keypage_header;
  keypos=     old_keypos + nod_flag;
  endpos=     anc_page->buff + anc_page->size;

  param->keydata+=   anc_page->size;
  param->totaldata+= keyinfo->block_length;	/* INFO */
  param->key_blocks++;
  if (level > param->max_level)
    param->max_level=level;

  if (_ma_get_keynr(share, anc_page->buff) !=
      (uint) (keyinfo - share->keyinfo))
    _ma_check_print_error(param, "Page at %s is not marked for index %u",
                          llstr(anc_page->pos, llbuff),
                          (uint) (keyinfo - share->keyinfo));
  if ((page_flag & KEYPAGE_FLAG_HAS_TRANSID) &&
      !share->base.born_transactional)
  {
    _ma_check_print_error(param,
                          "Page at %s is marked with HAS_TRANSID even if "
                          "table is not transactional",
                          llstr(anc_page->pos, llbuff));
  }

  if (anc_page->size > share->max_index_block_size)
  {
    _ma_check_print_error(param,
                          "Page at %s has impossible (too big) pagelength",
                          llstr(anc_page->pos, llbuff));
    goto err;
  }

  info->last_key.keyinfo= tmp_key.keyinfo= keyinfo;
  tmp_key.data= tmp_key_buff;
  for ( ;; )
  {
    if (nod_flag)
    {
      if (_ma_killed_ptr(param))
        goto err;
      next_page= _ma_kpos(nod_flag,keypos);
      if (chk_index_down(param,info,keyinfo,next_page,
                         temp_buff,keys,key_checksum,level+1))
      {
        DBUG_DUMP("page_data", old_keypos, (uint) (keypos - old_keypos));
	goto err;
      }
    }
    old_keypos=keypos;
    if (keypos >= endpos ||
	!(*keyinfo->get_key)(&tmp_key, page_flag, nod_flag, &keypos))
      break;
    if (keypos > endpos)
    {
      _ma_check_print_error(param,
                            "Page length and length of keys don't match at "
                            "page: %s",
                            llstr(anc_page->pos,llbuff));
      goto err;
    }
    if (share->data_file_type == BLOCK_RECORD &&
        !(page_flag & KEYPAGE_FLAG_HAS_TRANSID) &&
        key_has_transid(tmp_key.data + tmp_key.data_length +
                        share->rec_reflength-1))
    {
      _ma_check_print_error(param,
                            "Found key marked for transid on page that is not "
                            "marked for transid at: %s",
                            llstr(anc_page->pos,llbuff));
      goto err;
    }

    if ((*keys)++ &&
	(flag=ha_key_cmp(keyinfo->seg, info->last_key.data, tmp_key.data,
                         tmp_key.data_length + tmp_key.ref_length,
                         (comp_flag | SEARCH_INSERT | (tmp_key.flag >> 1) |
                          info->last_key.flag), diff_pos)) >=0)
    {
      DBUG_DUMP_KEY("old", &info->last_key);
      DBUG_DUMP_KEY("new", &tmp_key);
      DBUG_DUMP("new_in_page", old_keypos, (uint) (keypos-old_keypos));

      if ((comp_flag & SEARCH_FIND) && flag == 0)
	_ma_check_print_error(param,"Found duplicated key at page %s",
                              llstr(anc_page->pos,llbuff));
      else
	_ma_check_print_error(param,"Key in wrong position at page %s",
                              llstr(anc_page->pos,llbuff));
      goto err;
    }

    if (param->testflag & T_STATISTICS)
    {
      if (*keys != 1L)				/* not first_key */
      {
        if (param->stats_method == MI_STATS_METHOD_NULLS_NOT_EQUAL)
          ha_key_cmp(keyinfo->seg, info->last_key.data,
                     tmp_key.data, tmp_key.data_length,
                     SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL,
                     diff_pos);
        else if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
        {
          diff_pos[0]= maria_collect_stats_nonulls_next(keyinfo->seg,
                                                        param->notnull_count,
                                                        info->last_key.data,
                                                        tmp_key.data);
        }
	param->unique_count[diff_pos[0]-1]++;
      }
      else
      {
        if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
          maria_collect_stats_nonulls_first(keyinfo->seg, param->notnull_count,
                                            tmp_key.data);
      }
    }
    _ma_copy_key(&info->last_key, &tmp_key);
    (*key_checksum)+= maria_byte_checksum(tmp_key.data, tmp_key.data_length);
    record= _ma_row_pos_from_key(&tmp_key);

    if (keyinfo->flag & HA_FULLTEXT) /* special handling for ft2 */
    {
      uint off;
      int  subkeys;
      get_key_full_length_rdonly(off, tmp_key.data);
      subkeys= ft_sintXkorr(tmp_key.data + off);
      if (subkeys < 0)
      {
        ha_rows tmp_keys=0;
        if (chk_index_down(param,info,&share->ft2_keyinfo,record,
                           temp_buff,&tmp_keys,key_checksum,1))
          goto err;
        if (tmp_keys + subkeys)
        {
          _ma_check_print_error(param,
                               "Number of words in the 2nd level tree "
                               "does not match the number in the header. "
                               "Parent word in on the page %s, offset %u",
                               llstr(anc_page->pos,llbuff),
                                (uint) (old_keypos - anc_page->buff));
          goto err;
        }
        (*keys)+=tmp_keys-1;
        continue;
      }
      /* fall through */
    }
    if ((share->data_file_type != BLOCK_RECORD &&
         record >= share->state.state.data_file_length) ||
        (share->data_file_type == BLOCK_RECORD &&
         ma_recordpos_to_page(record) * share->base.min_block_length >=
         share->state.state.data_file_length))
    {
#ifndef DBUG_OFF
      char llbuff2[22], llbuff3[22];
#endif
      _ma_check_print_error(param,
                            "Found key at page %s that points to record "
                            "outside datafile",
                            llstr(anc_page->pos,llbuff));
      DBUG_PRINT("test",("page: %s  record: %s  filelength: %s",
			 llstr(anc_page->pos,llbuff),llstr(record,llbuff2),
			 llstr(share->state.state.data_file_length,llbuff3)));
      DBUG_DUMP_KEY("key", &tmp_key);
      DBUG_DUMP("new_in_page", old_keypos, (uint) (keypos-old_keypos));
      goto err;
    }
    param->record_checksum+= (ha_checksum) record;
  }
  if (keypos != endpos)
  {
    _ma_check_print_error(param,
                          "Keyblock size at page %s is not correct. "
                          "Block length: %u  key length: %u",
                          llstr(anc_page->pos, llbuff), anc_page->size,
                          (uint) (keypos - anc_page->buff));
    goto err;
  }
  my_afree(temp_buff);
  DBUG_RETURN(0);
 err:
  my_afree(temp_buff);
  DBUG_RETURN(1);
} /* chk_index */


	/* Calculate a checksum of 1+2+3+4...N = N*(N+1)/2 without overflow */

static ha_checksum calc_checksum(ha_rows count)
{
  ulonglong sum,a,b;
  DBUG_ENTER("calc_checksum");

  sum=0;
  a=count; b=count+1;
  if (a & 1)
    b>>=1;
  else
    a>>=1;
  while (b)
  {
    if (b & 1)
      sum+=a;
    a<<=1; b>>=1;
  }
  DBUG_PRINT("exit",("sum: %lx",(ulong) sum));
  DBUG_RETURN((ha_checksum) sum);
} /* calc_checksum */


	/* Calc length of key in normal isam */

static uint isam_key_length(MARIA_HA *info, register MARIA_KEYDEF *keyinfo)
{
  uint length;
  HA_KEYSEG *keyseg;
  DBUG_ENTER("isam_key_length");

  length= info->s->rec_reflength;
  for (keyseg=keyinfo->seg ; keyseg->type ; keyseg++)
    length+= keyseg->length;

  DBUG_PRINT("exit",("length: %d",length));
  DBUG_RETURN(length);
} /* key_length */



static void record_pos_to_txt(MARIA_HA *info, my_off_t recpos,
                              char *buff)
{
  if (info->s->data_file_type != BLOCK_RECORD)
    llstr(recpos, buff);
  else
  {
    my_off_t page= ma_recordpos_to_page(recpos);
    uint row= ma_recordpos_to_dir_entry(recpos);
    char *end= longlong10_to_str(page, buff, 10);
    *(end++)= ':';
    longlong10_to_str(row, end, 10);
  }
}


/*
  Check that keys in records exist in index tree

  SYNOPSIS
  check_keys_in_record()
  param		Check paramenter
  info		Maria handler
  extend	Type of check (extended or normal)
  start_recpos	Position to row
  record	Record buffer

  NOTES
    This function also calculates record checksum & number of rows
*/

static int check_keys_in_record(HA_CHECK *param, MARIA_HA *info, int extend,
                                my_off_t start_recpos, uchar *record)
{
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  char llbuff[22+4];
  uint keynr;

  param->tmp_record_checksum+= (ha_checksum) start_recpos;
  param->records++;
  if (param->testflag & T_WRITE_LOOP && param->records % WRITE_COUNT == 0)
  {
    printf("%s\r", llstr(param->records, llbuff));
    VOID(fflush(stdout));
  }

  /* Check if keys match the record */
  for (keynr=0, keyinfo= share->keyinfo; keynr < share->base.keys;
       keynr++, keyinfo++)
  {
    if (maria_is_key_active(share->state.key_map, keynr))
    {
      MARIA_KEY key;
      if (!(keyinfo->flag & HA_FULLTEXT))
      {
        (*keyinfo->make_key)(info, &key, keynr, info->lastkey_buff, record,
                             start_recpos, 0);
        if (extend)
        {
          /* We don't need to lock the key tree here as we don't allow
             concurrent threads when running maria_chk
          */
          int search_result=
#ifdef HAVE_RTREE_KEYS
            (keyinfo->flag & (HA_SPATIAL | HA_RTREE_INDEX)) ?
            maria_rtree_find_first(info, &key, MBR_EQUAL | MBR_DATA) :
#endif
            _ma_search(info, &key, SEARCH_SAME, share->state.key_root[keynr]);
          if (search_result)
          {
            record_pos_to_txt(info, start_recpos, llbuff);
            _ma_check_print_error(param,
                                  "Record at: %14s  "
                                  "Can't find key for index: %2d",
                                  llbuff, keynr+1);
            if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
              return -1;
          }
        }
        else
          param->tmp_key_crc[keynr]+=
            maria_byte_checksum(key.data, key.data_length);
      }
    }
  }
  return 0;
}


/*
  Functions to loop through all rows and check if they are ok

  NOTES
    One function for each record format

  RESULT
    0  ok
    -1 Interrupted by user
    1  Error
*/

static int check_static_record(HA_CHECK *param, MARIA_HA *info, int extend,
                               uchar *record)
{
  MARIA_SHARE *share= info->s;
  my_off_t start_recpos, pos;
  char llbuff[22];

  pos= 0;
  while (pos < share->state.state.data_file_length)
  {
    if (_ma_killed_ptr(param))
      return -1;
    if (my_b_read(&param->read_cache, record,
                  share->base.pack_reclength))
    {
      _ma_check_print_error(param,
                            "got error: %d when reading datafile at position: "
                            "%s",
                            my_errno, llstr(pos, llbuff));
      return 1;
    }
    start_recpos= pos;
    pos+= share->base.pack_reclength;
    param->splits++;
    if (*record == '\0')
    {
      param->del_blocks++;
      param->del_length+= share->base.pack_reclength;
      continue;					/* Record removed */
    }
    param->glob_crc+= _ma_static_checksum(info,record);
    param->used+= share->base.pack_reclength;
    if (check_keys_in_record(param, info, extend, start_recpos, record))
      return 1;
  }
  return 0;
}


static int check_dynamic_record(HA_CHECK *param, MARIA_HA *info, int extend,
                                uchar *record)
{
  MARIA_BLOCK_INFO block_info;
  MARIA_SHARE *share= info->s;
  my_off_t start_recpos, start_block, pos;
  uchar *to;
  ulong left_length;
  uint	b_type;
  char llbuff[22],llbuff2[22],llbuff3[22];
  DBUG_ENTER("check_dynamic_record");

  LINT_INIT(left_length);
  LINT_INIT(start_recpos);
  LINT_INIT(to);

  pos= 0;
  while (pos < share->state.state.data_file_length)
  {
    my_bool got_error= 0;
    int flag;
    if (_ma_killed_ptr(param))
      DBUG_RETURN(-1);

    flag= block_info.second_read=0;
    block_info.next_filepos=pos;
    do
    {
      if (_ma_read_cache(&param->read_cache, block_info.header,
                         (start_block=block_info.next_filepos),
                         sizeof(block_info.header),
                         (flag ? 0 : READING_NEXT) | READING_HEADER))
      {
        _ma_check_print_error(param,
                              "got error: %d when reading datafile at "
                              "position: %s",
                              my_errno, llstr(start_block, llbuff));
        DBUG_RETURN(1);
      }

      if (start_block & (MARIA_DYN_ALIGN_SIZE-1))
      {
        _ma_check_print_error(param,"Wrong aligned block at %s",
                              llstr(start_block,llbuff));
        DBUG_RETURN(1);
      }
      b_type= _ma_get_block_info(&block_info,-1,start_block);
      if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
                    BLOCK_FATAL_ERROR))
      {
        if (b_type & BLOCK_SYNC_ERROR)
        {
          if (flag)
          {
            _ma_check_print_error(param,"Unexpected byte: %d at link: %s",
                                  (int) block_info.header[0],
                                  llstr(start_block,llbuff));
            DBUG_RETURN(1);
          }
          pos=block_info.filepos+block_info.block_len;
          goto next;
        }
        if (b_type & BLOCK_DELETED)
        {
          if (block_info.block_len < share->base.min_block_length)
          {
            _ma_check_print_error(param,
                                  "Deleted block with impossible length %lu "
                                  "at %s",
                                  block_info.block_len,llstr(pos,llbuff));
            DBUG_RETURN(1);
          }
          if ((block_info.next_filepos != HA_OFFSET_ERROR &&
               block_info.next_filepos >= share->state.state.data_file_length) ||
              (block_info.prev_filepos != HA_OFFSET_ERROR &&
               block_info.prev_filepos >= share->state.state.data_file_length))
          {
            _ma_check_print_error(param,"Delete link points outside datafile "
                                  "at %s",
                                  llstr(pos,llbuff));
            DBUG_RETURN(1);
          }
          param->del_blocks++;
          param->del_length+= block_info.block_len;
          param->splits++;
          pos= block_info.filepos+block_info.block_len;
          goto next;
        }
        _ma_check_print_error(param,"Wrong bytesec: %d-%d-%d at linkstart: %s",
                              block_info.header[0],block_info.header[1],
                              block_info.header[2],
                              llstr(start_block,llbuff));
        DBUG_RETURN(1);
      }
      if (share->state.state.data_file_length < block_info.filepos+
          block_info.block_len)
      {
        _ma_check_print_error(param,
                              "Recordlink that points outside datafile at %s",
                              llstr(pos,llbuff));
        got_error=1;
        break;
      }
      param->splits++;
      if (!flag++)				/* First block */
      {
        start_recpos=pos;
        pos=block_info.filepos+block_info.block_len;
        if (block_info.rec_len > (uint) share->base.max_pack_length)
        {
          _ma_check_print_error(param,"Found too long record (%lu) at %s",
                                (ulong) block_info.rec_len,
                                llstr(start_recpos,llbuff));
          got_error=1;
          break;
        }
        if (share->base.blobs)
        {
          if (_ma_alloc_buffer(&info->rec_buff, &info->rec_buff_size,
                               block_info.rec_len +
                               share->base.extra_rec_buff_size))

          {
            _ma_check_print_error(param,
                                  "Not enough memory (%lu) for blob at %s",
                                  (ulong) block_info.rec_len,
                                  llstr(start_recpos,llbuff));
            got_error=1;
            break;
          }
        }
        to= info->rec_buff;
        left_length= block_info.rec_len;
      }
      if (left_length < block_info.data_len)
      {
        _ma_check_print_error(param,"Found too long record (%lu) at %s",
                              (ulong) block_info.data_len,
                              llstr(start_recpos,llbuff));
        got_error=1;
        break;
      }
      if (_ma_read_cache(&param->read_cache, to, block_info.filepos,
                         (uint) block_info.data_len,
                         flag == 1 ? READING_NEXT : 0))
      {
        _ma_check_print_error(param,
                              "got error: %d when reading datafile at "
                              "position: %s", my_errno,
                              llstr(block_info.filepos, llbuff));

        DBUG_RETURN(1);
      }
      to+=block_info.data_len;
      param->link_used+= block_info.filepos-start_block;
      param->used+= block_info.filepos - start_block + block_info.data_len;
      param->empty+= block_info.block_len-block_info.data_len;
      left_length-= block_info.data_len;
      if (left_length)
      {
        if (b_type & BLOCK_LAST)
        {
          _ma_check_print_error(param,
                                "Wrong record length %s of %s at %s",
                                llstr(block_info.rec_len-left_length,llbuff),
                                llstr(block_info.rec_len, llbuff2),
                                llstr(start_recpos,llbuff3));
          got_error=1;
          break;
        }
        if (share->state.state.data_file_length < block_info.next_filepos)
        {
          _ma_check_print_error(param,
                                "Found next-recordlink that points outside "
                                "datafile at %s",
                                llstr(block_info.filepos,llbuff));
          got_error=1;
          break;
        }
      }
    } while (left_length);

    if (! got_error)
    {
      if (_ma_rec_unpack(info,record,info->rec_buff,block_info.rec_len) ==
          MY_FILE_ERROR)
      {
        _ma_check_print_error(param,"Found wrong record at %s",
                              llstr(start_recpos,llbuff));
        got_error=1;
      }
      else
      {
        ha_checksum checksum= 0;
        if (share->calc_checksum)
          checksum= (*share->calc_checksum)(info, record);

        if (param->testflag & (T_EXTEND | T_MEDIUM | T_VERBOSE))
        {
          if (_ma_rec_check(info,record, info->rec_buff,block_info.rec_len,
                            test(share->calc_checksum), checksum))
          {
            _ma_check_print_error(param,"Found wrong packed record at %s",
                                  llstr(start_recpos,llbuff));
            got_error= 1;
          }
        }
        param->glob_crc+= checksum;
      }

      if (! got_error)
      {
        if (check_keys_in_record(param, info, extend, start_recpos, record))
          DBUG_RETURN(1);
      }
      else
      {
        if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
          DBUG_RETURN(1);
      }
    }
    else if (!flag)
      pos= block_info.filepos+block_info.block_len;
next:;
  }
  DBUG_RETURN(0);
}


static int check_compressed_record(HA_CHECK *param, MARIA_HA *info, int extend,
                                   uchar *record)
{
  MARIA_BLOCK_INFO block_info;
  MARIA_SHARE *share= info->s;
  my_off_t start_recpos, pos;
  char llbuff[22];
  my_bool got_error= 0;
  DBUG_ENTER("check_compressed_record");

  pos= share->pack.header_length;             /* Skip header */
  while (pos < share->state.state.data_file_length)
  {
    if (_ma_killed_ptr(param))
      DBUG_RETURN(-1);

    if (_ma_read_cache(&param->read_cache, block_info.header, pos,
                       share->pack.ref_length, READING_NEXT))
    {
      _ma_check_print_error(param,
                            "got error: %d when reading datafile at position: "
                            "%s",
                            my_errno, llstr(pos, llbuff));
      DBUG_RETURN(1);
    }

    start_recpos= pos;
    param->splits++;
    VOID(_ma_pack_get_block_info(info, &info->bit_buff, &block_info,
                                 &info->rec_buff, &info->rec_buff_size, -1,
                                 start_recpos));
    pos=block_info.filepos+block_info.rec_len;
    if (block_info.rec_len < (uint) share->min_pack_length ||
        block_info.rec_len > (uint) share->max_pack_length)
    {
      _ma_check_print_error(param,
                            "Found block with wrong recordlength: %lu at %s",
                            block_info.rec_len, llstr(start_recpos,llbuff));
      got_error=1;
      goto end;
    }
    if (_ma_read_cache(&param->read_cache, info->rec_buff,
                       block_info.filepos, block_info.rec_len, READING_NEXT))
    {
      _ma_check_print_error(param,
                            "got error: %d when reading datafile at position: "
                            "%s",
                            my_errno, llstr(block_info.filepos, llbuff));
      DBUG_RETURN(1);
    }
    if (_ma_pack_rec_unpack(info, &info->bit_buff, record,
                            info->rec_buff, block_info.rec_len))
    {
      _ma_check_print_error(param,"Found wrong record at %s",
                            llstr(start_recpos,llbuff));
      got_error=1;
      goto end;
    }
    param->glob_crc+= (*share->calc_checksum)(info,record);
    param->link_used+= (block_info.filepos - start_recpos);
    param->used+= (pos-start_recpos);

end:
    if (! got_error)
    {
      if (check_keys_in_record(param, info, extend, start_recpos, record))
        DBUG_RETURN(1);
    }
    else
    {
      got_error= 0;                             /* Reset for next loop */
      if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/*
  Check if layout on head or tail page is ok

  NOTES
    This is for rows-in-block format.
*/

static int check_page_layout(HA_CHECK *param, MARIA_HA *info,
                             my_off_t page_pos, uchar *page,
                             uint row_count, uint head_empty,
                             uint *real_rows_found, uint *free_slots_found)
{
  uint empty, last_row_end, row, first_dir_entry, free_entry, block_size;
  uint free_entries, prev_free_entry;
  uchar *dir_entry;
  char llbuff[22];
  my_bool error_in_free_list= 0;
  DBUG_ENTER("check_page_layout");

  block_size= info->s->block_size;
  empty= 0;
  last_row_end= PAGE_HEADER_SIZE;
  *real_rows_found= 0;

  /* Check free directory list */
  free_entry= (uint) page[DIR_FREE_OFFSET];
  free_entries= 0;
  prev_free_entry= END_OF_DIR_FREE_LIST;
  while (free_entry != END_OF_DIR_FREE_LIST)
  {
    uchar *dir;
    if (free_entry > row_count)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Directory free entry points outside "
                            "directory",
                            llstr(page_pos, llbuff));
      error_in_free_list= 1;
      break;
    }
    dir= dir_entry_pos(page, block_size, free_entry);
    if (uint2korr(dir) != 0)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Directory free entry points to "
                            "not deleted entry",
                            llstr(page_pos, llbuff));
      error_in_free_list= 1;
      break;
    }
    if (dir[2] != prev_free_entry)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Directory free list back pointer "
                            "points to wrong entry",
                            llstr(page_pos, llbuff));
      error_in_free_list= 1;
      break;
    }
    prev_free_entry= free_entry;
    free_entry= dir[3];
    free_entries++;
  }
  *free_slots_found= free_entries;

  /* Check directry */
  dir_entry= page+ block_size - PAGE_SUFFIX_SIZE;
  first_dir_entry= (block_size - row_count * DIR_ENTRY_SIZE -
                    PAGE_SUFFIX_SIZE);
  for (row= 0 ; row < row_count ; row++)
  {
    uint pos, length;
    dir_entry-= DIR_ENTRY_SIZE;
    pos= uint2korr(dir_entry);
    if (!pos)
    {
      free_entries--;
      if (row == row_count -1)
      {
        _ma_check_print_error(param,
                              "Page %9s:  First entry in directory is 0",
                              llstr(page_pos, llbuff));
        if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
          DBUG_RETURN(1);
      }
      continue;                                 /* Deleted row */
    }
    (*real_rows_found)++;
    length= uint2korr(dir_entry+2);
    param->used+= length;
    if (pos < last_row_end)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Row %3u overlapps with previous row",
                            llstr(page_pos, llbuff), row);
      DBUG_RETURN(1);
    }
    empty+= (pos - last_row_end);
    last_row_end= pos + length;
    if (last_row_end > first_dir_entry)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Row %3u overlapps with directory",
                            llstr(page_pos, llbuff), row);
      DBUG_RETURN(1);
    }
  }
  empty+= (first_dir_entry - last_row_end);

  if (empty != head_empty)
  {
    _ma_check_print_error(param,
                          "Page %9s:  Wrong empty size.  Stored: %5u  "
                          "Actual: %5u",
                          llstr(page_pos, llbuff), head_empty, empty);
    param->err_count++;
  }
  if (free_entries != 0 && !error_in_free_list)
  {
    _ma_check_print_error(param,
                          "Page %9s:  Directory free link don't include "
                          "all free entries",
                          llstr(page_pos, llbuff));
    param->err_count++;
  }
  DBUG_RETURN(param->err_count &&
              (param->err_count >= MAXERR || !(param->testflag & T_VERBOSE)));
}


/*
  Check all rows on head page

  NOTES
    This is for rows-in-block format.

    Before this, we have already called check_page_layout(), so
    we know the block is logicaly correct (even if the rows may not be that)

  RETURN
   0  ok
   1  error
*/


static my_bool check_head_page(HA_CHECK *param, MARIA_HA *info, uchar *record,
                               int extend, my_off_t page_pos, uchar *page_buff,
                               uint row_count)
{
  MARIA_SHARE *share= info->s;
  uchar *dir_entry;
  uint row;
  char llbuff[22], llbuff2[22];
  ulonglong page= page_pos / share->block_size;
  DBUG_ENTER("check_head_page");

  dir_entry= page_buff+ share->block_size - PAGE_SUFFIX_SIZE;
  for (row= 0 ; row < row_count ; row++)
  {
    uint pos, length, flag;
    dir_entry-= DIR_ENTRY_SIZE;
    pos= uint2korr(dir_entry);
    if (!pos)
      continue;
    length= uint2korr(dir_entry+2);
    if (length < share->base.min_block_length)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Row %3u is too short "
                            "(%d of min %d bytes)",
                            llstr(page, llbuff), row, length,
                            (uint) share->base.min_block_length);
      DBUG_RETURN(1);
    }
    flag= (uint) (uchar) page_buff[pos];
    if (flag & ~(ROW_FLAG_ALL))
      _ma_check_print_error(param,
                            "Page %9s: Row %3u has wrong flag: %u",
                            llstr(page, llbuff), row, flag);

    DBUG_PRINT("info", ("rowid: %s  page: %lu  row: %u",
                        llstr(ma_recordpos(page, row), llbuff),
                        (ulong) page, row));
    info->cur_row.trid= 0;
    if (_ma_read_block_record2(info, record, page_buff+pos,
                               page_buff+pos+length))
    {
      _ma_check_print_error(param,
                            "Page %9s:  Row %3d is crashed",
                            llstr(page, llbuff), row);
      if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
        DBUG_RETURN(1);
      continue;
    }
    set_if_bigger(param->max_found_trid, info->cur_row.trid);
    if (info->cur_row.trid > param->max_trid)
      _ma_check_print_not_visible_error(param, info->cur_row.trid);

    if (share->calc_checksum)
    {
      ha_checksum checksum= (*share->calc_checksum)(info, record);
      if (info->cur_row.checksum != (checksum & 255))
        _ma_check_print_error(param, "Page %9s:  Row %3d has wrong checksum",
                              llstr(page, llbuff), row);
      param->glob_crc+= checksum;
    }
    if (info->cur_row.extents_count)
    {
      uchar *extents= info->cur_row.extents;
      uint i;
      /* Check that bitmap has the right marker for the found extents */
      for (i= 0 ; i < info->cur_row.extents_count ; i++)
      {
        pgcache_page_no_t extent_page;
        uint page_count, page_type;
        extent_page= uint5korr(extents);
        page_count=  uint2korr(extents+5) & ~START_EXTENT_BIT;
        extents+=    ROW_EXTENT_SIZE;
        page_type=   BLOB_PAGE;
        if (page_count & TAIL_BIT)
        {
          page_count= 1;
          page_type= TAIL_PAGE;
        }
        /*
          TODO OPTIMIZE:
          Check the whole extent with one test and only do the loop if
          something is wrong (for exact error reporting)
        */
        for ( ; page_count--; extent_page++)
        {
          uint bitmap_pattern;
          if (_ma_check_if_right_bitmap_type(info, page_type, extent_page,
                                             &bitmap_pattern))
          {
            _ma_check_print_error(param,
                                  "Page %9s:  Row: %3d has an extent with "
                                  "wrong information in bitmap:  "
                                  "Page: %9s  Page_type: %d  Bitmap: %d",
                                  llstr(page, llbuff), row,
                                  llstr(extent_page, llbuff2),
                                  page_type, bitmap_pattern);
            if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
              DBUG_RETURN(1);
          }
        }
      }
    }
    param->full_page_count+= info->cur_row.full_page_count;
    param->tail_count+= info->cur_row.tail_count;
    if (check_keys_in_record(param, info, extend,
                             ma_recordpos(page, row), record))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Check if rows-in-block data file is consistent
*/

static int check_block_record(HA_CHECK *param, MARIA_HA *info, int extend,
                              uchar *record)
{
  MARIA_SHARE *share= info->s;
  my_off_t pos;
  pgcache_page_no_t page;
  uchar *page_buff, *bitmap_buff, *data;
  char llbuff[22], llbuff2[22];
  uint block_size= share->block_size;
  ha_rows full_page_count, tail_count;
  my_bool full_dir;
  uint offset_page, offset, free_count;

  LINT_INIT(full_dir);

  if (_ma_scan_init_block_record(info))
  {
    _ma_check_print_error(param, "got error %d when initializing scan",
                          my_errno);
    return 1;
  }
  bitmap_buff= info->scan.bitmap_buff;
  page_buff= info->scan.page_buff;
  full_page_count= tail_count= 0;
  param->full_page_count= param->tail_count= 0;
  param->used= param->link_used= 0;
  param->splits= share->state.state.data_file_length / block_size;

  for (pos= 0, page= 0;
       pos < share->state.state.data_file_length;
       pos+= block_size, page++)
  {
    uint row_count, real_row_count, empty_space, page_type, bitmap_pattern;
    LINT_INIT(row_count);
    LINT_INIT(empty_space);

    if (_ma_killed_ptr(param))
    {
      _ma_scan_end_block_record(info);
      return -1;
    }
    if ((page % share->bitmap.pages_covered) == 0)
    {
      /* Bitmap page */
      if (pagecache_read(share->pagecache,
                         &info->s->bitmap.file,
                         page, 1,
                         bitmap_buff,
                         PAGECACHE_PLAIN_PAGE,
                         PAGECACHE_LOCK_LEFT_UNLOCKED, 0) == 0)
      {
        _ma_check_print_error(param,
                              "Page %9s:  Got error: %d when reading datafile",
                              llstr(page, llbuff), my_errno);
        goto err;
      }
      param->used+= block_size;
      param->link_used+= block_size;
      if (param->verbose > 2)
        print_bitmap_description(share, page, bitmap_buff);
      continue;
    }
    /* Skip pages marked as empty in bitmap */
    offset_page= (uint) ((page % share->bitmap.pages_covered) -1) * 3;
    offset= offset_page & 7;
    data= bitmap_buff + offset_page / 8;
    bitmap_pattern= uint2korr(data);
    if (!((bitmap_pattern >> offset) & 7))
    {
      param->empty+= block_size;
      param->del_blocks++;
      continue;
    }

    if (pagecache_read(share->pagecache,
                       &info->dfile,
                       page, 1,
                       page_buff,
                       share->page_type,
                       PAGECACHE_LOCK_LEFT_UNLOCKED, 0) == 0)
    {
      _ma_check_print_error(param,
                            "Page %9s:  Got error: %d when reading datafile",
                            llstr(page, llbuff), my_errno);
      goto err;
    }
    page_type= page_buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK;
    if (page_type == UNALLOCATED_PAGE || page_type >= MAX_PAGE_TYPE)
    {
      _ma_check_print_error(param,
                            "Page: %9s  Found wrong page type %d",
                            llstr(page, llbuff), page_type);
      if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
        goto err;
      continue;
    }
    switch ((enum en_page_type) page_type) {
    case UNALLOCATED_PAGE:
    case MAX_PAGE_TYPE:
    default:
      DBUG_ASSERT(0);                           /* Impossible */
      break;
    case HEAD_PAGE:
      row_count= page_buff[DIR_COUNT_OFFSET];
      empty_space= uint2korr(page_buff + EMPTY_SPACE_OFFSET);
      param->used+= block_size - empty_space;
      param->link_used+= (PAGE_HEADER_SIZE + PAGE_SUFFIX_SIZE +
                          row_count * DIR_ENTRY_SIZE);
      if (empty_space < share->bitmap.sizes[3])
        param->lost+= empty_space;
      if (check_page_layout(param, info, pos, page_buff, row_count,
                            empty_space, &real_row_count, &free_count))
        goto err;
      full_dir= (row_count == MAX_ROWS_PER_PAGE &&
                 page_buff[DIR_FREE_OFFSET] == END_OF_DIR_FREE_LIST);
      break;
    case TAIL_PAGE:
      row_count= page_buff[DIR_COUNT_OFFSET];
      empty_space= uint2korr(page_buff + EMPTY_SPACE_OFFSET);
      param->used+= block_size - empty_space;
      param->link_used+= (PAGE_HEADER_SIZE + PAGE_SUFFIX_SIZE +
                          row_count * DIR_ENTRY_SIZE);
      if (empty_space < share->bitmap.sizes[6])
        param->lost+= empty_space;
      if (check_page_layout(param, info, pos, page_buff, row_count,
                            empty_space, &real_row_count, &free_count))
        goto err;
      full_dir= (row_count - free_count >= MAX_ROWS_PER_PAGE -
                 share->base.blobs);
      break;
    case BLOB_PAGE:
      full_page_count++;
      full_dir= 0;
      empty_space= block_size;                  /* for error reporting */
      param->link_used+= (LSN_SIZE + PAGE_TYPE_SIZE);
      param->used+= block_size;
      break;
    }
    if (_ma_check_bitmap_data(info, page_type, page,
                              full_dir ? 0 : empty_space,
                              &bitmap_pattern))
    {
      if (bitmap_pattern == ~(uint) 0)
        _ma_check_print_error(param,
                              "Page %9s: Wrong bitmap for data on page",
                              llstr(page, llbuff));
      else
        _ma_check_print_error(param,
                              "Page %9s:  Wrong data in bitmap.  Page_type: "
                              "%d  full: %d  empty_space: %u  Bitmap-bits: %d",
                              llstr(page, llbuff), page_type, full_dir,
                              empty_space, bitmap_pattern);
      if (param->err_count++ > MAXERR || !(param->testflag & T_VERBOSE))
        goto err;
    }
    if ((enum en_page_type) page_type == BLOB_PAGE)
      continue;
    param->empty+= empty_space;
    if ((enum en_page_type) page_type == TAIL_PAGE)
    {
      tail_count+= real_row_count;
      continue;
    }
    if (check_head_page(param, info, record, extend, pos, page_buff,
                        row_count))
      goto err;
  }

  /* Verify that rest of bitmap is zero */

  if (page % share->bitmap.pages_covered)
  {
    /* Not at end of bitmap */
    uint bitmap_pattern;
    offset_page= (uint) ((page % share->bitmap.pages_covered) -1) * 3;
    offset= offset_page & 7;
    data= bitmap_buff + offset_page / 8;
    bitmap_pattern= uint2korr(data);
    if (((bitmap_pattern >> offset)) ||
        (data + 2 < bitmap_buff + share->bitmap.total_size &&
         _ma_check_if_zero(data+2, bitmap_buff + share->bitmap.total_size -
                           data - 2)))
    {
      ulonglong bitmap_page;
      bitmap_page= page / share->bitmap.pages_covered;
      bitmap_page*= share->bitmap.pages_covered;

      _ma_check_print_error(param,
                            "Bitmap at page %s has pages reserved outside of "
                            "data file length",
                            llstr(bitmap_page, llbuff));
      DBUG_EXECUTE("bitmap", _ma_print_bitmap(&share->bitmap, bitmap_buff,
                                              bitmap_page););
    }
  }

  _ma_scan_end_block_record(info);

  if (full_page_count != param->full_page_count)
    _ma_check_print_error(param, "Full page count read through records was %s "
                          "but we found %s pages while scanning table",
                          llstr(param->full_page_count, llbuff),
                          llstr(full_page_count, llbuff2));
  if (tail_count != param->tail_count)
    _ma_check_print_error(param, "Tail count read through records was %s but "
                          "we found %s tails while scanning table",
                          llstr(param->tail_count, llbuff),
                          llstr(tail_count, llbuff2));

  return param->error_printed != 0;

err:
  _ma_scan_end_block_record(info);
  return 1;
}


/* Check that record-link is ok */

int maria_chk_data_link(HA_CHECK *param, MARIA_HA *info, my_bool extend)
{
  MARIA_SHARE *share= info->s;
  int	error;
  uchar *record;
  char llbuff[22],llbuff2[22],llbuff3[22];
  DBUG_ENTER("maria_chk_data_link");

  if (!(param->testflag & T_SILENT))
  {
    if (extend)
      puts("- check records and index references");
    else
      puts("- check record links");
  }

  if (!(record= (uchar*) my_malloc(share->base.default_rec_buff_size, MYF(0))))
  {
    _ma_check_print_error(param,"Not enough memory for record");
    DBUG_RETURN(-1);
  }
  param->records= param->del_blocks= 0;
  param->used= param->link_used= param->splits= param->del_length= 0;
  param->lost= 0;
  param->tmp_record_checksum= param->glob_crc= 0;
  param->err_count= 0;

  error= 0;
  param->empty= share->pack.header_length;

  bzero((char*) param->tmp_key_crc,
        share->base.keys * sizeof(param->tmp_key_crc[0]));

  info->in_check_table= 1;       /* Don't assert on checksum errors */

  switch (share->data_file_type) {
  case BLOCK_RECORD:
    error= check_block_record(param, info, extend, record);
    break;
  case STATIC_RECORD:
    error= check_static_record(param, info, extend, record);
    break;
  case DYNAMIC_RECORD:
    error= check_dynamic_record(param, info, extend, record);
    break;
  case COMPRESSED_RECORD:
    error= check_compressed_record(param, info, extend, record);
    break;
  } /* switch */

  info->in_check_table= 0;

  if (error)
    goto err;

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (param->records != share->state.state.records)
  {
    _ma_check_print_error(param,
                          "Record-count is not ok; found %-10s  Should be: %s",
                          llstr(param->records,llbuff),
                          llstr(share->state.state.records,llbuff2));
    error=1;
  }
  else if (param->record_checksum &&
	   param->record_checksum != param->tmp_record_checksum)
  {
    _ma_check_print_error(param,
                          "Key pointers and record positions doesn't match");
    error=1;
  }
  else if (param->glob_crc != share->state.state.checksum &&
	   (share->options &
	    (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD)))
  {
    _ma_check_print_warning(param,
                            "Record checksum is not the same as checksum "
                            "stored in the index file");
    error=1;
  }
  else if (!extend)
  {
    uint key;
    for (key=0 ; key < share->base.keys;  key++)
    {
      if (param->tmp_key_crc[key] != param->key_crc[key] &&
          !(share->keyinfo[key].flag &
            (HA_FULLTEXT | HA_SPATIAL | HA_RTREE_INDEX)))
      {
	_ma_check_print_error(param,"Checksum for key: %2d doesn't match "
                              "checksum for records",
                              key+1);
	error=1;
      }
    }
  }

  if (param->del_length != share->state.state.empty)
  {
    _ma_check_print_warning(param,
                            "Found %s deleted space.   Should be %s",
                            llstr(param->del_length,llbuff2),
                            llstr(share->state.state.empty,llbuff));
  }
  /* Skip following checks for BLOCK RECORD as they don't make any sence */
  if (share->data_file_type != BLOCK_RECORD)
  {
    if (param->used + param->empty + param->del_length !=
        share->state.state.data_file_length)
    {
      _ma_check_print_warning(param,
                              "Found %s record data and %s unused data and %s "
                              "deleted data",
                              llstr(param->used, llbuff),
                              llstr(param->empty,llbuff2),
                              llstr(param->del_length,llbuff3));
      _ma_check_print_warning(param,
                              "Total %s   Should be: %s",
                              llstr((param->used+param->empty +
                                     param->del_length), llbuff),
                              llstr(share->state.state.data_file_length,
                                    llbuff2));
    }
    if (param->del_blocks != share->state.state.del)
    {
      _ma_check_print_warning(param,
                              "Found %10s deleted blocks.  Should be: %s",
                              llstr(param->del_blocks,llbuff),
                              llstr(share->state.state.del,llbuff2));
    }
    if (param->splits != share->state.split)
    {
      _ma_check_print_warning(param,
                              "Found %10s parts.  Should be: %s",
                              llstr(param->splits, llbuff),
                              llstr(share->state.split,llbuff2));
    }
  }
  if (param->testflag & T_INFO)
  {
    if (param->warning_printed || param->error_printed)
      puts("");
    if (param->used != 0 && ! param->error_printed)
    {
      if (param->records)
      {
        printf("Records:%18s    M.recordlength:%9lu   Packed:%14.0f%%\n",
               llstr(param->records,llbuff),
               (long)((param->used - param->link_used)/param->records),
               (share->base.blobs ? 0.0 :
                (ulonglong2double((ulonglong) share->base.reclength *
                                  param->records)-
                 my_off_t2double(param->used))/
                ulonglong2double((ulonglong) share->base.reclength *
                                 param->records)*100.0));
        printf("Recordspace used:%9.0f%%   Empty space:%12d%%  "
               "Blocks/Record: %6.2f\n",
               (ulonglong2double(param->used - param->link_used)/
                ulonglong2double(param->used-param->link_used+param->empty) *
                100.0),
               (!param->records ? 100 :
                (int) (ulonglong2double(param->del_length+param->empty)/
                       my_off_t2double(param->used)*100.0)),
               ulonglong2double(param->splits - param->del_blocks) /
               param->records);
      }
      else
        printf("Records:%18s\n", "0");
    }
    printf("Record blocks:%12s    Delete blocks:%10s\n",
           llstr(param->splits - param->del_blocks, llbuff),
           llstr(param->del_blocks, llbuff2));
    printf("Record data:  %12s    Deleted data: %10s\n",
           llstr(param->used - param->link_used,llbuff),
           llstr(param->del_length, llbuff2));
    printf("Empty space:  %12s    Linkdata:     %10s\n",
           llstr(param->empty, llbuff),llstr(param->link_used, llbuff2));
    if (share->data_file_type == BLOCK_RECORD)
    {
      printf("Full pages:   %12s    Tail count: %12s\n",
             llstr(param->full_page_count, llbuff),
             llstr(param->tail_count, llbuff2));
      printf("Lost space:   %12s\n", llstr(param->lost, llbuff));
      if (param->max_found_trid)
      {
        printf("Max trans. id: %11s\n",
               llstr(param->max_found_trid, llbuff));
      }
    }
  }
  my_free(record,MYF(0));
  DBUG_RETURN (error);

err:
  my_free(record,MYF(0));
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  DBUG_RETURN(1);
} /* maria_chk_data_link */


/**
  Prepares a table for a repair or index sort: flushes pages, records durably
  in the table that it is undergoing the operation (if that op crashes, that
  info will serve for Recovery and the user).

  If we start overwriting the index file, and crash then, old REDOs will
  be tried and fail. To prevent that, we bump skip_redo_lsn, and thus we have
  to flush and sync pages so that old REDOs can be skipped.
  If this is not a bulk insert, which Recovery can handle gracefully (by
  truncating files, see UNDO_BULK_INSERT) we also mark the table
  crashed-on-repair, so that user knows it has to re-repair. If bulk insert we
  shouldn't mark it crashed-on-repair, because if we did this, the UNDO phase
  would skip the table (UNDO_BULK_INSERT would not be applied),
  and maria_chk would not improve that.
  If this is an OPTIMIZE which merely sorts index, we need to do the same
  too: old REDOs should not apply to the new index file.
  Only the flush is needed when in maria_chk which is not crash-safe.

  @param  info             table
  @param  param            repair parameters
  @param  discard_index    if index pages can be thrown away
*/

static my_bool protect_against_repair_crash(MARIA_HA *info,
                                            const HA_CHECK *param,
                                            my_bool discard_index)
{
  MARIA_SHARE *share= info->s;

  /*
    There are other than recovery-related reasons to do the writes below:
    - the physical size of the data file is sometimes used during repair: we
    need to flush to have it exact
    - we flush the state because maria_open(HA_OPEN_COPY) will want to read
    it from disk.
  */
  if (_ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                            FLUSH_FORCE_WRITE,
                            discard_index ? FLUSH_IGNORE_CHANGED :
                            FLUSH_FORCE_WRITE) ||
      (share->changed &&
       _ma_state_info_write(share,
                            MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                            MA_STATE_INFO_WRITE_FULL_INFO |
                            MA_STATE_INFO_WRITE_LOCK)))
    return TRUE;
  /* In maria_chk this is not needed: */
  if (maria_multi_threaded && share->base.born_transactional)
  {
    if ((param->testflag & T_NO_CREATE_RENAME_LSN) == 0)
    {
      /* this can be true only for a transactional table */
      maria_mark_in_repair(info);
      if (_ma_state_info_write(share,
                               MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                               MA_STATE_INFO_WRITE_LOCK))
        return TRUE;
    }
    if (translog_status == TRANSLOG_OK &&
        _ma_update_state_lsns(share, translog_get_horizon(),
                              share->state.create_trid, FALSE, FALSE))
      return TRUE;
    if (_ma_sync_table_files(info))
      return TRUE;
  }
  return FALSE;
}


/**
   @brief Initialize variables for repair
*/

static int initialize_variables_for_repair(HA_CHECK *param,
                                           MARIA_SORT_INFO *sort_info,
                                           MARIA_SORT_PARAM *sort_param,
                                           MARIA_HA *info,
                                           my_bool rep_quick,
                                           MARIA_SHARE *org_share)
{
  MARIA_SHARE *share= info->s;

  /* Ro allow us to restore state and check how state changed */
  memcpy(org_share, share, sizeof(*share));

  /* Repair code relies on share->state.state so we have to update it here */
  if (share->lock.update_status)
    (*share->lock.update_status)(info);

  bzero((char*) sort_info,  sizeof(*sort_info));
  bzero((char*) sort_param, sizeof(*sort_param));

  param->testflag|= T_REP;                     /* for easy checking */
  if (share->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    param->testflag|= T_CALC_CHECKSUM;
  param->glob_crc= 0;
  if (rep_quick)
    param->testflag|= T_QUICK;
  else
    param->testflag&= ~T_QUICK;
  param->org_key_map= share->state.key_map;

  sort_param->sort_info= sort_info;
  sort_param->fix_datafile= ! rep_quick;
  sort_param->calc_checksum= test(param->testflag & T_CALC_CHECKSUM);
  sort_info->info= sort_info->new_info= info;
  sort_info->param= param;
  set_data_file_type(sort_info, info->s);
  sort_info->org_data_file_type= share->data_file_type;

  bzero(&info->rec_cache, sizeof(info->rec_cache));
  info->rec_cache.file= info->dfile.file;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (protect_against_repair_crash(info, param, !test(param->testflag &
                                                      T_CREATE_MISSING_KEYS)))
    return 1;

  /* calculate max_records */
  sort_info->filelength= my_seek(info->dfile.file, 0L, MY_SEEK_END, MYF(0));
  if ((param->testflag & T_CREATE_MISSING_KEYS) ||
      sort_info->org_data_file_type == COMPRESSED_RECORD)
    sort_info->max_records= share->state.state.records;
  else
  {
    ulong rec_length;
    rec_length= max(share->base.min_pack_length,
                    share->base.min_block_length);
    sort_info->max_records= (ha_rows) (sort_info->filelength / rec_length);
  }

  /* Set up transaction handler so that we can see all rows */
  if (param->max_trid == 0)
  {
    if (!ma_control_file_inited())
      param->max_trid= 0;      /* Give warning for first trid found */
    else
      param->max_trid= max_trid_in_system();
  }
  maria_ignore_trids(info);
  /* Don't write transid's during repair */
  maria_versioning(info, 0);
  return 0;
}


/*
  During initialize_variables_for_repair and related functions we set some
  variables to values that makes sence during repair.
  This function restores these values to their original values so that we can
  use the handler in MariaDB without having to close and open the table.
*/

static void restore_table_state_after_repair(MARIA_HA *info,
                                             MARIA_SHARE *org_share)
{
  maria_versioning(info, info->s->have_versioning);
  info->s->lock_key_trees= org_share->lock_key_trees;
}




/**
  @brief Drop all indexes

  @param[in]    param           check parameters
  @param[in]    info            MARIA_HA handle
  @param[in]    force           if to force drop all indexes

  @return       status
    @retval     0               OK
    @retval     != 0            Error

  @note
    Once allocated, index blocks remain part of the key file forever.
    When indexes are disabled, no block is freed. When enabling indexes,
    no block is freed either. The new indexes are create from new
    blocks. (Bug #4692)

    Before recreating formerly disabled indexes, the unused blocks
    must be freed. There are two options to do this:
    - Follow the tree of disabled indexes, add all blocks to the
      deleted blocks chain. Would require a lot of random I/O.
    - Drop all blocks by clearing all index root pointers and all
      delete chain pointers and resetting key_file_length to the end
      of the index file header. This requires to recreate all indexes,
      even those that may still be intact.
    The second method is probably faster in most cases.

    When disabling indexes, MySQL disables either all indexes or all
    non-unique indexes. When MySQL [re-]enables disabled indexes
    (T_CREATE_MISSING_KEYS), then we either have "lost" blocks in the
    index file, or there are no non-unique indexes. In the latter case,
    maria_repair*() would not be called as there would be no disabled
    indexes.

    If there would be more unique indexes than disabled (non-unique)
    indexes, we could do the first method. But this is not implemented
    yet. By now we drop and recreate all indexes when repair is called.

    However, there is an exception. Sometimes MySQL disables non-unique
    indexes when the table is empty (e.g. when copying a table in
    mysql_alter_table()). When enabling the non-unique indexes, they
    are still empty. So there is no index block that can be lost. This
    optimization is implemented in this function.

    Note that in normal repair (T_CREATE_MISSING_KEYS not set) we
    recreate all enabled indexes unconditonally. We do not change the
    key_map. Otherwise we invert the key map temporarily (outside of
    this function) and recreate the then "seemingly" enabled indexes.
    When we cannot use the optimization, and drop all indexes, we
    pretend that all indexes were disabled. By the inversion, we will
    then recrate all indexes.
*/

static int maria_drop_all_indexes(HA_CHECK *param, MARIA_HA *info,
                                  my_bool force)
{
  MARIA_SHARE *share= info->s;
  MARIA_STATE_INFO *state= &share->state;
  uint i;
  DBUG_ENTER("maria_drop_all_indexes");

  /*
    If any of the disabled indexes has a key block assigned, we must
    drop and recreate all indexes to avoid losing index blocks.

    If we want to recreate disabled indexes only _and_ all of these
    indexes are empty, we don't need to recreate the existing indexes.
  */
  if (!force && (param->testflag & T_CREATE_MISSING_KEYS))
  {
    DBUG_PRINT("repair", ("creating missing indexes"));
    for (i= 0; i < share->base.keys; i++)
    {
      DBUG_PRINT("repair", ("index #: %u  key_root: 0x%lx  active: %d",
                            i, (long) state->key_root[i],
                            maria_is_key_active(state->key_map, i)));
      if ((state->key_root[i] != HA_OFFSET_ERROR) &&
          !maria_is_key_active(state->key_map, i))
      {
        /*
          This index has at least one key block and it is disabled.
          We would lose its block(s) if would just recreate it.
          So we need to drop and recreate all indexes.
        */
        DBUG_PRINT("repair", ("nonempty and disabled: recreate all"));
        break;
      }
    }
    if (i >= share->base.keys)
      goto end;

    /*
      We do now drop all indexes and declare them disabled. With the
      T_CREATE_MISSING_KEYS flag, maria_repair*() will recreate all
      disabled indexes and enable them.
    */
    maria_clear_all_keys_active(state->key_map);
    DBUG_PRINT("repair", ("declared all indexes disabled"));
  }

  /* Clear index root block pointers. */
  for (i= 0; i < share->base.keys; i++)
    state->key_root[i]= HA_OFFSET_ERROR;

  /* Drop the delete chain. */
  share->state.key_del=  HA_OFFSET_ERROR;

  /* Reset index file length to end of index file header. */
  share->state.state.key_file_length= share->base.keystart;

end:
  DBUG_RETURN(0);
}


/*
  Recover old table by reading each record and writing all keys

  NOTES
    Save new datafile-name in temp_filename.
    We overwrite the index file as we go (writekeys() for example), so if we
    crash during this the table is unusable and user (or Recovery in the
    future) must repeat the REPAIR/OPTIMIZE operation. We could use a
    temporary index file in the future (drawback: more disk space).

  IMPLEMENTATION (for hard repair with block format)
   - Create new, unrelated MARIA_HA of the table
   - Create new datafile and associate it with new handler
   - Reset all statistic information in new handler
   - Copy all data to new handler with normal write operations
   - Move state of new handler to old handler
   - Close new handler
   - Close data file in old handler
   - Rename old data file to new data file.
   - Reopen data file in old handler
*/

int maria_repair(HA_CHECK *param, register MARIA_HA *info,
                 char *name, my_bool rep_quick)
{
  int error, got_error;
  ha_rows start_records,new_header_length;
  my_off_t del;
  File new_file;
  MARIA_SHARE *share= info->s;
  char llbuff[22],llbuff2[22];
  MARIA_SORT_INFO sort_info;
  MARIA_SORT_PARAM sort_param;
  my_bool block_record, scan_inited= 0, reenable_logging= 0;
  enum data_file_type org_data_file_type= share->data_file_type;
  myf sync_dir= ((share->now_transactional && !share->temporary) ?
                 MY_SYNC_DIR : 0);
  MARIA_SHARE backup_share;
  DBUG_ENTER("maria_repair");

  got_error= 1;
  new_file= -1;
  start_records= share->state.state.records;
  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with keycache) MARIA-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records, llbuff));
  }

  if (initialize_variables_for_repair(param, &sort_info, &sort_param, info,
                                      rep_quick, &backup_share))
    goto err;

  if ((reenable_logging= share->now_transactional))
    _ma_tmp_disable_logging_for_table(info, 0);

  sort_param.current_filepos= sort_param.filepos= new_header_length=
    ((param->testflag & T_UNPACK) ? 0L : share->pack.header_length);

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file= my_create(fn_format(param->temp_filename,
                                       share->data_file_name.str, "",
                                       DATA_TMP_EXT, 2+4),
                             0,param->tmpfile_createflag,
                             MYF(0))) < 0)
    {
      _ma_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (new_header_length &&
        maria_filecopy(param, new_file, info->dfile.file, 0L,
                       new_header_length, "datafile-header"))
      goto err;
    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file= new_file;             /* For sort_delete_record */
    if (share->data_file_type == BLOCK_RECORD ||
        (param->testflag & T_UNPACK))
    {
      if (create_new_data_handle(&sort_param, new_file))
        goto err;
      sort_info.new_info->rec_cache.file= new_file;
    }
  }

  block_record= sort_info.new_info->s->data_file_type == BLOCK_RECORD;

  if (org_data_file_type != BLOCK_RECORD)
  {
    /* We need a read buffer to read rows in big blocks */
    if (init_io_cache(&param->read_cache, info->dfile.file,
                      (uint) param->read_buffer_length,
                      READ_CACHE, share->pack.header_length, 1, MYF(MY_WME)))
      goto err;
  }
  if (sort_info.new_info->s->data_file_type != BLOCK_RECORD)
  {
    /* When writing to not block records, we need a write buffer */
    if (!rep_quick)
    {
      if (init_io_cache(&sort_info.new_info->rec_cache, new_file,
                        (uint) param->write_buffer_length,
                        WRITE_CACHE, new_header_length, 1,
                        MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw))
        goto err;
      sort_info.new_info->opt_flag|=WRITE_CACHE_USED;
    }
  }
  else if (block_record)
  {
    scan_inited= 1;
    if (maria_scan_init(sort_info.info))
      goto err;
  }

  if (!(sort_param.record=
        (uchar *) my_malloc((uint)
                            share->base.default_rec_buff_size, MYF(0))) ||
      _ma_alloc_buffer(&sort_param.rec_buff, &sort_param.rec_buff_size,
                       share->base.default_rec_buff_size))
  {
    _ma_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }

  sort_param.read_cache=param->read_cache;
  sort_param.pos=sort_param.max_pos=share->pack.header_length;
  param->read_cache.end_of_file= sort_info.filelength;
  sort_param.master=1;
  sort_info.max_records= ~(ha_rows) 0;

  del= share->state.state.del;
  share->state.state.records= share->state.state.del= share->state.split= 0;
  share->state.state.empty= 0;

  if (param->testflag & T_CREATE_MISSING_KEYS)
    maria_set_all_keys_active(share->state.key_map, share->base.keys);
  maria_drop_all_indexes(param, info, TRUE);

  maria_lock_memory(param);			/* Everything is alloced */

  /* Re-create all keys, which are set in key_map. */
  while (!(error=sort_get_next_record(&sort_param)))
  {
    if (block_record && _ma_sort_write_record(&sort_param))
      goto err;

    if (writekeys(&sort_param))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY)
	goto err;
      DBUG_DUMP("record", sort_param.record,
                share->base.default_rec_buff_size);
      _ma_check_print_warning(param,
                              "Duplicate key %2d for record at %10s against "
                              "new record at %10s",
                              info->errkey+1,
                              llstr(sort_param.current_filepos, llbuff),
                              llstr(info->dup_key_pos,llbuff2));
      if (param->testflag & T_VERBOSE)
      {
        MARIA_KEY tmp_key;
        MARIA_KEYDEF *keyinfo= share->keyinfo + info->errkey;
	(*keyinfo->make_key)(info, &tmp_key, (uint) info->errkey,
                             info->lastkey_buff,
                             sort_param.record, 0L, 0);
        _ma_print_key(stdout, &tmp_key);
      }
      sort_info.dupp++;
      if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
      {
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	param->error_printed=1;
	goto err;
      }
      /* purecov: begin tested */
      if (block_record)
      {
        sort_info.new_info->s->state.state.records--;
        if ((*sort_info.new_info->s->write_record_abort)(sort_info.new_info))
        {
          _ma_check_print_error(param,"Couldn't delete duplicate row");
          goto err;
        }
      }
      /* purecov: end */
      continue;
    }
    if (!block_record)
    {
      if (_ma_sort_write_record(&sort_param))
        goto err;
      /* Filepos is pointer to where next row will be stored */
      sort_param.current_filepos= sort_param.filepos;
    }
  }
  if (error > 0 || maria_write_data_suffix(&sort_info, !rep_quick) ||
      flush_io_cache(&sort_info.new_info->rec_cache) ||
      param->read_cache.error < 0)
    goto err;

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (my_chsize(share->kfile.file, share->state.state.key_file_length, 0, MYF(0)))
  {
    _ma_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   my_errno);
    goto err;
  }

  if (rep_quick && del+sort_info.dupp != share->state.state.del)
  {
    _ma_check_print_error(param,"Couldn't fix table with quick recovery: "
                          "Found wrong number of deleted records");
    _ma_check_print_error(param,"Run recovery again without -q");
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (param->testflag & T_SAFE_REPAIR)
  {
    /* Don't repair if we loosed more than one row */
    if (sort_info.new_info->s->state.state.records+1 < start_records)
    {
      share->state.state.records= start_records;
      goto err;
    }
  }

  VOID(end_io_cache(&sort_info.new_info->rec_cache));
  info->opt_flag&= ~WRITE_CACHE_USED;

  /*
    As we have read the data file (sort_get_next_record()) we may have
    cached, non-changed blocks of it in the page cache. We must throw them
    away as we are going to close their descriptor ('new_file'). We also want
    to flush any index block, so that it is ready for the upcoming sync.
  */
  if (_ma_flush_table_files_before_swap(param, info))
    goto err;

  if (!rep_quick)
  {
    sort_info.new_info->s->state.state.data_file_length= sort_param.filepos;
    if (sort_info.new_info != sort_info.info)
    {
      MARIA_STATE_INFO save_state= sort_info.new_info->s->state;
      if (maria_close(sort_info.new_info))
      {
        _ma_check_print_error(param, "Got error %d on close", my_errno);
        goto err;
      }
      copy_data_file_state(&share->state, &save_state);
      new_file= -1;
      sort_info.new_info= info;
    }
    share->state.version=(ulong) time((time_t*) 0);	/* Force reopen */

    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
      my_close(new_file, MYF(MY_WME));
    new_file= -1;
    change_data_file_descriptor(info, -1);
    if (maria_change_to_newfile(share->data_file_name.str, MARIA_NAME_DEXT,
                                DATA_TMP_EXT, param->backup_time,
                                (param->testflag & T_BACKUP_DATA ?
                                 MYF(MY_REDEL_MAKE_BACKUP): MYF(0)) |
                                sync_dir) ||
        _ma_open_datafile(info, share, NullS, -1))
    {
      goto err;
    }
  }
  else
  {
    share->state.state.data_file_length= sort_param.max_pos;
  }
  if (param->testflag & T_CALC_CHECKSUM)
    share->state.state.checksum= param->glob_crc;

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != share->state.state.records)
      printf("Data records: %s\n", llstr(share->state.state.records,llbuff));
  }
  if (sort_info.dupp)
    _ma_check_print_warning(param,
                            "%s records have been removed",
                            llstr(sort_info.dupp,llbuff));

  got_error= 0;
  /* If invoked by external program that uses thr_lock */
  if (&share->state.state != info->state)
    *info->state= *info->state_start= share->state.state;

err:
  if (scan_inited)
    maria_scan_end(sort_info.info);
  _ma_reset_state(info);

  VOID(end_io_cache(&param->read_cache));
  VOID(end_io_cache(&sort_info.new_info->rec_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  sort_info.new_info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  /* this below could fail, shouldn't we detect error? */
  if (got_error)
  {
    if (! param->error_printed)
      _ma_check_print_error(param,"%d for record at pos %s",my_errno,
		  llstr(sort_param.start_recpos,llbuff));
    (void)_ma_flush_table_files_before_swap(param, info);
    if (sort_info.new_info && sort_info.new_info != sort_info.info)
    {
      unuse_data_file_descriptor(sort_info.new_info);
      maria_close(sort_info.new_info);
    }
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_delete(param->temp_filename, MYF(MY_WME)));
    }
    maria_mark_crashed_on_repair(info);
  }
  /* If caller had disabled logging it's not up to us to re-enable it */
  if (reenable_logging)
    _ma_reenable_logging_for_table(info, FALSE);
  restore_table_state_after_repair(info, &backup_share);

  my_free(sort_param.rec_buff, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  if (!got_error && (param->testflag & T_UNPACK))
    restore_data_file_type(share);
  share->state.changed|= (STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES |
			  STATE_NOT_ANALYZED | STATE_NOT_ZEROFILLED);
  if (!rep_quick)
    share->state.changed&= ~(STATE_NOT_OPTIMIZED_ROWS | STATE_NOT_MOVABLE);
  DBUG_RETURN(got_error);
}


/* Uppdate keyfile when doing repair */

static int writekeys(MARIA_SORT_PARAM *sort_param)
{
  uint i;
  MARIA_HA *info=     sort_param->sort_info->info;
  MARIA_SHARE *share= info->s;
  uchar *record=    sort_param->record;
  uchar *key_buff;
  my_off_t filepos=   sort_param->current_filepos;
  MARIA_KEY key;
  DBUG_ENTER("writekeys");

  key_buff= info->lastkey_buff+share->base.max_key_length;

  for (i=0 ; i < share->base.keys ; i++)
  {
    if (maria_is_key_active(share->state.key_map, i))
    {
      if (share->keyinfo[i].flag & HA_FULLTEXT )
      {
        if (_ma_ft_add(info, i, key_buff, record, filepos))
	  goto err;
      }
      else
      {
	if (!(*share->keyinfo[i].make_key)(info, &key, i, key_buff, record,
                                         filepos, 0))
          goto err;
	if ((*share->keyinfo[i].ck_insert)(info, &key))
	  goto err;
      }
    }
  }
  DBUG_RETURN(0);

 err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey=(int) i;			/* This key was found */
    while ( i-- > 0 )
    {
      if (maria_is_key_active(share->state.key_map, i))
      {
	if (share->keyinfo[i].flag & HA_FULLTEXT)
        {
          if (_ma_ft_del(info,i,key_buff,record,filepos))
	    break;
        }
        else
	{
	  (*share->keyinfo[i].make_key)(info, &key, i, key_buff, record,
                                        filepos, 0);
	  if (_ma_ck_delete(info, &key))
	    break;
	}
      }
    }
  }
  /* Remove checksum that was added to glob_crc in sort_get_next_record */
  if (sort_param->calc_checksum)
    sort_param->sort_info->param->glob_crc-= info->cur_row.checksum;
  DBUG_PRINT("error",("errno: %d",my_errno));
  DBUG_RETURN(-1);
} /* writekeys */


	/* Change all key-pointers that points to a records */

int maria_movepoint(register MARIA_HA *info, uchar *record,
                    MARIA_RECORD_POS oldpos, MARIA_RECORD_POS newpos,
                    uint prot_key)
{
  uint i;
  uchar *key_buff;
  MARIA_SHARE *share= info->s;
  MARIA_PAGE page;
  DBUG_ENTER("maria_movepoint");

  key_buff= info->lastkey_buff + share->base.max_key_length;
  for (i=0 ; i < share->base.keys; i++)
  {
    if (i != prot_key && maria_is_key_active(share->state.key_map, i))
    {
      MARIA_KEY key;
      (*share->keyinfo[i].make_key)(info, &key, i, key_buff, record, oldpos,
                                    0);
      if (key.keyinfo->flag & HA_NOSAME)
      {					/* Change pointer direct */
	MARIA_KEYDEF *keyinfo;
	keyinfo=share->keyinfo+i;
	if (_ma_search(info, &key, (uint32) (SEARCH_SAME | SEARCH_SAVE_BUFF),
		       share->state.key_root[i]))
	  DBUG_RETURN(-1);
        _ma_page_setup(&page, info, keyinfo, info->last_keypage,
                       info->keyread_buff);

	_ma_dpointer(share, info->int_keypos - page.node -
		     share->rec_reflength,newpos);

	if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_UNLOCKED,
                              DFLT_INIT_HITS))
	  DBUG_RETURN(-1);
      }
      else
      {					/* Change old key to new */
	if (_ma_ck_delete(info, &key))
	  DBUG_RETURN(-1);
	(*share->keyinfo[i].make_key)(info, &key, i, key_buff, record, newpos,
                                      0);
	if (_ma_ck_write(info, &key))
	  DBUG_RETURN(-1);
      }
    }
  }
  DBUG_RETURN(0);
} /* maria_movepoint */


	/* Tell system that we want all memory for our cache */

void maria_lock_memory(HA_CHECK *param __attribute__((unused)))
{
#ifdef SUN_OS				/* Key-cacheing thrases on sun 4.1 */
  if (param->opt_maria_lock_memory)
  {
    int success = mlockall(MCL_CURRENT);	/* or plock(DATLOCK); */
    if (geteuid() == 0 && success != 0)
      _ma_check_print_warning(param,
			     "Failed to lock memory. errno %d",my_errno);
  }
#endif
} /* maria_lock_memory */


/**
   Flush all changed blocks to disk.

   We release blocks as it's unlikely that they would all be needed soon.
   This function needs to be called before swapping data or index files or
   syncing them.

   @param  param           description of the repair operation
   @param  info            table
*/

static my_bool _ma_flush_table_files_before_swap(HA_CHECK *param,
                                                 MARIA_HA *info)
{
  DBUG_ENTER("_ma_flush_table_files_before_swap");
  if (_ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                            FLUSH_RELEASE, FLUSH_RELEASE))
  {
    _ma_check_print_error(param, "%d when trying to write buffers", my_errno);
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


	/* Sort index for more efficent reads */

int maria_sort_index(HA_CHECK *param, register MARIA_HA *info, char *name)
{
  reg2 uint key;
  reg1 MARIA_KEYDEF *keyinfo;
  File new_file;
  my_off_t index_pos[HA_MAX_POSSIBLE_KEY];
  uint r_locks,w_locks;
  int old_lock;
  MARIA_SHARE *share= info->s;
  MARIA_STATE_INFO old_state;
  myf sync_dir= ((share->now_transactional && !share->temporary) ?
                 MY_SYNC_DIR : 0);
  DBUG_ENTER("maria_sort_index");

  /* cannot sort index files with R-tree indexes */
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       key++,keyinfo++)
    if (keyinfo->key_alg == HA_KEY_ALG_RTREE)
      DBUG_RETURN(0);

  if (!(param->testflag & T_SILENT))
    printf("- Sorting index for MARIA-table '%s'\n",name);

  if (protect_against_repair_crash(info, param, FALSE))
    DBUG_RETURN(1);

  /* Get real path for index file */
  fn_format(param->temp_filename,name,"", MARIA_NAME_IEXT,2+4+32);
  if ((new_file=my_create(fn_format(param->temp_filename,param->temp_filename,
				    "", INDEX_TMP_EXT,2+4),
			  0,param->tmpfile_createflag,MYF(0))) <= 0)
  {
    _ma_check_print_error(param,"Can't create new tempfile: '%s'",
			 param->temp_filename);
    DBUG_RETURN(-1);
  }
  if (maria_filecopy(param, new_file, share->kfile.file, 0L,
                     (ulong) share->base.keystart, "headerblock"))
    goto err;

  param->new_file_pos=share->base.keystart;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       key++,keyinfo++)
  {
    if (! maria_is_key_active(share->state.key_map, key))
      continue;

    if (share->state.key_root[key] != HA_OFFSET_ERROR)
    {
      index_pos[key]=param->new_file_pos;	/* Write first block here */
      if (sort_one_index(param,info,keyinfo,share->state.key_root[key],
			 new_file))
	goto err;
    }
    else
      index_pos[key]= HA_OFFSET_ERROR;		/* No blocks */
  }

  /* Flush key cache for this file if we are calling this outside maria_chk */
  flush_pagecache_blocks(share->pagecache, &share->kfile,
                         FLUSH_IGNORE_CHANGED);

  share->state.version=(ulong) time((time_t*) 0);
  old_state= share->state;			/* save state if not stored */
  r_locks=   share->r_locks;
  w_locks=   share->w_locks;
  old_lock=  info->lock_type;

	/* Put same locks as old file */
  share->r_locks= share->w_locks= share->tot_locks= 0;
  (void) _ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  pthread_mutex_lock(&share->intern_lock);
  VOID(my_close(share->kfile.file, MYF(MY_WME)));
  share->kfile.file = -1;
  pthread_mutex_unlock(&share->intern_lock);
  VOID(my_close(new_file,MYF(MY_WME)));
  if (maria_change_to_newfile(share->index_file_name.str, MARIA_NAME_IEXT,
                              INDEX_TMP_EXT, 0, sync_dir) ||
      _ma_open_keyfile(share))
    goto err2;
  info->lock_type= F_UNLCK;			/* Force maria_readinfo to lock */
  _ma_readinfo(info,F_WRLCK,0);			/* Will lock the table */
  info->lock_type=  old_lock;
  share->r_locks=   r_locks;
  share->w_locks=   w_locks;
  share->tot_locks= r_locks+w_locks;
  share->state=     old_state;			/* Restore old state */

  share->state.state.key_file_length=param->new_file_pos;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (key=0 ; key < share->base.keys ; key++)
    share->state.key_root[key]=index_pos[key];
  share->state.key_del=  HA_OFFSET_ERROR;

  share->state.changed&= ~STATE_NOT_SORTED_PAGES;
  DBUG_EXECUTE_IF("maria_flush_whole_log",
                  {
                    DBUG_PRINT("maria_flush_whole_log", ("now"));
                    translog_flush(translog_get_horizon());
                  });
  DBUG_EXECUTE_IF("maria_crash_sort_index",
                  {
                    DBUG_PRINT("maria_crash_sort_index", ("now"));
                    DBUG_ABORT();
                  });
  DBUG_RETURN(0);

err:
  VOID(my_close(new_file,MYF(MY_WME)));
err2:
  VOID(my_delete(param->temp_filename,MYF(MY_WME)));
  DBUG_RETURN(-1);
} /* maria_sort_index */


/**
  @brief put CRC on the page

  @param buff            reference on the page buffer.
  @param pos             position of the page in the file.
  @param length          length of the page
*/

static void put_crc(uchar *buff, my_off_t pos, MARIA_SHARE *share)
{
  maria_page_crc_set_index(buff, (pgcache_page_no_t) (pos / share->block_size),
                           (uchar*) share);
}


/* Sort index blocks recursive using one index */

static int sort_one_index(HA_CHECK *param, MARIA_HA *info,
                          MARIA_KEYDEF *keyinfo,
			  my_off_t pagepos, File new_file)
{
  uint length,nod_flag;
  uchar *buff,*keypos,*endpos;
  my_off_t new_page_pos,next_page;
  MARIA_SHARE *share= info->s;
  MARIA_KEY key;
  MARIA_PAGE page;
  DBUG_ENTER("sort_one_index");

  /* cannot walk over R-tree indices */
  DBUG_ASSERT(keyinfo->key_alg != HA_KEY_ALG_RTREE);
  new_page_pos=param->new_file_pos;
  param->new_file_pos+=keyinfo->block_length;
  key.keyinfo= keyinfo;

  if (!(buff= (uchar*) my_alloca((uint) keyinfo->block_length +
                                 keyinfo->maxlength +
                                 MARIA_INDEX_OVERHEAD_SIZE)))
  {
    _ma_check_print_error(param,"Not enough memory for key block");
    DBUG_RETURN(-1);
  }
  key.data= buff + keyinfo->block_length;

  if (_ma_fetch_keypage(&page, info, keyinfo, pagepos,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        DFLT_INIT_HITS, buff, 0))
  {
    report_keypage_fault(param, info, pagepos);
    goto err;
  }

  if ((nod_flag= page.node) || keyinfo->flag & HA_FULLTEXT)
  {
    keypos= page.buff + share->keypage_header + nod_flag;
    endpos= page.buff + page.size;

    for ( ;; )
    {
      if (nod_flag)
      {
	next_page= _ma_kpos(nod_flag,keypos);
        /* Save new pos */
	_ma_kpointer(info,keypos-nod_flag,param->new_file_pos);
	if (sort_one_index(param,info,keyinfo,next_page,new_file))
	{
	  DBUG_PRINT("error",
		     ("From page: %ld, keyoffset: %lu  used_length: %d",
		      (ulong) pagepos, (ulong) (keypos - buff),
		      (int) page.size));
	  DBUG_DUMP("buff", page.buff, page.size);
	  goto err;
	}
      }
      if (keypos >= endpos ||
	  !(*keyinfo->get_key)(&key, page.flag, nod_flag, &keypos))
	break;
      DBUG_ASSERT(keypos <= endpos);
      if (keyinfo->flag & HA_FULLTEXT)
      {
        uint off;
        int  subkeys;
        get_key_full_length_rdonly(off, key.data);
        subkeys= ft_sintXkorr(key.data + off);
        if (subkeys < 0)
        {
          next_page= _ma_row_pos_from_key(&key);
          _ma_dpointer(share, keypos - nod_flag - share->rec_reflength,
                       param->new_file_pos); /* Save new pos */
          if (sort_one_index(param,info,&share->ft2_keyinfo,
                             next_page,new_file))
            goto err;
        }
      }
    }
  }

  /* Fill block with zero and write it to the new index file */
  length= page.size;
  bzero(buff+length,keyinfo->block_length-length);
  put_crc(buff, new_page_pos, share);
  if (my_pwrite(new_file, buff,(uint) keyinfo->block_length,
		new_page_pos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
  {
    _ma_check_print_error(param,"Can't write indexblock, error: %d",my_errno);
    goto err;
  }
  my_afree(buff);
  DBUG_RETURN(0);
err:
  my_afree(buff);
  DBUG_RETURN(1);
} /* sort_one_index */


/**
   @brief Fill empty space in index file with zeroes

   @return
   @retval 0  Ok
   @retval 1  Error
*/

static my_bool maria_zerofill_index(HA_CHECK *param, MARIA_HA *info,
                                    const char *name)
{
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE page_link;
  char llbuff[21];
  uchar *buff;
  pgcache_page_no_t page;
  my_off_t pos;
  my_off_t key_file_length= share->state.state.key_file_length;
  uint block_size= share->block_size;
  my_bool zero_lsn= (share->base.born_transactional &&
                     !(param->testflag & T_ZEROFILL_KEEP_LSN));
  DBUG_ENTER("maria_zerofill_index");

  if (!(param->testflag & T_SILENT))
    printf("- Zerofilling index for MARIA-table '%s'\n",name);

  /* Go through the index file */
  for (pos= share->base.keystart, page= (ulonglong) (pos / block_size);
       pos < key_file_length;
       pos+= block_size, page++)
  {
    uint length;
    if (!(buff= pagecache_read(share->pagecache,
                               &share->kfile, page,
                               DFLT_INIT_HITS, 0,
                               PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                               &page_link.link)))
    {
      pagecache_unlock_by_link(share->pagecache, page_link.link,
                               PAGECACHE_LOCK_WRITE_UNLOCK,
                               PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                               LSN_IMPOSSIBLE, 0, FALSE);
      _ma_check_print_error(param,
                            "Page %9s: Got error %d when reading index file",
                            llstr(pos, llbuff), my_errno);
      DBUG_RETURN(1);
    }
    if (zero_lsn)
      bzero(buff, LSN_SIZE);

    if (share->base.born_transactional)
    {
      uint keynr= _ma_get_keynr(share, buff);
      if (keynr != MARIA_DELETE_KEY_NR)
      {
        MARIA_PAGE page;
        DBUG_ASSERT(keynr < share->base.keys);

        _ma_page_setup(&page, info, share->keyinfo + keynr, pos, buff);
        if (_ma_compact_keypage(&page, ~(TrID) 0))
        {
          _ma_check_print_error(param,
                                "Page %9s: Got error %d when reading index "
                                "file",
                                llstr(pos, llbuff), my_errno);
          DBUG_RETURN(1);
        }
      }
    }

    length= _ma_get_page_used(share, buff);
    DBUG_ASSERT(length <= block_size);
    if (length < block_size)
      bzero(buff + length, block_size - length);
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 1, FALSE);
  }
  if (flush_pagecache_blocks(share->pagecache, &share->kfile,
                             FLUSH_FORCE_WRITE))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/**
   @brief Fill empty space in data file with zeroes

   @todo
   Zerofill all pages marked in bitmap as empty and change them to
   be of type UNALLOCATED_PAGE

   @return
   @retval 0  Ok
   @retval 1  Error
*/

static my_bool maria_zerofill_data(HA_CHECK *param, MARIA_HA *info,
                                   const char *name)
{
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE page_link;
  char llbuff[21];
  my_off_t pos;
  pgcache_page_no_t page;
  uint block_size= share->block_size;
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  my_bool zero_lsn= !(param->testflag & T_ZEROFILL_KEEP_LSN), error;
  DBUG_ENTER("maria_zerofill_data");

  /* This works only with BLOCK_RECORD files */
  if (share->data_file_type != BLOCK_RECORD)
    DBUG_RETURN(0);

  if (!(param->testflag & T_SILENT))
    printf("- Zerofilling data  for MARIA-table '%s'\n",name);

  /* Go through the record file */
  for (page= 1, pos= block_size;
       pos < share->state.state.data_file_length;
       pos+= block_size, page++)
  {
    uchar *buff;
    enum en_page_type page_type;

    /* Ignore bitmap pages */
    if ((page % share->bitmap.pages_covered) == 0)
      continue;
    if (!(buff= pagecache_read(share->pagecache,
                               &info->dfile,
                               page, 1, 0,
                               PAGECACHE_PLAIN_PAGE, PAGECACHE_LOCK_WRITE,
                               &page_link.link)))
    {
      _ma_check_print_error(param,
                            "Page %9s:  Got error: %d when reading datafile",
                            llstr(pos, llbuff), my_errno);
      goto err;
    }
    page_type= (enum en_page_type) (buff[PAGE_TYPE_OFFSET] & PAGE_TYPE_MASK);
    switch (page_type) {
    case UNALLOCATED_PAGE:
      if (zero_lsn)
        bzero(buff, block_size);
      else
        bzero(buff + LSN_SIZE, block_size - LSN_SIZE);
      break;
    case BLOB_PAGE:
      if (_ma_bitmap_get_page_bits(info, bitmap, page) == 0)
      {
        /* Unallocated page */
        if (zero_lsn)
          bzero(buff, block_size);
        else
          bzero(buff + LSN_SIZE, block_size - LSN_SIZE);
      }
      else
        if (zero_lsn)
          bzero(buff, LSN_SIZE);
      break;
    case HEAD_PAGE:
    case TAIL_PAGE:
    {
      uint max_entry= (uint) buff[DIR_COUNT_OFFSET];
      uint offset, dir_start, empty_space;
      uchar *dir;

      if (zero_lsn)
        bzero(buff, LSN_SIZE);
      if (max_entry != 0)
      {
        my_bool is_head_page= (page_type == HEAD_PAGE);
        dir= dir_entry_pos(buff, block_size, max_entry - 1);
        _ma_compact_block_page(buff, block_size, max_entry -1, 0,
                               is_head_page ? ~(TrID) 0 : 0,
                               is_head_page ?
                               share->base.min_block_length : 0);

        /* compactation may have increased free space */
        empty_space= uint2korr(buff + EMPTY_SPACE_OFFSET);
        if (!enough_free_entries_on_page(share, buff))
          empty_space= 0;                         /* Page is full */
        if (_ma_bitmap_set(info, page, is_head_page,
                           empty_space))
          goto err;

        /* Zerofill the not used part */
        offset= uint2korr(dir) + uint2korr(dir+2);
        dir_start= (uint) (dir - buff);
        DBUG_ASSERT(dir_start >= offset);
        if (dir_start > offset)
          bzero(buff + offset, dir_start - offset);
      }
      break;
    }
    default:
      _ma_check_print_error(param,
                            "Page %9s:  Found unrecognizable block of type %d",
                            llstr(pos, llbuff), page_type);
      goto err;
    }
    pagecache_unlock_by_link(share->pagecache, page_link.link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                             LSN_IMPOSSIBLE, 1, FALSE);
  }
  error= _ma_bitmap_flush(share);
  if (flush_pagecache_blocks(share->pagecache, &info->dfile,
                             FLUSH_FORCE_WRITE))
    error= 1;
  DBUG_RETURN(error);

err:
  pagecache_unlock_by_link(share->pagecache, page_link.link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_UNPIN, LSN_IMPOSSIBLE,
                           LSN_IMPOSSIBLE, 0, FALSE);
  /* flush what was changed so far */
  (void) _ma_bitmap_flush(share);
  (void) flush_pagecache_blocks(share->pagecache, &info->dfile,
                                FLUSH_FORCE_WRITE);

  DBUG_RETURN(1);
}


/**
   @brief Fill empty space in index and data files with zeroes

   @return
   @retval 0  Ok
   @retval 1  Error
*/

int maria_zerofill(HA_CHECK *param, MARIA_HA *info, const char *name)
{
  my_bool error, reenable_logging,
    zero_lsn= !(param->testflag & T_ZEROFILL_KEEP_LSN);
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("maria_zerofill");
  if ((reenable_logging= share->now_transactional))
    _ma_tmp_disable_logging_for_table(info, 0);
  if (!(error= (maria_zerofill_index(param, info, name) ||
                maria_zerofill_data(param, info, name) ||
                _ma_set_uuid(info, 0))))
  {
    /*
      Mark that we have done zerofill of data and index. If we zeroed pages'
      LSN, table is movable.
    */
    share->state.changed&= ~STATE_NOT_ZEROFILLED;
    if (zero_lsn)
    {
      share->state.changed&= ~(STATE_NOT_MOVABLE | STATE_MOVED);
      /* Table should get new LSNs */
      share->state.create_rename_lsn= share->state.is_of_horizon=
        share->state.skip_redo_lsn= LSN_NEEDS_NEW_STATE_LSNS;
    }
    /* Ensure state is later flushed to disk, if within maria_chk */
    info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

    /* Reset create_trid to make file comparable */
    share->state.create_trid= 0;
  }
  if (reenable_logging)
    _ma_reenable_logging_for_table(info, FALSE);
  DBUG_RETURN(error);
}


/*
  Let temporary file replace old file.
  This assumes that the new file was created in the same
  directory as given by realpath(filename).
  This will ensure that any symlinks that are used will still work.
  Copy stats from old file to new file, deletes orignal and
  changes new file name to old file name
*/

int maria_change_to_newfile(const char * filename, const char * old_ext,
                            const char * new_ext, time_t backup_time,
                            myf MyFlags)
{
  char old_filename[FN_REFLEN],new_filename[FN_REFLEN];
  /* Get real path to filename */
  (void) fn_format(old_filename,filename,"",old_ext,2+4+32);
  return my_redel(old_filename,
		  fn_format(new_filename,old_filename,"",new_ext,2+4),
                  backup_time,
		  MYF(MY_WME | MY_LINK_WARNING | MyFlags));
} /* maria_change_to_newfile */


/* Copy a block between two files */

int maria_filecopy(HA_CHECK *param, File to,File from,my_off_t start,
                   my_off_t length, const char *type)
{
  uchar tmp_buff[IO_SIZE], *buff;
  ulong buff_length;
  DBUG_ENTER("maria_filecopy");

  buff_length=(ulong) min(param->write_buffer_length,length);
  if (!(buff=my_malloc(buff_length,MYF(0))))
  {
    buff=tmp_buff; buff_length=IO_SIZE;
  }

  VOID(my_seek(from,start,MY_SEEK_SET,MYF(0)));
  while (length > buff_length)
  {
    if (my_read(from, buff, buff_length, MYF(MY_NABP)) ||
	my_write(to,  buff, buff_length, param->myf_rw))
      goto err;
    length-= buff_length;
  }
  if (my_read(from, buff, (size_t) length,MYF(MY_NABP)) ||
      my_write(to,  buff, (size_t) length,param->myf_rw))
    goto err;
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  DBUG_RETURN(0);
err:
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  _ma_check_print_error(param,"Can't copy %s to tempfile, error %d",
		       type,my_errno);
  DBUG_RETURN(1);
}


/*
  Repair table or given index using sorting

  SYNOPSIS
    maria_repair_by_sort()
    param		Repair parameters
    info		MARIA handler to repair
    name		Name of table (for warnings)
    rep_quick		set to <> 0 if we should not change data file

  RESULT
    0	ok
    <>0	Error
*/

int maria_repair_by_sort(HA_CHECK *param, register MARIA_HA *info,
                         const char * name, my_bool rep_quick)
{
  int got_error;
  uint i;
  ha_rows start_records;
  my_off_t new_header_length, org_header_length, del;
  File new_file;
  MARIA_SORT_PARAM sort_param;
  MARIA_SHARE *share= info->s;
  HA_KEYSEG *keyseg;
  double  *rec_per_key_part;
  char llbuff[22];
  MARIA_SORT_INFO sort_info;
  ulonglong key_map;
  myf sync_dir= ((share->now_transactional && !share->temporary) ?
                 MY_SYNC_DIR : 0);
  my_bool scan_inited= 0, reenable_logging= 0;
  MARIA_SHARE backup_share;
  DBUG_ENTER("maria_repair_by_sort");
  LINT_INIT(key_map);

  got_error= 1;
  new_file= -1;
  start_records= share->state.state.records;
  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with sort) MARIA-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records,llbuff));
  }

  if (initialize_variables_for_repair(param, &sort_info, &sort_param, info,
                                      rep_quick, &backup_share))
    goto err;

  if ((reenable_logging= share->now_transactional))
    _ma_tmp_disable_logging_for_table(info, 0);

  org_header_length= share->pack.header_length;
  new_header_length= (param->testflag & T_UNPACK) ? 0 : org_header_length;
  sort_param.filepos= new_header_length;

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_create(fn_format(param->temp_filename,
                                      share->data_file_name.str, "",
                                      DATA_TMP_EXT, 2+4),
                            0,param->tmpfile_createflag,
                            MYF(0))) < 0)
    {
      _ma_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (new_header_length &&
        maria_filecopy(param, new_file, info->dfile.file, 0L,
                       new_header_length, "datafile-header"))
      goto err;

    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file= new_file;             /* For sort_delete_record */
    if (share->data_file_type == BLOCK_RECORD ||
        (param->testflag & T_UNPACK))
    {
      if (create_new_data_handle(&sort_param, new_file))
        goto err;
      sort_info.new_info->rec_cache.file= new_file;
    }
  }

  if (!(sort_info.key_block=
	alloc_key_blocks(param,
			 (uint) param->sort_key_blocks,
			 share->base.max_key_block_length)))
    goto err;
  sort_info.key_block_end=sort_info.key_block+param->sort_key_blocks;

  if (share->data_file_type != BLOCK_RECORD)
  {
    /* We need a read buffer to read rows in big blocks */
    if (init_io_cache(&param->read_cache, info->dfile.file,
                      (uint) param->read_buffer_length,
                      READ_CACHE, org_header_length, 1, MYF(MY_WME)))
      goto err;
  }
  if (sort_info.new_info->s->data_file_type != BLOCK_RECORD)
  {
    /* When writing to not block records, we need a write buffer */
    if (!rep_quick)
    {
      if (init_io_cache(&sort_info.new_info->rec_cache, new_file,
                        (uint) param->write_buffer_length,
                        WRITE_CACHE, new_header_length, 1,
                        MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw))
        goto err;
      sort_info.new_info->opt_flag|= WRITE_CACHE_USED;
    }
  }

  if (!(sort_param.record=
        (uchar*) my_malloc((size_t) share->base.default_rec_buff_size,
                           MYF(0))) ||
      _ma_alloc_buffer(&sort_param.rec_buff, &sort_param.rec_buff_size,
                       share->base.default_rec_buff_size))
  {
    _ma_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }

  /* Optionally drop indexes and optionally modify the key_map */
  maria_drop_all_indexes(param, info, FALSE);
  key_map= share->state.key_map;
  if (param->testflag & T_CREATE_MISSING_KEYS)
  {
    /* Invert the copied key_map to recreate all disabled indexes. */
    key_map= ~key_map;
  }

  param->read_cache.end_of_file= sort_info.filelength;
  sort_param.wordlist=NULL;
  init_alloc_root(&sort_param.wordroot, FTPARSER_MEMROOT_ALLOC_SIZE, 0);

  sort_param.key_cmp=sort_key_cmp;
  sort_param.lock_in_memory=maria_lock_memory;
  sort_param.tmpdir=param->tmpdir;
  sort_param.master =1;

  del=share->state.state.del;

  rec_per_key_part= param->new_rec_per_key_part;
  for (sort_param.key=0 ; sort_param.key < share->base.keys ;
       rec_per_key_part+=sort_param.keyinfo->keysegs, sort_param.key++)
  {
    sort_param.keyinfo=share->keyinfo+sort_param.key;
    /*
      Skip this index if it is marked disabled in the copied
      (and possibly inverted) key_map.
    */
    if (! maria_is_key_active(key_map, sort_param.key))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part +
		      (uint) (rec_per_key_part - param->new_rec_per_key_part)),
	     sort_param.keyinfo->keysegs*sizeof(*rec_per_key_part));
      DBUG_PRINT("repair", ("skipping seemingly disabled index #: %u",
                            sort_param.key));
      continue;
    }

    if ((!(param->testflag & T_SILENT)))
      printf ("- Fixing index %d\n",sort_param.key+1);

    sort_param.read_cache=param->read_cache;
    sort_param.seg=sort_param.keyinfo->seg;
    sort_param.max_pos= sort_param.pos= org_header_length;
    keyseg=sort_param.seg;
    bzero((char*) sort_param.unique,sizeof(sort_param.unique));
    sort_param.key_length=share->rec_reflength;
    for (i=0 ; keyseg[i].type != HA_KEYTYPE_END; i++)
    {
      sort_param.key_length+=keyseg[i].length;
      if (keyseg[i].flag & HA_SPACE_PACK)
	sort_param.key_length+=get_pack_length(keyseg[i].length);
      if (keyseg[i].flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
	sort_param.key_length+=2 + test(keyseg[i].length >= 127);
      if (keyseg[i].flag & HA_NULL_PART)
	sort_param.key_length++;
    }
    share->state.state.records=share->state.state.del=share->state.split=0;
    share->state.state.empty=0;

    if (sort_param.keyinfo->flag & HA_FULLTEXT)
    {
      uint ft_max_word_len_for_sort=FT_MAX_WORD_LEN_FOR_SORT*
                                    sort_param.keyinfo->seg->charset->mbmaxlen;
      sort_param.key_length+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
      /*
        fulltext indexes may have much more entries than the
        number of rows in the table. We estimate the number here.

        Note, built-in parser is always nr. 0 - see ftparser_call_initializer()
      */
      if (sort_param.keyinfo->ftkey_nr == 0)
      {
        /*
          for built-in parser the number of generated index entries
          cannot be larger than the size of the data file divided
          by the minimal word's length
        */
        sort_info.max_records=
          (ha_rows) (sort_info.filelength/ft_min_word_len+1);
      }
      else
      {
        /*
          for external plugin parser we cannot tell anything at all :(
          so, we'll use all the sort memory and start from ~10 buffpeks.
          (see _ma_create_index_by_sort)
        */
        sort_info.max_records=
          10*param->sort_buffer_length/sort_param.key_length;
      }

      sort_param.key_read=  sort_maria_ft_key_read;
      sort_param.key_write= sort_maria_ft_key_write;
    }
    else
    {
      sort_param.key_read=  sort_key_read;
      sort_param.key_write= sort_key_write;
    }

    if (sort_info.new_info->s->data_file_type == BLOCK_RECORD)
    {
      scan_inited= 1;
      if (maria_scan_init(sort_info.info))
        goto err;
    }
    if (_ma_create_index_by_sort(&sort_param,
                                 (my_bool) (!(param->testflag & T_VERBOSE)),
                                 (size_t) param->sort_buffer_length))
    {
      param->retry_repair=1;
      _ma_check_print_error(param, "Create index by sort failed");
      goto err;
    }
    DBUG_EXECUTE_IF("maria_flush_whole_log",
                    {
                      DBUG_PRINT("maria_flush_whole_log", ("now"));
                      translog_flush(translog_get_horizon());
                    });
    DBUG_EXECUTE_IF("maria_crash_create_index_by_sort",
                    {
                      DBUG_PRINT("maria_crash_create_index_by_sort", ("now"));
                      DBUG_ABORT();
                    });
    if (scan_inited)
    {
      scan_inited= 0;
      maria_scan_end(sort_info.info);
    }

    /* No need to calculate checksum again. */
    sort_param.calc_checksum= 0;
    free_root(&sort_param.wordroot, MYF(0));

    /* Set for next loop */
    sort_info.max_records= (ha_rows) sort_info.new_info->s->state.state.records;
    if (param->testflag & T_STATISTICS)
      maria_update_key_parts(sort_param.keyinfo, rec_per_key_part,
                             sort_param.unique,
                             (param->stats_method ==
                              MI_STATS_METHOD_IGNORE_NULLS ?
                              sort_param.notnull : NULL),
                             (ulonglong) share->state.state.records);
    maria_set_key_active(share->state.key_map, sort_param.key);
    DBUG_PRINT("repair", ("set enabled index #: %u", sort_param.key));

    if (_ma_flush_table_files_before_swap(param, info))
      goto err;

    if (sort_param.fix_datafile)
    {
      param->read_cache.end_of_file=sort_param.filepos;
      if (maria_write_data_suffix(&sort_info,1) ||
          end_io_cache(&sort_info.new_info->rec_cache))
      {
        _ma_check_print_error(param, "Got error when flushing row cache");
	goto err;
      }
      sort_info.new_info->opt_flag&= ~WRITE_CACHE_USED;

      if (param->testflag & T_SAFE_REPAIR)
      {
	/* Don't repair if we loosed more than one row */
	if (share->state.state.records+1 < start_records)
	{
          _ma_check_print_error(param,
                                "Rows lost; Aborting because safe repair was "
                                "requested");
          share->state.state.records=start_records;
	  goto err;
	}
      }

      sort_info.new_info->s->state.state.data_file_length= sort_param.filepos;
      if (sort_info.new_info != sort_info.info)
      {
        MARIA_STATE_INFO save_state= sort_info.new_info->s->state;
        if (maria_close(sort_info.new_info))
        {
          _ma_check_print_error(param, "Got error %d on close", my_errno);
          goto err;
        }
        copy_data_file_state(&share->state, &save_state);
        new_file= -1;
        sort_info.new_info= info;
        info->rec_cache.file= info->dfile.file;
      }

      share->state.version=(ulong) time((time_t*) 0);	/* Force reopen */

      /* Replace the actual file with the temporary file */
      if (new_file >= 0)
      {
        my_close(new_file, MYF(MY_WME));
        new_file= -1;
      }
      change_data_file_descriptor(info, -1);
      if (maria_change_to_newfile(share->data_file_name.str, MARIA_NAME_DEXT,
                                  DATA_TMP_EXT, param->backup_time,
                                  (param->testflag & T_BACKUP_DATA ?
                                   MYF(MY_REDEL_MAKE_BACKUP): MYF(0)) |
                                  sync_dir) ||
          _ma_open_datafile(info, share, NullS, -1))
      {
        _ma_check_print_error(param, "Couldn't change to new data file");
        goto err;
      }
      if (param->testflag & T_UNPACK)
        restore_data_file_type(share);

      org_header_length= share->pack.header_length;
      sort_info.org_data_file_type= share->data_file_type;
      sort_info.filelength= share->state.state.data_file_length;
      sort_param.fix_datafile=0;
    }
    else
      share->state.state.data_file_length=sort_param.max_pos;

    param->read_cache.file= info->dfile.file;	/* re-init read cache */
    reinit_io_cache(&param->read_cache,READ_CACHE,share->pack.header_length,
                    1,1);
  }

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }

  if (rep_quick && del+sort_info.dupp != share->state.state.del)
  {
    _ma_check_print_error(param,"Couldn't fix table with quick recovery: "
                          "Found wrong number of deleted records");
    _ma_check_print_error(param,"Run recovery again without -q");
    got_error=1;
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (rep_quick && (param->testflag & T_FORCE_UNIQUENESS))
  {
    my_off_t skr= (share->state.state.data_file_length +
                   (sort_info.org_data_file_type == COMPRESSED_RECORD) ?
                   MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (sort_info.org_data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength)
      if (my_chsize(info->dfile.file, skr, 0, MYF(0)))
	_ma_check_print_warning(param,
			       "Can't change size of datafile,  error: %d",
			       my_errno);
  }

  if (param->testflag & T_CALC_CHECKSUM)
    share->state.state.checksum=param->glob_crc;

  if (my_chsize(share->kfile.file, share->state.state.key_file_length, 0,
                MYF(0)))
    _ma_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   my_errno);

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != share->state.state.records)
      printf("Data records: %s\n", llstr(share->state.state.records,llbuff));
  }
  if (sort_info.dupp)
    _ma_check_print_warning(param,
                            "%s records have been removed",
                            llstr(sort_info.dupp,llbuff));
  got_error=0;
  /* If invoked by external program that uses thr_lock */
  if (&share->state.state != info->state)
    *info->state= *info->state_start= share->state.state;

err:
  if (scan_inited)
    maria_scan_end(sort_info.info);
  _ma_reset_state(info);

  VOID(end_io_cache(&sort_info.new_info->rec_cache));
  VOID(end_io_cache(&param->read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  sort_info.new_info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  if (got_error)
  {
    if (! param->error_printed)
      _ma_check_print_error(param,"%d when fixing table",my_errno);
    (void)_ma_flush_table_files_before_swap(param, info);
    if (sort_info.new_info && sort_info.new_info != sort_info.info)
    {
      unuse_data_file_descriptor(sort_info.new_info);
      maria_close(sort_info.new_info);
    }
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_delete(param->temp_filename, MYF(MY_WME)));
    }
    maria_mark_crashed_on_repair(info);
  }
  else
  {
    if (key_map == share->state.key_map)
      share->state.changed&= ~STATE_NOT_OPTIMIZED_KEYS;
    /*
      Now that we have flushed and forced everything, we can bump
      create_rename_lsn:
    */
    DBUG_EXECUTE_IF("maria_flush_whole_log",
                    {
                      DBUG_PRINT("maria_flush_whole_log", ("now"));
                      translog_flush(translog_get_horizon());
                    });
    DBUG_EXECUTE_IF("maria_crash_repair",
                    {
                      DBUG_PRINT("maria_crash_repair", ("now"));
                      DBUG_ABORT();
                    });
  }
  share->state.changed|= STATE_NOT_SORTED_PAGES;
  if (!rep_quick)
    share->state.changed&= ~(STATE_NOT_OPTIMIZED_ROWS | STATE_NOT_ZEROFILLED |
                             STATE_NOT_MOVABLE);

  /* If caller had disabled logging it's not up to us to re-enable it */
  if (reenable_logging)
    _ma_reenable_logging_for_table(info, FALSE);
  restore_table_state_after_repair(info, &backup_share);

  my_free(sort_param.rec_buff, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.key_block, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.ft_buf, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(got_error);
}


/*
  Threaded repair of table using sorting

  SYNOPSIS
    maria_repair_parallel()
    param		Repair parameters
    info		MARIA handler to repair
    name		Name of table (for warnings)
    rep_quick		set to <> 0 if we should not change data file

  DESCRIPTION
    Same as maria_repair_by_sort but do it multithreaded
    Each key is handled by a separate thread.
    TODO: make a number of threads a parameter

    In parallel repair we use one thread per index. There are two modes:

    Quick

      Only the indexes are rebuilt. All threads share a read buffer.
      Every thread that needs fresh data in the buffer enters the shared
      cache lock. The last thread joining the lock reads the buffer from
      the data file and wakes all other threads.

    Non-quick

      The data file is rebuilt and all indexes are rebuilt to point to
      the new record positions. One thread is the master thread. It
      reads from the old data file and writes to the new data file. It
      also creates one of the indexes. The other threads read from a
      buffer which is filled by the master. If they need fresh data,
      they enter the shared cache lock. If the masters write buffer is
      full, it flushes it to the new data file and enters the shared
      cache lock too. When all threads joined in the lock, the master
      copies its write buffer to the read buffer for the other threads
      and wakes them.

  RESULT
    0	ok
    <>0	Error
*/

int maria_repair_parallel(HA_CHECK *param, register MARIA_HA *info,
			const char * name, my_bool rep_quick)
{
#ifndef THREAD
  return maria_repair_by_sort(param, info, name, rep_quick);
#else
  int got_error;
  uint i,key, total_key_length, istep;
  ha_rows start_records;
  my_off_t new_header_length,del;
  File new_file;
  MARIA_SORT_PARAM *sort_param=0, tmp_sort_param;
  MARIA_SHARE *share= info->s;
  double  *rec_per_key_part;
  HA_KEYSEG *keyseg;
  char llbuff[22];
  IO_CACHE new_data_cache; /* For non-quick repair. */
  IO_CACHE_SHARE io_share;
  MARIA_SORT_INFO sort_info;
  MARIA_SHARE backup_share;
  ulonglong key_map;
  pthread_attr_t thr_attr;
  myf sync_dir= ((share->now_transactional && !share->temporary) ?
                 MY_SYNC_DIR : 0);
  my_bool reenable_logging= 0;
  DBUG_ENTER("maria_repair_parallel");
  LINT_INIT(key_map);

  got_error= 1;
  new_file= -1;
  start_records= share->state.state.records;
  if (!(param->testflag & T_SILENT))
  {
    printf("- parallel recovering (with sort) MARIA-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records, llbuff));
  }

  if (initialize_variables_for_repair(param, &sort_info, &tmp_sort_param, info,
                                      rep_quick, &backup_share))
    goto err;

  if ((reenable_logging= share->now_transactional))
    _ma_tmp_disable_logging_for_table(info, 0);

  new_header_length= ((param->testflag & T_UNPACK) ? 0 :
                      share->pack.header_length);

  /*
    Quick repair (not touching data file, rebuilding indexes):
    {
      Read cache is (HA_CHECK *param)->read_cache using info->dfile.file.
    }

    Non-quick repair (rebuilding data file and indexes):
    {
      Master thread:

        Read  cache is (HA_CHECK *param)->read_cache using info->dfile.file.
        Write cache is (MARIA_INFO *info)->rec_cache using new_file.

      Slave threads:

        Read cache is new_data_cache synced to master rec_cache.

      The final assignment of the filedescriptor for rec_cache is done
      after the cache creation.

      Don't check file size on new_data_cache, as the resulting file size
      is not known yet.

      As rec_cache and new_data_cache are synced, write_buffer_length is
      used for the read cache 'new_data_cache'. Both start at the same
      position 'new_header_length'.
    }
  */
  DBUG_PRINT("info", ("is quick repair: %d", (int) rep_quick));

  /* Initialize pthread structures before goto err. */
  pthread_mutex_init(&sort_info.mutex, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&sort_info.cond, 0);

  if (!(sort_info.key_block=
	alloc_key_blocks(param, (uint) param->sort_key_blocks,
			 share->base.max_key_block_length)) ||
      init_io_cache(&param->read_cache, info->dfile.file,
                    (uint) param->read_buffer_length,
                    READ_CACHE, share->pack.header_length, 1, MYF(MY_WME)) ||
      (!rep_quick &&
       (init_io_cache(&info->rec_cache, info->dfile.file,
                      (uint) param->write_buffer_length,
                      WRITE_CACHE, new_header_length, 1,
                      MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw) ||
        init_io_cache(&new_data_cache, -1,
                      (uint) param->write_buffer_length,
                      READ_CACHE, new_header_length, 1,
                      MYF(MY_WME | MY_DONT_CHECK_FILESIZE)))))
    goto err;
  sort_info.key_block_end=sort_info.key_block+param->sort_key_blocks;
  info->opt_flag|=WRITE_CACHE_USED;
  info->rec_cache.file= info->dfile.file;         /* for sort_delete_record */

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file= my_create(fn_format(param->temp_filename,
                                       share->data_file_name.str, "",
                                       DATA_TMP_EXT,
                                       2+4),
                             0,param->tmpfile_createflag,
                             MYF(0))) < 0)
    {
      _ma_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (new_header_length &&
        maria_filecopy(param, new_file, info->dfile.file,0L,new_header_length,
                       "datafile-header"))
      goto err;
    if (param->testflag & T_UNPACK)
      restore_data_file_type(share);
    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
  }

  /* Optionally drop indexes and optionally modify the key_map. */
  maria_drop_all_indexes(param, info, FALSE);
  key_map= share->state.key_map;
  if (param->testflag & T_CREATE_MISSING_KEYS)
  {
    /* Invert the copied key_map to recreate all disabled indexes. */
    key_map= ~key_map;
  }

  param->read_cache.end_of_file= sort_info.filelength;

  /*
    +1 below is required hack for parallel repair mode.
    The share->state.state.records value, that is compared later
    to sort_info.max_records and cannot exceed it, is
    increased in sort_key_write. In maria_repair_by_sort, sort_key_write
    is called after sort_key_read, where the comparison is performed,
    but in parallel mode master thread can call sort_key_write
    before some other repair thread calls sort_key_read.
    Furthermore I'm not even sure +1 would be enough.
    May be sort_info.max_records shold be always set to max value in
    parallel mode.
  */
  sort_info.max_records++;

  del=share->state.state.del;

  if (!(sort_param=(MARIA_SORT_PARAM *)
        my_malloc((uint) share->base.keys *
		  (sizeof(MARIA_SORT_PARAM) + share->base.pack_reclength),
		  MYF(MY_ZEROFILL))))
  {
    _ma_check_print_error(param,"Not enough memory for key!");
    goto err;
  }
  total_key_length=0;
  rec_per_key_part= param->new_rec_per_key_part;
  share->state.state.records=share->state.state.del=share->state.split=0;
  share->state.state.empty=0;

  for (i=key=0, istep=1 ; key < share->base.keys ;
       rec_per_key_part+=sort_param[i].keyinfo->keysegs, i+=istep, key++)
  {
    sort_param[i].key=key;
    sort_param[i].keyinfo=share->keyinfo+key;
    sort_param[i].seg=sort_param[i].keyinfo->seg;
    /*
      Skip this index if it is marked disabled in the copied
      (and possibly inverted) key_map.
    */
    if (! maria_is_key_active(key_map, key))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part+
		      (uint) (rec_per_key_part - param->new_rec_per_key_part)),
	     sort_param[i].keyinfo->keysegs*sizeof(*rec_per_key_part));
      istep=0;
      continue;
    }
    istep=1;
    if ((!(param->testflag & T_SILENT)))
      printf ("- Fixing index %d\n",key+1);
    if (sort_param[i].keyinfo->flag & HA_FULLTEXT)
    {
      sort_param[i].key_read=sort_maria_ft_key_read;
      sort_param[i].key_write=sort_maria_ft_key_write;
    }
    else
    {
      sort_param[i].key_read=sort_key_read;
      sort_param[i].key_write=sort_key_write;
    }
    sort_param[i].key_cmp=sort_key_cmp;
    sort_param[i].lock_in_memory=maria_lock_memory;
    sort_param[i].tmpdir=param->tmpdir;
    sort_param[i].sort_info=&sort_info;
    sort_param[i].master=0;
    sort_param[i].fix_datafile=0;
    sort_param[i].calc_checksum= 0;

    sort_param[i].filepos=new_header_length;
    sort_param[i].max_pos=sort_param[i].pos=share->pack.header_length;

    sort_param[i].record= (((uchar *)(sort_param+share->base.keys))+
                          (share->base.pack_reclength * i));
    if (_ma_alloc_buffer(&sort_param[i].rec_buff, &sort_param[i].rec_buff_size,
                         share->base.default_rec_buff_size))
    {
      _ma_check_print_error(param,"Not enough memory!");
      goto err;
    }
    sort_param[i].key_length=share->rec_reflength;
    for (keyseg=sort_param[i].seg; keyseg->type != HA_KEYTYPE_END;
	 keyseg++)
    {
      sort_param[i].key_length+=keyseg->length;
      if (keyseg->flag & HA_SPACE_PACK)
        sort_param[i].key_length+=get_pack_length(keyseg->length);
      if (keyseg->flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
        sort_param[i].key_length+=2 + test(keyseg->length >= 127);
      if (keyseg->flag & HA_NULL_PART)
        sort_param[i].key_length++;
    }
    total_key_length+=sort_param[i].key_length;

    if (sort_param[i].keyinfo->flag & HA_FULLTEXT)
    {
      uint ft_max_word_len_for_sort=
        (FT_MAX_WORD_LEN_FOR_SORT *
         sort_param[i].keyinfo->seg->charset->mbmaxlen);
      sort_param[i].key_length+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
      init_alloc_root(&sort_param[i].wordroot, FTPARSER_MEMROOT_ALLOC_SIZE, 0);
    }
  }
  sort_info.total_keys=i;
  sort_param[0].master= 1;
  sort_param[0].fix_datafile= ! rep_quick;
  sort_param[0].calc_checksum= test(param->testflag & T_CALC_CHECKSUM);

  if (!maria_ftparser_alloc_param(info))
    goto err;

  sort_info.got_error=0;
  pthread_mutex_lock(&sort_info.mutex);

  /*
    Initialize the I/O cache share for use with the read caches and, in
    case of non-quick repair, the write cache. When all threads join on
    the cache lock, the writer copies the write cache contents to the
    read caches.
  */
  if (i > 1)
  {
    if (rep_quick)
      init_io_cache_share(&param->read_cache, &io_share, NULL, i);
    else
      init_io_cache_share(&new_data_cache, &io_share, &info->rec_cache, i);
  }
  else
    io_share.total_threads= 0; /* share not used */

  (void) pthread_attr_init(&thr_attr);
  (void) pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);

  for (i=0 ; i < sort_info.total_keys ; i++)
  {
    /*
      Copy the properly initialized IO_CACHE structure so that every
      thread has its own copy. In quick mode param->read_cache is shared
      for use by all threads. In non-quick mode all threads but the
      first copy the shared new_data_cache, which is synchronized to the
      write cache of the first thread. The first thread copies
      param->read_cache, which is not shared.
    */
    sort_param[i].read_cache= ((rep_quick || !i) ? param->read_cache :
                               new_data_cache);
    DBUG_PRINT("io_cache_share", ("thread: %u  read_cache: 0x%lx",
                                  i, (long) &sort_param[i].read_cache));

    /*
      two approaches: the same amount of memory for each thread
      or the memory for the same number of keys for each thread...
      In the second one all the threads will fill their sort_buffers
      (and call write_keys) at the same time, putting more stress on i/o.
    */
    sort_param[i].sortbuff_size=
#ifndef USING_SECOND_APPROACH
      param->sort_buffer_length/sort_info.total_keys;
#else
      param->sort_buffer_length*sort_param[i].key_length/total_key_length;
#endif
    if (pthread_create(&sort_param[i].thr, &thr_attr,
		       _ma_thr_find_all_keys,
		       (void *) (sort_param+i)))
    {
      _ma_check_print_error(param,"Cannot start a repair thread");
      /* Cleanup: Detach from the share. Avoid others to be blocked. */
      if (io_share.total_threads)
        remove_io_thread(&sort_param[i].read_cache);
      DBUG_PRINT("error", ("Cannot start a repair thread"));
      sort_info.got_error=1;
    }
    else
      sort_info.threads_running++;
  }
  (void) pthread_attr_destroy(&thr_attr);

  /* waiting for all threads to finish */
  while (sort_info.threads_running)
    pthread_cond_wait(&sort_info.cond, &sort_info.mutex);
  pthread_mutex_unlock(&sort_info.mutex);

  if ((got_error= _ma_thr_write_keys(sort_param)))
  {
    param->retry_repair=1;
    goto err;
  }
  got_error=1;				/* Assume the following may go wrong */

  if (_ma_flush_table_files_before_swap(param, info))
    goto err;

  if (sort_param[0].fix_datafile)
  {
    /*
      Append some nulls to the end of a memory mapped file. Destroy the
      write cache. The master thread did already detach from the share
      by remove_io_thread() in sort.c:thr_find_all_keys().
    */
    if (maria_write_data_suffix(&sort_info,1) ||
        end_io_cache(&info->rec_cache))
      goto err;
    if (param->testflag & T_SAFE_REPAIR)
    {
      /* Don't repair if we loosed more than one row */
      if (share->state.state.records+1 < start_records)
      {
        share->state.state.records=start_records;
        goto err;
      }
    }
    share->state.state.data_file_length= sort_param->filepos;
    /* Only whole records */
    share->state.version= (ulong) time((time_t*) 0);
    /*
      Exchange the data file descriptor of the table, so that we use the
      new file from now on.
     */
    my_close(info->dfile.file, MYF(0));
    info->dfile.file= new_file;
    share->pack.header_length=(ulong) new_header_length;
  }
  else
    share->state.state.data_file_length=sort_param->max_pos;

  if (rep_quick && del+sort_info.dupp != share->state.state.del)
  {
    _ma_check_print_error(param,"Couldn't fix table with quick recovery: "
                          "Found wrong number of deleted records");
    _ma_check_print_error(param,"Run recovery again without -q");
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (rep_quick && (param->testflag & T_FORCE_UNIQUENESS))
  {
    my_off_t skr= (share->state.state.data_file_length +
                   (sort_info.org_data_file_type == COMPRESSED_RECORD) ?
                   MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (sort_info.org_data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength)
      if (my_chsize(info->dfile.file, skr, 0, MYF(0)))
	_ma_check_print_warning(param,
			       "Can't change size of datafile,  error: %d",
			       my_errno);
  }
  if (param->testflag & T_CALC_CHECKSUM)
    share->state.state.checksum=param->glob_crc;

  if (my_chsize(share->kfile.file, share->state.state.key_file_length, 0,
                MYF(0)))
    _ma_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
                            my_errno);

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != share->state.state.records)
      printf("Data records: %s\n", llstr(share->state.state.records,llbuff));
  }
  if (sort_info.dupp)
    _ma_check_print_warning(param,
                            "%s records have been removed",
                            llstr(sort_info.dupp,llbuff));
  got_error=0;
  /* If invoked by external program that uses thr_lock */
  if (&share->state.state != info->state)
    *info->state= *info->state_start= share->state.state;

err:
  _ma_reset_state(info);

  /*
    Destroy the write cache. The master thread did already detach from
    the share by remove_io_thread() or it was not yet started (if the
    error happend before creating the thread).
  */
  VOID(end_io_cache(&sort_info.new_info->rec_cache));
  VOID(end_io_cache(&param->read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  sort_info.new_info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  /*
    Destroy the new data cache in case of non-quick repair. All slave
    threads did either detach from the share by remove_io_thread()
    already or they were not yet started (if the error happend before
    creating the threads).
  */
  if (!rep_quick)
    VOID(end_io_cache(&new_data_cache));
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      my_close(new_file,MYF(0));
      info->dfile.file= new_file= -1;
      if (maria_change_to_newfile(share->data_file_name.str, MARIA_NAME_DEXT,
                                  DATA_TMP_EXT, param->backup_time,
                                  MYF((param->testflag & T_BACKUP_DATA ?
                                       MY_REDEL_MAKE_BACKUP : 0) |
                                      sync_dir)) ||
	  _ma_open_datafile(info,share, NullS, -1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      _ma_check_print_error(param,"%d when fixing table",my_errno);
    (void)_ma_flush_table_files_before_swap(param, info);
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_delete(param->temp_filename, MYF(MY_WME)));
      if (info->dfile.file == new_file)
	info->dfile.file= -1;
    }
    maria_mark_crashed_on_repair(info);
  }
  else if (key_map == share->state.key_map)
    share->state.changed&= ~STATE_NOT_OPTIMIZED_KEYS;
  share->state.changed|= STATE_NOT_SORTED_PAGES;
  if (!rep_quick)
    share->state.changed&= ~(STATE_NOT_OPTIMIZED_ROWS | STATE_NOT_ZEROFILLED |
                             STATE_NOT_MOVABLE);

  pthread_cond_destroy (&sort_info.cond);
  pthread_mutex_destroy(&sort_info.mutex);

  /* If caller had disabled logging it's not up to us to re-enable it */
  if (reenable_logging)
    _ma_reenable_logging_for_table(info, FALSE);
  restore_table_state_after_repair(info, &backup_share);

  my_free(sort_info.ft_buf, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.key_block,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_param,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  if (!got_error && (param->testflag & T_UNPACK))
    restore_data_file_type(share);
  DBUG_RETURN(got_error);
#endif /* THREAD */
}

	/* Read next record and return next key */

static int sort_key_read(MARIA_SORT_PARAM *sort_param, uchar *key)
{
  int error;
  MARIA_SORT_INFO *sort_info= sort_param->sort_info;
  MARIA_HA *info= sort_info->info;
  MARIA_KEY int_key;
  DBUG_ENTER("sort_key_read");

  if ((error=sort_get_next_record(sort_param)))
    DBUG_RETURN(error);
  if (info->s->state.state.records == sort_info->max_records)
  {
    _ma_check_print_error(sort_info->param,
			 "Key %d - Found too many records; Can't continue",
                         sort_param->key+1);
    DBUG_RETURN(1);
  }
  if (_ma_sort_write_record(sort_param))
    DBUG_RETURN(1);

  (*info->s->keyinfo[sort_param->key].make_key)(info, &int_key,
                                                sort_param->key, key,
                                                sort_param->record,
                                                sort_param->current_filepos,
                                                0);
  sort_param->real_key_length= int_key.data_length + int_key.ref_length;
#ifdef HAVE_valgrind
  bzero(key+sort_param->real_key_length,
	(sort_param->key_length-sort_param->real_key_length));
#endif
  DBUG_RETURN(0);
} /* sort_key_read */


static int sort_maria_ft_key_read(MARIA_SORT_PARAM *sort_param, uchar *key)
{
  int error;
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  MARIA_HA *info=sort_info->info;
  FT_WORD *wptr=0;
  MARIA_KEY int_key;
  DBUG_ENTER("sort_maria_ft_key_read");

  if (!sort_param->wordlist)
  {
    for (;;)
    {
      free_root(&sort_param->wordroot, MYF(MY_MARK_BLOCKS_FREE));
      if ((error=sort_get_next_record(sort_param)))
        DBUG_RETURN(error);
      if ((error= _ma_sort_write_record(sort_param)))
        DBUG_RETURN(error);
      if (!(wptr= _ma_ft_parserecord(info,sort_param->key,sort_param->record,
                                     &sort_param->wordroot)))

        DBUG_RETURN(1);
      if (wptr->pos)
        break;
    }
    sort_param->wordptr=sort_param->wordlist=wptr;
  }
  else
  {
    error=0;
    wptr=(FT_WORD*)(sort_param->wordptr);
  }

  _ma_ft_make_key(info, &int_key, sort_param->key, key, wptr++,
                  sort_param->current_filepos);
  sort_param->real_key_length= int_key.data_length + int_key.ref_length;

#ifdef HAVE_valgrind
  if (sort_param->key_length > sort_param->real_key_length)
    bzero(key+sort_param->real_key_length,
	  (sort_param->key_length-sort_param->real_key_length));
#endif
  if (!wptr->pos)
  {
    free_root(&sort_param->wordroot, MYF(MY_MARK_BLOCKS_FREE));
    sort_param->wordlist=0;
  }
  else
    sort_param->wordptr=(void*)wptr;

  DBUG_RETURN(error);
} /* sort_maria_ft_key_read */


/*
  Read next record from file using parameters in sort_info.

  SYNOPSIS
    sort_get_next_record()
      sort_param                Information about and for the sort process

  NOTES
    Dynamic Records With Non-Quick Parallel Repair

    For non-quick parallel repair we use a synchronized read/write
    cache. This means that one thread is the master who fixes the data
    file by reading each record from the old data file and writing it
    to the new data file. By doing this the records in the new data
    file are written contiguously. Whenever the write buffer is full,
    it is copied to the read buffer. The slaves read from the read
    buffer, which is not associated with a file. Thus read_cache.file
    is -1. When using _mi_read_cache(), the slaves must always set
    flag to READING_NEXT so that the function never tries to read from
    file. This is safe because the records are contiguous. There is no
    need to read outside the cache. This condition is evaluated in the
    variable 'parallel_flag' for quick reference. read_cache.file must
    be >= 0 in every other case.

  RETURN
    -1          end of file
    0           ok
                sort_param->current_filepos points to record position.
                sort_param->record contains record
                sort_param->max_pos contains position to last byte read
    > 0         error
*/

static int sort_get_next_record(MARIA_SORT_PARAM *sort_param)
{
  int searching;
  int parallel_flag;
  uint found_record,b_type,left_length;
  my_off_t pos;
  MARIA_BLOCK_INFO block_info;
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  HA_CHECK *param=sort_info->param;
  MARIA_HA *info=sort_info->info;
  MARIA_SHARE *share= info->s;
  char llbuff[22],llbuff2[22];
  DBUG_ENTER("sort_get_next_record");

  if (_ma_killed_ptr(param))
    DBUG_RETURN(1);

  switch (sort_info->org_data_file_type) {
  case BLOCK_RECORD:
  {
    for (;;)
    {
      int flag;
      /*
        Assume table is transactional and it had LSN pages in the
        cache. Repair has flushed them, left data pages stay in
        cache, and disabled transactionality (so share's current page
        type is PLAIN); page cache would assert if it finds a cached LSN page
        while _ma_scan_block_record() requested a PLAIN page. So we use
        UNKNOWN.
      */
      enum pagecache_page_type save_page_type= share->page_type;
      share->page_type= PAGECACHE_READ_UNKNOWN_PAGE;
      if (info != sort_info->new_info)
      {
        /* Safe scanning */
        flag= _ma_safe_scan_block_record(sort_info, info,
                                         sort_param->record);
      }
      else
      {
        /*
          Scan on clean table.
          It requires a reliable data_file_length so we set it.
        */
        share->state.state.data_file_length= sort_info->filelength;
        info->cur_row.trid= 0;
        flag= _ma_scan_block_record(info, sort_param->record,
                                    info->cur_row.nextpos, 1);
        set_if_bigger(param->max_found_trid, info->cur_row.trid);
        if (info->cur_row.trid > param->max_trid)
        {
          _ma_check_print_not_visible_error(param, info->cur_row.trid);
          flag= HA_ERR_ROW_NOT_VISIBLE;
        }
      }
      share->page_type= save_page_type;
      if (!flag)
      {
	if (sort_param->calc_checksum)
        {
          ha_checksum checksum;
          checksum= (*share->calc_check_checksum)(info, sort_param->record);
          if (share->calc_checksum &&
              info->cur_row.checksum != (checksum & 255))
          {
            if (param->testflag & T_VERBOSE)
            {
              record_pos_to_txt(info, info->cur_row.lastpos, llbuff);
              _ma_check_print_info(param,
                                   "Found record with wrong checksum at %s",
                                   llbuff);
            }
            continue;
          }
          info->cur_row.checksum= checksum;
	  param->glob_crc+= checksum;
        }
        sort_param->start_recpos= sort_param->current_filepos=
          info->cur_row.lastpos;
        DBUG_RETURN(0);
      }
      if (flag == HA_ERR_END_OF_FILE)
      {
        sort_param->max_pos= share->state.state.data_file_length;
        DBUG_RETURN(-1);
      }
      /* Retry only if wrong record, not if disk error */
      if (flag != HA_ERR_WRONG_IN_RECORD)
      {
        retry_if_quick(sort_param, flag);
        DBUG_RETURN(flag);
      }
    }
    break;                                      /* Impossible */
  }
  case STATIC_RECORD:
    for (;;)
    {
      if (my_b_read(&sort_param->read_cache,sort_param->record,
		    share->base.pack_reclength))
      {
	if (sort_param->read_cache.error)
	  param->out_flag |= O_DATA_LOST;
        retry_if_quick(sort_param, my_errno);
	DBUG_RETURN(-1);
      }
      sort_param->start_recpos=sort_param->pos;
      if (!sort_param->fix_datafile)
      {
	sort_param->current_filepos= sort_param->pos;
        if (sort_param->master)
	  share->state.split++;
      }
      sort_param->max_pos=(sort_param->pos+=share->base.pack_reclength);
      if (*sort_param->record)
      {
	if (sort_param->calc_checksum)
	  param->glob_crc+= (info->cur_row.checksum=
			     _ma_static_checksum(info,sort_param->record));
	DBUG_RETURN(0);
      }
      if (!sort_param->fix_datafile && sort_param->master)
      {
	share->state.state.del++;
	share->state.state.empty+=share->base.pack_reclength;
      }
    }
  case DYNAMIC_RECORD:
  {
    uchar *to;
    ha_checksum checksum= 0;
    LINT_INIT(to);

    pos=sort_param->pos;
    searching=(sort_param->fix_datafile && (param->testflag & T_EXTEND));
    parallel_flag= (sort_param->read_cache.file < 0) ? READING_NEXT : 0;
    for (;;)
    {
      found_record=block_info.second_read= 0;
      left_length=1;
      if (searching)
      {
	pos=MY_ALIGN(pos,MARIA_DYN_ALIGN_SIZE);
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	sort_param->start_recpos=pos;
      }
      do
      {
	if (pos > sort_param->max_pos)
	  sort_param->max_pos=pos;
	if (pos & (MARIA_DYN_ALIGN_SIZE-1))
	{
	  if ((param->testflag & T_VERBOSE) || searching == 0)
	    _ma_check_print_info(param,"Wrong aligned block at %s",
				llstr(pos,llbuff));
	  if (searching)
	    goto try_next;
	}
	if (found_record && pos == param->search_after_block)
	  _ma_check_print_info(param,"Block: %s used by record at %s",
		     llstr(param->search_after_block,llbuff),
		     llstr(sort_param->start_recpos,llbuff2));
	if (_ma_read_cache(&sort_param->read_cache,
                           block_info.header, pos,
			   MARIA_BLOCK_INFO_HEADER_LENGTH,
			   (! found_record ? READING_NEXT : 0) |
			   parallel_flag | READING_HEADER))
	{
	  if (found_record)
	  {
	    _ma_check_print_info(param,
				"Can't read whole record at %s (errno: %d)",
				llstr(sort_param->start_recpos,llbuff),errno);
	    goto try_next;
	  }
	  DBUG_RETURN(-1);
	}
	if (searching && ! sort_param->fix_datafile)
	{
	  param->error_printed=1;
          param->retry_repair=1;
          param->testflag|=T_RETRY_WITHOUT_QUICK;
	  DBUG_RETURN(1);	/* Something wrong with data */
	}
	b_type= _ma_get_block_info(&block_info,-1,pos);
	if ((b_type & (BLOCK_ERROR | BLOCK_FATAL_ERROR)) ||
	   ((b_type & BLOCK_FIRST) &&
	     (block_info.rec_len < (uint) share->base.min_pack_length ||
	      block_info.rec_len > (uint) share->base.max_pack_length)))
	{
	  uint i;
	  if (param->testflag & T_VERBOSE || searching == 0)
	    _ma_check_print_info(param,
				"Wrong bytesec: %3d-%3d-%3d at %10s; Skipped",
		       block_info.header[0],block_info.header[1],
		       block_info.header[2],llstr(pos,llbuff));
	  if (found_record)
	    goto try_next;
	  block_info.second_read=0;
	  searching=1;
	  /* Search after block in read header string */
	  for (i=MARIA_DYN_ALIGN_SIZE ;
	       i < MARIA_BLOCK_INFO_HEADER_LENGTH ;
	       i+= MARIA_DYN_ALIGN_SIZE)
	    if (block_info.header[i] >= 1 &&
		block_info.header[i] <= MARIA_MAX_DYN_HEADER_BYTE)
	      break;
	  pos+=(ulong) i;
	  sort_param->start_recpos=pos;
	  continue;
	}
	if (b_type & BLOCK_DELETED)
	{
	  my_bool error=0;
	  if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	      share->base.min_block_length)
	  {
	    if (!searching)
	      _ma_check_print_info(param,
                                   "Deleted block with impossible length %lu "
                                   "at %s",
                                   block_info.block_len,llstr(pos,llbuff));
	    error=1;
	  }
	  else
	  {
	    if ((block_info.next_filepos != HA_OFFSET_ERROR &&
		 block_info.next_filepos >=
		 share->state.state.data_file_length) ||
		(block_info.prev_filepos != HA_OFFSET_ERROR &&
		 block_info.prev_filepos >=
                 share->state.state.data_file_length))
	    {
	      if (!searching)
		_ma_check_print_info(param,
				    "Delete link points outside datafile at "
                                     "%s",
                                     llstr(pos,llbuff));
	      error=1;
	    }
	  }
	  if (error)
	  {
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MARIA_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	    block_info.second_read=0;
	    continue;
	  }
	}
	else
	{
	  if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	      share->base.min_block_length ||
	      block_info.block_len > (uint) share->base.max_pack_length+
	      MARIA_SPLIT_LENGTH)
	  {
	    if (!searching)
	      _ma_check_print_info(param,
                                   "Found block with impossible length %lu "
                                   "at %s; Skipped",
                                   block_info.block_len+
                                   (uint) (block_info.filepos-pos),
                                   llstr(pos,llbuff));
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MARIA_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	    block_info.second_read=0;
	    continue;
	  }
	}
	if (b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
	{
          if (!sort_param->fix_datafile && sort_param->master &&
              (b_type & BLOCK_DELETED))
	  {
	    share->state.state.empty+=block_info.block_len;
	    share->state.state.del++;
	    share->state.split++;
	  }
	  if (found_record)
	    goto try_next;
	  if (searching)
	  {
	    pos+=MARIA_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	  }
	  else
	    pos=block_info.filepos+block_info.block_len;
	  block_info.second_read=0;
	  continue;
	}

	if (!sort_param->fix_datafile && sort_param->master)
	  share->state.split++;
	if (! found_record++)
	{
	  sort_param->find_length=left_length=block_info.rec_len;
	  sort_param->start_recpos=pos;
	  if (!sort_param->fix_datafile)
	    sort_param->current_filepos= sort_param->start_recpos;
	  if (sort_param->fix_datafile && (param->testflag & T_EXTEND))
	    sort_param->pos=block_info.filepos+1;
	  else
	    sort_param->pos=block_info.filepos+block_info.block_len;
	  if (share->base.blobs)
	  {
	    if (_ma_alloc_buffer(&sort_param->rec_buff,
                                 &sort_param->rec_buff_size,
                                 block_info.rec_len +
                                 share->base.extra_rec_buff_size))

	    {
	      if (param->max_record_length >= block_info.rec_len)
	      {
		_ma_check_print_error(param,"Not enough memory for blob at %s "
                                      "(need %lu)",
				     llstr(sort_param->start_recpos,llbuff),
				     (ulong) block_info.rec_len);
		DBUG_RETURN(1);
	      }
	      else
	      {
		_ma_check_print_info(param,"Not enough memory for blob at %s "
                                     "(need %lu); Row skipped",
				    llstr(sort_param->start_recpos,llbuff),
				    (ulong) block_info.rec_len);
		goto try_next;
	      }
	    }
	  }
          to= sort_param->rec_buff;
	}
	if (left_length < block_info.data_len || ! block_info.data_len)
	{
	  _ma_check_print_info(param,
			      "Found block with too small length at %s; "
                               "Skipped",
                               llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (block_info.filepos + block_info.data_len >
	    sort_param->read_cache.end_of_file)
	{
	  _ma_check_print_info(param,
			      "Found block that points outside data file "
                               "at %s",
                               llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
        /*
          Copy information that is already read. Avoid accessing data
          below the cache start. This could happen if the header
          streched over the end of the previous buffer contents.
        */
        {
          uint header_len= (uint) (block_info.filepos - pos);
          uint prefetch_len= (MARIA_BLOCK_INFO_HEADER_LENGTH - header_len);

          if (prefetch_len > block_info.data_len)
            prefetch_len= block_info.data_len;
          if (prefetch_len)
          {
            memcpy(to, block_info.header + header_len, prefetch_len);
            block_info.filepos+= prefetch_len;
            block_info.data_len-= prefetch_len;
            left_length-= prefetch_len;
            to+= prefetch_len;
          }
        }
        if (block_info.data_len &&
            _ma_read_cache(&sort_param->read_cache,to,block_info.filepos,
                           block_info.data_len,
                           (found_record == 1 ? READING_NEXT : 0) |
                           parallel_flag))
	{
	  _ma_check_print_info(param,
			      "Read error for block at: %s (error: %d); "
                               "Skipped",
			      llstr(block_info.filepos,llbuff),my_errno);
	  goto try_next;
	}
	left_length-=block_info.data_len;
	to+=block_info.data_len;
	pos=block_info.next_filepos;
	if (pos == HA_OFFSET_ERROR && left_length)
	{
	  _ma_check_print_info(param,
                               "Wrong block with wrong total length "
                               "starting at %s",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (pos + MARIA_BLOCK_INFO_HEADER_LENGTH >
            sort_param->read_cache.end_of_file)
	{
	  _ma_check_print_info(param,
                               "Found link that points at %s (outside data "
                               "file) at %s",
			      llstr(pos,llbuff2),
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
      } while (left_length);

      if (_ma_rec_unpack(info,sort_param->record,sort_param->rec_buff,
			 sort_param->find_length) != MY_FILE_ERROR)
      {
	if (sort_param->read_cache.error < 0)
	  DBUG_RETURN(1);
	if (sort_param->calc_checksum)
	  checksum= (share->calc_check_checksum)(info, sort_param->record);
	if ((param->testflag & (T_EXTEND | T_REP)) || searching)
	{
	  if (_ma_rec_check(info, sort_param->record, sort_param->rec_buff,
                            sort_param->find_length,
                            (param->testflag & T_QUICK) &&
                            sort_param->calc_checksum &&
                            test(share->calc_checksum), checksum))
	  {
	    _ma_check_print_info(param,"Found wrong packed record at %s",
				llstr(sort_param->start_recpos,llbuff));
	    goto try_next;
	  }
	}
	if (sort_param->calc_checksum)
	  param->glob_crc+= checksum;
	DBUG_RETURN(0);
      }
      if (!searching)
        _ma_check_print_info(param,"Key %d - Found wrong stored record at %s",
                            sort_param->key+1,
                            llstr(sort_param->start_recpos,llbuff));
    try_next:
      pos=(sort_param->start_recpos+=MARIA_DYN_ALIGN_SIZE);
      searching=1;
    }
  }
  case COMPRESSED_RECORD:
    for (searching=0 ;; searching=1, sort_param->pos++)
    {
      if (_ma_read_cache(&sort_param->read_cache, block_info.header,
			 sort_param->pos,
			 share->pack.ref_length,READING_NEXT))
	DBUG_RETURN(-1);
      if (searching && ! sort_param->fix_datafile)
      {
	param->error_printed=1;
        param->retry_repair=1;
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	DBUG_RETURN(1);		/* Something wrong with data */
      }
      sort_param->start_recpos=sort_param->pos;
      if (_ma_pack_get_block_info(info, &sort_param->bit_buff, &block_info,
                                  &sort_param->rec_buff,
                                  &sort_param->rec_buff_size, -1,
                                  sort_param->pos))
	DBUG_RETURN(-1);
      if (!block_info.rec_len &&
	  sort_param->pos + MEMMAP_EXTRA_MARGIN ==
	  sort_param->read_cache.end_of_file)
	DBUG_RETURN(-1);
      if (block_info.rec_len < (uint) share->min_pack_length ||
	  block_info.rec_len > (uint) share->max_pack_length)
      {
	if (! searching)
	  _ma_check_print_info(param,
                               "Found block with wrong recordlength: %lu "
                               "at %s\n",
                               block_info.rec_len,
                               llstr(sort_param->pos,llbuff));
	continue;
      }
      if (_ma_read_cache(&sort_param->read_cache, sort_param->rec_buff,
			 block_info.filepos, block_info.rec_len,
			 READING_NEXT))
      {
	if (! searching)
	  _ma_check_print_info(param,"Couldn't read whole record from %s",
			      llstr(sort_param->pos,llbuff));
	continue;
      }
#ifdef HAVE_valgrind
      bzero(sort_param->rec_buff + block_info.rec_len,
            share->base.extra_rec_buff_size);
#endif
      if (_ma_pack_rec_unpack(info, &sort_param->bit_buff, sort_param->record,
                              sort_param->rec_buff, block_info.rec_len))
      {
	if (! searching)
	  _ma_check_print_info(param,"Found wrong record at %s",
			      llstr(sort_param->pos,llbuff));
	continue;
      }
      if (!sort_param->fix_datafile)
      {
	sort_param->current_filepos= sort_param->pos;
        if (sort_param->master)
	  share->state.split++;
      }
      sort_param->max_pos= (sort_param->pos=block_info.filepos+
                            block_info.rec_len);
      info->packed_length=block_info.rec_len;

      if (sort_param->calc_checksum)
      {
        info->cur_row.checksum= (*share->calc_check_checksum)(info,
                                                                sort_param->
                                                                record);
	param->glob_crc+= info->cur_row.checksum;
      }
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);		/* Impossible */
}


/**
   @brief Write record to new file.

   @fn    _ma_sort_write_record()
   @param sort_param                Sort parameters.

   @note
   This is only called by a master thread if parallel repair is used.

   @return
   @retval  0   OK
                sort_param->current_filepos points to inserted record for
                block_records and to the place for the next record for
                other row types.
                sort_param->filepos points to end of file
  @retval   1   Error
*/

int _ma_sort_write_record(MARIA_SORT_PARAM *sort_param)
{
  int flag;
  uint length;
  ulong block_length,reclength;
  uchar *from;
  uchar block_buff[8];
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  HA_CHECK *param= sort_info->param;
  MARIA_HA *info= sort_info->new_info;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_sort_write_record");

  if (sort_param->fix_datafile)
  {
    sort_param->current_filepos= sort_param->filepos;
    switch (sort_info->new_data_file_type) {
    case BLOCK_RECORD:
      if ((sort_param->current_filepos=
           (*share->write_record_init)(info, sort_param->record)) ==
          HA_OFFSET_ERROR)
        DBUG_RETURN(1);
      /* Pointer to end of file */
      sort_param->filepos= share->state.state.data_file_length;
      break;
    case STATIC_RECORD:
      if (my_b_write(&info->rec_cache,sort_param->record,
		     share->base.pack_reclength))
      {
	_ma_check_print_error(param,"%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_param->filepos+=share->base.pack_reclength;
      share->state.split++;
      break;
    case DYNAMIC_RECORD:
      if (! info->blobs)
	from=sort_param->rec_buff;
      else
      {
	/* must be sure that local buffer is big enough */
	reclength=share->base.pack_reclength+
	  _ma_calc_total_blob_length(info,sort_param->record)+
	  ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER)+MARIA_SPLIT_LENGTH+
	  MARIA_DYN_DELETE_BLOCK_HEADER;
	if (sort_info->buff_length < reclength)
	{
	  if (!(sort_info->buff=my_realloc(sort_info->buff, (uint) reclength,
					   MYF(MY_FREE_ON_ERROR |
					       MY_ALLOW_ZERO_PTR))))
	    DBUG_RETURN(1);
	  sort_info->buff_length=reclength;
	}
	from= (uchar *) sort_info->buff+ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER);
      }
      /* We can use info->checksum here as only one thread calls this */
      info->cur_row.checksum= (*share->calc_check_checksum)(info,
                                                              sort_param->
                                                              record);
      reclength= _ma_rec_pack(info,from,sort_param->record);
      flag=0;

      do
      {
	block_length=reclength+ 3 + test(reclength >= (65520-3));
	if (block_length < share->base.min_block_length)
	  block_length=share->base.min_block_length;
	info->update|=HA_STATE_WRITE_AT_END;
	block_length=MY_ALIGN(block_length,MARIA_DYN_ALIGN_SIZE);
	if (block_length > MARIA_MAX_BLOCK_LENGTH)
	  block_length=MARIA_MAX_BLOCK_LENGTH;
	if (_ma_write_part_record(info,0L,block_length,
				  sort_param->filepos+block_length,
				  &from,&reclength,&flag))
	{
	  _ma_check_print_error(param,"%d when writing to datafile",my_errno);
	  DBUG_RETURN(1);
	}
	sort_param->filepos+=block_length;
	share->state.split++;
      } while (reclength);
      break;
    case COMPRESSED_RECORD:
      reclength=info->packed_length;
      length= _ma_save_pack_length((uint) share->pack.version, block_buff,
                               reclength);
      if (share->base.blobs)
	length+= _ma_save_pack_length((uint) share->pack.version,
	                          block_buff + length, info->blob_length);
      if (my_b_write(&info->rec_cache,block_buff,length) ||
	  my_b_write(&info->rec_cache, sort_param->rec_buff, reclength))
      {
	_ma_check_print_error(param,"%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_param->filepos+=reclength+length;
      share->state.split++;
      break;
    }
  }
  if (sort_param->master)
  {
    share->state.state.records++;
    if ((param->testflag & T_WRITE_LOOP) &&
        (share->state.state.records % WRITE_COUNT) == 0)
    {
      char llbuff[22];
      printf("%s\r", llstr(share->state.state.records,llbuff));
      VOID(fflush(stdout));
    }
  }
  DBUG_RETURN(0);
} /* _ma_sort_write_record */


/* Compare two keys from _ma_create_index_by_sort */

static int sort_key_cmp(MARIA_SORT_PARAM *sort_param, const void *a,
			const void *b)
{
  uint not_used[2];
  return (ha_key_cmp(sort_param->seg, *((uchar* const *) a),
                     *((uchar* const *) b),
		     USE_WHOLE_KEY, SEARCH_SAME, not_used));
} /* sort_key_cmp */


static int sort_key_write(MARIA_SORT_PARAM *sort_param, const uchar *a)
{
  uint diff_pos[2];
  char llbuff[22],llbuff2[22];
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  HA_CHECK *param= sort_info->param;
  int cmp;

  if (sort_info->key_block->inited)
  {
    cmp= ha_key_cmp(sort_param->seg, sort_info->key_block->lastkey,
                    a, USE_WHOLE_KEY,
                    SEARCH_FIND | SEARCH_UPDATE | SEARCH_INSERT,
                    diff_pos);
    if (param->stats_method == MI_STATS_METHOD_NULLS_NOT_EQUAL)
      ha_key_cmp(sort_param->seg, sort_info->key_block->lastkey,
                 a, USE_WHOLE_KEY,
                 SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL, diff_pos);
    else if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
    {
      diff_pos[0]= maria_collect_stats_nonulls_next(sort_param->seg,
                                                 sort_param->notnull,
                                                 sort_info->key_block->lastkey,
                                                 a);
    }
    sort_param->unique[diff_pos[0]-1]++;
  }
  else
  {
    cmp= -1;
    if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
      maria_collect_stats_nonulls_first(sort_param->seg, sort_param->notnull,
                                        a);
  }
  if ((sort_param->keyinfo->flag & HA_NOSAME) && cmp == 0)
  {
    sort_info->dupp++;
    sort_info->info->cur_row.lastpos= get_record_for_key(sort_param->keyinfo,
                                                         a);
    _ma_check_print_warning(param,
			   "Duplicate key %2u for record at %10s against "
                            "record at %10s",
                            sort_param->key + 1,
                            llstr(sort_info->info->cur_row.lastpos, llbuff),
                            llstr(get_record_for_key(sort_param->keyinfo,
                                                     sort_info->key_block->
                                                     lastkey),
                                  llbuff2));
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    if (sort_info->param->testflag & T_VERBOSE)
      _ma_print_keydata(stdout,sort_param->seg, a, USE_WHOLE_KEY);
    return (sort_delete_record(sort_param));
  }
#ifndef DBUG_OFF
  if (cmp > 0)
  {
    _ma_check_print_error(param,
			 "Internal error: Keys are not in order from sort");
    return(1);
  }
#endif
  return (sort_insert_key(sort_param, sort_info->key_block,
			  a, HA_OFFSET_ERROR));
} /* sort_key_write */


int _ma_sort_ft_buf_flush(MARIA_SORT_PARAM *sort_param)
{
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  SORT_KEY_BLOCKS *key_block=sort_info->key_block;
  MARIA_SHARE *share=sort_info->info->s;
  uint val_off, val_len;
  int error;
  SORT_FT_BUF *maria_ft_buf=sort_info->ft_buf;
  uchar *from, *to;

  val_len=share->ft2_keyinfo.keylength;
  get_key_full_length_rdonly(val_off, maria_ft_buf->lastkey);
  to= maria_ft_buf->lastkey+val_off;

  if (maria_ft_buf->buf)
  {
    /* flushing first-level tree */
    error= sort_insert_key(sort_param,key_block,maria_ft_buf->lastkey,
                           HA_OFFSET_ERROR);
    for (from=to+val_len;
         !error && from < maria_ft_buf->buf;
         from+= val_len)
    {
      memcpy(to, from, val_len);
      error= sort_insert_key(sort_param,key_block,maria_ft_buf->lastkey,
                             HA_OFFSET_ERROR);
    }
    return error;
  }
  /* flushing second-level tree keyblocks */
  error=_ma_flush_pending_blocks(sort_param);
  /* updating lastkey with second-level tree info */
  ft_intXstore(maria_ft_buf->lastkey+val_off, -maria_ft_buf->count);
  _ma_dpointer(sort_info->info->s, maria_ft_buf->lastkey+val_off+HA_FT_WLEN,
      share->state.key_root[sort_param->key]);
  /* restoring first level tree data in sort_info/sort_param */
  sort_info->key_block=sort_info->key_block_end- sort_info->param->sort_key_blocks;
  sort_param->keyinfo=share->keyinfo+sort_param->key;
  share->state.key_root[sort_param->key]=HA_OFFSET_ERROR;
  /* writing lastkey in first-level tree */
  return error ? error :
                 sort_insert_key(sort_param,sort_info->key_block,
                                 maria_ft_buf->lastkey,HA_OFFSET_ERROR);
}


static int sort_maria_ft_key_write(MARIA_SORT_PARAM *sort_param,
                                   const uchar *a)
{
  uint a_len, val_off, val_len, error;
  MARIA_SORT_INFO *sort_info= sort_param->sort_info;
  SORT_FT_BUF *ft_buf= sort_info->ft_buf;
  SORT_KEY_BLOCKS *key_block= sort_info->key_block;
  MARIA_SHARE *share= sort_info->info->s;

  val_len=HA_FT_WLEN+share->base.rec_reflength;
  get_key_full_length_rdonly(a_len, a);

  if (!ft_buf)
  {
    /*
      use two-level tree only if key_reflength fits in rec_reflength place
      and row format is NOT static - for _ma_dpointer not to garble offsets
     */
    if ((share->base.key_reflength <=
         share->base.rec_reflength) &&
        (share->options &
          (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
      ft_buf= (SORT_FT_BUF *)my_malloc(sort_param->keyinfo->block_length +
                                       sizeof(SORT_FT_BUF), MYF(MY_WME));

    if (!ft_buf)
    {
      sort_param->key_write=sort_key_write;
      return sort_key_write(sort_param, a);
    }
    sort_info->ft_buf= ft_buf;
    goto word_init_ft_buf;              /* no need to duplicate the code */
  }
  get_key_full_length_rdonly(val_off, ft_buf->lastkey);

  if (ha_compare_text(sort_param->seg->charset,
                      a+1,a_len-1,
                      ft_buf->lastkey+1,val_off-1, 0, 0)==0)
  {
    uchar *p;
    if (!ft_buf->buf)                   /* store in second-level tree */
    {
      ft_buf->count++;
      return sort_insert_key(sort_param,key_block,
                             a + a_len, HA_OFFSET_ERROR);
    }

    /* storing the key in the buffer. */
    memcpy (ft_buf->buf, (const char *)a+a_len, val_len);
    ft_buf->buf+=val_len;
    if (ft_buf->buf < ft_buf->end)
      return 0;

    /* converting to two-level tree */
    p=ft_buf->lastkey+val_off;

    while (key_block->inited)
      key_block++;
    sort_info->key_block=key_block;
    sort_param->keyinfo= &share->ft2_keyinfo;
    ft_buf->count=(ft_buf->buf - p)/val_len;

    /* flushing buffer to second-level tree */
    for (error=0; !error && p < ft_buf->buf; p+= val_len)
      error=sort_insert_key(sort_param,key_block,p,HA_OFFSET_ERROR);
    ft_buf->buf=0;
    return error;
  }

  /* flushing buffer */
  if ((error=_ma_sort_ft_buf_flush(sort_param)))
    return error;

word_init_ft_buf:
  a_len+=val_len;
  memcpy(ft_buf->lastkey, a, a_len);
  ft_buf->buf=ft_buf->lastkey+a_len;
  /*
    32 is just a safety margin here
    (at least max(val_len, sizeof(nod_flag)) should be there).
    May be better performance could be achieved if we'd put
      (sort_info->keyinfo->block_length-32)/XXX
      instead.
        TODO: benchmark the best value for XXX.
  */
  ft_buf->end= ft_buf->lastkey+ (sort_param->keyinfo->block_length-32);
  return 0;
} /* sort_maria_ft_key_write */


/* get pointer to record from a key */

static my_off_t get_record_for_key(MARIA_KEYDEF *keyinfo,
				   const uchar *key_data)
{
  MARIA_KEY key;
  key.keyinfo= keyinfo;
  key.data= (uchar*) key_data;
  key.data_length= _ma_keylength(keyinfo, key_data);
  return _ma_row_pos_from_key(&key);
} /* get_record_for_key */


/* Insert a key in sort-key-blocks */

static int sort_insert_key(MARIA_SORT_PARAM *sort_param,
			   register SORT_KEY_BLOCKS *key_block,
                           const uchar *key,
			   my_off_t prev_block)
{
  uint a_length,t_length,nod_flag;
  my_off_t filepos,key_file_length;
  uchar *anc_buff,*lastkey;
  MARIA_KEY_PARAM s_temp;
  MARIA_KEYDEF *keyinfo=sort_param->keyinfo;
  MARIA_SORT_INFO *sort_info= sort_param->sort_info;
  HA_CHECK *param=sort_info->param;
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  MARIA_KEY tmp_key;
  MARIA_HA *info= sort_info->info;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("sort_insert_key");

  anc_buff= key_block->buff;
  lastkey=key_block->lastkey;
  nod_flag= (key_block == sort_info->key_block ? 0 :
	     share->base.key_reflength);

  if (!key_block->inited)
  {
    key_block->inited=1;
    if (key_block == sort_info->key_block_end)
    {
      _ma_check_print_error(param,
                            "To many key-block-levels; "
                            "Try increasing sort_key_blocks");
      DBUG_RETURN(1);
    }
    a_length= share->keypage_header + nod_flag;
    key_block->end_pos= anc_buff + share->keypage_header;
    bzero(anc_buff, share->keypage_header);
    _ma_store_keynr(share, anc_buff, (uint) (sort_param->keyinfo -
                                            share->keyinfo));
    lastkey=0;					/* No previous key in block */
  }
  else
    a_length= _ma_get_page_used(share, anc_buff);

	/* Save pointer to previous block */
  if (nod_flag)
  {
    _ma_store_keypage_flag(share, anc_buff, KEYPAGE_FLAG_ISNOD);
    _ma_kpointer(info,key_block->end_pos,prev_block);
  }

  tmp_key.keyinfo= keyinfo;
  tmp_key.data= (uchar*) key;
  tmp_key.data_length= _ma_keylength(keyinfo, key) - share->base.rec_reflength;
  tmp_key.ref_length=  share->base.rec_reflength;

  t_length= (*keyinfo->pack_key)(&tmp_key, nod_flag,
                                 (uchar*) 0, lastkey, lastkey, &s_temp);
  (*keyinfo->store_key)(keyinfo, key_block->end_pos+nod_flag,&s_temp);
  a_length+=t_length;
  _ma_store_page_used(share, anc_buff, a_length);
  key_block->end_pos+=t_length;
  if (a_length <= share->max_index_block_size)
  {
    MARIA_KEY tmp_key2;
    tmp_key2.data= key_block->lastkey;
    _ma_copy_key(&tmp_key2, &tmp_key);
    key_block->last_length=a_length-t_length;
    DBUG_RETURN(0);
  }

  /* Fill block with end-zero and write filled block */
  _ma_store_page_used(share, anc_buff, key_block->last_length);
  bzero(anc_buff+key_block->last_length,
	keyinfo->block_length- key_block->last_length);
  key_file_length=share->state.state.key_file_length;
  if ((filepos= _ma_new(info, DFLT_INIT_HITS, &page_link)) == HA_OFFSET_ERROR)
    DBUG_RETURN(1);
  _ma_fast_unlock_key_del(info);

  /* If we read the page from the key cache, we have to write it back to it */
  if (page_link->changed)
  {
    MARIA_PAGE page;
    pop_dynamic(&info->pinned_pages);
    _ma_page_setup(&page, info, keyinfo, filepos, anc_buff);
    if (_ma_write_keypage(&page, PAGECACHE_LOCK_WRITE_UNLOCK, DFLT_INIT_HITS))
      DBUG_RETURN(1);
  }
  else
  {
    put_crc(anc_buff, filepos, share);
    if (my_pwrite(share->kfile.file, anc_buff,
                  (uint) keyinfo->block_length, filepos, param->myf_rw))
      DBUG_RETURN(1);
  }
  DBUG_DUMP("buff", anc_buff, _ma_get_page_used(share, anc_buff));

	/* Write separator-key to block in next level */
  if (sort_insert_key(sort_param,key_block+1,key_block->lastkey,filepos))
    DBUG_RETURN(1);

	/* clear old block and write new key in it */
  key_block->inited=0;
  DBUG_RETURN(sort_insert_key(sort_param, key_block,key,prev_block));
} /* sort_insert_key */


/* Delete record when we found a duplicated key */

static int sort_delete_record(MARIA_SORT_PARAM *sort_param)
{
  uint i;
  int old_file,error;
  uchar *key;
  MARIA_SORT_INFO *sort_info=sort_param->sort_info;
  HA_CHECK *param=sort_info->param;
  MARIA_HA *row_info= sort_info->new_info, *key_info= sort_info->info;
  DBUG_ENTER("sort_delete_record");

  if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
  {
    _ma_check_print_error(param,
			 "Quick-recover aborted; Run recovery without switch "
                          "-q or with switch -qq");
    DBUG_RETURN(1);
  }
  if (key_info->s->options & HA_OPTION_COMPRESS_RECORD)
  {
    _ma_check_print_error(param,
                          "Recover aborted; Can't run standard recovery on "
                          "compressed tables with errors in data-file. "
                          "Use 'maria_chk --safe-recover' to fix it");
    DBUG_RETURN(1);
  }

  old_file= row_info->dfile.file;
  /* This only affects static and dynamic row formats */
  row_info->dfile.file= row_info->rec_cache.file;
  if (flush_io_cache(&row_info->rec_cache))
    DBUG_RETURN(1);

  key= key_info->lastkey_buff + key_info->s->base.max_key_length;
  if ((error=(*row_info->s->read_record)(row_info, sort_param->record,
                                         key_info->cur_row.lastpos)) &&
	error != HA_ERR_RECORD_DELETED)
  {
    _ma_check_print_error(param,"Can't read record to be removed");
    row_info->dfile.file= old_file;
    DBUG_RETURN(1);
  }
  row_info->cur_row.lastpos= key_info->cur_row.lastpos;

  for (i=0 ; i < sort_info->current_key ; i++)
  {
    MARIA_KEY tmp_key;
    (*key_info->s->keyinfo[i].make_key)(key_info, &tmp_key, i, key,
                                        sort_param->record,
                                        key_info->cur_row.lastpos, 0);
    if (_ma_ck_delete(key_info, &tmp_key))
    {
      _ma_check_print_error(param,
                            "Can't delete key %d from record to be removed",
                            i+1);
      row_info->dfile.file= old_file;
      DBUG_RETURN(1);
    }
  }
  if (sort_param->calc_checksum)
    param->glob_crc-=(*key_info->s->calc_check_checksum)(key_info,
                                                         sort_param->record);
  error= (*row_info->s->delete_record)(row_info, sort_param->record);
  if (error)
    _ma_check_print_error(param,"Got error %d when deleting record",
                          my_errno);
  row_info->dfile.file= old_file;           /* restore actual value */
  row_info->s->state.state.records--;
  DBUG_RETURN(error);
} /* sort_delete_record */


/* Fix all pending blocks and flush everything to disk */

int _ma_flush_pending_blocks(MARIA_SORT_PARAM *sort_param)
{
  uint nod_flag,length;
  my_off_t filepos,key_file_length;
  SORT_KEY_BLOCKS *key_block;
  MARIA_SORT_INFO *sort_info= sort_param->sort_info;
  myf myf_rw=sort_info->param->myf_rw;
  MARIA_HA *info=sort_info->info;
  MARIA_KEYDEF *keyinfo=sort_param->keyinfo;
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  DBUG_ENTER("_ma_flush_pending_blocks");

  filepos= HA_OFFSET_ERROR;			/* if empty file */
  nod_flag=0;
  for (key_block=sort_info->key_block ; key_block->inited ; key_block++)
  {
    key_block->inited=0;
    length= _ma_get_page_used(info->s, key_block->buff);
    if (nod_flag)
      _ma_kpointer(info,key_block->end_pos,filepos);
    key_file_length= info->s->state.state.key_file_length;
    bzero(key_block->buff+length, keyinfo->block_length-length);
    if ((filepos= _ma_new(info, DFLT_INIT_HITS, &page_link)) ==
        HA_OFFSET_ERROR)
      goto err;

    /* If we read the page from the key cache, we have to write it back */
    if (page_link->changed)
    {
      MARIA_PAGE page;
      pop_dynamic(&info->pinned_pages);

      _ma_page_setup(&page, info, keyinfo, filepos, key_block->buff);
      if (_ma_write_keypage(&page, PAGECACHE_LOCK_WRITE_UNLOCK,
                            DFLT_INIT_HITS))
	goto err;
    }
    else
    {
      put_crc(key_block->buff, filepos, info->s);
      if (my_pwrite(info->s->kfile.file, key_block->buff,
                    (uint) keyinfo->block_length,filepos, myf_rw))
        goto err;
    }
    DBUG_DUMP("buff",key_block->buff,length);
    nod_flag=1;
  }
  info->s->state.key_root[sort_param->key]=filepos; /* Last is root for tree */
  _ma_fast_unlock_key_del(info);
  DBUG_RETURN(0);

err:
  _ma_fast_unlock_key_del(info);
  DBUG_RETURN(1);
} /* _ma_flush_pending_blocks */

	/* alloc space and pointers for key_blocks */

static SORT_KEY_BLOCKS *alloc_key_blocks(HA_CHECK *param, uint blocks,
                                         uint buffer_length)
{
  reg1 uint i;
  SORT_KEY_BLOCKS *block;
  DBUG_ENTER("alloc_key_blocks");

  if (!(block= (SORT_KEY_BLOCKS*) my_malloc((sizeof(SORT_KEY_BLOCKS)+
                                             buffer_length+IO_SIZE)*blocks,
                                            MYF(0))))
  {
    _ma_check_print_error(param,"Not enough memory for sort-key-blocks");
    return(0);
  }
  for (i=0 ; i < blocks ; i++)
  {
    block[i].inited=0;
    block[i].buff= (uchar*) (block+blocks)+(buffer_length+IO_SIZE)*i;
  }
  DBUG_RETURN(block);
} /* alloc_key_blocks */


	/* Check if file is almost full */

int maria_test_if_almost_full(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;

  if (share->options & HA_OPTION_COMPRESS_RECORD)
    return 0;
  return my_seek(share->kfile.file, 0L, MY_SEEK_END,
                 MYF(MY_THREADSAFE))/10*9 >
    (my_off_t) share->base.max_key_file_length ||
    my_seek(info->dfile.file, 0L, MY_SEEK_END, MYF(0)) / 10 * 9 >
    (my_off_t) share->base.max_data_file_length;
}


/* Recreate table with bigger more alloced record-data */

int maria_recreate_table(HA_CHECK *param, MARIA_HA **org_info, char *filename)
{
  int error;
  MARIA_HA info;
  MARIA_SHARE share;
  MARIA_KEYDEF *keyinfo,*key,*key_end;
  HA_KEYSEG *keysegs,*keyseg;
  MARIA_COLUMNDEF *columndef,*column,*end;
  MARIA_UNIQUEDEF *uniquedef,*u_ptr,*u_end;
  MARIA_STATUS_INFO status_info;
  uint unpack,key_parts;
  ha_rows max_records;
  ulonglong file_length,tmp_length;
  MARIA_CREATE_INFO create_info;
  DBUG_ENTER("maria_recreate_table");

  error=1;					/* Default error */
  info= **org_info;
  status_info= (*org_info)->state[0];
  info.state= &status_info;
  share= *(*org_info)->s;
  unpack= ((share.data_file_type == COMPRESSED_RECORD) &&
           (param->testflag & T_UNPACK));
  if (!(keyinfo=(MARIA_KEYDEF*) my_alloca(sizeof(MARIA_KEYDEF) *
                                          share.base.keys)))
    DBUG_RETURN(0);
  memcpy((uchar*) keyinfo,(uchar*) share.keyinfo,
	 (size_t) (sizeof(MARIA_KEYDEF)*share.base.keys));

  key_parts= share.base.all_key_parts;
  if (!(keysegs=(HA_KEYSEG*) my_alloca(sizeof(HA_KEYSEG)*
				       (key_parts+share.base.keys))))
  {
    my_afree(keyinfo);
    DBUG_RETURN(1);
  }
  if (!(columndef=(MARIA_COLUMNDEF*)
	my_alloca(sizeof(MARIA_COLUMNDEF)*(share.base.fields+1))))
  {
    my_afree(keyinfo);
    my_afree(keysegs);
    DBUG_RETURN(1);
  }
  if (!(uniquedef=(MARIA_UNIQUEDEF*)
	my_alloca(sizeof(MARIA_UNIQUEDEF)*(share.state.header.uniques+1))))
  {
    my_afree(columndef);
    my_afree(keyinfo);
    my_afree(keysegs);
    DBUG_RETURN(1);
  }

  /* Copy the column definitions in their original order */
  for (column= share.columndef, end= share.columndef+share.base.fields;
       column != end ;
       column++)
    columndef[column->column_nr]= *column;

  /* Change the new key to point at the saved key segments */
  memcpy((uchar*) keysegs,(uchar*) share.keyparts,
	 (size_t) (sizeof(HA_KEYSEG)*(key_parts+share.base.keys+
				      share.state.header.uniques)));
  keyseg=keysegs;
  for (key=keyinfo,key_end=keyinfo+share.base.keys; key != key_end ; key++)
  {
    key->seg=keyseg;
    for (; keyseg->type ; keyseg++)
    {
      if (param->language)
	keyseg->language=param->language;	/* change language */
    }
    keyseg++;					/* Skip end pointer */
  }

  /*
    Copy the unique definitions and change them to point at the new key
    segments
  */
  memcpy((uchar*) uniquedef,(uchar*) share.uniqueinfo,
	 (size_t) (sizeof(MARIA_UNIQUEDEF)*(share.state.header.uniques)));
  for (u_ptr=uniquedef,u_end=uniquedef+share.state.header.uniques;
       u_ptr != u_end ; u_ptr++)
  {
    u_ptr->seg=keyseg;
    keyseg+=u_ptr->keysegs+1;
  }

  file_length=(ulonglong) my_seek(info.dfile.file, 0L, MY_SEEK_END, MYF(0));
  if (share.options & HA_OPTION_COMPRESS_RECORD)
    share.base.records=max_records=info.state->records;
  else if (share.base.min_pack_length)
    max_records=(ha_rows) (file_length / share.base.min_pack_length);
  else
    max_records=0;
  share.options&= ~HA_OPTION_TEMP_COMPRESS_RECORD;

  tmp_length= file_length+file_length/10;
  set_if_bigger(file_length,param->max_data_file_length);
  set_if_bigger(file_length,tmp_length);
  set_if_bigger(file_length,(ulonglong) share.base.max_data_file_length);

  VOID(maria_close(*org_info));

  bzero((char*) &create_info,sizeof(create_info));
  create_info.max_rows=max(max_records,share.base.records);
  create_info.reloc_rows=share.base.reloc;
  create_info.old_options=(share.options |
			   (unpack ? HA_OPTION_TEMP_COMPRESS_RECORD : 0));

  create_info.data_file_length=file_length;
  create_info.auto_increment=share.state.auto_increment;
  create_info.language = (param->language ? param->language :
			  share.state.header.language);
  create_info.key_file_length=  status_info.key_file_length;
  create_info.org_data_file_type= ((enum data_file_type)
                                   share.state.header.org_data_file_type);

  /*
    Allow for creating an auto_increment key. This has an effect only if
    an auto_increment key exists in the original table.
  */
  create_info.with_auto_increment= TRUE;
  create_info.null_bytes= share.base.null_bytes;
  create_info.transactional= share.base.born_transactional;

  /*
    We don't have to handle symlinks here because we are using
    HA_DONT_TOUCH_DATA
  */
  if (maria_create(filename, share.data_file_type,
                   share.base.keys - share.state.header.uniques,
                   keyinfo, share.base.fields, columndef,
                   share.state.header.uniques, uniquedef,
                   &create_info,
                   HA_DONT_TOUCH_DATA))
  {
    _ma_check_print_error(param,
                          "Got error %d when trying to recreate indexfile",
                          my_errno);
    goto end;
  }
  *org_info= maria_open(filename,O_RDWR,
                        (HA_OPEN_FOR_REPAIR |
                         ((param->testflag & T_WAIT_FOREVER) ?
                          HA_OPEN_WAIT_IF_LOCKED :
                          (param->testflag & T_DESCRIPT) ?
                          HA_OPEN_IGNORE_IF_LOCKED :
                          HA_OPEN_ABORT_IF_LOCKED)));
  if (!*org_info)
  {
    _ma_check_print_error(param,
                          "Got error %d when trying to open re-created "
                          "indexfile", my_errno);
    goto end;
  }
  /* We are modifing */
  (*org_info)->s->options&= ~HA_OPTION_READ_ONLY_DATA;
  VOID(_ma_readinfo(*org_info,F_WRLCK,0));
  (*org_info)->s->state.state.records= info.state->records;
  if (share.state.create_time)
    (*org_info)->s->state.create_time=share.state.create_time;
#ifdef EXTERNAL_LOCKING
  (*org_info)->s->state.unique= (*org_info)->this_unique= share.state.unique;
#endif
  (*org_info)->s->state.state.checksum= info.state->checksum;
  (*org_info)->s->state.state.del= info.state->del;
  (*org_info)->s->state.dellink= share.state.dellink;
  (*org_info)->s->state.state.empty= info.state->empty;
  (*org_info)->s->state.state.data_file_length= info.state->data_file_length;
  *(*org_info)->state= (*org_info)->s->state.state;
  if (maria_update_state_info(param,*org_info,UPDATE_TIME | UPDATE_STAT |
                              UPDATE_OPEN_COUNT))
    goto end;
  error=0;
end:
  my_afree(uniquedef);
  my_afree(keyinfo);
  my_afree(columndef);
  my_afree(keysegs);
  DBUG_RETURN(error);
}


	/* write suffix to data file if neaded */

int maria_write_data_suffix(MARIA_SORT_INFO *sort_info, my_bool fix_datafile)
{
  MARIA_HA *info=sort_info->new_info;

  if (info->s->data_file_type == COMPRESSED_RECORD && fix_datafile)
  {
    uchar buff[MEMMAP_EXTRA_MARGIN];
    bzero(buff,sizeof(buff));
    if (my_b_write(&info->rec_cache,buff,sizeof(buff)))
    {
      _ma_check_print_error(sort_info->param,
			   "%d when writing to datafile",my_errno);
      return 1;
    }
    sort_info->param->read_cache.end_of_file+=sizeof(buff);
  }
  return 0;
}


/* Update state and maria_chk time of indexfile */

int maria_update_state_info(HA_CHECK *param, MARIA_HA *info,uint update)
{
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("maria_update_state_info");

  if (update & UPDATE_OPEN_COUNT)
  {
    share->state.open_count=0;
    share->global_changed=0;
  }
  if (update & UPDATE_STAT)
  {
    uint i, key_parts= mi_uint2korr(share->state.header.key_parts);
    share->state.records_at_analyze= share->state.state.records;
    share->state.changed&= ~STATE_NOT_ANALYZED;
    if (share->state.state.records)
    {
      for (i=0; i<key_parts; i++)
      {
        if (!(share->state.rec_per_key_part[i]=param->new_rec_per_key_part[i]))
          share->state.changed|= STATE_NOT_ANALYZED;
      }
    }
  }
  if (update & (UPDATE_STAT | UPDATE_SORT | UPDATE_TIME | UPDATE_AUTO_INC))
  {
    if (update & UPDATE_TIME)
    {
      share->state.check_time= time((time_t*) 0);
      if (!share->state.create_time)
	share->state.create_time= share->state.check_time;
    }
    if (_ma_state_info_write(share,
                             MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                             MA_STATE_INFO_WRITE_FULL_INFO))
      goto err;
    share->changed=0;
  }
  {						/* Force update of status */
    int error;
    uint r_locks=share->r_locks,w_locks=share->w_locks;
    share->r_locks= share->w_locks= share->tot_locks= 0;
    error= _ma_writeinfo(info,WRITEINFO_NO_UNLOCK);
    share->r_locks=r_locks;
    share->w_locks=w_locks;
    share->tot_locks=r_locks+w_locks;
    if (!error)
      DBUG_RETURN(0);
  }
err:
  _ma_check_print_error(param,"%d when updating keyfile",my_errno);
  DBUG_RETURN(1);
}

/*
  Update auto increment value for a table
  When setting the 'repair_only' flag we only want to change the
  old auto_increment value if its wrong (smaller than some given key).
  The reason is that we shouldn't change the auto_increment value
  for a table without good reason when only doing a repair; If the
  user have inserted and deleted rows, the auto_increment value
  may be bigger than the biggest current row and this is ok.

  If repair_only is not set, we will update the flag to the value in
  param->auto_increment is bigger than the biggest key.
*/

void _ma_update_auto_increment_key(HA_CHECK *param, MARIA_HA *info,
                                   my_bool repair_only)
{
  MARIA_SHARE *share= info->s;
  uchar *record;
  DBUG_ENTER("update_auto_increment_key");

  if (!share->base.auto_key ||
      ! maria_is_key_active(share->state.key_map, share->base.auto_key - 1))
  {
    if (!(param->testflag & T_VERY_SILENT))
      _ma_check_print_info(param,
			  "Table: %s doesn't have an auto increment key\n",
			  param->isam_file_name);
    DBUG_VOID_RETURN;
  }
  if (!(param->testflag & T_SILENT) &&
      !(param->testflag & T_REP))
    printf("Updating MARIA file: %s\n", param->isam_file_name);
  /*
    We have to use an allocated buffer instead of info->rec_buff as
    _ma_put_key_in_record() may use info->rec_buff
  */
  if (!(record= (uchar*) my_malloc((size_t) share->base.default_rec_buff_size,
                                   MYF(0))))
  {
    _ma_check_print_error(param,"Not enough memory for extra record");
    DBUG_VOID_RETURN;
  }

  maria_extra(info,HA_EXTRA_KEYREAD,0);
  if (maria_rlast(info, record, share->base.auto_key-1))
  {
    if (my_errno != HA_ERR_END_OF_FILE)
    {
      maria_extra(info,HA_EXTRA_NO_KEYREAD,0);
      my_free((char*) record, MYF(0));
      _ma_check_print_error(param,"%d when reading last record",my_errno);
      DBUG_VOID_RETURN;
    }
    if (!repair_only)
      share->state.auto_increment=param->auto_increment_value;
  }
  else
  {
    const HA_KEYSEG *keyseg= share->keyinfo[share->base.auto_key-1].seg;
    ulonglong auto_increment=
      ma_retrieve_auto_increment(record + keyseg->start, keyseg->type);
    set_if_bigger(share->state.auto_increment,auto_increment);
    if (!repair_only)
      set_if_bigger(share->state.auto_increment, param->auto_increment_value);
  }
  maria_extra(info,HA_EXTRA_NO_KEYREAD,0);
  my_free((char*) record, MYF(0));
  maria_update_state_info(param, info, UPDATE_AUTO_INC);
  DBUG_VOID_RETURN;
}


/*
  Update statistics for each part of an index

  SYNOPSIS
    maria_update_key_parts()
      keyinfo           IN  Index information (only key->keysegs used)
      rec_per_key_part  OUT Store statistics here
      unique            IN  Array of (#distinct tuples)
      notnull_tuples    IN  Array of (#tuples), or NULL
      records               Number of records in the table

  DESCRIPTION
    This function is called produce index statistics values from unique and
    notnull_tuples arrays after these arrays were produced with sequential
    index scan (the scan is done in two places: chk_index() and
    sort_key_write()).

    This function handles all 3 index statistics collection methods.

    Unique is an array:
      unique[0]= (#different values of {keypart1}) - 1
      unique[1]= (#different values of {keypart1,keypart2} tuple)-unique[0]-1
      ...

    For MI_STATS_METHOD_IGNORE_NULLS method, notnull_tuples is an array too:
      notnull_tuples[0]= (#of {keypart1} tuples such that keypart1 is not NULL)
      notnull_tuples[1]= (#of {keypart1,keypart2} tuples such that all
                          keypart{i} are not NULL)
      ...
    For all other statistics collection methods notnull_tuples==NULL.

    Output is an array:
    rec_per_key_part[k] =
     = E(#records in the table such that keypart_1=c_1 AND ... AND
         keypart_k=c_k for arbitrary constants c_1 ... c_k)

     = {assuming that values have uniform distribution and index contains all
        tuples from the domain (or that {c_1, ..., c_k} tuple is choosen from
        index tuples}

     = #tuples-in-the-index / #distinct-tuples-in-the-index.

    The #tuples-in-the-index and #distinct-tuples-in-the-index have different
    meaning depending on which statistics collection method is used:

    MI_STATS_METHOD_*  how are nulls compared?  which tuples are counted?
     NULLS_EQUAL            NULL == NULL           all tuples in table
     NULLS_NOT_EQUAL        NULL != NULL           all tuples in table
     IGNORE_NULLS               n/a             tuples that don't have NULLs
*/

void maria_update_key_parts(MARIA_KEYDEF *keyinfo, double *rec_per_key_part,
                      ulonglong *unique, ulonglong *notnull,
                      ulonglong records)
{
  ulonglong count=0, unique_tuples;
  ulonglong tuples= records;
  uint parts;
  double tmp;
  for (parts=0 ; parts < keyinfo->keysegs  ; parts++)
  {
    count+=unique[parts];
    unique_tuples= count + 1;
    if (notnull)
    {
      tuples= notnull[parts];
      /*
        #(unique_tuples not counting tuples with NULLs) =
          #(unique_tuples counting tuples with NULLs as different) -
          #(tuples with NULLs)
      */
      unique_tuples -= (records - notnull[parts]);
    }

    if (unique_tuples == 0)
      tmp= 1;
    else if (count == 0)
      tmp= ulonglong2double(tuples); /* 1 unique tuple */
    else
      tmp= ulonglong2double(tuples) / ulonglong2double(unique_tuples);

    /*
      for some weird keys (e.g. FULLTEXT) tmp can be <1 here.
      let's ensure it is not
    */
    set_if_bigger(tmp,1);

    *rec_per_key_part++= tmp;
  }
}


static ha_checksum maria_byte_checksum(const uchar *buf, uint length)
{
  ha_checksum crc;
  const uchar *end=buf+length;
  for (crc=0; buf != end; buf++)
    crc=((crc << 1) + *buf) +
      test(crc & (((ha_checksum) 1) << (8*sizeof(ha_checksum)-1)));
  return crc;
}

static my_bool maria_too_big_key_for_sort(MARIA_KEYDEF *key, ha_rows rows)
{
  uint key_maxlength=key->maxlength;
  if (key->flag & HA_FULLTEXT)
  {
    uint ft_max_word_len_for_sort=FT_MAX_WORD_LEN_FOR_SORT*
                                  key->seg->charset->mbmaxlen;
    key_maxlength+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
  }
  return (key->flag & HA_SPATIAL) ||
          (key->flag & (HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY | HA_FULLTEXT) &&
	  ((ulonglong) rows * key_maxlength >
	   (ulonglong) maria_max_temp_length));
}

/*
  Deactivate all not unique index that can be recreated fast
  These include packed keys on which sorting will use more temporary
  space than the max allowed file length or for which the unpacked keys
  will take much more space than packed keys.
  Note that 'rows' may be zero for the case when we don't know how many
  rows we will put into the file.
 */

void maria_disable_non_unique_index(MARIA_HA *info, ha_rows rows)
{
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF    *key=share->keyinfo;
  uint          i;

  DBUG_ASSERT(share->state.state.records == 0 &&
              (!rows || rows >= MARIA_MIN_ROWS_TO_DISABLE_INDEXES));
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!(key->flag &
          (HA_NOSAME | HA_SPATIAL | HA_AUTO_KEY | HA_RTREE_INDEX)) &&
        ! maria_too_big_key_for_sort(key,rows) && share->base.auto_key != i+1)
    {
      maria_clear_key_active(share->state.key_map, i);
      info->update|= HA_STATE_CHANGED;
    }
  }
}


/*
  Return TRUE if we can use repair by sorting
  One can set the force argument to force to use sorting
  even if the temporary file would be quite big!
*/

my_bool maria_test_if_sort_rep(MARIA_HA *info, ha_rows rows,
                               ulonglong key_map, my_bool force)
{
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *key=share->keyinfo;
  uint i;

  /*
    maria_repair_by_sort only works if we have at least one key. If we don't
    have any keys, we should use the normal repair.
  */
  if (! maria_is_any_key_active(key_map))
    return FALSE;				/* Can't use sort */
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!force && maria_too_big_key_for_sort(key,rows))
      return FALSE;
  }
  return TRUE;
}


/**
   @brief Create a new handle for manipulation the new record file

   @note
   It's ok for Recovery to have two MARIA_SHARE on the same index file
   because the one we create here is not transactional
*/

static my_bool create_new_data_handle(MARIA_SORT_PARAM *param, File new_file)
{

  MARIA_SORT_INFO *sort_info= param->sort_info;
  MARIA_HA *info= sort_info->info;
  MARIA_HA *new_info;
  DBUG_ENTER("create_new_data_handle");

  if (!(sort_info->new_info= maria_open(info->s->open_file_name.str, O_RDWR,
                                        HA_OPEN_COPY | HA_OPEN_FOR_REPAIR)))
    DBUG_RETURN(1);

  new_info= sort_info->new_info;
  _ma_bitmap_set_pagecache_callbacks(&new_info->s->bitmap.file,
                                     new_info->s);
  _ma_set_data_pagecache_callbacks(&new_info->dfile, new_info->s);
  change_data_file_descriptor(new_info, new_file);
  maria_lock_database(new_info, F_EXTRA_LCK);
  if ((sort_info->param->testflag & T_UNPACK) &&
      info->s->data_file_type == COMPRESSED_RECORD)
  {
    (*new_info->s->once_end)(new_info->s);
    (*new_info->s->end)(new_info);
    restore_data_file_type(new_info->s);
    _ma_setup_functions(new_info->s);
    if ((*new_info->s->once_init)(new_info->s, new_file) ||
        (*new_info->s->init)(new_info))
      DBUG_RETURN(1);
  }
  _ma_reset_status(new_info);
  if (_ma_initialize_data_file(new_info->s, new_file))
    DBUG_RETURN(1);

  /* Take into account any bitmap page created above: */
  param->filepos= new_info->s->state.state.data_file_length;

  /* Use new virtual functions for key generation */
  info->s->keypos_to_recpos= new_info->s->keypos_to_recpos;
  info->s->recpos_to_keypos= new_info->s->recpos_to_keypos;
  DBUG_RETURN(0);
}


static void
set_data_file_type(MARIA_SORT_INFO *sort_info, MARIA_SHARE *share)
{
  if ((sort_info->new_data_file_type=share->data_file_type) ==
      COMPRESSED_RECORD && sort_info->param->testflag & T_UNPACK)
  {
    MARIA_SHARE tmp;
    sort_info->new_data_file_type= share->state.header.org_data_file_type;
    /* Set delete_function for sort_delete_record() */
    tmp= *share;
    tmp.state.header.data_file_type= tmp.state.header.org_data_file_type;
    tmp.options= ~HA_OPTION_COMPRESS_RECORD;
    _ma_setup_functions(&tmp);
    share->delete_record=tmp.delete_record;
  }
}

static void restore_data_file_type(MARIA_SHARE *share)
{
  MARIA_SHARE tmp_share;
  share->options&= ~HA_OPTION_COMPRESS_RECORD;
  mi_int2store(share->state.header.options,share->options);
  share->state.header.data_file_type=
    share->state.header.org_data_file_type;
  share->data_file_type= share->state.header.data_file_type;
  share->pack.header_length= 0;

  /* Use new virtual functions for key generation */
  tmp_share= *share;
  _ma_setup_functions(&tmp_share);
  share->keypos_to_recpos= tmp_share.keypos_to_recpos;
  share->recpos_to_keypos= tmp_share.recpos_to_keypos;
}


static void change_data_file_descriptor(MARIA_HA *info, File new_file)
{
  my_close(info->dfile.file, MYF(MY_WME));
  info->dfile.file= info->s->bitmap.file.file= new_file;
  _ma_bitmap_reset_cache(info->s);
}


/**
   @brief Mark the data file to not be used

   @note
   This is used in repair when we want to ensure the handler will not
   write anything to the data file anymore
*/

static void unuse_data_file_descriptor(MARIA_HA *info)
{
  info->dfile.file= info->s->bitmap.file.file= -1;
  _ma_bitmap_reset_cache(info->s);
}


/*
  Copy all states that has to do with the data file

  NOTES
    This is done to copy the state from the data file generated from
    repair to the original handler
*/

static void copy_data_file_state(MARIA_STATE_INFO *to,
                                 MARIA_STATE_INFO *from)
{
  to->state.records=           from->state.records;
  to->state.del=               from->state.del;
  to->state.empty=             from->state.empty;
  to->state.data_file_length=  from->state.data_file_length;
  to->split=                   from->split;
  to->dellink=		       from->dellink;
  to->first_bitmap_with_space= from->first_bitmap_with_space;
}


/*
  Read 'safely' next record while scanning table.

  SYNOPSIS
    _ma_safe_scan_block_record()
    info                Maria handler
    record              Store found here

  NOTES
    - One must have called mi_scan() before this

    Differences compared to  _ma_scan_block_records() are:
    - We read all blocks, not only blocks marked by the bitmap to be safe
    - In case of errors, next read will read next record.
    - More sanity checks

  RETURN
    0   ok
    HA_ERR_END_OF_FILE  End of file
    #   error number
*/


static int _ma_safe_scan_block_record(MARIA_SORT_INFO *sort_info,
                                      MARIA_HA *info, uchar *record)
{
  MARIA_SHARE *share= info->s;
  MARIA_RECORD_POS record_pos= info->cur_row.nextpos;
  pgcache_page_no_t page= sort_info->page;
  DBUG_ENTER("_ma_safe_scan_block_record");

  for (;;)
  {
    /* Find next row in current page */
    if (likely(record_pos < info->scan.number_of_rows))
    {
      uint length, offset;
      uchar *data, *end_of_data;
      char llbuff[22];

      while (!(offset= uint2korr(info->scan.dir)))
      {
        info->scan.dir-= DIR_ENTRY_SIZE;
        record_pos++;
        if (info->scan.dir < info->scan.dir_end)
        {
          _ma_check_print_info(sort_info->param,
                               "Wrong directory on page %s",
                               llstr(page, llbuff));
          goto read_next_page;
        }
      }
      /* found row */
      info->cur_row.lastpos= info->scan.row_base_page + record_pos;
      info->cur_row.nextpos= record_pos + 1;
      data= info->scan.page_buff + offset;
      length= uint2korr(info->scan.dir + 2);
      end_of_data= data + length;
      info->scan.dir-= DIR_ENTRY_SIZE;          /* Point to previous row */

      if (end_of_data > info->scan.dir_end ||
          offset < PAGE_HEADER_SIZE || length < share->base.min_block_length)
      {
        _ma_check_print_info(sort_info->param,
                             "Wrong directory entry %3u at page %s",
                             (uint) record_pos, llstr(page, llbuff));
        record_pos++;
        continue;
      }
      else
      {
        DBUG_PRINT("info", ("rowid: %lu", (ulong) info->cur_row.lastpos));
        DBUG_RETURN(_ma_read_block_record2(info, record, data, end_of_data));
      }
    }

read_next_page:
    /* Read until we find next head page */
    for (;;)
    {
      uint page_type;
      char llbuff[22];

      sort_info->page++;                        /* In case of errors */
      page++;
      if (!(page % share->bitmap.pages_covered))
      {
        /* Skip bitmap */
        page++;
        sort_info->page++;
      }
      if ((my_off_t) (page + 1) * share->block_size > sort_info->filelength)
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      if (!(pagecache_read(share->pagecache,
                           &info->dfile,
                           page, 0, info->scan.page_buff,
                           PAGECACHE_READ_UNKNOWN_PAGE,
                           PAGECACHE_LOCK_LEFT_UNLOCKED, 0)))
      {
        if (my_errno == HA_ERR_WRONG_CRC)
        {
          _ma_check_print_info(sort_info->param,
                               "Wrong CRC on datapage at %s",
                               llstr(page, llbuff));
          continue;
        }
        DBUG_RETURN(my_errno);
      }
      page_type= (info->scan.page_buff[PAGE_TYPE_OFFSET] &
                  PAGE_TYPE_MASK);
      if (page_type == HEAD_PAGE)
      {
        if ((info->scan.number_of_rows=
             (uint) (uchar) info->scan.page_buff[DIR_COUNT_OFFSET]) != 0)
          break;
        _ma_check_print_info(sort_info->param,
                             "Wrong head page at page %s",
                             llstr(page, llbuff));
      }
      else if (page_type >= MAX_PAGE_TYPE)
      {
        _ma_check_print_info(sort_info->param,
                             "Found wrong page type: %d at page %s",
                             page_type, llstr(page, llbuff));
      }
    }

    /* New head page */
    info->scan.dir= (info->scan.page_buff + share->block_size -
                     PAGE_SUFFIX_SIZE - DIR_ENTRY_SIZE);
    info->scan.dir_end= (info->scan.dir -
                         (info->scan.number_of_rows - 1) *
                         DIR_ENTRY_SIZE);
    info->scan.row_base_page= ma_recordpos(page, 0);
    record_pos= 0;
  }
}


/**
   @brief Writes a LOGREC_REPAIR_TABLE record and updates create_rename_lsn
   if needed (so that maria_read_log does not redo the repair).

   @param  param            description of the REPAIR operation
   @param  info             table

   @return Operation status
     @retval 0      ok
     @retval 1      error (disk problem)
*/

my_bool write_log_record_for_repair(const HA_CHECK *param, MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  /* in case this is maria_chk or recovery... */
  if (translog_status == TRANSLOG_OK && !maria_in_recovery &&
      share->base.born_transactional)
  {
    my_bool save_now_transactional= share->now_transactional;

    /*
      For now this record is only informative. It could serve when applying
      logs to a backup, but that needs more thought. Assume table became
      corrupted. It is repaired, then some writes happen to it.
      Later we restore an old backup, and want to apply this REDO_REPAIR_TABLE
      record. For it to give the same result as originally, the table should
      be corrupted the same way, so applying previous REDOs should produce the
      same corruption; that's really not guaranteed (different execution paths
      in execution of REDOs vs runtime code so not same bugs hit, temporary
      hardware issues not repeatable etc). Corruption may not be repeatable.
      A reasonable solution is to execute the REDO_REPAIR_TABLE record and
      check if the checksum of the resulting table matches what it was at the
      end of the original repair (should be stored in log record); or execute
      the REDO_REPAIR_TABLE if the checksum of the table-before-repair matches
      was it was at the start of the original repair (should be stored in log
      record).
    */
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    uchar log_data[FILEID_STORE_SIZE + 8 + 8];
    LSN lsn;

    /*
      testflag gives an idea of what REPAIR did (in particular T_QUICK
      or not: did it touch the data file or not?).
    */
    int8store(log_data + FILEID_STORE_SIZE, param->testflag);
    /* org_key_map is used when recreating index after a load data infile */
    int8store(log_data + FILEID_STORE_SIZE + 8, param->org_key_map);

    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);

    share->now_transactional= 1;
    if (unlikely(translog_write_record(&lsn, LOGREC_REDO_REPAIR_TABLE,
                                       &dummy_transaction_object, info,
                                       (translog_size_t) sizeof(log_data),
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, log_data, NULL) ||
                 translog_flush(lsn)))
      return TRUE;
    /*
      The table's existence was made durable earlier (MY_SYNC_DIR passed to
      maria_change_to_newfile()). All pages have been flushed, state too, we
      need to force it to disk. Old REDOs should not be applied to the table,
      which is already enforced as skip_redos_lsn was increased in
      protect_against_repair_crash(). But if this is an explicit repair,
      even UNDO phase should ignore this table: create_rename_lsn should be
      increased, and this also serves for the REDO_REPAIR to be ignored by
      maria_read_log.
      The fully correct order would be: sync data and index file, remove crash
      mark and update LSNs then write state and sync index file. But at this
      point state (without crash mark) is already written.
    */
    if ((!(param->testflag & T_NO_CREATE_RENAME_LSN) &&
         _ma_update_state_lsns(share, lsn, share->state.create_trid, FALSE,
                               FALSE)) ||
        _ma_sync_table_files(info))
      return TRUE;
    share->now_transactional= save_now_transactional;
  }
  return FALSE;
}


/**
  Writes an UNDO record which if executed in UNDO phase, will empty the
  table. Such record is thus logged only in certain cases of bulk insert
  (table needs to be empty etc).
*/
my_bool write_log_record_for_bulk_insert(MARIA_HA *info)
{
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE];
  LSN lsn;
  lsn_store(log_data, info->trn->undo_lsn);
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
  return translog_write_record(&lsn, LOGREC_UNDO_BULK_INSERT,
                               info->trn, info,
                               (translog_size_t)
                               log_array[TRANSLOG_INTERNAL_PARTS +
                                         0].length,
                               TRANSLOG_INTERNAL_PARTS + 1, log_array,
                               log_data + LSN_STORE_SIZE, NULL) ||
    translog_flush(lsn); /* WAL */
}


/* Give error message why reading of key page failed */

static void report_keypage_fault(HA_CHECK *param, MARIA_HA *info,
                                 my_off_t position)
{
  char buff[11];
  uint32 block_size= info->s->block_size;

  if (my_errno == HA_ERR_CRASHED)
    _ma_check_print_error(param,
                          "Wrong base information on indexpage at page: %s",
                          llstr(position / block_size, buff));
  else
    _ma_check_print_error(param,
                          "Can't read indexpage from page: %s, "
                          "error: %d",
                          llstr(position / block_size, buff), my_errno);
}


/**
  When we want to check a table, we verify that the transaction ids of rows
  and keys are not bigger than the biggest id generated by Maria so far, which
  is returned by the function below.

  @note If control file is not open, 0 may be returned; to not confuse
  this with a valid max trid of 0, the caller should notice that it failed to
  open the control file (ma_control_file_inited() can serve for that).
*/

static TrID max_trid_in_system(void)
{
  TrID id= trnman_get_max_trid(); /* 0 if transac manager not initialized */
  /* 'id' may be far bigger, if last shutdown is old */
  return max(id, max_trid_in_control_file);
}


static void _ma_check_print_not_visible_error(HA_CHECK *param, TrID used_trid)
{
  char buff[22], buff2[22];
  if (!param->not_visible_rows_found++)
  {
    if (!ma_control_file_inited())
    {
      _ma_check_print_warning(param,
                              "Found row with transaction id %s but no "
                              "maria_control_file was used or specified.  "
                              "The table may be corrupted",
                              llstr(used_trid, buff));
    }
    else
    {
      _ma_check_print_error(param,
                            "Found row with transaction id %s when max "
                            "transaction id according to maria_control_file "
                            "is %s",
                            llstr(used_trid, buff),
                            llstr(param->max_trid, buff2));
    }
  }
}


/**
  Mark that we can retry normal repair if we used quick repair

  We shouldn't do this in case of disk error as in this case we are likely
  to loose much more than expected.
*/

void retry_if_quick(MARIA_SORT_PARAM *sort_param, int error)
{
  HA_CHECK *param=sort_param->sort_info->param;

  if (!sort_param->fix_datafile && error >= HA_ERR_FIRST)
  {
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
  }
}

/* Print information about bitmap page */

static void print_bitmap_description(MARIA_SHARE *share,
                                     pgcache_page_no_t page,
                                     uchar *bitmap_data)
{
  uchar *pos, *end;
  MARIA_FILE_BITMAP *bitmap= &share->bitmap;
  uint count=0, dot_printed= 0;
  char buff[80], last[80];

  printf("Bitmap page %lu\n", (ulong) page);
  page++;
  last[0]=0;
  for (pos= bitmap_data, end= pos+ bitmap->used_size ; pos < end ; pos+= 6)
  {
    ulonglong bits= uint6korr(pos);    /* 6 bytes = 6*8/3= 16 patterns */
    uint i;

    for (i= 0; i < 16 ; i++, bits>>= 3)
    {
      if (count > 60)
      {
        buff[count]= 0;
        if (strcmp(buff, last))
        {
          memcpy(last, buff, count+1);
          printf("%8lu: %s\n", (ulong) page - count, buff);
          dot_printed= 0;
        }
        else if (!(dot_printed++))
          printf("...\n");
        count= 0;
      }
      buff[count++]= '0' + (uint) (bits & 7);
      page++;
    }
  }
  buff[count]= 0;
  printf("%8lu: %s\n", (ulong) page - count, buff);
  fputs("\n", stdout);
}
