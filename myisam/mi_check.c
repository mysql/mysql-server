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

/* Describe, check and repair of MyISAM tables */

#include "ftdefs.h"
#include <m_ctype.h>
#include <stdarg.h>
#include <my_getopt.h>
#ifdef HAVE_SYS_VADVISE_H
#include <sys/vadvise.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifndef USE_RAID
#define my_raid_create(A,B,C,D,E,F,G) my_create(A,B,C,G)
#define my_raid_delete(A,B,C) my_delete(A,B)
#endif

	/* Functions defined in this file */

static int check_k_link(MI_CHECK *param, MI_INFO *info,uint nr);
static int chk_index(MI_CHECK *param, MI_INFO *info,MI_KEYDEF *keyinfo,
		     my_off_t page, uchar *buff, ha_rows *keys,
		     ha_checksum *key_checksum, uint level);
static uint isam_key_length(MI_INFO *info,MI_KEYDEF *keyinfo);
static ha_checksum calc_checksum(ha_rows count);
static int writekeys(MI_CHECK *param, MI_INFO *info,byte *buff,
		     my_off_t filepos);
static int sort_one_index(MI_CHECK *param, MI_INFO *info,MI_KEYDEF *keyinfo,
			  my_off_t pagepos, File new_file);
static int sort_key_read(MI_SORT_PARAM *sort_param,void *key);
static int sort_ft_key_read(MI_SORT_PARAM *sort_param,void *key);
static int sort_get_next_record(MI_SORT_PARAM *sort_param);
static int sort_key_cmp(MI_SORT_PARAM *sort_param, const void *a,const void *b);
static int sort_ft_key_write(MI_SORT_PARAM *sort_param, const void *a);
static int sort_key_write(MI_SORT_PARAM *sort_param, const void *a);
static my_off_t get_record_for_key(MI_INFO *info,MI_KEYDEF *keyinfo,
				uchar *key);
static int sort_insert_key(MI_SORT_PARAM  *sort_param,
                           reg1 SORT_KEY_BLOCKS *key_block,
			   uchar *key, my_off_t prev_block);
static int sort_delete_record(MI_SORT_PARAM *sort_param);
/*static int flush_pending_blocks(MI_CHECK *param);*/
static SORT_KEY_BLOCKS	*alloc_key_blocks(MI_CHECK *param, uint blocks,
					  uint buffer_length);
static ha_checksum mi_byte_checksum(const byte *buf, uint length);
static void set_data_file_type(SORT_INFO *sort_info, MYISAM_SHARE *share);

void myisamchk_init(MI_CHECK *param)
{
  bzero((gptr) param,sizeof(*param));
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
  param->key_cache_block_size= KEY_CACHE_BLOCK_SIZE;
}

	/* Check the status flags for the table */

int chk_status(MI_CHECK *param, register MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;

  if (mi_is_crashed_on_repair(info))
    mi_check_print_warning(param,
			   "Table is marked as crashed and last repair failed");
  else if (mi_is_crashed(info))
    mi_check_print_warning(param,
			   "Table is marked as crashed");
  if (share->state.open_count != (uint) (info->s->global_changed ? 1 : 0))
  {
    /* Don't count this as a real warning, as check can correct this ! */
    uint save=param->warning_printed;
    mi_check_print_warning(param,
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

	/* Check delete links */

int chk_del(MI_CHECK *param, register MI_INFO *info, uint test_flag)
{
  reg2 ha_rows i;
  uint delete_link_length;
  my_off_t empty,next_link,old_link;
  char buff[22],buff2[22];
  DBUG_ENTER("chk_del");

  LINT_INIT(old_link);
  param->record_checksum=0;
  delete_link_length=((info->s->options & HA_OPTION_PACK_RECORD) ? 20 :
		      info->s->rec_reflength+1);

  if (!(test_flag & T_SILENT))
    puts("- check record delete-chain");

  next_link=info->s->state.dellink;
  if (info->state->del == 0)
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
    for (i= info->state->del ; i > 0L && next_link != HA_OFFSET_ERROR ; i--)
    {
      if (*killed_ptr(param))
        DBUG_RETURN(1);
      if (test_flag & T_VERBOSE)
	printf(" %9s",llstr(next_link,buff));
      if (next_link >= info->state->data_file_length)
	goto wrong;
      if (my_pread(info->dfile,(char*) buff,delete_link_length,
		   next_link,MYF(MY_NABP)))
      {
	if (test_flag & T_VERBOSE) puts("");
	mi_check_print_error(param,"Can't read delete-link at filepos: %s",
		    llstr(next_link,buff));
	DBUG_RETURN(1);
      }
      if (*buff != '\0')
      {
	if (test_flag & T_VERBOSE) puts("");
	mi_check_print_error(param,"Record at pos: %s is not remove-marked",
		    llstr(next_link,buff));
	goto wrong;
      }
      if (info->s->options & HA_OPTION_PACK_RECORD)
      {
	my_off_t prev_link=mi_sizekorr(buff+12);
	if (empty && prev_link != old_link)
	{
	  if (test_flag & T_VERBOSE) puts("");
	  mi_check_print_error(param,"Deleted block at %s doesn't point back at previous delete link",llstr(next_link,buff2));
	  goto wrong;
	}
	old_link=next_link;
	next_link=mi_sizekorr(buff+4);
	empty+=mi_uint3korr(buff+1);
      }
      else
      {
	param->record_checksum+=(ha_checksum) next_link;
	next_link=_mi_rec_pos(info->s,(uchar*) buff+1);
	empty+=info->s->base.pack_reclength;
      }
    }
    if (test_flag & T_VERBOSE)
      puts("\n");
    if (empty != info->state->empty)
    {
      mi_check_print_warning(param,
			     "Found %s deleted space in delete link chain. Should be %s",
			     llstr(empty,buff2),
			     llstr(info->state->empty,buff));
    }
    if (next_link != HA_OFFSET_ERROR)
    {
      mi_check_print_error(param,
			   "Found more than the expected %s deleted rows in delete link chain",
			   llstr(info->state->del, buff));
      goto wrong;
    }
    if (i != 0)
    {
      mi_check_print_error(param,
			   "Found %s deleted rows in delete link chain. Should be %s",
			   llstr(info->state->del - i, buff2),
			   llstr(info->state->del, buff));
      goto wrong;
    }
  }
  DBUG_RETURN(0);

wrong:
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  if (test_flag & T_VERBOSE) puts("");
  mi_check_print_error(param,"record delete-link-chain corrupted");
  DBUG_RETURN(1);
} /* chk_del */


	/* Check delete links in index file */

static int check_k_link(MI_CHECK *param, register MI_INFO *info, uint nr)
{
  my_off_t next_link;
  uint block_size=(nr+1)*MI_MIN_KEY_BLOCK_LENGTH;
  ha_rows records;
  char llbuff[21],*buff;
  DBUG_ENTER("check_k_link");

  if (param->testflag & T_VERBOSE)
    printf("block_size %4d:",block_size);

  next_link=info->s->state.key_del[nr];
  records= (ha_rows) (info->state->key_file_length / block_size);
  while (next_link != HA_OFFSET_ERROR && records > 0)
  {
    if (*killed_ptr(param))
      DBUG_RETURN(1);
    if (param->testflag & T_VERBOSE)
      printf("%16s",llstr(next_link,llbuff));
    if (next_link > info->state->key_file_length ||
	next_link & (info->s->blocksize-1))
      DBUG_RETURN(1);
    if (!(buff=key_cache_read(info->s->key_cache,
                              info->s->kfile, next_link, DFLT_INIT_HITS,
                              (byte*) info->buff,
			      myisam_block_size, block_size, 1)))
      DBUG_RETURN(1);
    next_link=mi_sizekorr(buff);
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

int chk_size(MI_CHECK *param, register MI_INFO *info)
{
  int error=0;
  register my_off_t skr,size;
  char buff[22],buff2[22];
  DBUG_ENTER("chk_size");

  if (!(param->testflag & T_SILENT)) puts("- check file-size");

  /* The following is needed if called externally (not from myisamchk) */
  flush_key_blocks(info->s->key_cache,
		   info->s->kfile, FLUSH_FORCE_WRITE);

  size=my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0));
  if ((skr=(my_off_t) info->state->key_file_length) != size)
  {
    if (skr > size)
    {
      error=1;
      mi_check_print_error(param,
			   "Size of indexfile is: %-8s        Should be: %s",
			   llstr(size,buff), llstr(skr,buff2));
    }
    else
      mi_check_print_warning(param,
			     "Size of indexfile is: %-8s      Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      ! (info->s->options & HA_OPTION_COMPRESS_RECORD) &&
      ulonglong2double(info->state->key_file_length) >
      ulonglong2double(info->s->base.margin_key_file_length)*0.9)
    mi_check_print_warning(param,"Keyfile is almost full, %10s of %10s used",
			   llstr(info->state->key_file_length,buff),
			   llstr(info->s->base.max_key_file_length-1,buff));

  size=my_seek(info->dfile,0L,MY_SEEK_END,MYF(0));
  skr=(my_off_t) info->state->data_file_length;
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
    skr+= MEMMAP_EXTRA_MARGIN;
#ifdef USE_RELOC
  if (info->data_file_type == STATIC_RECORD &&
      skr < (my_off_t) info->s->base.reloc*info->s->base.min_pack_length)
    skr=(my_off_t) info->s->base.reloc*info->s->base.min_pack_length;
#endif
  if (skr != size)
  {
    info->state->data_file_length=size;	/* Skip other errors */
    if (skr > size && skr != size + MEMMAP_EXTRA_MARGIN)
    {
      error=1;
      mi_check_print_error(param,"Size of datafile is: %-9s         Should be: %s",
		    llstr(size,buff), llstr(skr,buff2));
      param->testflag|=T_RETRY_WITHOUT_QUICK;
    }
    else
    {
      mi_check_print_warning(param,
			     "Size of datafile is: %-9s       Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
    }
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      !(info->s->options & HA_OPTION_COMPRESS_RECORD) &&
      ulonglong2double(info->state->data_file_length) >
      (ulonglong2double(info->s->base.max_data_file_length)*0.9))
    mi_check_print_warning(param, "Datafile is almost full, %10s of %10s used",
			   llstr(info->state->data_file_length,buff),
			   llstr(info->s->base.max_data_file_length-1,buff2));
  DBUG_RETURN(error);
} /* chk_size */


	/* Check keys */

int chk_key(MI_CHECK *param, register MI_INFO *info)
{
  uint key,found_keys=0,full_text_keys=0,result=0;
  ha_rows keys;
  ha_checksum old_record_checksum,init_checksum;
  my_off_t all_keydata,all_totaldata,key_totlength,length;
  ulong   *rec_per_key_part;
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *keyinfo;
  char buff[22],buff2[22];
  DBUG_ENTER("chk_key");

  if (!(param->testflag & T_SILENT))
    puts("- check key delete-chain");

  param->key_file_blocks=info->s->base.keystart;
  for (key=0 ; key < info->s->state.header.max_block_size ; key++)
    if (check_k_link(param,info,key))
    {
      if (param->testflag & T_VERBOSE) puts("");
      mi_check_print_error(param,"key delete-link-chain corrupted");
      DBUG_RETURN(-1);
    }

  if (!(param->testflag & T_SILENT)) puts("- check index reference");

  all_keydata=all_totaldata=key_totlength=0;
  old_record_checksum=0;
  init_checksum=param->record_checksum;
  if (!(share->options &
	(HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    old_record_checksum=calc_checksum(info->state->records+info->state->del-1)*
      share->base.pack_reclength;
  rec_per_key_part= param->rec_per_key_part;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       rec_per_key_part+=keyinfo->keysegs, key++, keyinfo++)
  {
    param->key_crc[key]=0;
    if (!(((ulonglong) 1 << key) & share->state.key_map))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part +
		      (uint) (rec_per_key_part - param->rec_per_key_part)),
	     keyinfo->keysegs*sizeof(*rec_per_key_part));
      continue;
    }
    found_keys++;

    param->record_checksum=init_checksum;
    bzero((char*) &param->unique_count,sizeof(param->unique_count));
    if ((!(param->testflag & T_SILENT)))
      printf ("- check data record references index: %d\n",key+1);
    if (keyinfo->flag & HA_FULLTEXT)
      full_text_keys++;
    if (share->state.key_root[key] == HA_OFFSET_ERROR &&
	(info->state->records == 0 || keyinfo->flag & HA_FULLTEXT))
      continue;
    if (!_mi_fetch_keypage(info,keyinfo,share->state.key_root[key],
                           DFLT_INIT_HITS,info->buff,0))
    {
      mi_check_print_error(param,"Can't read indexpage from filepos: %s",
		  llstr(share->state.key_root[key],buff));
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
    if (chk_index(param,info,keyinfo,share->state.key_root[key],info->buff,
		  &keys, param->key_crc+key,1))
      DBUG_RETURN(-1);
    if(!(keyinfo->flag & (HA_FULLTEXT | HA_SPATIAL)))
    {
      if (keys != info->state->records)
      {
	mi_check_print_error(param,"Found %s keys of %s",llstr(keys,buff),
		    llstr(info->state->records,buff2));
	if (!(param->testflag & T_INFO))
	DBUG_RETURN(-1);
	result= -1;
	continue;
      }
      if (found_keys - full_text_keys == 1 &&
	  ((share->options &
	    (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
	   (param->testflag & T_DONT_CHECK_CHECKSUM)))
	old_record_checksum=param->record_checksum;
      else if (old_record_checksum != param->record_checksum)
      {
	if (key)
	  mi_check_print_error(param,"Key %u doesn't point at same records that key 1",
		      key+1);
	else
	  mi_check_print_error(param,"Key 1 doesn't point at all records");
	if (!(param->testflag & T_INFO))
	  DBUG_RETURN(-1);
	result= -1;
	continue;
      }
    }
    if ((uint) share->base.auto_key -1 == key)
    {
      /* Check that auto_increment key is bigger than max key value */
      ulonglong save_auto_value=info->s->state.auto_increment;
      info->s->state.auto_increment=0;
      info->lastinx=key;
      _mi_read_key_record(info, 0L, info->rec_buff);
      update_auto_increment(info, info->rec_buff);
      if (info->s->state.auto_increment > save_auto_value)
      {
	mi_check_print_warning(param,
			       "Auto-increment value: %s is smaller than max used value: %s",
			       llstr(save_auto_value,buff2),
			       llstr(info->s->state.auto_increment, buff));
      }
      if (param->testflag & T_AUTO_INC)
      {
	set_if_bigger(info->s->state.auto_increment,
		      param->auto_increment_value);
      }
      else
	info->s->state.auto_increment=save_auto_value;

      /* Check that there isn't a row with auto_increment = 0 in the table */
      mi_extra(info,HA_EXTRA_KEYREAD,0);
      bzero(info->lastkey,keyinfo->seg->length);
      if (!mi_rkey(info, info->rec_buff, key, (const byte*) info->lastkey,
		   keyinfo->seg->length, HA_READ_KEY_EXACT))
      {
	/* Don't count this as a real warning, as myisamchk can't correct it */
	uint save=param->warning_printed;
	mi_check_print_warning(param,
			       "Found row where the auto_increment column has the value 0");
	param->warning_printed=save;
      }
      mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
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

    if (param->testflag & T_STATISTICS)
      update_key_parts(keyinfo, rec_per_key_part, param->unique_count,
		       (ulonglong) info->state->records);
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
    else if (all_totaldata != 0L && share->state.key_map)
      puts("");
  }
  if (param->key_file_blocks != info->state->key_file_length &&
      param->keys_in_use != ~(ulonglong) 0)
    mi_check_print_warning(param, "Some data are unreferenced in keyfile");
  if (found_keys != full_text_keys)
    param->record_checksum=old_record_checksum-init_checksum;	/* Remove delete links */
  else
    param->record_checksum=0;
  DBUG_RETURN(result);
} /* chk_key */


static int chk_index_down(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
                     my_off_t page, uchar *buff, ha_rows *keys,
                     ha_checksum *key_checksum, uint level)
{
  char llbuff[22],llbuff2[22];
  if (page > info->state->key_file_length || (page & (info->s->blocksize -1)))
  {
    my_off_t max_length=my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0));
    mi_check_print_error(param,"Wrong pagepointer: %s at page: %s",
                llstr(page,llbuff),llstr(page,llbuff2));

    if (page+info->s->blocksize > max_length)
      goto err;
    info->state->key_file_length=(max_length &
                                  ~ (my_off_t) (info->s->blocksize-1));
  }
  if (!_mi_fetch_keypage(info,keyinfo,page, DFLT_INIT_HITS,buff,0))
  {
    mi_check_print_error(param,"Can't read key from filepos: %s",
        llstr(page,llbuff));
    goto err;
  }
  param->key_file_blocks+=keyinfo->block_length;
  if (chk_index(param,info,keyinfo,page,buff,keys,key_checksum,level))
    goto err;

  return 0;
err:
  return 1;
}

	/* Check if index is ok */

static int chk_index(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
		     my_off_t page, uchar *buff, ha_rows *keys,
		     ha_checksum *key_checksum, uint level)
{
  int flag;
  uint used_length,comp_flag,nod_flag,key_length=0,not_used;
  uchar key[MI_MAX_POSSIBLE_KEY_BUFF],*temp_buff,*keypos,*old_keypos,*endpos;
  my_off_t next_page,record;
  char llbuff[22];
  DBUG_ENTER("chk_index");
  DBUG_DUMP("buff",(byte*) buff,mi_getint(buff));

  /* TODO: implement appropriate check for RTree keys */
  if (keyinfo->flag & HA_SPATIAL)
    DBUG_RETURN(0);

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    mi_check_print_error(param,"Not enough memory for keyblock");
    DBUG_RETURN(-1);
  }

  if (keyinfo->flag & HA_NOSAME)
    comp_flag=SEARCH_FIND | SEARCH_UPDATE;	/* Not real duplicates */
  else
    comp_flag=SEARCH_SAME;			/* Keys in positionorder */
  nod_flag=mi_test_if_nod(buff);
  used_length=mi_getint(buff);
  keypos=buff+2+nod_flag;
  endpos=buff+used_length;

  param->keydata+=used_length; param->totaldata+=keyinfo->block_length;	/* INFO */
  param->key_blocks++;
  if (level > param->max_level)
    param->max_level=level;

  if (used_length > keyinfo->block_length)
  {
    mi_check_print_error(param,"Wrong pageinfo at page: %s",
			 llstr(page,llbuff));
    goto err;
  }
  for ( ;; )
  {
    if (*killed_ptr(param))
      goto err;
    memcpy((char*) info->lastkey,(char*) key,key_length);
    info->lastkey_length=key_length;
    if (nod_flag)
    {
      next_page=_mi_kpos(nod_flag,keypos);
      if (chk_index_down(param,info,keyinfo,next_page,
                         temp_buff,keys,key_checksum,level+1))
	goto err;
    }
    old_keypos=keypos;
    if (keypos >= endpos ||
	(key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,key)) == 0)
      break;
    if (keypos > endpos)
    {
      mi_check_print_error(param,"Wrong key block length at page: %s",llstr(page,llbuff));
      goto err;
    }
    if ((*keys)++ &&
	(flag=ha_key_cmp(keyinfo->seg,info->lastkey,key,key_length,
			 comp_flag, &not_used)) >=0)
    {
      DBUG_DUMP("old",(byte*) info->lastkey, info->lastkey_length);
      DBUG_DUMP("new",(byte*) key, key_length);
      DBUG_DUMP("new_in_page",(char*) old_keypos,(uint) (keypos-old_keypos));

      if (comp_flag & SEARCH_FIND && flag == 0)
	mi_check_print_error(param,"Found duplicated key at page %s",llstr(page,llbuff));
      else
	mi_check_print_error(param,"Key in wrong position at page %s",llstr(page,llbuff));
      goto err;
    }
    if (param->testflag & T_STATISTICS)
    {
      if (*keys != 1L)				/* not first_key */
      {
	uint diff;
	ha_key_cmp(keyinfo->seg,info->lastkey,key,USE_WHOLE_KEY,
		   SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL,
		   &diff);
	param->unique_count[diff-1]++;
      }
    }
    (*key_checksum)+= mi_byte_checksum((byte*) key,
				       key_length- info->s->rec_reflength);
    record= _mi_dpos(info,0,key+key_length);
    if (keyinfo->flag & HA_FULLTEXT) /* special handling for ft2 */
    {
      uint off;
      int  subkeys;
      get_key_full_length_rdonly(off, key);
      subkeys=ft_sintXkorr(key+off);
      if (subkeys < 0)
      {
        ha_rows tmp_keys=0;
        if (chk_index_down(param,info,&info->s->ft2_keyinfo,record,
                           temp_buff,&tmp_keys,key_checksum,1))
          goto err;
        if (tmp_keys + subkeys)
        {
          mi_check_print_error(param,
                               "Number of words in the 2nd level tree "
                               "does not match the number in the header. "
                               "Parent word in on the page %s, offset %u",
                               llstr(page,llbuff), (uint) (old_keypos-buff));
          goto err;
        }
        (*keys)+=tmp_keys-1;
        continue;
      }
      /* fall through */
    }
    if (record >= info->state->data_file_length)
    {
#ifndef DBUG_OFF
      char llbuff2[22], llbuff3[22];
#endif
      mi_check_print_error(param,"Found key at page %s that points to record outside datafile",llstr(page,llbuff));
      DBUG_PRINT("test",("page: %s  record: %s  filelength: %s",
			 llstr(page,llbuff),llstr(record,llbuff2),
			 llstr(info->state->data_file_length,llbuff3)));
      DBUG_DUMP("key",(byte*) key,key_length);
      DBUG_DUMP("new_in_page",(char*) old_keypos,(uint) (keypos-old_keypos));
      goto err;
    }
    param->record_checksum+=(ha_checksum) record;
  }
  if (keypos != endpos)
  {
    mi_check_print_error(param,"Keyblock size at page %s is not correct.  Block length: %d  key length: %d",
                llstr(page,llbuff), used_length, (keypos - buff));
    goto err;
  }
  my_afree((byte*) temp_buff);
  DBUG_RETURN(0);
 err:
  my_afree((byte*) temp_buff);
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

static uint isam_key_length(MI_INFO *info, register MI_KEYDEF *keyinfo)
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


	/* Check that record-link is ok */

int chk_data_link(MI_CHECK *param, MI_INFO *info,int extend)
{
  int	error,got_error,flag;
  uint	key,left_length,b_type,field;
  ha_rows records,del_blocks;
  my_off_t used,empty,pos,splits,start_recpos,
	   del_length,link_used,start_block;
  byte	*record,*to;
  char llbuff[22],llbuff2[22],llbuff3[22];
  ha_checksum intern_record_checksum;
  ha_checksum key_checksum[MI_MAX_POSSIBLE_KEY];
  my_bool static_row_size;
  MI_KEYDEF *keyinfo;
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("chk_data_link");

  if (!(param->testflag & T_SILENT))
  {
    if (extend)
      puts("- check records and index references");
    else
      puts("- check record links");
  }

  if (!(record= (byte*) my_malloc(info->s->base.pack_reclength,MYF(0))))
  {
    mi_check_print_error(param,"Not enough memory for record");
    DBUG_RETURN(-1);
  }
  records=del_blocks=0;
  used=link_used=splits=del_length=0;
  intern_record_checksum=param->glob_crc=0;
  LINT_INIT(left_length);  LINT_INIT(start_recpos);  LINT_INIT(to);
  got_error=error=0;
  empty=info->s->pack.header_length;

  /* Check how to calculate checksum of rows */
  static_row_size=1;
  if (info->s->data_file_type == COMPRESSED_RECORD)
  {
    for (field=0 ; field < info->s->base.fields ; field++)
    {
      if (info->s->rec[field].base_type == FIELD_BLOB ||
	  info->s->rec[field].base_type == FIELD_VARCHAR)
      {
	static_row_size=0;
	break;
      }
    }
  }

  pos=my_b_tell(&param->read_cache);
  bzero((char*) key_checksum, info->s->base.keys * sizeof(key_checksum[0]));
  while (pos < info->state->data_file_length)
  {
    if (*killed_ptr(param))
      goto err2;
    switch (info->s->data_file_type) {
    case STATIC_RECORD:
      if (my_b_read(&param->read_cache,(byte*) record,
		    info->s->base.pack_reclength))
	goto err;
      start_recpos=pos;
      pos+=info->s->base.pack_reclength;
      splits++;
      if (*record == '\0')
      {
	del_blocks++;
	del_length+=info->s->base.pack_reclength;
	continue;					/* Record removed */
      }
      param->glob_crc+= mi_static_checksum(info,record);
      used+=info->s->base.pack_reclength;
      break;
    case DYNAMIC_RECORD:
      flag=block_info.second_read=0;
      block_info.next_filepos=pos;
      do
      {
	if (_mi_read_cache(&param->read_cache,(byte*) block_info.header,
			   (start_block=block_info.next_filepos),
			   sizeof(block_info.header),
			   (flag ? 0 : READING_NEXT) | READING_HEADER))
	  goto err;
	if (start_block & (MI_DYN_ALIGN_SIZE-1))
	{
	  mi_check_print_error(param,"Wrong aligned block at %s",
			       llstr(start_block,llbuff));
	  goto err2;
	}
	b_type=_mi_get_block_info(&block_info,-1,start_block);
	if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
		      BLOCK_FATAL_ERROR))
	{
	  if (b_type & BLOCK_SYNC_ERROR)
	  {
	    if (flag)
	    {
	      mi_check_print_error(param,"Unexpected byte: %d at link: %s",
			  (int) block_info.header[0],
			  llstr(start_block,llbuff));
	      goto err2;
	    }
	    pos=block_info.filepos+block_info.block_len;
	    goto next;
	  }
	  if (b_type & BLOCK_DELETED)
	  {
	    if (block_info.block_len < info->s->base.min_block_length)
	    {
	      mi_check_print_error(param,
				   "Deleted block with impossible length %lu at %s",
				   block_info.block_len,llstr(pos,llbuff));
	      goto err2;
	    }
	    if ((block_info.next_filepos != HA_OFFSET_ERROR &&
		 block_info.next_filepos >= info->state->data_file_length) ||
		(block_info.prev_filepos != HA_OFFSET_ERROR &&
		 block_info.prev_filepos >= info->state->data_file_length))
	    {
	      mi_check_print_error(param,"Delete link points outside datafile at %s",
			  llstr(pos,llbuff));
	      goto err2;
	    }
	    del_blocks++;
	    del_length+=block_info.block_len;
	    pos=block_info.filepos+block_info.block_len;
	    splits++;
	    goto next;
	  }
	  mi_check_print_error(param,"Wrong bytesec: %d-%d-%d at linkstart: %s",
			       block_info.header[0],block_info.header[1],
			       block_info.header[2],
			       llstr(start_block,llbuff));
	  goto err2;
	}
	if (info->state->data_file_length < block_info.filepos+
	    block_info.block_len)
	{
	  mi_check_print_error(param,
			       "Recordlink that points outside datafile at %s",
			       llstr(pos,llbuff));
	  got_error=1;
	  break;
	}
	splits++;
	if (!flag++)				/* First block */
	{
	  start_recpos=pos;
	  pos=block_info.filepos+block_info.block_len;
	  if (block_info.rec_len > (uint) info->s->base.max_pack_length)
	  {
	    mi_check_print_error(param,"Found too long record (%lu) at %s",
				 (ulong) block_info.rec_len,
				 llstr(start_recpos,llbuff));
	    got_error=1;
	    break;
	  }
	  if (info->s->base.blobs)
	  {
	    if (!(to= mi_alloc_rec_buff(info, block_info.rec_len,
					&info->rec_buff)))
	    {
	      mi_check_print_error(param,
				   "Not enough memory (%lu) for blob at %s",
				   (ulong) block_info.rec_len,
				   llstr(start_recpos,llbuff));
	      got_error=1;
	      break;
	    }
	  }
	  else
	    to= info->rec_buff;
	  left_length=block_info.rec_len;
	}
	if (left_length < block_info.data_len)
	{
	  mi_check_print_error(param,"Found too long record (%lu) at %s",
			       (ulong) block_info.data_len,
			       llstr(start_recpos,llbuff));
	  got_error=1;
	  break;
	}
	if (_mi_read_cache(&param->read_cache,(byte*) to,block_info.filepos,
			   (uint) block_info.data_len,
			   flag == 1 ? READING_NEXT : 0))
	  goto err;
	to+=block_info.data_len;
	link_used+= block_info.filepos-start_block;
	used+= block_info.filepos - start_block + block_info.data_len;
	empty+=block_info.block_len-block_info.data_len;
	left_length-=block_info.data_len;
	if (left_length)
	{
	  if (b_type & BLOCK_LAST)
	  {
	    mi_check_print_error(param,
				 "Wrong record length %s of %s at %s",
				 llstr(block_info.rec_len-left_length,llbuff),
				 llstr(block_info.rec_len, llbuff2),
				 llstr(start_recpos,llbuff3));
	    got_error=1;
	    break;
	  }
	  if (info->state->data_file_length < block_info.next_filepos)
	  {
	    mi_check_print_error(param,
				 "Found next-recordlink that points outside datafile at %s",
				 llstr(block_info.filepos,llbuff));
	    got_error=1;
	    break;
	  }
	}
      } while (left_length);
      if (! got_error)
      {
	if (_mi_rec_unpack(info,record,info->rec_buff,block_info.rec_len) ==
	    MY_FILE_ERROR)
	{
	  mi_check_print_error(param,"Found wrong record at %s",
			       llstr(start_recpos,llbuff));
	  got_error=1;
	}
	else
	{
	  info->checksum=mi_checksum(info,record);
	  if (param->testflag & (T_EXTEND | T_MEDIUM | T_VERBOSE))
	  {
	    if (_mi_rec_check(info,record, info->rec_buff,block_info.rec_len))
	    {
	      mi_check_print_error(param,"Found wrong packed record at %s",
			  llstr(start_recpos,llbuff));
	      got_error=1;
	    }
	  }
	  if (!got_error)
	    param->glob_crc+= info->checksum;
	}
      }
      else if (!flag)
	pos=block_info.filepos+block_info.block_len;
      break;
    case COMPRESSED_RECORD:
      if (_mi_read_cache(&param->read_cache,(byte*) block_info.header, pos,
			 info->s->pack.ref_length, READING_NEXT))
	goto err;
      start_recpos=pos;
      splits++;
      VOID(_mi_pack_get_block_info(info,&block_info, -1, start_recpos));
      pos=block_info.filepos+block_info.rec_len;
      if (block_info.rec_len < (uint) info->s->min_pack_length ||
	  block_info.rec_len > (uint) info->s->max_pack_length)
      {
	mi_check_print_error(param,
			     "Found block with wrong recordlength: %d at %s",
			     block_info.rec_len, llstr(start_recpos,llbuff));
	got_error=1;
	break;
      }
      if (_mi_read_cache(&param->read_cache,(byte*) info->rec_buff,
			block_info.filepos, block_info.rec_len, READING_NEXT))
	goto err;
      if (_mi_pack_rec_unpack(info,record,info->rec_buff,block_info.rec_len))
      {
	mi_check_print_error(param,"Found wrong record at %s",
			     llstr(start_recpos,llbuff));
	got_error=1;
      }
      if (static_row_size)
	param->glob_crc+= mi_static_checksum(info,record);
      else
	param->glob_crc+= mi_checksum(info,record);
      link_used+= (block_info.filepos - start_recpos);
      used+= (pos-start_recpos);
    } /* switch */
    if (! got_error)
    {
      intern_record_checksum+=(ha_checksum) start_recpos;
      records++;
      if (param->testflag & T_WRITE_LOOP && records % WRITE_COUNT == 0)
      {
	printf("%s\r", llstr(records,llbuff)); VOID(fflush(stdout));
      }

      /* Check if keys match the record */

      for (key=0,keyinfo= info->s->keyinfo; key < info->s->base.keys;
	   key++,keyinfo++)
      {
	if ((((ulonglong) 1 << key) & info->s->state.key_map))
	{
	  if(!(keyinfo->flag & HA_FULLTEXT))
	  {
	    uint key_length=_mi_make_key(info,key,info->lastkey,record,
					 start_recpos);
	    if (extend)
	    {
	      /* We don't need to lock the key tree here as we don't allow
		 concurrent threads when running myisamchk
	      */
	      if (_mi_search(info,keyinfo,info->lastkey,key_length,
			     SEARCH_SAME, info->s->state.key_root[key]))
	      {
		mi_check_print_error(param,"Record at: %10s  Can't find key for index: %2d",
			    llstr(start_recpos,llbuff),key+1);
		if (error++ > MAXERR || !(param->testflag & T_VERBOSE))
		  goto err2;
	      }
	    }
	    else
	      key_checksum[key]+=mi_byte_checksum((byte*) info->lastkey,
						  key_length);
	  }
	}
      }
    }
    else
    {
      got_error=0;
      if (error++ > MAXERR || !(param->testflag & T_VERBOSE))
	goto err2;
    }
  next:;				/* Next record */
  }
  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (records != info->state->records)
  {
    mi_check_print_error(param,"Record-count is not ok; is %-10s   Should be: %s",
		llstr(records,llbuff), llstr(info->state->records,llbuff2));
    error=1;
  }
  else if (param->record_checksum &&
	   param->record_checksum != intern_record_checksum)
  {
    mi_check_print_error(param,
			 "Keypointers and record positions doesn't match");
    error=1;
  }
  else if (param->glob_crc != info->s->state.checksum &&
	   (info->s->options &
	    (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD)))
  {
    mi_check_print_warning(param,
			   "Record checksum is not the same as checksum stored in the index file\n");
    error=1;
  }
  else if (!extend)
  {
    for (key=0 ; key < info->s->base.keys;  key++)
    {
      if (key_checksum[key] != param->key_crc[key] &&
          !(info->s->keyinfo[key].flag & (HA_FULLTEXT | HA_SPATIAL)))
      {
	mi_check_print_error(param,"Checksum for key: %2d doesn't match checksum for records",
		    key+1);
	error=1;
      }
    }
  }

  if (del_length != info->state->empty)
  {
    mi_check_print_warning(param,
			   "Found %s deleted space.   Should be %s",
			   llstr(del_length,llbuff2),
			   llstr(info->state->empty,llbuff));
  }
  if (used+empty+del_length != info->state->data_file_length)
  {
    mi_check_print_warning(param,
			   "Found %s record-data and %s unused data and %s deleted-data",
			   llstr(used,llbuff),llstr(empty,llbuff2),
			   llstr(del_length,llbuff3));
    mi_check_print_warning(param,
			   "Total %s, Should be: %s",
			   llstr((used+empty+del_length),llbuff),
			   llstr(info->state->data_file_length,llbuff2));
  }
  if (del_blocks != info->state->del)
  {
    mi_check_print_warning(param,
			   "Found %10s deleted blocks       Should be: %s",
			   llstr(del_blocks,llbuff),
			   llstr(info->state->del,llbuff2));
  }
  if (splits != info->s->state.split)
  {
    mi_check_print_warning(param,
			   "Found %10s parts                Should be: %s parts",
			   llstr(splits,llbuff),
			   llstr(info->s->state.split,llbuff2));
  }
  if (param->testflag & T_INFO)
  {
    if (param->warning_printed || param->error_printed)
      puts("");
    if (used != 0 && ! param->error_printed)
    {
      printf("Records:%18s    M.recordlength:%9lu   Packed:%14.0f%%\n",
	     llstr(records,llbuff), (long)((used-link_used)/records),
	     (info->s->base.blobs ? 0.0 :
	      (ulonglong2double((ulonglong) info->s->base.reclength*records)-
	       my_off_t2double(used))/
	      ulonglong2double((ulonglong) info->s->base.reclength*records)*100.0));
      printf("Recordspace used:%9.0f%%   Empty space:%12d%%  Blocks/Record: %6.2f\n",
	     (ulonglong2double(used-link_used)/ulonglong2double(used-link_used+empty)*100.0),
	     (!records ? 100 : (int) (ulonglong2double(del_length+empty)/
				      my_off_t2double(used)*100.0)),
	     ulonglong2double(splits - del_blocks) / records);
    }
    printf("Record blocks:%12s    Delete blocks:%10s\n",
	   llstr(splits-del_blocks,llbuff),llstr(del_blocks,llbuff2));
    printf("Record data:  %12s    Deleted data: %10s\n",
	   llstr(used-link_used,llbuff),llstr(del_length,llbuff2));
    printf("Lost space:   %12s    Linkdata:     %10s\n",
	   llstr(empty,llbuff),llstr(link_used,llbuff2));
  }
  my_free((gptr) record,MYF(0));
  DBUG_RETURN (error);
 err:
  mi_check_print_error(param,"got error: %d when reading datafile at record: %s",my_errno, llstr(records,llbuff));
 err2:
  my_free((gptr) record,MYF(0));
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  DBUG_RETURN(1);
} /* chk_data_link */


	/* Recover old table by reading each record and writing all keys */
	/* Save new datafile-name in temp_filename */

int mi_repair(MI_CHECK *param, register MI_INFO *info,
	      my_string name, int rep_quick)
{
  int error,got_error;
  uint i;
  ha_rows start_records,new_header_length;
  my_off_t del;
  File new_file;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];
  SORT_INFO sort_info;
  MI_SORT_PARAM sort_param;
  DBUG_ENTER("mi_repair");

  bzero((char *)&sort_info, sizeof(sort_info));
  bzero((char *)&sort_param, sizeof(sort_param));
  start_records=info->state->records;
  new_header_length=(param->testflag & T_UNPACK) ? 0L :
    share->pack.header_length;
  got_error=1;
  new_file= -1;
  sort_param.sort_info=&sort_info;

  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with keycache) MyISAM-table '%s'\n",name);
    printf("Data records: %s\n", llstr(info->state->records,llbuff));
  }
  param->testflag|=T_REP; /* for easy checking */

  if (info->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    param->testflag|=T_CALC_CHECKSUM;

  if (!param->using_global_keycache)
    VOID(init_key_cache(dflt_key_cache, param->key_cache_block_size,
                        param->use_buffers, 0, 0));

  if (init_io_cache(&param->read_cache,info->dfile,
		    (uint) param->read_buffer_length,
		    READ_CACHE,share->pack.header_length,1,MYF(MY_WME)))
  {
    bzero(&info->rec_cache,sizeof(info->rec_cache));
    goto err;
  }
  if (!rep_quick)
    if (init_io_cache(&info->rec_cache,-1,(uint) param->write_buffer_length,
		      WRITE_CACHE, new_header_length, 1,
		      MYF(MY_WME | MY_WAIT_IF_FULL)))
      goto err;
  info->opt_flag|=WRITE_CACHE_USED;
  if (!(sort_param.record=(byte*) my_malloc((uint) share->base.pack_reclength,
					   MYF(0))) ||
      !mi_alloc_rec_buff(info, -1, &sort_param.rec_buff))
  {
    mi_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_raid_create(fn_format(param->temp_filename,
					   share->data_file_name, "",
					   DATA_TMP_EXT, 2+4),
				 0,param->tmpfile_createflag,
				 share->base.raid_type,
				 share->base.raid_chunks,
				 share->base.raid_chunksize,
				 MYF(0))) < 0)
    {
      mi_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (filecopy(param,new_file,info->dfile,0L,new_header_length,
		 "datafile-header"))
      goto err;
    info->s->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
    if (param->testflag & T_UNPACK)
    {
      share->options&= ~HA_OPTION_COMPRESS_RECORD;
      mi_int2store(share->state.header.options,share->options);
    }
  }
  sort_info.info=info;
  sort_info.param = param;
  sort_param.read_cache=param->read_cache;
  sort_param.pos=sort_param.max_pos=share->pack.header_length;
  sort_param.filepos=new_header_length;
  param->read_cache.end_of_file=sort_info.filelength=
    my_seek(info->dfile,0L,MY_SEEK_END,MYF(0));
  sort_info.dupp=0;
  sort_param.fix_datafile= (my_bool) (! rep_quick);
  sort_param.master=1;
  sort_info.max_records= ~(ha_rows) 0;

  set_data_file_type(&sort_info, share);
  del=info->state->del;
  info->state->records=info->state->del=share->state.split=0;
  info->state->empty=0;
  param->glob_crc=0;
  if (param->testflag & T_CALC_CHECKSUM)
    param->calc_checksum=1;

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (i=0 ; i < info->s->base.keys ; i++)
    share->state.key_root[i]= HA_OFFSET_ERROR;
  for (i=0 ; i < share->state.header.max_block_size ; i++)
    share->state.key_del[i]=  HA_OFFSET_ERROR;

  /*
    I think mi_repair and mi_repair_by_sort should do the same
    (according, e.g. to ha_myisam::repair), but as mi_repair doesn't
    touch key_map it cannot be used to T_CREATE_MISSING_KEYS.
    That is what the next line is for
  */

  if (param->testflag & T_CREATE_MISSING_KEYS)
    share->state.key_map= ((((ulonglong) 1L << share->base.keys)-1) &
			   param->keys_in_use);

  info->state->key_file_length=share->base.keystart;

  lock_memory(param);			/* Everything is alloced */
  while (!(error=sort_get_next_record(&sort_param)))
  {
    if (writekeys(param,info,(byte*)sort_param.record,sort_param.filepos))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY)
	goto err;
      DBUG_DUMP("record",(byte*) sort_param.record,share->base.pack_reclength);
      mi_check_print_info(param,"Duplicate key %2d for record at %10s against new record at %10s",
			  info->errkey+1,
			  llstr(sort_param.start_recpos,llbuff),
			  llstr(info->dupp_key_pos,llbuff2));
      if (param->testflag & T_VERBOSE)
      {
	VOID(_mi_make_key(info,(uint) info->errkey,info->lastkey,
			  sort_param.record,0L));
	_mi_print_key(stdout,share->keyinfo[info->errkey].seg,info->lastkey,
		      USE_WHOLE_KEY);
      }
      sort_info.dupp++;
      if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
      {
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	param->error_printed=1;
	goto err;
      }
      continue;
    }
    if (sort_write_record(&sort_param))
      goto err;
  }
  if (error > 0 || write_data_suffix(&sort_info, (my_bool)!rep_quick) ||
      flush_io_cache(&info->rec_cache) || param->read_cache.error < 0)
    goto err;

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (my_chsize(share->kfile,info->state->key_file_length,0,MYF(0)))
  {
    mi_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   my_errno);
    goto err;
  }

  if (rep_quick && del+sort_info.dupp != info->state->del)
  {
    mi_check_print_error(param,"Couldn't fix table with quick recovery: Found wrong number of deleted records");
    mi_check_print_error(param,"Run recovery again without -q");
    got_error=1;
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }
  if (param->testflag & T_SAFE_REPAIR)
  {
    /* Don't repair if we loosed more than one row */
    if (info->state->records+1 < start_records)
    {
      info->state->records=start_records;
      got_error=1;
      goto err;
    }
  }

  if (!rep_quick)
  {
    my_close(info->dfile,MYF(0));
    info->dfile=new_file;
    info->state->data_file_length=sort_param.filepos;
    share->state.version=(ulong) time((time_t*) 0);	/* Force reopen */
  }
  else
  {
    info->state->data_file_length=sort_param.max_pos;
  }
  if (param->testflag & T_CALC_CHECKSUM)
    share->state.checksum=param->glob_crc;

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != info->state->records)
      printf("Data records: %s\n", llstr(info->state->records,llbuff));
    if (sort_info.dupp)
      mi_check_print_warning(param,
			     "%s records have been removed",
			     llstr(sort_info.dupp,llbuff));
  }

  got_error=0;
  /* If invoked by external program that uses thr_lock */
  if (&share->state.state != info->state)
    memcpy( &share->state.state, info->state, sizeof(*info->state));

err:
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      my_close(new_file,MYF(0));
      info->dfile=new_file= -1;
      if (change_to_newfile(share->data_file_name,MI_NAME_DEXT,
			    DATA_TMP_EXT, share->base.raid_chunks,
			    (param->testflag & T_BACKUP_DATA ?
			     MYF(MY_REDEL_MAKE_BACKUP): MYF(0))) ||
	  mi_open_datafile(info,share,-1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      mi_check_print_error(param,"%d for record at pos %s",my_errno,
		  llstr(sort_param.start_recpos,llbuff));
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_raid_delete(param->temp_filename,info->s->base.raid_chunks,
			  MYF(MY_WME)));
      info->rec_cache.file=-1; /* don't flush data to new_file, it's closed */
    }
    mi_mark_crashed_on_repair(info);
  }
  my_free(mi_get_rec_buff_ptr(info, sort_param.rec_buff),
                            MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  VOID(end_io_cache(&param->read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  VOID(end_io_cache(&info->rec_cache));
  got_error|=flush_blocks(param, share->key_cache, share->kfile);
  if (!got_error && param->testflag & T_UNPACK)
  {
    share->state.header.options[0]&= (uchar) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
    share->data_file_type=sort_info.new_data_file_type;
  }
  share->state.changed|= (STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES |
			  STATE_NOT_ANALYZED);
  DBUG_RETURN(got_error);
}


/* Uppate keyfile when doing repair */

static int writekeys(MI_CHECK *param, register MI_INFO *info, byte *buff,
		     my_off_t filepos)
{
  register uint i;
  uchar *key;
  DBUG_ENTER("writekeys");

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->base.keys ; i++)
  {
    if (((ulonglong) 1 << i) & info->s->state.key_map)
    {
      if (info->s->keyinfo[i].flag & HA_FULLTEXT )
      {
        if (_mi_ft_add(info,i,(char*) key,buff,filepos))
	  goto err;
      }
      else
      {
	uint key_length=_mi_make_key(info,i,key,buff,filepos);
	if (_mi_ck_write(info,i,key,key_length))
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
      if (((ulonglong) 1 << i) & info->s->state.key_map)
      {
	if (info->s->keyinfo[i].flag & HA_FULLTEXT)
        {
          if (_mi_ft_del(info,i,(char*) key,buff,filepos))
	    break;
        }
        else
	{
	  uint key_length=_mi_make_key(info,i,key,buff,filepos);
	  if (_mi_ck_delete(info,i,key,key_length))
	    break;
	}
      }
    }
  }
  /* Remove checksum that was added to glob_crc in sort_get_next_record */
  if (param->calc_checksum)
    param->glob_crc-= info->checksum;
  DBUG_PRINT("error",("errno: %d",my_errno));
  DBUG_RETURN(-1);
} /* writekeys */


	/* Change all key-pointers that points to a records */

int movepoint(register MI_INFO *info, byte *record, my_off_t oldpos,
	      my_off_t newpos, uint prot_key)
{
  register uint i;
  uchar *key;
  uint key_length;
  DBUG_ENTER("movepoint");

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->base.keys; i++)
  {
    if (i != prot_key && (((ulonglong) 1 << i) & info->s->state.key_map))
    {
      key_length=_mi_make_key(info,i,key,record,oldpos);
      if (info->s->keyinfo[i].flag & HA_NOSAME)
      {					/* Change pointer direct */
	uint nod_flag;
	MI_KEYDEF *keyinfo;
	keyinfo=info->s->keyinfo+i;
	if (_mi_search(info,keyinfo,key,USE_WHOLE_KEY,
		       (uint) (SEARCH_SAME | SEARCH_SAVE_BUFF),
		       info->s->state.key_root[i]))
	  DBUG_RETURN(-1);
	nod_flag=mi_test_if_nod(info->buff);
	_mi_dpointer(info,info->int_keypos-nod_flag-
		     info->s->rec_reflength,newpos);
	if (_mi_write_keypage(info,keyinfo,info->last_keypage,
                              DFLT_INIT_HITS,info->buff))
	  DBUG_RETURN(-1);
      }
      else
      {					/* Change old key to new */
	if (_mi_ck_delete(info,i,key,key_length))
	  DBUG_RETURN(-1);
	key_length=_mi_make_key(info,i,key,record,newpos);
	if (_mi_ck_write(info,i,key,key_length))
	  DBUG_RETURN(-1);
      }
    }
  }
  DBUG_RETURN(0);
} /* movepoint */


	/* Tell system that we want all memory for our cache */

void lock_memory(MI_CHECK *param __attribute__((unused)))
{
#ifdef SUN_OS				/* Key-cacheing thrases on sun 4.1 */
  if (param->opt_lock_memory)
  {
    int success = mlockall(MCL_CURRENT);	/* or plock(DATLOCK); */
    if (geteuid() == 0 && success != 0)
      mi_check_print_warning(param,
			     "Failed to lock memory. errno %d",my_errno);
  }
#endif
} /* lock_memory */


	/* Flush all changed blocks to disk */

int flush_blocks(MI_CHECK *param, KEY_CACHE *key_cache, File file)
{
  if (flush_key_blocks(key_cache, file, FLUSH_RELEASE))
  {
    mi_check_print_error(param,"%d when trying to write bufferts",my_errno);
    return(1);
  }
  if (!param->using_global_keycache)
    end_key_cache(key_cache,1);
  return 0;
} /* flush_blocks */


	/* Sort index for more efficent reads */

int mi_sort_index(MI_CHECK *param, register MI_INFO *info, my_string name)
{
  reg2 uint key;
  reg1 MI_KEYDEF *keyinfo;
  File new_file;
  my_off_t index_pos[MI_MAX_POSSIBLE_KEY];
  uint r_locks,w_locks;
  int old_lock;
  MYISAM_SHARE *share=info->s;
  MI_STATE_INFO old_state;
  DBUG_ENTER("mi_sort_index");

  if (!(param->testflag & T_SILENT))
    printf("- Sorting index for MyISAM-table '%s'\n",name);

  /* Get real path for index file */
  fn_format(param->temp_filename,name,"", MI_NAME_IEXT,2+4+32);
  if ((new_file=my_create(fn_format(param->temp_filename,param->temp_filename,
				    "", INDEX_TMP_EXT,2+4),
			  0,param->tmpfile_createflag,MYF(0))) <= 0)
  {
    mi_check_print_error(param,"Can't create new tempfile: '%s'",
			 param->temp_filename);
    DBUG_RETURN(-1);
  }
  if (filecopy(param, new_file,share->kfile,0L,
	       (ulong) share->base.keystart, "headerblock"))
    goto err;

  param->new_file_pos=share->base.keystart;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       key++,keyinfo++)
  {
    if (!(((ulonglong) 1 << key) & share->state.key_map))
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

  /* Flush key cache for this file if we are calling this outside myisamchk */
  flush_key_blocks(share->key_cache,share->kfile, FLUSH_IGNORE_CHANGED);

  share->state.version=(ulong) time((time_t*) 0);
  old_state= share->state;			/* save state if not stored */
  r_locks=   share->r_locks;
  w_locks=   share->w_locks;
  old_lock=  info->lock_type;

	/* Put same locks as old file */
  share->r_locks= share->w_locks= share->tot_locks= 0;
  (void) _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  VOID(my_close(share->kfile,MYF(MY_WME)));
  share->kfile = -1;
  VOID(my_close(new_file,MYF(MY_WME)));
  if (change_to_newfile(share->index_file_name,MI_NAME_IEXT,INDEX_TMP_EXT,0,
			MYF(0)) ||
      mi_open_keyfile(share))
    goto err2;
  info->lock_type= F_UNLCK;			/* Force mi_readinfo to lock */
  _mi_readinfo(info,F_WRLCK,0);			/* Will lock the table */
  info->lock_type=  old_lock;
  share->r_locks=   r_locks;
  share->w_locks=   w_locks;
  share->tot_locks= r_locks+w_locks;
  share->state=     old_state;			/* Restore old state */

  info->state->key_file_length=param->new_file_pos;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (key=0 ; key < info->s->base.keys ; key++)
    info->s->state.key_root[key]=index_pos[key];
  for (key=0 ; key < info->s->state.header.max_block_size ; key++)
    info->s->state.key_del[key]=  HA_OFFSET_ERROR;

  info->s->state.changed&= ~STATE_NOT_SORTED_PAGES;
  DBUG_RETURN(0);

err:
  VOID(my_close(new_file,MYF(MY_WME)));
err2:
  VOID(my_delete(param->temp_filename,MYF(MY_WME)));
  DBUG_RETURN(-1);
} /* mi_sort_index */


	 /* Sort records recursive using one index */

static int sort_one_index(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
			  my_off_t pagepos, File new_file)
{
  uint length,nod_flag,used_length, key_length;
  uchar *buff,*keypos,*endpos;
  uchar key[MI_MAX_POSSIBLE_KEY_BUFF];
  my_off_t new_page_pos,next_page;
  char llbuff[22];
  DBUG_ENTER("sort_one_index");

  new_page_pos=param->new_file_pos;
  param->new_file_pos+=keyinfo->block_length;

  if (!(buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    mi_check_print_error(param,"Not enough memory for key block");
    DBUG_RETURN(-1);
  }
  if (!_mi_fetch_keypage(info,keyinfo,pagepos,DFLT_INIT_HITS,buff,0))
  {
    mi_check_print_error(param,"Can't read key block from filepos: %s",
		llstr(pagepos,llbuff));
    goto err;
  }
  if ((nod_flag=mi_test_if_nod(buff)) || keyinfo->flag & HA_FULLTEXT)
  {
    used_length=mi_getint(buff);
    keypos=buff+2+nod_flag;
    endpos=buff+used_length;
    for ( ;; )
    {
      if (nod_flag)
      {
	next_page=_mi_kpos(nod_flag,keypos);
	_mi_kpointer(info,keypos-nod_flag,param->new_file_pos); /* Save new pos */
	if (sort_one_index(param,info,keyinfo,next_page,new_file))
	{
	  DBUG_PRINT("error",("From page: %ld, keyoffset: %d  used_length: %d",
			      (ulong) pagepos, (int) (keypos - buff),
			      (int) used_length));
	  DBUG_DUMP("buff",(byte*) buff,used_length);
	  goto err;
	}
      }
      if (keypos >= endpos ||
	  (key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,key)) == 0)
	break;
      DBUG_ASSERT(keypos <= endpos);
      if (keyinfo->flag & HA_FULLTEXT)
      {
        uint off;
        int  subkeys;
        get_key_full_length_rdonly(off, key);
        subkeys=ft_sintXkorr(key+off);
        if (subkeys < 0)
        {
          next_page= _mi_dpos(info,0,key+key_length);
          _mi_dpointer(info,keypos-nod_flag-info->s->rec_reflength,
                       param->new_file_pos); /* Save new pos */
          if (sort_one_index(param,info,&info->s->ft2_keyinfo,
                             next_page,new_file))
            goto err;
        }
      }
    }
  }

  /* Fill block with zero and write it to the new index file */
  length=mi_getint(buff);
  bzero((byte*) buff+length,keyinfo->block_length-length);
  if (my_pwrite(new_file,(byte*) buff,(uint) keyinfo->block_length,
		new_page_pos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
  {
    mi_check_print_error(param,"Can't write indexblock, error: %d",my_errno);
    goto err;
  }
  my_afree((gptr) buff);
  DBUG_RETURN(0);
err:
  my_afree((gptr) buff);
  DBUG_RETURN(1);
} /* sort_one_index */


	/*
	  Let temporary file replace old file.
	  This assumes that the new file was created in the same
	  directory as given by realpath(filename).
	  This will ensure that any symlinks that are used will still work.
	  Copy stats from old file to new file, deletes orignal and
	  changes new file name to old file name
	*/

int change_to_newfile(const char * filename, const char * old_ext,
		      const char * new_ext,
		      uint raid_chunks __attribute__((unused)),
		      myf MyFlags)
{
  char old_filename[FN_REFLEN],new_filename[FN_REFLEN];
#ifdef USE_RAID
  if (raid_chunks)
    return my_raid_redel(fn_format(old_filename,filename,"",old_ext,2+4),
			 fn_format(new_filename,filename,"",new_ext,2+4),
			 raid_chunks,
			 MYF(MY_WME | MY_LINK_WARNING | MyFlags));
#endif
  /* Get real path to filename */
  (void) fn_format(old_filename,filename,"",old_ext,2+4+32);
  return my_redel(old_filename,
		  fn_format(new_filename,old_filename,"",new_ext,2+4),
		  MYF(MY_WME | MY_LINK_WARNING | MyFlags));
} /* change_to_newfile */


	/* Locks a whole file */
	/* Gives an error-message if file can't be locked */

int lock_file(MI_CHECK *param, File file, my_off_t start, int lock_type,
	      const char *filetype, const char *filename)
{
  if (my_lock(file,lock_type,start,F_TO_EOF,
	      param->testflag & T_WAIT_FOREVER ? MYF(MY_SEEK_NOT_DONE) :
	      MYF(MY_SEEK_NOT_DONE |  MY_DONT_WAIT)))
  {
    mi_check_print_error(param," %d when locking %s '%s'",my_errno,filetype,filename);
    param->error_printed=2;		/* Don't give that data is crashed */
    return 1;
  }
  return 0;
} /* lock_file */


	/* Copy a block between two files */

int filecopy(MI_CHECK *param, File to,File from,my_off_t start,
	     my_off_t length, const char *type)
{
  char tmp_buff[IO_SIZE],*buff;
  ulong buff_length;
  DBUG_ENTER("filecopy");

  buff_length=(ulong) min(param->write_buffer_length,length);
  if (!(buff=my_malloc(buff_length,MYF(0))))
  {
    buff=tmp_buff; buff_length=IO_SIZE;
  }

  VOID(my_seek(from,start,MY_SEEK_SET,MYF(0)));
  while (length > buff_length)
  {
    if (my_read(from,(byte*) buff,buff_length,MYF(MY_NABP)) ||
	my_write(to,(byte*) buff,buff_length,param->myf_rw))
      goto err;
    length-= buff_length;
  }
  if (my_read(from,(byte*) buff,(uint) length,MYF(MY_NABP)) ||
      my_write(to,(byte*) buff,(uint) length,param->myf_rw))
    goto err;
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  DBUG_RETURN(0);
err:
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  mi_check_print_error(param,"Can't copy %s to tempfile, error %d",
		       type,my_errno);
  DBUG_RETURN(1);
}


/*
  Repair table or given index using sorting

  SYNOPSIS
    mi_repair_by_sort()
    param		Repair parameters
    info		MyISAM handler to repair
    name		Name of table (for warnings)
    rep_quick		set to <> 0 if we should not change data file

  RESULT
    0	ok
    <>0	Error
*/

int mi_repair_by_sort(MI_CHECK *param, register MI_INFO *info,
		      const char * name, int rep_quick)
{
  int got_error;
  uint i;
  ulong length;
  ha_rows start_records;
  my_off_t new_header_length,del;
  File new_file;
  MI_SORT_PARAM sort_param;
  MYISAM_SHARE *share=info->s;
  HA_KEYSEG *keyseg;
  ulong   *rec_per_key_part;
  char llbuff[22];
  SORT_INFO sort_info;
  ulonglong key_map=share->state.key_map;
  DBUG_ENTER("mi_repair_by_sort");

  start_records=info->state->records;
  got_error=1;
  new_file= -1;
  new_header_length=(param->testflag & T_UNPACK) ? 0 :
    share->pack.header_length;
  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with sort) MyISAM-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records,llbuff));
  }
  param->testflag|=T_REP; /* for easy checking */

  if (info->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    param->testflag|=T_CALC_CHECKSUM;

  bzero((char*)&sort_info,sizeof(sort_info));
  bzero((char *)&sort_param, sizeof(sort_param));
  if (!(sort_info.key_block=
	alloc_key_blocks(param,
			 (uint) param->sort_key_blocks,
			 share->base.max_key_block_length))
      || init_io_cache(&param->read_cache,info->dfile,
		       (uint) param->read_buffer_length,
		       READ_CACHE,share->pack.header_length,1,MYF(MY_WME)) ||
      (! rep_quick &&
       init_io_cache(&info->rec_cache,info->dfile,
		     (uint) param->write_buffer_length,
		     WRITE_CACHE,new_header_length,1,
		     MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw)))
    goto err;
  sort_info.key_block_end=sort_info.key_block+param->sort_key_blocks;
  info->opt_flag|=WRITE_CACHE_USED;
  info->rec_cache.file=info->dfile;		/* for sort_delete_record */

  if (!(sort_param.record=(byte*) my_malloc((uint) share->base.pack_reclength,
					   MYF(0))) ||
      !mi_alloc_rec_buff(info, -1, &sort_param.rec_buff))
  {
    mi_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }
  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_raid_create(fn_format(param->temp_filename,
					   share->data_file_name, "",
					   DATA_TMP_EXT, 2+4),
				 0,param->tmpfile_createflag,
				 share->base.raid_type,
				 share->base.raid_chunks,
				 share->base.raid_chunksize,
				 MYF(0))) < 0)
    {
      mi_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (filecopy(param, new_file,info->dfile,0L,new_header_length,
		 "datafile-header"))
      goto err;
    if (param->testflag & T_UNPACK)
    {
      share->options&= ~HA_OPTION_COMPRESS_RECORD;
      mi_int2store(share->state.header.options,share->options);
    }
    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
  }

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if (!(param->testflag & T_CREATE_MISSING_KEYS))
  {
    /*
      Flush key cache for this file if we are calling this outside
      myisamchk
    */
    flush_key_blocks(share->key_cache,share->kfile, FLUSH_IGNORE_CHANGED);
    /* Clear the pointers to the given rows */
    for (i=0 ; i < share->base.keys ; i++)
      share->state.key_root[i]= HA_OFFSET_ERROR;
    for (i=0 ; i < share->state.header.max_block_size ; i++)
      share->state.key_del[i]=  HA_OFFSET_ERROR;
    info->state->key_file_length=share->base.keystart;
  }
  else
  {
    if (flush_key_blocks(share->key_cache,share->kfile, FLUSH_FORCE_WRITE))
      goto err;
    key_map= ~key_map;				/* Create the missing keys */
  }

  sort_info.info=info;
  sort_info.param = param;

  set_data_file_type(&sort_info, share);
  sort_param.filepos=new_header_length;
  sort_info.dupp=0;
  sort_info.buff=0;
  param->read_cache.end_of_file=sort_info.filelength=
    my_seek(param->read_cache.file,0L,MY_SEEK_END,MYF(0));

  sort_param.wordlist=NULL;

  if (share->data_file_type == DYNAMIC_RECORD)
    length=max(share->base.min_pack_length+1,share->base.min_block_length);
  else if (share->data_file_type == COMPRESSED_RECORD)
    length=share->base.min_block_length;
  else
    length=share->base.pack_reclength;
  sort_info.max_records=
    ((param->testflag & T_CREATE_MISSING_KEYS) ? info->state->records :
     (ha_rows) (sort_info.filelength/length+1));
  sort_param.key_cmp=sort_key_cmp;
  sort_param.lock_in_memory=lock_memory;
  sort_param.tmpdir=param->tmpdir;
  sort_param.sort_info=&sort_info;
  sort_param.fix_datafile= (my_bool) (! rep_quick);
  sort_param.master =1;

  del=info->state->del;
  param->glob_crc=0;
  if (param->testflag & T_CALC_CHECKSUM)
    param->calc_checksum=1;

  rec_per_key_part= param->rec_per_key_part;
  for (sort_param.key=0 ; sort_param.key < share->base.keys ;
       rec_per_key_part+=sort_param.keyinfo->keysegs, sort_param.key++)
  {
    sort_param.read_cache=param->read_cache;
    sort_param.keyinfo=share->keyinfo+sort_param.key;
    sort_param.seg=sort_param.keyinfo->seg;
    if (!(((ulonglong) 1 << sort_param.key) & key_map))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part +
		      (uint) (rec_per_key_part - param->rec_per_key_part)),
	     sort_param.keyinfo->keysegs*sizeof(*rec_per_key_part));
      continue;
    }

    if ((!(param->testflag & T_SILENT)))
      printf ("- Fixing index %d\n",sort_param.key+1);
    sort_param.max_pos=sort_param.pos=share->pack.header_length;
    keyseg=sort_param.seg;
    bzero((char*) sort_param.unique,sizeof(sort_param.unique));
    sort_param.key_length=share->rec_reflength;
    for (i=0 ; keyseg[i].type != HA_KEYTYPE_END; i++)
    {
      sort_param.key_length+=keyseg[i].length;
      if (keyseg[i].flag & HA_SPACE_PACK)
	sort_param.key_length+=get_pack_length(keyseg[i].length);
      if (keyseg[i].flag & (HA_BLOB_PART | HA_VAR_LENGTH))
	sort_param.key_length+=2 + test(keyseg[i].length >= 127);
      if (keyseg[i].flag & HA_NULL_PART)
	sort_param.key_length++;
    }
    info->state->records=info->state->del=share->state.split=0;
    info->state->empty=0;

    if (sort_param.keyinfo->flag & HA_FULLTEXT)
    {
      uint ft_max_word_len_for_sort=FT_MAX_WORD_LEN_FOR_SORT*
                                    sort_param.keyinfo->seg->charset->mbmaxlen;
      sort_info.max_records=
        (ha_rows) (sort_info.filelength/ft_min_word_len+1);

      sort_param.key_read=sort_ft_key_read;
      sort_param.key_write=sort_ft_key_write;
      sort_param.key_length+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
    }
    else
    {
      sort_param.key_read=sort_key_read;
      sort_param.key_write=sort_key_write;
    }

    if (_create_index_by_sort(&sort_param,
			      (my_bool) (!(param->testflag & T_VERBOSE)),
			      (uint) param->sort_buffer_length))
    {
      param->retry_repair=1;
      goto err;
    }
    param->calc_checksum=0;			/* No need to calc glob_crc */

    /* Set for next loop */
    sort_info.max_records= (ha_rows) info->state->records;

    if (param->testflag & T_STATISTICS)
      update_key_parts(sort_param.keyinfo, rec_per_key_part, sort_param.unique,
		       (ulonglong) info->state->records);
    share->state.key_map|=(ulonglong) 1 << sort_param.key;

    if (sort_param.fix_datafile)
    {
      param->read_cache.end_of_file=sort_param.filepos;
      if (write_data_suffix(&sort_info,1) || end_io_cache(&info->rec_cache))
	goto err;
      if (param->testflag & T_SAFE_REPAIR)
      {
	/* Don't repair if we loosed more than one row */
	if (info->state->records+1 < start_records)
	{
	  info->state->records=start_records;
	  goto err;
	}
      }
      share->state.state.data_file_length = info->state->data_file_length=
	sort_param.filepos;
      /* Only whole records */
      share->state.version=(ulong) time((time_t*) 0);
      my_close(info->dfile,MYF(0));
      info->dfile=new_file;
      share->data_file_type=sort_info.new_data_file_type;
      share->pack.header_length=(ulong) new_header_length;
      sort_param.fix_datafile=0;
    }
    else
      info->state->data_file_length=sort_param.max_pos;

    param->read_cache.file=info->dfile;		/* re-init read cache */
    reinit_io_cache(&param->read_cache,READ_CACHE,share->pack.header_length,
                    1,1);
  }

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }

  if (rep_quick && del+sort_info.dupp != info->state->del)
  {
    mi_check_print_error(param,"Couldn't fix table with quick recovery: Found wrong number of deleted records");
    mi_check_print_error(param,"Run recovery again without -q");
    got_error=1;
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (rep_quick & T_FORCE_UNIQUENESS)
  {
    my_off_t skr=info->state->data_file_length+
      (share->options & HA_OPTION_COMPRESS_RECORD ?
       MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (share->data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength && !info->s->base.raid_type)
      if (my_chsize(info->dfile,skr,0,MYF(0)))
	mi_check_print_warning(param,
			       "Can't change size of datafile,  error: %d",
			       my_errno);
  }
  if (param->testflag & T_CALC_CHECKSUM)
    share->state.checksum=param->glob_crc;

  if (my_chsize(share->kfile,info->state->key_file_length,0,MYF(0)))
    mi_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   my_errno);

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != info->state->records)
      printf("Data records: %s\n", llstr(info->state->records,llbuff));
    if (sort_info.dupp)
      mi_check_print_warning(param,
			     "%s records have been removed",
			     llstr(sort_info.dupp,llbuff));
  }
  got_error=0;

  if (&share->state.state != info->state)
    memcpy( &share->state.state, info->state, sizeof(*info->state));

err:
  got_error|= flush_blocks(param, share->key_cache, share->kfile);
  VOID(end_io_cache(&info->rec_cache));
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      my_close(new_file,MYF(0));
      info->dfile=new_file= -1;
      if (change_to_newfile(share->data_file_name,MI_NAME_DEXT,
			    DATA_TMP_EXT, share->base.raid_chunks,
			    (param->testflag & T_BACKUP_DATA ?
			     MYF(MY_REDEL_MAKE_BACKUP): MYF(0))) ||
	  mi_open_datafile(info,share,-1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      mi_check_print_error(param,"%d when fixing table",my_errno);
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_raid_delete(param->temp_filename,share->base.raid_chunks,
			  MYF(MY_WME)));
      if (info->dfile == new_file)
	info->dfile= -1;
    }
    mi_mark_crashed_on_repair(info);
  }
  else if (key_map == share->state.key_map)
    share->state.changed&= ~STATE_NOT_OPTIMIZED_KEYS;
  share->state.changed|=STATE_NOT_SORTED_PAGES;

  my_free(mi_get_rec_buff_ptr(info, sort_param.rec_buff),
                            MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) sort_info.key_block,MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) sort_info.ft_buf, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  VOID(end_io_cache(&param->read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  if (!got_error && (param->testflag & T_UNPACK))
  {
    share->state.header.options[0]&= (uchar) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
  }
  DBUG_RETURN(got_error);
}

/*
  Threaded repair of table using sorting

  SYNOPSIS
    mi_repair_parallel()
    param		Repair parameters
    info		MyISAM handler to repair
    name		Name of table (for warnings)
    rep_quick		set to <> 0 if we should not change data file

  DESCRIPTION
    Same as mi_repair_by_sort but do it multithreaded
    Each key is handled by a separate thread.
    TODO: make a number of threads a parameter

  RESULT
    0	ok
    <>0	Error
*/

int mi_repair_parallel(MI_CHECK *param, register MI_INFO *info,
			const char * name, int rep_quick)
{
#ifndef THREAD
  return mi_repair_by_sort(param, info, name, rep_quick);
#else
  int got_error;
  uint i,key, total_key_length, istep;
  ulong rec_length;
  ha_rows start_records;
  my_off_t new_header_length,del;
  File new_file;
  MI_SORT_PARAM *sort_param=0;
  MYISAM_SHARE *share=info->s;
  ulong   *rec_per_key_part;
  HA_KEYSEG *keyseg;
  char llbuff[22];
  IO_CACHE_SHARE io_share;
  SORT_INFO sort_info;
  ulonglong key_map=share->state.key_map;
  pthread_attr_t thr_attr;
  DBUG_ENTER("mi_repair_parallel");

  start_records=info->state->records;
  got_error=1;
  new_file= -1;
  new_header_length=(param->testflag & T_UNPACK) ? 0 :
    share->pack.header_length;
  if (!(param->testflag & T_SILENT))
  {
    printf("- parallel recovering (with sort) MyISAM-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records,llbuff));
  }
  param->testflag|=T_REP; /* for easy checking */

  if (info->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    param->testflag|=T_CALC_CHECKSUM;

  bzero((char*)&sort_info,sizeof(sort_info));
  if (!(sort_info.key_block=
	alloc_key_blocks(param,
			 (uint) param->sort_key_blocks,
			 share->base.max_key_block_length))
      || init_io_cache(&param->read_cache,info->dfile,
		       (uint) param->read_buffer_length,
		       READ_CACHE,share->pack.header_length,1,MYF(MY_WME)) ||
      (! rep_quick &&
       init_io_cache(&info->rec_cache,info->dfile,
		     (uint) param->write_buffer_length,
		     WRITE_CACHE,new_header_length,1,
		     MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw)))
    goto err;
  sort_info.key_block_end=sort_info.key_block+param->sort_key_blocks;
  info->opt_flag|=WRITE_CACHE_USED;
  info->rec_cache.file=info->dfile;         /* for sort_delete_record */

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_raid_create(fn_format(param->temp_filename,
					   share->data_file_name, "",
					   DATA_TMP_EXT,
					   2+4),
				 0,param->tmpfile_createflag,
				 share->base.raid_type,
				 share->base.raid_chunks,
				 share->base.raid_chunksize,
				 MYF(0))) < 0)
    {
      mi_check_print_error(param,"Can't create new tempfile: '%s'",
			   param->temp_filename);
      goto err;
    }
    if (filecopy(param, new_file,info->dfile,0L,new_header_length,
		 "datafile-header"))
      goto err;
    if (param->testflag & T_UNPACK)
    {
      share->options&= ~HA_OPTION_COMPRESS_RECORD;
      mi_int2store(share->state.header.options,share->options);
    }
    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
  }

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if (!(param->testflag & T_CREATE_MISSING_KEYS))
  {
    /*
      Flush key cache for this file if we are calling this outside
      myisamchk
    */
    flush_key_blocks(share->key_cache,share->kfile, FLUSH_IGNORE_CHANGED);
    /* Clear the pointers to the given rows */
    for (i=0 ; i < share->base.keys ; i++)
      share->state.key_root[i]= HA_OFFSET_ERROR;
    for (i=0 ; i < share->state.header.max_block_size ; i++)
      share->state.key_del[i]=  HA_OFFSET_ERROR;
    info->state->key_file_length=share->base.keystart;
  }
  else
  {
    if (flush_key_blocks(share->key_cache,share->kfile, FLUSH_FORCE_WRITE))
      goto err;
    key_map= ~key_map;				/* Create the missing keys */
  }

  sort_info.info=info;
  sort_info.param = param;

  set_data_file_type(&sort_info, share);
  sort_info.dupp=0;
  sort_info.buff=0;
  param->read_cache.end_of_file=sort_info.filelength=
    my_seek(param->read_cache.file,0L,MY_SEEK_END,MYF(0));

  if (share->data_file_type == DYNAMIC_RECORD)
    rec_length=max(share->base.min_pack_length+1,share->base.min_block_length);
  else if (share->data_file_type == COMPRESSED_RECORD)
    rec_length=share->base.min_block_length;
  else
    rec_length=share->base.pack_reclength;
  /*
    +1 below is required hack for parallel repair mode.
    The info->state->records value, that is compared later
    to sort_info.max_records and cannot exceed it, is
    increased in sort_key_write. In mi_repair_by_sort, sort_key_write
    is called after sort_key_read, where the comparison is performed,
    but in parallel mode master thread can call sort_key_write
    before some other repair thread calls sort_key_read.
    Furthermore I'm not even sure +1 would be enough.
    May be sort_info.max_records shold be always set to max value in
    parallel mode.
  */
  sort_info.max_records=
    ((param->testflag & T_CREATE_MISSING_KEYS) ? info->state->records + 1:
     (ha_rows) (sort_info.filelength/rec_length+1));

  del=info->state->del;
  param->glob_crc=0;
  if (param->testflag & T_CALC_CHECKSUM)
    param->calc_checksum=1;

  if (!(sort_param=(MI_SORT_PARAM *)
        my_malloc((uint) share->base.keys *
		  (sizeof(MI_SORT_PARAM) + share->base.pack_reclength),
		  MYF(MY_ZEROFILL))))
  {
    mi_check_print_error(param,"Not enough memory for key!");
    goto err;
  }
  total_key_length=0;
  rec_per_key_part= param->rec_per_key_part;
  info->state->records=info->state->del=share->state.split=0;
  info->state->empty=0;

  for (i=key=0, istep=1 ; key < share->base.keys ;
       rec_per_key_part+=sort_param[i].keyinfo->keysegs, i+=istep, key++)
  {
    sort_param[i].key=key;
    sort_param[i].keyinfo=share->keyinfo+key;
    sort_param[i].seg=sort_param[i].keyinfo->seg;
    if (!(((ulonglong) 1 << key) & key_map))
    {
      /* Remember old statistics for key */
      memcpy((char*) rec_per_key_part,
	     (char*) (share->state.rec_per_key_part+
		      (uint) (rec_per_key_part - param->rec_per_key_part)),
	     sort_param[i].keyinfo->keysegs*sizeof(*rec_per_key_part));
      istep=0;
      continue;
    }
    istep=1;
    if ((!(param->testflag & T_SILENT)))
      printf ("- Fixing index %d\n",key+1);
    if (sort_param[i].keyinfo->flag & HA_FULLTEXT)
    {
      sort_param[i].key_read=sort_ft_key_read;
      sort_param[i].key_write=sort_ft_key_write;
    }
    else
    {
      sort_param[i].key_read=sort_key_read;
      sort_param[i].key_write=sort_key_write;
    }
    sort_param[i].key_cmp=sort_key_cmp;
    sort_param[i].lock_in_memory=lock_memory;
    sort_param[i].tmpdir=param->tmpdir;
    sort_param[i].sort_info=&sort_info;
    sort_param[i].master=0;
    sort_param[i].fix_datafile=0;

    sort_param[i].filepos=new_header_length;
    sort_param[i].max_pos=sort_param[i].pos=share->pack.header_length;

    sort_param[i].record= (((char *)(sort_param+share->base.keys))+
			   (share->base.pack_reclength * i));
    if (!mi_alloc_rec_buff(info, -1, &sort_param[i].rec_buff))
    {
      mi_check_print_error(param,"Not enough memory!");
      goto err;
    }

    sort_param[i].key_length=share->rec_reflength;
    for (keyseg=sort_param[i].seg; keyseg->type != HA_KEYTYPE_END;
	 keyseg++)
    {
      sort_param[i].key_length+=keyseg->length;
      if (keyseg->flag & HA_SPACE_PACK)
        sort_param[i].key_length+=get_pack_length(keyseg->length);
      if (keyseg->flag & (HA_BLOB_PART | HA_VAR_LENGTH))
        sort_param[i].key_length+=2 + test(keyseg->length >= 127);
      if (keyseg->flag & HA_NULL_PART)
        sort_param[i].key_length++;
    }
    total_key_length+=sort_param[i].key_length;

    if (sort_param[i].keyinfo->flag & HA_FULLTEXT)
    {
      uint ft_max_word_len_for_sort=FT_MAX_WORD_LEN_FOR_SORT*
                                    sort_param[i].keyinfo->seg->charset->mbmaxlen;
      sort_param[i].key_length+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
    }
  }
  sort_info.total_keys=i;
  sort_param[0].master= 1;
  sort_param[0].fix_datafile= (my_bool)(! rep_quick);

  sort_info.got_error=0;
  pthread_mutex_init(&sort_info.mutex, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&sort_info.cond, 0);
  pthread_mutex_lock(&sort_info.mutex);

  init_io_cache_share(&param->read_cache, &io_share, i);
  (void) pthread_attr_init(&thr_attr);
  (void) pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);

  for (i=0 ; i < sort_info.total_keys ; i++)
  {
    sort_param[i].read_cache=param->read_cache;
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
		       thr_find_all_keys,
		       (void *) (sort_param+i)))
    {
      mi_check_print_error(param,"Cannot start a repair thread");
      remove_io_thread(&param->read_cache);
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

  if ((got_error= thr_write_keys(sort_param)))
  {
    param->retry_repair=1;
    goto err;
  }
  got_error=1;				/* Assume the following may go wrong */

  if (sort_param[0].fix_datafile)
  {
    if (write_data_suffix(&sort_info,1) || end_io_cache(&info->rec_cache))
      goto err;
    if (param->testflag & T_SAFE_REPAIR)
    {
      /* Don't repair if we loosed more than one row */
      if (info->state->records+1 < start_records)
      {
        info->state->records=start_records;
        goto err;
      }
    }
    share->state.state.data_file_length= info->state->data_file_length=
      sort_param->filepos;
    /* Only whole records */
    share->state.version=(ulong) time((time_t*) 0);
    my_close(info->dfile,MYF(0));
    info->dfile=new_file;
    share->data_file_type=sort_info.new_data_file_type;
    share->pack.header_length=(ulong) new_header_length;
  }
  else
    info->state->data_file_length=sort_param->max_pos;

  if (rep_quick && del+sort_info.dupp != info->state->del)
  {
    mi_check_print_error(param,"Couldn't fix table with quick recovery: Found wrong number of deleted records");
    mi_check_print_error(param,"Run recovery again without -q");
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (rep_quick & T_FORCE_UNIQUENESS)
  {
    my_off_t skr=info->state->data_file_length+
      (share->options & HA_OPTION_COMPRESS_RECORD ?
       MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (share->data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength && !info->s->base.raid_type)
      if (my_chsize(info->dfile,skr,0,MYF(0)))
	mi_check_print_warning(param,
			       "Can't change size of datafile,  error: %d",
			       my_errno);
  }
  if (param->testflag & T_CALC_CHECKSUM)
    share->state.checksum=param->glob_crc;

  if (my_chsize(share->kfile,info->state->key_file_length,0,MYF(0)))
    mi_check_print_warning(param,
			   "Can't change size of indexfile, error: %d", my_errno);

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != info->state->records)
      printf("Data records: %s\n", llstr(info->state->records,llbuff));
    if (sort_info.dupp)
      mi_check_print_warning(param,
			     "%s records have been removed",
			     llstr(sort_info.dupp,llbuff));
  }
  got_error=0;

  if (&share->state.state != info->state)
    memcpy(&share->state.state, info->state, sizeof(*info->state));

err:
  got_error|= flush_blocks(param, share->key_cache, share->kfile);
  VOID(end_io_cache(&info->rec_cache));
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      my_close(new_file,MYF(0));
      info->dfile=new_file= -1;
      if (change_to_newfile(share->data_file_name,MI_NAME_DEXT,
			    DATA_TMP_EXT, share->base.raid_chunks,
			    (param->testflag & T_BACKUP_DATA ?
			     MYF(MY_REDEL_MAKE_BACKUP): MYF(0))) ||
	  mi_open_datafile(info,share,-1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      mi_check_print_error(param,"%d when fixing table",my_errno);
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_raid_delete(param->temp_filename,share->base.raid_chunks,
			  MYF(MY_WME)));
      if (info->dfile == new_file)
	info->dfile= -1;
    }
    mi_mark_crashed_on_repair(info);
  }
  else if (key_map == share->state.key_map)
    share->state.changed&= ~STATE_NOT_OPTIMIZED_KEYS;
  share->state.changed|=STATE_NOT_SORTED_PAGES;

  pthread_cond_destroy (&sort_info.cond);
  pthread_mutex_destroy(&sort_info.mutex);

  my_free((gptr) sort_info.ft_buf, MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) sort_info.key_block,MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) sort_param,MYF(MY_ALLOW_ZERO_PTR));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  VOID(end_io_cache(&param->read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  if (!got_error && (param->testflag & T_UNPACK))
  {
    share->state.header.options[0]&= (uchar) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
  }
  DBUG_RETURN(got_error);
#endif /* THREAD */
}

	/* Read next record and return next key */

static int sort_key_read(MI_SORT_PARAM *sort_param, void *key)
{
  int error;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_INFO *info=sort_info->info;
  DBUG_ENTER("sort_key_read");

  if ((error=sort_get_next_record(sort_param)))
    DBUG_RETURN(error);
  if (info->state->records == sort_info->max_records)
  {
    mi_check_print_error(sort_info->param,
			 "Key %d - Found too many records; Can't continue",
                         sort_param->key+1);
    DBUG_RETURN(1);
  }
  sort_param->real_key_length=
    (info->s->rec_reflength+
     _mi_make_key(info, sort_param->key, (uchar*) key,
		  sort_param->record, sort_param->filepos));
#ifdef HAVE_purify
  bzero(key+sort_param->real_key_length,
	(sort_param->key_length-sort_param->real_key_length));
#endif
  DBUG_RETURN(sort_write_record(sort_param));
} /* sort_key_read */

static int sort_ft_key_read(MI_SORT_PARAM *sort_param, void *key)
{
  int error;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_INFO *info=sort_info->info;
  FT_WORD *wptr=0;
  DBUG_ENTER("sort_ft_key_read");

  if (!sort_param->wordlist)
  {
    for (;;)
    {
      my_free((char*) wptr, MYF(MY_ALLOW_ZERO_PTR));
      if ((error=sort_get_next_record(sort_param)))
        DBUG_RETURN(error);
      if (!(wptr=_mi_ft_parserecord(info,sort_param->key,sort_param->record)))
        DBUG_RETURN(1);
      if (wptr->pos)
        break;
      error=sort_write_record(sort_param);
    }
    sort_param->wordptr=sort_param->wordlist=wptr;
  }
  else
  {
    error=0;
    wptr=(FT_WORD*)(sort_param->wordptr);
  }

  sort_param->real_key_length=(info->s->rec_reflength+
			       _ft_make_key(info, sort_param->key,
					    key, wptr++, sort_param->filepos));
#ifdef HAVE_purify
  if (sort_param->key_length > sort_param->real_key_length)
    bzero(key+sort_param->real_key_length,
	  (sort_param->key_length-sort_param->real_key_length));
#endif
  if (!wptr->pos)
  {
    my_free((char*) sort_param->wordlist, MYF(0));
    sort_param->wordlist=0;
    error=sort_write_record(sort_param);
  }
  else
    sort_param->wordptr=(void*)wptr;

  DBUG_RETURN(error);
} /* sort_ft_key_read */


	/* Read next record from file using parameters in sort_info */
	/* Return -1 if end of file, 0 if ok and > 0 if error */

static int sort_get_next_record(MI_SORT_PARAM *sort_param)
{
  int searching;
  uint found_record,b_type,left_length;
  my_off_t pos;
  byte *to;
  MI_BLOCK_INFO block_info;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];
  DBUG_ENTER("sort_get_next_record");

  if (*killed_ptr(param))
    DBUG_RETURN(1);

  switch (share->data_file_type) {
  case STATIC_RECORD:
    for (;;)
    {
      if (my_b_read(&sort_param->read_cache,sort_param->record,
		    share->base.pack_reclength))
      {
	if (sort_param->read_cache.error)
	  param->out_flag |= O_DATA_LOST;
        param->retry_repair=1;
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	DBUG_RETURN(-1);
      }
      sort_param->start_recpos=sort_param->pos;
      if (!sort_param->fix_datafile)
      {
	sort_param->filepos=sort_param->pos;
        if (sort_param->master)
	  share->state.split++;
      }
      sort_param->max_pos=(sort_param->pos+=share->base.pack_reclength);
      if (*sort_param->record)
      {
	if (param->calc_checksum)
	  param->glob_crc+= (info->checksum=
			     mi_static_checksum(info,sort_param->record));
	DBUG_RETURN(0);
      }
      if (!sort_param->fix_datafile && sort_param->master)
      {
	info->state->del++;
	info->state->empty+=share->base.pack_reclength;
      }
    }
  case DYNAMIC_RECORD:
    LINT_INIT(to);
    pos=sort_param->pos;
    searching=(sort_param->fix_datafile && (param->testflag & T_EXTEND));
    for (;;)
    {
      found_record=block_info.second_read= 0;
      left_length=1;
      if (searching)
      {
	pos=MY_ALIGN(pos,MI_DYN_ALIGN_SIZE);
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	sort_param->start_recpos=pos;
      }
      do
      {
	if (pos > sort_param->max_pos)
	  sort_param->max_pos=pos;
	if (pos & (MI_DYN_ALIGN_SIZE-1))
	{
	  if ((param->testflag & T_VERBOSE) || searching == 0)
	    mi_check_print_info(param,"Wrong aligned block at %s",
				llstr(pos,llbuff));
	  if (searching)
	    goto try_next;
	}
	if (found_record && pos == param->search_after_block)
	  mi_check_print_info(param,"Block: %s used by record at %s",
		     llstr(param->search_after_block,llbuff),
		     llstr(sort_param->start_recpos,llbuff2));
	if (_mi_read_cache(&sort_param->read_cache,
                           (byte*) block_info.header,pos,
			   MI_BLOCK_INFO_HEADER_LENGTH,
			   (! found_record ? READING_NEXT : 0) |
			   READING_HEADER))
	{
	  if (found_record)
	  {
	    mi_check_print_info(param,
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
	b_type=_mi_get_block_info(&block_info,-1,pos);
	if ((b_type & (BLOCK_ERROR | BLOCK_FATAL_ERROR)) ||
	   ((b_type & BLOCK_FIRST) &&
	     (block_info.rec_len < (uint) share->base.min_pack_length ||
	      block_info.rec_len > (uint) share->base.max_pack_length)))
	{
	  uint i;
	  if (param->testflag & T_VERBOSE || searching == 0)
	    mi_check_print_info(param,
				"Wrong bytesec: %3d-%3d-%3d at %10s; Skipped",
		       block_info.header[0],block_info.header[1],
		       block_info.header[2],llstr(pos,llbuff));
	  if (found_record)
	    goto try_next;
	  block_info.second_read=0;
	  searching=1;
	  /* Search after block in read header string */
	  for (i=MI_DYN_ALIGN_SIZE ;
	       i < MI_BLOCK_INFO_HEADER_LENGTH ;
	       i+= MI_DYN_ALIGN_SIZE)
	    if (block_info.header[i] >= 1 &&
		block_info.header[i] <= MI_MAX_DYN_HEADER_BYTE)
	      break;
	  pos+=(ulong) i;
	  sort_param->start_recpos=pos;
	  continue;
	}
	if (b_type & BLOCK_DELETED)
	{
	  bool error=0;
	  if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	      share->base.min_block_length)
	  {
	    if (!searching)
	      mi_check_print_info(param,
				  "Deleted block with impossible length %u at %s",
				  block_info.block_len,llstr(pos,llbuff));
	    error=1;
	  }
	  else
	  {
	    if ((block_info.next_filepos != HA_OFFSET_ERROR &&
		 block_info.next_filepos >=
		 info->state->data_file_length) ||
		(block_info.prev_filepos != HA_OFFSET_ERROR &&
		 block_info.prev_filepos >= info->state->data_file_length))
	    {
	      if (!searching)
		mi_check_print_info(param,
				    "Delete link points outside datafile at %s",
				    llstr(pos,llbuff));
	      error=1;
	    }
	  }
	  if (error)
	  {
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MI_DYN_ALIGN_SIZE;
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
	      MI_SPLIT_LENGTH)
	  {
	    if (!searching)
	      mi_check_print_info(param,
				  "Found block with impossible length %u at %s; Skipped",
				  block_info.block_len+ (uint) (block_info.filepos-pos),
				  llstr(pos,llbuff));
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MI_DYN_ALIGN_SIZE;
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
	    info->state->empty+=block_info.block_len;
	    info->state->del++;
	    share->state.split++;
	  }
	  if (found_record)
	    goto try_next;
	  if (searching)
	  {
	    pos+=MI_DYN_ALIGN_SIZE;
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
	    sort_param->filepos=sort_param->start_recpos;
	  if (sort_param->fix_datafile && (param->testflag & T_EXTEND))
	    sort_param->pos=block_info.filepos+1;
	  else
	    sort_param->pos=block_info.filepos+block_info.block_len;
	  if (share->base.blobs)
	  {
	    if (!(to=mi_alloc_rec_buff(info,block_info.rec_len,
				       &(sort_param->rec_buff))))
	    {
	      if (param->max_record_length >= block_info.rec_len)
	      {
		mi_check_print_error(param,"Not enough memory for blob at %s (need %lu)",
				     llstr(sort_param->start_recpos,llbuff),
				     (ulong) block_info.rec_len);
		DBUG_RETURN(1);
	      }
	      else
	      {
		mi_check_print_info(param,"Not enough memory for blob at %s (need %lu); Row skipped",
				    llstr(sort_param->start_recpos,llbuff),
				    (ulong) block_info.rec_len);
		goto try_next;
	      }
	    }
	  }
	  else
	    to= sort_param->rec_buff;
	}
	if (left_length < block_info.data_len || ! block_info.data_len)
	{
	  mi_check_print_info(param,
			      "Found block with too small length at %s; Skipped",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (block_info.filepos + block_info.data_len >
	    sort_param->read_cache.end_of_file)
	{
	  mi_check_print_info(param,
			      "Found block that points outside data file at %s",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (_mi_read_cache(&sort_param->read_cache,to,block_info.filepos,
			   block_info.data_len,
			   (found_record == 1 ? READING_NEXT : 0)))
	{
	  mi_check_print_info(param,
			      "Read error for block at: %s (error: %d); Skipped",
			      llstr(block_info.filepos,llbuff),my_errno);
	  goto try_next;
	}
	left_length-=block_info.data_len;
	to+=block_info.data_len;
	pos=block_info.next_filepos;
	if (pos == HA_OFFSET_ERROR && left_length)
	{
	  mi_check_print_info(param,"Wrong block with wrong total length starting at %s",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (pos + MI_BLOCK_INFO_HEADER_LENGTH > sort_param->read_cache.end_of_file)
	{
	  mi_check_print_info(param,"Found link that points at %s (outside data file) at %s",
			      llstr(pos,llbuff2),
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
      } while (left_length);

      if (_mi_rec_unpack(info,sort_param->record,sort_param->rec_buff,
			 sort_param->find_length) != MY_FILE_ERROR)
      {
	if (sort_param->read_cache.error < 0)
	  DBUG_RETURN(1);
	if (info->s->calc_checksum)
	  info->checksum=mi_checksum(info,sort_param->record);
	if ((param->testflag & (T_EXTEND | T_REP)) || searching)
	{
	  if (_mi_rec_check(info, sort_param->record, sort_param->rec_buff,
                            sort_param->find_length))
	  {
	    mi_check_print_info(param,"Found wrong packed record at %s",
				llstr(sort_param->start_recpos,llbuff));
	    goto try_next;
	  }
	}
	if (param->calc_checksum)
	  param->glob_crc+= info->checksum;
	DBUG_RETURN(0);
      }
      if (!searching)
        mi_check_print_info(param,"Key %d - Found wrong stored record at %s",
                            sort_param->key+1,
                            llstr(sort_param->start_recpos,llbuff));
    try_next:
      pos=(sort_param->start_recpos+=MI_DYN_ALIGN_SIZE);
      searching=1;
    }
  case COMPRESSED_RECORD:
    for (searching=0 ;; searching=1, sort_param->pos++)
    {
      if (_mi_read_cache(&sort_param->read_cache,(byte*) block_info.header,
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
      if (_mi_pack_get_block_info(info,&block_info,-1,sort_param->pos))
	DBUG_RETURN(-1);
      if (!block_info.rec_len &&
	  sort_param->pos + MEMMAP_EXTRA_MARGIN ==
	  sort_param->read_cache.end_of_file)
	DBUG_RETURN(-1);
      if (block_info.rec_len < (uint) share->min_pack_length ||
	  block_info.rec_len > (uint) share->max_pack_length)
      {
	if (! searching)
	  mi_check_print_info(param,"Found block with wrong recordlength: %d at %s\n",
			      block_info.rec_len,
			      llstr(sort_param->pos,llbuff));
	continue;
      }
      if (_mi_read_cache(&sort_param->read_cache,(byte*) sort_param->rec_buff,
			 block_info.filepos, block_info.rec_len,
			 READING_NEXT))
      {
	if (! searching)
	  mi_check_print_info(param,"Couldn't read whole record from %s",
			      llstr(sort_param->pos,llbuff));
	continue;
      }
      if (_mi_pack_rec_unpack(info,sort_param->record,sort_param->rec_buff,
			      block_info.rec_len))
      {
	if (! searching)
	  mi_check_print_info(param,"Found wrong record at %s",
			      llstr(sort_param->pos,llbuff));
	continue;
      }
      info->checksum=mi_checksum(info,sort_param->record);
      if (!sort_param->fix_datafile)
      {
	sort_param->filepos=sort_param->pos;
        if (sort_param->master)
	  share->state.split++;
      }
      sort_param->max_pos=(sort_param->pos=block_info.filepos+
			 block_info.rec_len);
      info->packed_length=block_info.rec_len;
      if (param->calc_checksum)
	param->glob_crc+= info->checksum;
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);		/* Impossible */
}


	/* Write record to new file */

int sort_write_record(MI_SORT_PARAM *sort_param)
{
  int flag;
  uint length;
  ulong block_length,reclength;
  byte *from;
  byte block_buff[8];
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("sort_write_record");

  if (sort_param->fix_datafile)
  {
    switch (sort_info->new_data_file_type) {
    case STATIC_RECORD:
      if (my_b_write(&info->rec_cache,sort_param->record,
		     share->base.pack_reclength))
      {
	mi_check_print_error(param,"%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_param->filepos+=share->base.pack_reclength;
      info->s->state.split++;
      /* sort_info->param->glob_crc+=mi_static_checksum(info, sort_param->record); */
      break;
    case DYNAMIC_RECORD:
      if (! info->blobs)
	from=sort_param->rec_buff;
      else
      {
	/* must be sure that local buffer is big enough */
	reclength=info->s->base.pack_reclength+
	  _my_calc_total_blob_length(info,sort_param->record)+
	  ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	  MI_DYN_DELETE_BLOCK_HEADER;
	if (sort_info->buff_length < reclength)
	{
	  if (!(sort_info->buff=my_realloc(sort_info->buff, (uint) reclength,
					   MYF(MY_FREE_ON_ERROR |
					       MY_ALLOW_ZERO_PTR))))
	    DBUG_RETURN(1);
	  sort_info->buff_length=reclength;
	}
	from=sort_info->buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER);
      }
      info->checksum=mi_checksum(info,sort_param->record);
      reclength=_mi_rec_pack(info,from,sort_param->record);
      flag=0;
      /* sort_info->param->glob_crc+=info->checksum; */

      do
      {
	block_length=reclength+ 3 + test(reclength >= (65520-3));
	if (block_length < share->base.min_block_length)
	  block_length=share->base.min_block_length;
	info->update|=HA_STATE_WRITE_AT_END;
	block_length=MY_ALIGN(block_length,MI_DYN_ALIGN_SIZE);
	if (block_length > MI_MAX_BLOCK_LENGTH)
	  block_length=MI_MAX_BLOCK_LENGTH;
	if (_mi_write_part_record(info,0L,block_length,
				  sort_param->filepos+block_length,
				  &from,&reclength,&flag))
	{
	  mi_check_print_error(param,"%d when writing to datafile",my_errno);
	  DBUG_RETURN(1);
	}
	sort_param->filepos+=block_length;
	info->s->state.split++;
      } while (reclength);
      /* sort_info->param->glob_crc+=info->checksum; */
      break;
    case COMPRESSED_RECORD:
      reclength=info->packed_length;
      length=save_pack_length(block_buff,reclength);
      if (info->s->base.blobs)
	length+=save_pack_length(block_buff+length,info->blob_length);
      if (my_b_write(&info->rec_cache,block_buff,length) ||
	  my_b_write(&info->rec_cache,(byte*) sort_param->rec_buff,reclength))
      {
	mi_check_print_error(param,"%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      /* sort_info->param->glob_crc+=info->checksum; */
      sort_param->filepos+=reclength+length;
      info->s->state.split++;
      break;
    }
  }
  if (sort_param->master)
  {
    info->state->records++;
    if ((param->testflag & T_WRITE_LOOP) &&
        (info->state->records % WRITE_COUNT) == 0)
    {
      char llbuff[22];
      printf("%s\r", llstr(info->state->records,llbuff));
      VOID(fflush(stdout));
    }
  }
  DBUG_RETURN(0);
} /* sort_write_record */


	/* Compare two keys from _create_index_by_sort */

static int sort_key_cmp(MI_SORT_PARAM *sort_param, const void *a,
			const void *b)
{
  uint not_used;
  return (ha_key_cmp(sort_param->seg, *((uchar**) a), *((uchar**) b),
		     USE_WHOLE_KEY, SEARCH_SAME,&not_used));
} /* sort_key_cmp */


static int sort_key_write(MI_SORT_PARAM *sort_param, const void *a)
{
  uint diff_pos;
  char llbuff[22],llbuff2[22];
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param= sort_info->param;
  int cmp;

  if (sort_info->key_block->inited)
  {
    cmp=ha_key_cmp(sort_param->seg,sort_info->key_block->lastkey,
		   (uchar*) a, USE_WHOLE_KEY,SEARCH_FIND | SEARCH_UPDATE,
		   &diff_pos);
    sort_param->unique[diff_pos-1]++;
  }
  else
  {
    cmp= -1;
  }
  if ((sort_param->keyinfo->flag & HA_NOSAME) && cmp == 0)
  {
    sort_info->dupp++;
    sort_info->info->lastpos=get_record_for_key(sort_info->info,
						sort_param->keyinfo,
						(uchar*) a);
    mi_check_print_warning(param,
			   "Duplicate key for record at %10s against record at %10s",
			   llstr(sort_info->info->lastpos,llbuff),
			   llstr(get_record_for_key(sort_info->info,
						    sort_param->keyinfo,
						    sort_info->key_block->
						    lastkey),
				 llbuff2));
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    if (sort_info->param->testflag & T_VERBOSE)
      _mi_print_key(stdout,sort_param->seg,(uchar*) a, USE_WHOLE_KEY);
    return (sort_delete_record(sort_param));
  }
#ifndef DBUG_OFF
  if (cmp > 0)
  {
    mi_check_print_error(param,
			 "Internal error: Keys are not in order from sort");
    return(1);
  }
#endif
  return (sort_insert_key(sort_param,sort_info->key_block,
			  (uchar*) a, HA_OFFSET_ERROR));
} /* sort_key_write */

int sort_ft_buf_flush(MI_SORT_PARAM *sort_param)
{
  SORT_INFO *sort_info=sort_param->sort_info;
  SORT_KEY_BLOCKS *key_block=sort_info->key_block;
  MYISAM_SHARE *share=sort_info->info->s;
  uint val_off, val_len;
  int error;
  SORT_FT_BUF *ft_buf=sort_info->ft_buf;
  uchar *from, *to;

  val_len=share->ft2_keyinfo.keylength;
  get_key_full_length_rdonly(val_off, ft_buf->lastkey);
  to=ft_buf->lastkey+val_off;

  if (ft_buf->buf)
  {
    /* flushing first-level tree */
    error=sort_insert_key(sort_param,key_block,ft_buf->lastkey,
			  HA_OFFSET_ERROR);
    for (from=to+val_len;
         !error && from < ft_buf->buf;
         from+= val_len)
    {
      memcpy(to, from, val_len);
      error=sort_insert_key(sort_param,key_block,ft_buf->lastkey,
			    HA_OFFSET_ERROR);
    }
    return error;
  }
  /* flushing second-level tree keyblocks */
  error=flush_pending_blocks(sort_param);
  /* updating lastkey with second-level tree info */
  ft_intXstore(ft_buf->lastkey+val_off, -ft_buf->count);
  _mi_dpointer(sort_info->info, ft_buf->lastkey+val_off+HA_FT_WLEN,
      share->state.key_root[sort_param->key]);
  /* restoring first level tree data in sort_info/sort_param */
  sort_info->key_block=sort_info->key_block_end- sort_info->param->sort_key_blocks;
  sort_param->keyinfo=share->keyinfo+sort_param->key;
  share->state.key_root[sort_param->key]=HA_OFFSET_ERROR;
  /* writing lastkey in first-level tree */
  return error ? error :
                 sort_insert_key(sort_param,sort_info->key_block,
                                 ft_buf->lastkey,HA_OFFSET_ERROR);
}

static int sort_ft_key_write(MI_SORT_PARAM *sort_param, const void *a)
{
  uint a_len, val_off, val_len, error;
  uchar *p;
  SORT_INFO *sort_info=sort_param->sort_info;
  SORT_FT_BUF *ft_buf=sort_info->ft_buf;
  SORT_KEY_BLOCKS *key_block=sort_info->key_block;

  val_len=HA_FT_WLEN+sort_info->info->s->base.rec_reflength;
  get_key_full_length_rdonly(a_len, (uchar *)a);

  if (!ft_buf)
  {
    /*
      use two-level tree only if key_reflength fits in rec_reflength place
      and row format is NOT static - for _mi_dpointer not to garble offsets
     */
    if ((sort_info->info->s->base.key_reflength <=
         sort_info->info->s->base.rec_reflength) &&
        (sort_info->info->s->options &
          (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
      ft_buf=(SORT_FT_BUF *)my_malloc(sort_param->keyinfo->block_length +
                                      sizeof(SORT_FT_BUF), MYF(MY_WME));

    if (!ft_buf)
    {
      sort_param->key_write=sort_key_write;
      return sort_key_write(sort_param, a);
    }
    sort_info->ft_buf=ft_buf;
    goto word_init_ft_buf; /* no need to duplicate the code */
  }
  get_key_full_length_rdonly(val_off, ft_buf->lastkey);

  if (mi_compare_text(sort_param->seg->charset,
                      ((uchar *)a)+1,a_len-1,
                      ft_buf->lastkey+1,val_off-1, 0, 0)==0)
  {
    if (!ft_buf->buf) /* store in second-level tree */
    {
      ft_buf->count++;
      return sort_insert_key(sort_param,key_block,
                             ((uchar *)a)+a_len, HA_OFFSET_ERROR);
    }

    /* storing the key in the buffer. */
    memcpy (ft_buf->buf, (char *)a+a_len, val_len);
    ft_buf->buf+=val_len;
    if (ft_buf->buf < ft_buf->end)
      return 0;

    /* converting to two-level tree */
    p=ft_buf->lastkey+val_off;

    while (key_block->inited)
      key_block++;
    sort_info->key_block=key_block;
    sort_param->keyinfo=& sort_info->info->s->ft2_keyinfo;
    ft_buf->count=(ft_buf->buf - p)/val_len;

    /* flushing buffer to second-level tree */
    for (error=0; !error && p < ft_buf->buf; p+= val_len)
      error=sort_insert_key(sort_param,key_block,p,HA_OFFSET_ERROR);
    ft_buf->buf=0;
    return error;
  }

  /* flushing buffer */
  if ((error=sort_ft_buf_flush(sort_param)))
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
  ft_buf->end=ft_buf->lastkey+ (sort_param->keyinfo->block_length-32);
  return 0;
} /* sort_ft_key_write */


	/* get pointer to record from a key */

static my_off_t get_record_for_key(MI_INFO *info, MI_KEYDEF *keyinfo,
				   uchar *key)
{
  return _mi_dpos(info,0,key+_mi_keylength(keyinfo,key));
} /* get_record_for_key */


	/* Insert a key in sort-key-blocks */

static int sort_insert_key(MI_SORT_PARAM *sort_param,
			   register SORT_KEY_BLOCKS *key_block, uchar *key,
			   my_off_t prev_block)
{
  uint a_length,t_length,nod_flag;
  my_off_t filepos,key_file_length;
  uchar *anc_buff,*lastkey;
  MI_KEY_PARAM s_temp;
  MI_INFO *info;
  MI_KEYDEF *keyinfo=sort_param->keyinfo;
  SORT_INFO *sort_info= sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  DBUG_ENTER("sort_insert_key");

  anc_buff=key_block->buff;
  info=sort_info->info;
  lastkey=key_block->lastkey;
  nod_flag= (key_block == sort_info->key_block ? 0 :
	     info->s->base.key_reflength);

  if (!key_block->inited)
  {
    key_block->inited=1;
    if (key_block == sort_info->key_block_end)
    {
      mi_check_print_error(param,"To many key-block-levels; Try increasing sort_key_blocks");
      DBUG_RETURN(1);
    }
    a_length=2+nod_flag;
    key_block->end_pos=anc_buff+2;
    lastkey=0;					/* No previous key in block */
  }
  else
    a_length=mi_getint(anc_buff);

	/* Save pointer to previous block */
  if (nod_flag)
    _mi_kpointer(info,key_block->end_pos,prev_block);

  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,
				(uchar*) 0,lastkey,lastkey,key,
				 &s_temp);
  (*keyinfo->store_key)(keyinfo, key_block->end_pos+nod_flag,&s_temp);
  a_length+=t_length;
  mi_putint(anc_buff,a_length,nod_flag);
  key_block->end_pos+=t_length;
  if (a_length <= keyinfo->block_length)
  {
    VOID(_mi_move_key(keyinfo,key_block->lastkey,key));
    key_block->last_length=a_length-t_length;
    DBUG_RETURN(0);
  }

	/* Fill block with end-zero and write filled block */
  mi_putint(anc_buff,key_block->last_length,nod_flag);
  bzero((byte*) anc_buff+key_block->last_length,
	keyinfo->block_length- key_block->last_length);
  key_file_length=info->state->key_file_length;
  if ((filepos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
    DBUG_RETURN(1);

  /* If we read the page from the key cache, we have to write it back to it */
  if (key_file_length == info->state->key_file_length)
  {
    if (_mi_write_keypage(info, keyinfo, filepos, DFLT_INIT_HITS, anc_buff))
      DBUG_RETURN(1);
  }
  else if (my_pwrite(info->s->kfile,(byte*) anc_buff,
		     (uint) keyinfo->block_length,filepos, param->myf_rw))
    DBUG_RETURN(1);
  DBUG_DUMP("buff",(byte*) anc_buff,mi_getint(anc_buff));

	/* Write separator-key to block in next level */
  if (sort_insert_key(sort_param,key_block+1,key_block->lastkey,filepos))
    DBUG_RETURN(1);

	/* clear old block and write new key in it */
  key_block->inited=0;
  DBUG_RETURN(sort_insert_key(sort_param, key_block,key,prev_block));
} /* sort_insert_key */


	/* Delete record when we found a duplicated key */

static int sort_delete_record(MI_SORT_PARAM *sort_param)
{
  uint i;
  int old_file,error;
  uchar *key;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;
  DBUG_ENTER("sort_delete_record");

  if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
  {
    mi_check_print_error(param,
			 "Quick-recover aborted; Run recovery without switch -q or with switch -qq");
    DBUG_RETURN(1);
  }
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
  {
    mi_check_print_error(param,
			 "Recover aborted; Can't run standard recovery on compressed tables with errors in data-file. Use switch 'myisamchk --safe-recover' to fix it\n",stderr);;
    DBUG_RETURN(1);
  }

  old_file=info->dfile;
  info->dfile=info->rec_cache.file;
  if (sort_info->current_key)
  {
    key=info->lastkey+info->s->base.max_key_length;
    if ((error=(*info->s->read_rnd)(info,sort_param->record,info->lastpos,0)) &&
	error != HA_ERR_RECORD_DELETED)
    {
      mi_check_print_error(param,"Can't read record to be removed");
      info->dfile=old_file;
      DBUG_RETURN(1);
    }

    for (i=0 ; i < sort_info->current_key ; i++)
    {
      uint key_length=_mi_make_key(info,i,key,sort_param->record,info->lastpos);
      if (_mi_ck_delete(info,i,key,key_length))
      {
	mi_check_print_error(param,"Can't delete key %d from record to be removed",i+1);
	info->dfile=old_file;
	DBUG_RETURN(1);
      }
    }
    if (param->calc_checksum)
      param->glob_crc-=(*info->s->calc_checksum)(info, sort_param->record);
  }
  error=flush_io_cache(&info->rec_cache) || (*info->s->delete_record)(info);
  info->dfile=old_file;				/* restore actual value */
  info->state->records--;
  DBUG_RETURN(error);
} /* sort_delete_record */

	/* Fix all pending blocks and flush everything to disk */

int flush_pending_blocks(MI_SORT_PARAM *sort_param)
{
  uint nod_flag,length;
  my_off_t filepos,key_file_length;
  SORT_KEY_BLOCKS *key_block;
  SORT_INFO *sort_info= sort_param->sort_info;
  myf myf_rw=sort_info->param->myf_rw;
  MI_INFO *info=sort_info->info;
  MI_KEYDEF *keyinfo=sort_param->keyinfo;
  DBUG_ENTER("flush_pending_blocks");

  filepos= HA_OFFSET_ERROR;			/* if empty file */
  nod_flag=0;
  for (key_block=sort_info->key_block ; key_block->inited ; key_block++)
  {
    key_block->inited=0;
    length=mi_getint(key_block->buff);
    if (nod_flag)
      _mi_kpointer(info,key_block->end_pos,filepos);
    key_file_length=info->state->key_file_length;
    bzero((byte*) key_block->buff+length, keyinfo->block_length-length);
    if ((filepos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
      DBUG_RETURN(1);

    /* If we read the page from the key cache, we have to write it back */
    if (key_file_length == info->state->key_file_length)
    {
      if (_mi_write_keypage(info, keyinfo, filepos,
                            DFLT_INIT_HITS, key_block->buff))
	DBUG_RETURN(1);
    }
    else if (my_pwrite(info->s->kfile,(byte*) key_block->buff,
		       (uint) keyinfo->block_length,filepos, myf_rw))
      DBUG_RETURN(1);
    DBUG_DUMP("buff",(byte*) key_block->buff,length);
    nod_flag=1;
  }
  info->s->state.key_root[sort_param->key]=filepos; /* Last is root for tree */
  DBUG_RETURN(0);
} /* flush_pending_blocks */

	/* alloc space and pointers for key_blocks */

static SORT_KEY_BLOCKS *alloc_key_blocks(MI_CHECK *param, uint blocks,
                                         uint buffer_length)
{
  reg1 uint i;
  SORT_KEY_BLOCKS *block;
  DBUG_ENTER("alloc_key_blocks");

  if (!(block=(SORT_KEY_BLOCKS*) my_malloc((sizeof(SORT_KEY_BLOCKS)+
					    buffer_length+IO_SIZE)*blocks,
					   MYF(0))))
  {
    mi_check_print_error(param,"Not enough memory for sort-key-blocks");
    return(0);
  }
  for (i=0 ; i < blocks ; i++)
  {
    block[i].inited=0;
    block[i].buff=(uchar*) (block+blocks)+(buffer_length+IO_SIZE)*i;
  }
  DBUG_RETURN(block);
} /* alloc_key_blocks */


	/* Check if file is almost full */

int test_if_almost_full(MI_INFO *info)
{
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
    return 0;
  return (my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0))/10*9 >
	  (my_off_t) (info->s->base.max_key_file_length) ||
	  my_seek(info->dfile,0L,MY_SEEK_END,MYF(0))/10*9 >
	  (my_off_t) info->s->base.max_data_file_length);
}

	/* Recreate table with bigger more alloced record-data */

int recreate_table(MI_CHECK *param, MI_INFO **org_info, char *filename)
{
  int error;
  MI_INFO info;
  MYISAM_SHARE share;
  MI_KEYDEF *keyinfo,*key,*key_end;
  HA_KEYSEG *keysegs,*keyseg;
  MI_COLUMNDEF *recdef,*rec,*end;
  MI_UNIQUEDEF *uniquedef,*u_ptr,*u_end;
  MI_STATUS_INFO status_info;
  uint unpack,key_parts;
  ha_rows max_records;
  ulonglong file_length,tmp_length;
  MI_CREATE_INFO create_info;

  error=1;					/* Default error */
  info= **org_info;
  status_info= (*org_info)->state[0];
  info.state= &status_info;
  share= *(*org_info)->s;
  unpack= (share.options & HA_OPTION_COMPRESS_RECORD) &&
    (param->testflag & T_UNPACK);
  if (!(keyinfo=(MI_KEYDEF*) my_alloca(sizeof(MI_KEYDEF)*share.base.keys)))
    return 0;
  memcpy((byte*) keyinfo,(byte*) share.keyinfo,
	 (size_t) (sizeof(MI_KEYDEF)*share.base.keys));

  key_parts= share.base.all_key_parts;
  if (!(keysegs=(HA_KEYSEG*) my_alloca(sizeof(HA_KEYSEG)*
				       (key_parts+share.base.keys))))
  {
    my_afree((gptr) keyinfo);
    return 1;
  }
  if (!(recdef=(MI_COLUMNDEF*)
	my_alloca(sizeof(MI_COLUMNDEF)*(share.base.fields+1))))
  {
    my_afree((gptr) keyinfo);
    my_afree((gptr) keysegs);
    return 1;
  }
  if (!(uniquedef=(MI_UNIQUEDEF*)
	my_alloca(sizeof(MI_UNIQUEDEF)*(share.state.header.uniques+1))))
  {
    my_afree((gptr) recdef);
    my_afree((gptr) keyinfo);
    my_afree((gptr) keysegs);
    return 1;
  }

  /* Copy the column definitions */
  memcpy((byte*) recdef,(byte*) share.rec,
	 (size_t) (sizeof(MI_COLUMNDEF)*(share.base.fields+1)));
  for (rec=recdef,end=recdef+share.base.fields; rec != end ; rec++)
  {
    if (unpack && !(share.options & HA_OPTION_PACK_RECORD) &&
	rec->type != FIELD_BLOB &&
	rec->type != FIELD_VARCHAR &&
	rec->type != FIELD_CHECK)
      rec->type=(int) FIELD_NORMAL;
  }

  /* Change the new key to point at the saved key segments */
  memcpy((byte*) keysegs,(byte*) share.keyparts,
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

  /* Copy the unique definitions and change them to point at the new key
     segments*/
  memcpy((byte*) uniquedef,(byte*) share.uniqueinfo,
	 (size_t) (sizeof(MI_UNIQUEDEF)*(share.state.header.uniques)));
  for (u_ptr=uniquedef,u_end=uniquedef+share.state.header.uniques;
       u_ptr != u_end ; u_ptr++)
  {
    u_ptr->seg=keyseg;
    keyseg+=u_ptr->keysegs+1;
  }
  if (share.options & HA_OPTION_COMPRESS_RECORD)
    share.base.records=max_records=info.state->records;
  else if (share.base.min_pack_length)
    max_records=(ha_rows) (my_seek(info.dfile,0L,MY_SEEK_END,MYF(0)) /
			   (ulong) share.base.min_pack_length);
  else
    max_records=0;
  unpack= (share.options & HA_OPTION_COMPRESS_RECORD) &&
    (param->testflag & T_UNPACK);
  share.options&= ~HA_OPTION_TEMP_COMPRESS_RECORD;

  file_length=(ulonglong) my_seek(info.dfile,0L,MY_SEEK_END,MYF(0));
  tmp_length= file_length+file_length/10;
  set_if_bigger(file_length,param->max_data_file_length);
  set_if_bigger(file_length,tmp_length);
  set_if_bigger(file_length,(ulonglong) share.base.max_data_file_length);

  VOID(mi_close(*org_info));
  bzero((char*) &create_info,sizeof(create_info));
  create_info.max_rows=max(max_records,share.base.records);
  create_info.reloc_rows=share.base.reloc;
  create_info.old_options=(share.options |
			   (unpack ? HA_OPTION_TEMP_COMPRESS_RECORD : 0));

  create_info.data_file_length=file_length;
  create_info.auto_increment=share.state.auto_increment;
  create_info.raid_type=   share.base.raid_type;
  create_info.raid_chunks= share.base.raid_chunks;
  create_info.raid_chunksize= share.base.raid_chunksize;
  create_info.language = (param->language ? param->language :
			  share.state.header.language);
  create_info.key_file_length=  status_info.key_file_length;
  /* We don't have to handle symlinks here because we are using
     HA_DONT_TOUCH_DATA */
  if (mi_create(filename,
		share.base.keys - share.state.header.uniques,
		keyinfo, share.base.fields, recdef,
		share.state.header.uniques, uniquedef,
		&create_info,
		HA_DONT_TOUCH_DATA))
  {
    mi_check_print_error(param,"Got error %d when trying to recreate indexfile",my_errno);
    goto end;
  }
  *org_info=mi_open(filename,O_RDWR,
		    (param->testflag & T_WAIT_FOREVER) ? HA_OPEN_WAIT_IF_LOCKED :
		    (param->testflag & T_DESCRIPT) ? HA_OPEN_IGNORE_IF_LOCKED :
		    HA_OPEN_ABORT_IF_LOCKED);
  if (!*org_info)
  {
    mi_check_print_error(param,"Got error %d when trying to open re-created indexfile",
		my_errno);
    goto end;
  }
  /* We are modifing */
  (*org_info)->s->options&= ~HA_OPTION_READ_ONLY_DATA;
  VOID(_mi_readinfo(*org_info,F_WRLCK,0));
  (*org_info)->state->records=info.state->records;
  if (share.state.create_time)
    (*org_info)->s->state.create_time=share.state.create_time;
  (*org_info)->s->state.unique=(*org_info)->this_unique=
    share.state.unique;
  (*org_info)->s->state.checksum=share.state.checksum;
  (*org_info)->state->del=info.state->del;
  (*org_info)->s->state.dellink=share.state.dellink;
  (*org_info)->state->empty=info.state->empty;
  (*org_info)->state->data_file_length=info.state->data_file_length;
  if (update_state_info(param,*org_info,UPDATE_TIME | UPDATE_STAT |
			UPDATE_OPEN_COUNT))
    goto end;
  error=0;
end:
  my_afree((gptr) uniquedef);
  my_afree((gptr) keyinfo);
  my_afree((gptr) recdef);
  my_afree((gptr) keysegs);
  return error;
}


	/* write suffix to data file if neaded */

int write_data_suffix(SORT_INFO *sort_info, my_bool fix_datafile)
{
  MI_INFO *info=sort_info->info;

  if (info->s->options & HA_OPTION_COMPRESS_RECORD && fix_datafile)
  {
    char buff[MEMMAP_EXTRA_MARGIN];
    bzero(buff,sizeof(buff));
    if (my_b_write(&info->rec_cache,buff,sizeof(buff)))
    {
      mi_check_print_error(sort_info->param,
			   "%d when writing to datafile",my_errno);
      return 1;
    }
    sort_info->param->read_cache.end_of_file+=sizeof(buff);
  }
  return 0;
}

	/* Update state and myisamchk_time of indexfile */

int update_state_info(MI_CHECK *param, MI_INFO *info,uint update)
{
  MYISAM_SHARE *share=info->s;

  if (update & UPDATE_OPEN_COUNT)
  {
    share->state.open_count=0;
    share->global_changed=0;
  }
  if (update & UPDATE_STAT)
  {
    uint i, key_parts= mi_uint2korr(share->state.header.key_parts);
    share->state.rec_per_key_rows=info->state->records;
    share->state.changed&= ~STATE_NOT_ANALYZED;
    if (info->state->records)
    {
      for (i=0; i<key_parts; i++)
      {
        if (!(share->state.rec_per_key_part[i]=param->rec_per_key_part[i]))
          share->state.changed|= STATE_NOT_ANALYZED;
      }
    }
  }
  if (update & (UPDATE_STAT | UPDATE_SORT | UPDATE_TIME | UPDATE_AUTO_INC))
  {
    if (update & UPDATE_TIME)
    {
      share->state.check_time= (long) time((time_t*) 0);
      if (!share->state.create_time)
	share->state.create_time=share->state.check_time;
    }
    if (mi_state_info_write(share->kfile,&share->state,1+2))
      goto err;
    share->changed=0;
  }
  {						/* Force update of status */
    int error;
    uint r_locks=share->r_locks,w_locks=share->w_locks;
    share->r_locks= share->w_locks= share->tot_locks= 0;
    error=_mi_writeinfo(info,WRITEINFO_NO_UNLOCK);
    share->r_locks=r_locks;
    share->w_locks=w_locks;
    share->tot_locks=r_locks+w_locks;
    if (!error)
      return 0;
  }
err:
  mi_check_print_error(param,"%d when updating keyfile",my_errno);
  return 1;
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

void update_auto_increment_key(MI_CHECK *param, MI_INFO *info,
			       my_bool repair_only)
{
  byte *record;
  if (!info->s->base.auto_key ||
      !(((ulonglong) 1 << (info->s->base.auto_key-1)
	 & info->s->state.key_map)))
  {
    if (!(param->testflag & T_VERY_SILENT))
      mi_check_print_info(param,
			  "Table: %s doesn't have an auto increment key\n",
			  param->isam_file_name);
    return;
  }
  if (!(param->testflag & T_SILENT) &&
      !(param->testflag & T_REP))
    printf("Updating MyISAM file: %s\n", param->isam_file_name);
  /*
    We have to use an allocated buffer instead of info->rec_buff as 
    _mi_put_key_in_record() may use info->rec_buff
  */
  if (!(record= (byte*) my_malloc((uint) info->s->base.pack_reclength,
				  MYF(0))))
  {
    mi_check_print_error(param,"Not enough memory for extra record");
    return;
  }

  mi_extra(info,HA_EXTRA_KEYREAD,0);
  if (mi_rlast(info, record, info->s->base.auto_key-1))
  {
    if (my_errno != HA_ERR_END_OF_FILE)
    {
      mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
      my_free((char*) record, MYF(0));
      mi_check_print_error(param,"%d when reading last record",my_errno);
      return;
    }
    if (!repair_only)
      info->s->state.auto_increment=param->auto_increment_value;
  }
  else
  {
    ulonglong auto_increment= (repair_only ? info->s->state.auto_increment :
			       param->auto_increment_value);
    info->s->state.auto_increment=0;
    update_auto_increment(info, record);
    set_if_bigger(info->s->state.auto_increment,auto_increment);
  }
  mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
  my_free((char*) record, MYF(0));
  update_state_info(param, info, UPDATE_AUTO_INC);
  return;
}

    /* calculate unique keys for each part key */

void update_key_parts(MI_KEYDEF *keyinfo, ulong *rec_per_key_part,
			     ulonglong *unique, ulonglong records)
{
  ulonglong count=0,tmp;
  uint parts;
  for (parts=0 ; parts < keyinfo->keysegs  ; parts++)
  {
    count+=unique[parts];
    if (count == 0)
      tmp=records;
    else
      tmp= (records + (count+1)/2) / (count+1);
    /* for some weird keys (e.g. FULLTEXT) tmp can be <1 here.
       let's ensure it is not */
    set_if_bigger(tmp,1);
    if (tmp >= (ulonglong) ~(ulong) 0)
      tmp=(ulonglong) ~(ulong) 0;
    *rec_per_key_part=(ulong) tmp;
    rec_per_key_part++;
  }
}


static ha_checksum mi_byte_checksum(const byte *buf, uint length)
{
  ha_checksum crc;
  const byte *end=buf+length;
  for (crc=0; buf != end; buf++)
    crc=((crc << 1) + *((uchar*) buf)) +
      test(crc & (((ha_checksum) 1) << (8*sizeof(ha_checksum)-1)));
  return crc;
}

static my_bool mi_too_big_key_for_sort(MI_KEYDEF *key, ha_rows rows)
{
  uint key_maxlength=key->maxlength;
  if (key->flag & HA_FULLTEXT)
  {
    uint ft_max_word_len_for_sort=FT_MAX_WORD_LEN_FOR_SORT*
                                  key->seg->charset->mbmaxlen;
    key_maxlength+=ft_max_word_len_for_sort-HA_FT_MAXBYTELEN;
  }
  return (key->flag & (HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY | HA_FULLTEXT) &&
	  ((ulonglong) rows * key_maxlength >
	   (ulonglong) myisam_max_temp_length));
}

/*
  Deactivate all not unique index that can be recreated fast
  These include packed keys on which sorting will use more temporary
  space than the max allowed file length or for which the unpacked keys
  will take much more space than packed keys.
  Note that 'rows' may be zero for the case when we don't know how many
  rows we will put into the file.
 */

void mi_disable_non_unique_index(MI_INFO *info, ha_rows rows)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF    *key=share->keyinfo;
  uint          i;

  DBUG_ASSERT(info->state->records == 0 &&
              (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES));
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!(key->flag & (HA_NOSAME | HA_SPATIAL | HA_AUTO_KEY)) &&
        ! mi_too_big_key_for_sort(key,rows) && info->s->base.auto_key != i+1)
    {
      share->state.key_map&= ~ ((ulonglong) 1 << i);
      info->update|= HA_STATE_CHANGED;
    }
  }
}


/*
  Return TRUE if we can use repair by sorting
  One can set the force argument to force to use sorting
  even if the temporary file would be quite big!
*/

my_bool mi_test_if_sort_rep(MI_INFO *info, ha_rows rows,
			    ulonglong key_map, my_bool force)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *key=share->keyinfo;
  uint i;

  /*
    mi_repair_by_sort only works if we have at least one key. If we don't
    have any keys, we should use the normal repair.
  */
  if (!key_map)
    return FALSE;				/* Can't use sort */
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!force && mi_too_big_key_for_sort(key,rows))
      return FALSE;
  }
  return TRUE;
}


static void
set_data_file_type(SORT_INFO *sort_info, MYISAM_SHARE *share)
{
  if ((sort_info->new_data_file_type=share->data_file_type) ==
      COMPRESSED_RECORD && sort_info->param->testflag & T_UNPACK)
  {
    MYISAM_SHARE tmp;

    if (share->options & HA_OPTION_PACK_RECORD)
      sort_info->new_data_file_type = DYNAMIC_RECORD;
    else
      sort_info->new_data_file_type = STATIC_RECORD;

    /* Set delete_function for sort_delete_record() */
    memcpy((char*) &tmp, share, sizeof(*share));
    tmp.options= ~HA_OPTION_COMPRESS_RECORD;
    mi_setup_functions(&tmp);
    share->delete_record=tmp.delete_record;
  }
}
