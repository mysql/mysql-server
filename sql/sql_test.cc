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


/* Write some debug info */


#include "mysql_priv.h"
#include "sql_select.h"
#include <hash.h>
#include <thr_alarm.h>
#if defined(HAVE_MALLINFO) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#elif defined(HAVE_MALLINFO) && defined(HAVE_SYS_MALLOC_H)
#include <sys/malloc.h>
#endif

/* Intern key cache variables */
extern "C" pthread_mutex_t THR_LOCK_keycache;

#ifndef DBUG_OFF

void
print_where(COND *cond,const char *info)
{
  if (cond)
  {
    char buff[256];
    String str(buff,(uint32) sizeof(buff));
    str.length(0);
    cond->print(&str);
    str.append('\0');
    DBUG_LOCK_FILE;
    (void) fprintf(DBUG_FILE,"\nWHERE:(%s) ",info);
    (void) fputs(str.ptr(),DBUG_FILE);
    (void) fputc('\n',DBUG_FILE);
    DBUG_UNLOCK_FILE;
  }
}

	/* This is for debugging purposes */

extern HASH open_cache;
extern TABLE *unused_tables;

void print_cached_tables(void)
{
  uint idx,count,unused;
  TABLE *start_link,*lnk;

  VOID(pthread_mutex_lock(&LOCK_open));
  puts("DB             Table                            Version  Thread  L.thread  Open");

  for (idx=unused=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    printf("%-14.14s %-32s%6ld%8ld%10ld%6d\n",
	   entry->table_cache_key,entry->real_name,entry->version,
	   entry->in_use ? entry->in_use->thread_id : 0L,
	   entry->in_use ? entry->in_use->dbug_thread_id : 0L,
	   entry->db_stat ? 1 : 0);
    if (!entry->in_use)
      unused++;
  }
  count=0;
  if ((start_link=lnk=unused_tables))
  {
    do
    {
      if (lnk != lnk->next->prev || lnk != lnk->prev->next)
      {
	printf("unused_links isn't linked properly\n");
	return;
      }
    } while (count++ < open_cache.records && (lnk=lnk->next) != start_link);
    if (lnk != start_link)
    {
      printf("Unused_links aren't connected\n");
    }
  }
  if (count != unused)
    printf("Unused_links (%d) doesn't match open_cache: %d\n", count,unused);
  printf("\nCurrent refresh version: %ld\n",refresh_version);
  if (hash_check(&open_cache))
    printf("Error: File hash table is corrupted\n");
  fflush(stdout);
  VOID(pthread_mutex_unlock(&LOCK_open));
  return;
}


void TEST_filesort(SORT_FIELD *sortorder,uint s_length, ha_rows special)
{
  char buff[256],buff2[256];
  String str(buff,sizeof(buff)),out(buff2,sizeof(buff2));
  const char *sep;
  DBUG_ENTER("TEST_filesort");

  out.length(0);
  for (sep=""; s_length-- ; sortorder++, sep=" ")
  {
    out.append(sep);
    if (sortorder->reverse)
      out.append('-');
    if (sortorder->field)
    {
      if (sortorder->field->table_name)
      {
	out.append(sortorder->field->table_name);
	out.append('.');
      }
      out.append(sortorder->field->field_name ? sortorder->field->field_name:
		 "tmp_table_column");
    }
    else
    {
      str.length(0);
      sortorder->item->print(&str);
      out.append(str);
    }
  }
  out.append('\0');				// Purify doesn't like c_ptr()
  DBUG_LOCK_FILE;
  VOID(fputs("\nInfo about FILESORT\n",DBUG_FILE));
  if (special)
    fprintf(DBUG_FILE,"Records to sort: %lu\n",(ulong) special);
  fprintf(DBUG_FILE,"Sortorder: %s\n",out.ptr());
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}


void
TEST_join(JOIN *join)
{
  uint i,ref;
  DBUG_ENTER("TEST_join");

  DBUG_LOCK_FILE;
  VOID(fputs("\nInfo about JOIN\n",DBUG_FILE));
  for (i=0 ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *form=tab->table;
    fprintf(DBUG_FILE,"%-16.16s  type: %-7s  q_keys: %4d  refs: %d  key: %d  len: %d\n",
	    form->table_name,
	    join_type_str[tab->type],
	    tab->keys,
	    tab->ref.key_parts,
	    tab->ref.key,
	    tab->ref.key_length);
    if (tab->select)
    {
      if (tab->use_quick == 2)
	fprintf(DBUG_FILE,
		"                  quick select checked for each record (keys: %d)\n",
		(int) tab->select->quick_keys);
      else if (tab->select->quick)
	fprintf(DBUG_FILE,"                  quick select used on key %s, length: %d\n",
		form->key_info[tab->select->quick->index].name,
		tab->select->quick->max_used_key_length);
      else
	VOID(fputs("                  select used\n",DBUG_FILE));
    }
    if (tab->ref.key_parts)
    {
      VOID(fputs("                  refs: ",DBUG_FILE));
      for (ref=0 ; ref < tab->ref.key_parts ; ref++)
      {
	Item *item=tab->ref.items[ref];
	fprintf(DBUG_FILE,"%s  ", item->full_name());
      }
      VOID(fputc('\n',DBUG_FILE));
    }
  }
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

#endif

void mysql_print_status(THD *thd)
{
  char current_dir[FN_REFLEN];
  char llbuff[22];

  printf("\nStatus information:\n\n");
  my_getwd(current_dir, sizeof(current_dir),MYF(0));
  printf("Current dir: %s\n", current_dir);
  printf("Running threads: %d  Stack size: %ld\n", thread_count,
	 (long) thread_stack);
  if (thd)
    thd->proc_info="locks";
  thr_print_locks();				// Write some debug info
#ifndef DBUG_OFF
  if (thd)
    thd->proc_info="table cache";
  print_cached_tables();
#endif
  /* Print key cache status */
  if (thd)
    thd->proc_info="key cache";
  pthread_mutex_lock(&THR_LOCK_keycache);
  printf("key_cache status:\n\
blocks used:%10lu\n\
not flushed:%10lu\n",
         _my_blocks_used, _my_blocks_changed);
  printf("w_requests: %10s\n", llstr(_my_cache_w_requests, llbuff));
  printf("writes:     %10s\n", llstr(_my_cache_write,      llbuff));
  printf("r_requests: %10s\n", llstr(_my_cache_r_requests, llbuff));
  printf("reads:      %10s\n", llstr(_my_cache_read,       llbuff));
  pthread_mutex_unlock(&THR_LOCK_keycache);

  if (thd)
    thd->proc_info="status";
  pthread_mutex_lock(&LOCK_status);
  printf("\nhandler status:\n\
read_key:   %10lu\n\
read_next:  %10lu\n\
read_rnd    %10lu\n\
read_first: %10lu\n\
write:      %10lu\n\
delete      %10lu\n\
update:     %10lu\n",
	 ha_read_key_count, ha_read_next_count,
	 ha_read_rnd_count, ha_read_first_count,
	 ha_write_count, ha_delete_count, ha_update_count);
  pthread_mutex_unlock(&LOCK_status);
  printf("\nTable status:\n\
Opened tables: %10lu\n\
Open tables:   %10lu\n\
Open files:    %10lu\n\
Open streams:  %10lu\n",
	 opened_tables,
	 (ulong) cached_tables(),
	 (ulong) my_file_opened,
	 (ulong) my_stream_opened);

  ALARM_INFO alarm_info;
#ifndef DONT_USE_THR_ALARM
  thr_alarm_info(&alarm_info);
  printf("\nAlarm status:\n\
Active alarms:   %u\n\
Max used alarms: %u\n\
Next alarm time: %lu\n",
	 alarm_info.active_alarms,
	 alarm_info.max_used_alarms,
	 alarm_info.next_alarm_time);
#endif
  fflush(stdout);
  if (thd)
    thd->proc_info="malloc";
  my_checkmalloc();
  TERMINATE(stdout);				// Write malloc information

#ifdef HAVE_MALLINFO
  struct mallinfo info= mallinfo();
  printf("\nMemory status:\n\
Non-mmapped space allocated from system: %d\n\
Number of free chunks:			 %d\n\
Number of fastbin blocks:		 %d\n\
Number of mmapped regions:		 %d\n\
Space in mmapped regions:		 %d\n\
Maximum total allocated space:		 %d\n\
Space available in freed fastbin blocks: %d\n\
Total allocated space:			 %d\n\
Total free space:			 %d\n\
Top-most, releasable space:		 %d\n\
Estimated memory (with thread stack):    %ld\n",
	 (int) info.arena	,
	 (int) info.ordblks,
	 (int) info.smblks,
	 (int) info.hblks,
	 (int) info.hblkhd,
	 (int) info.usmblks,
	 (int) info.fsmblks,
	 (int) info.uordblks,
	 (int) info.fordblks,
	 (int) info.keepcost,
	 (long) (thread_count * thread_stack + info.hblkhd + info.arena));
#endif
  puts("");
  if (thd)
    thd->proc_info=0;
}
