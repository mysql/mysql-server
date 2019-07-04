/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
  Creates a index for a database by reading keys, sorting them and outputing
  them in sorted order through SORT_INFO functions.
*/

#include <sys/types.h>
#include <algorithm>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "storage/myisam/fulltext.h"
#include "storage/myisam/myisamdef.h"
#if defined(_WIN32)
#include <fcntl.h>
#else
#include <stddef.h>
#endif
#include "storage/myisam/queues.h"
#include "template_utils.h"

/* static variables */

#undef MYF_RW
#undef DISK_BUFFER_SIZE

#define MERGEBUFF 15
#define MERGEBUFF2 31
#define MYF_RW MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL)
#define DISK_BUFFER_SIZE (IO_SIZE * 16)

/*
 Pointers of functions for store and read keys from temp file
*/

extern void print_error(const char *fmt, ...);

/* Functions defined in this file */

static ha_rows find_all_keys(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                             DYNAMIC_ARRAY *buffpek, int *maxbuffer,
                             IO_CACHE *tempfile,
                             IO_CACHE *tempfile_for_exceptions);
static int write_keys(MI_SORT_PARAM *info, uchar **sort_keys, uint count,
                      BUFFPEK *buffpek, IO_CACHE *tempfile);
static int write_key(MI_SORT_PARAM *info, uchar *key, IO_CACHE *tempfile);
static int write_index(MI_SORT_PARAM *info, uchar **sort_keys, uint count);
static int merge_many_buff(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                           BUFFPEK *buffpek, int *maxbuffer, IO_CACHE *t_file);
static uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
                           uint sort_length);
static int merge_buffers(MI_SORT_PARAM *info, uint keys, IO_CACHE *from_file,
                         IO_CACHE *to_file, uchar **sort_keys,
                         BUFFPEK *lastbuff, BUFFPEK *Fb, BUFFPEK *Tb);
static int merge_index(MI_SORT_PARAM *, uint, uchar **, BUFFPEK *, int,
                       IO_CACHE *);
static int flush_ft_buf(MI_SORT_PARAM *info);

static int write_keys_varlen(MI_SORT_PARAM *info, uchar **sort_keys, uint count,
                             BUFFPEK *buffpek, IO_CACHE *tempfile);
static uint read_to_buffer_varlen(IO_CACHE *fromfile, BUFFPEK *buffpek,
                                  uint sort_length);
static int write_merge_key(MI_SORT_PARAM *info, IO_CACHE *to_file, uchar *key,
                           uint sort_length, uint count);
static int write_merge_key_varlen(MI_SORT_PARAM *info, IO_CACHE *to_file,
                                  uchar *key, uint sort_length, uint count);
static inline int my_var_write(MI_SORT_PARAM *info, IO_CACHE *to_file,
                               uchar *bufs);

/*
  Creates a index of sorted keys

  SYNOPSIS
    _create_index_by_sort()
    info		Sort parameters
    no_messages		Set to 1 if no output
    sortbuff_size	Size if sortbuffer to allocate

  RESULT
    0	ok
   <> 0 Error
*/

int _create_index_by_sort(MI_SORT_PARAM *info, bool no_messages,
                          ulonglong sortbuff_size) {
  int error, maxbuffer, skr;
  uint sort_length, keys = 0;
  ulonglong memavl, old_memavl;
  DYNAMIC_ARRAY buffpek;
  ha_rows records;
  uchar **sort_keys;
  IO_CACHE tempfile, tempfile_for_exceptions;
  DBUG_ENTER("_create_index_by_sort");
  DBUG_PRINT("enter", ("sort_length: %d", info->key_length));

  if (info->keyinfo->flag & HA_VAR_LENGTH_KEY) {
    info->write_keys = write_keys_varlen;
    info->read_to_buffer = read_to_buffer_varlen;
    info->write_key = write_merge_key_varlen;
  } else {
    info->write_keys = write_keys;
    info->read_to_buffer = read_to_buffer;
    info->write_key = write_merge_key;
  }

  my_b_clear(&tempfile);
  my_b_clear(&tempfile_for_exceptions);
  memset(&buffpek, 0, sizeof(buffpek));
  sort_keys = (uchar **)NULL;
  error = 1;
  maxbuffer = 1;

  memavl = MY_MAX(sortbuff_size, MIN_SORT_BUFFER);
  records = info->sort_info->max_records;
  sort_length = info->key_length;

  if ((memavl - sizeof(BUFFPEK)) / (sort_length + sizeof(char *)) > UINT_MAX32)
    memavl = sizeof(BUFFPEK) + UINT_MAX32 * (sort_length + sizeof(char *));

  while (memavl >= MIN_SORT_BUFFER) {
    if ((records < UINT_MAX32) &&
        ((my_off_t)(records + 1) * (sort_length + sizeof(char *)) <=
         (my_off_t)memavl))
      keys = (uint)records + 1;
    else
      do {
        skr = maxbuffer;
        if (memavl < sizeof(BUFFPEK) * (uint)maxbuffer ||
            (keys = (uint)((memavl - sizeof(BUFFPEK) * (uint)maxbuffer) /
                           (sort_length + sizeof(char *)))) <= 1 ||
            keys < (uint)maxbuffer) {
          mi_check_print_error(info->sort_info->param,
                               "myisam_sort_buffer_size is too small");
          goto err;
        }
      } while ((maxbuffer = (int)(records / (keys - 1) + 1)) != skr);

    if ((sort_keys = (uchar **)my_malloc(
             PSI_NOT_INSTRUMENTED,
             keys * (sort_length + sizeof(char *)) + HA_FT_MAXBYTELEN,
             MYF(0)))) {
      if (my_init_dynamic_array(&buffpek, PSI_NOT_INSTRUMENTED, sizeof(BUFFPEK),
                                NULL, maxbuffer, maxbuffer / 2)) {
        my_free(sort_keys);
        sort_keys = 0;
      } else
        break;
    }
    old_memavl = memavl;
    if ((memavl = memavl / 4 * 3) < MIN_SORT_BUFFER &&
        old_memavl > MIN_SORT_BUFFER)
      memavl = MIN_SORT_BUFFER;
  }
  if (memavl < MIN_SORT_BUFFER) {
    mi_check_print_error(info->sort_info->param,
                         "MyISAM sort buffer too small"); /* purecov: tested */
    goto err;                                             /* purecov: tested */
  }

  if (!no_messages)
    printf("  - Searching for keys, allocating buffer for %d keys\n", keys);

  if ((records = find_all_keys(info, keys, sort_keys, &buffpek, &maxbuffer,
                               &tempfile, &tempfile_for_exceptions)) ==
      HA_POS_ERROR)
    goto err; /* purecov: tested */
  if (maxbuffer == 0) {
    if (!no_messages) printf("  - Dumping %lu keys\n", (ulong)records);
    if (write_index(info, sort_keys, (uint)records))
      goto err; /* purecov: inspected */
  } else {
    keys = (keys * (sort_length + sizeof(char *))) / sort_length;
    if (maxbuffer >= MERGEBUFF2) {
      if (!no_messages)
        printf("  - Merging %lu keys\n", (ulong)records); /* purecov: tested */
      if (merge_many_buff(info, keys, sort_keys,
                          dynamic_element(&buffpek, 0, BUFFPEK *), &maxbuffer,
                          &tempfile))
        goto err; /* purecov: inspected */
    }
    if (flush_io_cache(&tempfile) ||
        reinit_io_cache(&tempfile, READ_CACHE, 0L, 0, 0))
      goto err; /* purecov: inspected */
    if (!no_messages)
      printf("  - Last merge and dumping keys\n"); /* purecov: tested */
    if (merge_index(info, keys, sort_keys,
                    dynamic_element(&buffpek, 0, BUFFPEK *), maxbuffer,
                    &tempfile))
      goto err; /* purecov: inspected */
  }

  if (flush_ft_buf(info) || flush_pending_blocks(info)) goto err;

  if (my_b_inited(&tempfile_for_exceptions)) {
    MI_INFO *idx = info->sort_info->info;
    uint keyno = info->key;
    uint key_length, ref_length = idx->s->rec_reflength;

    if (!no_messages) printf("  - Adding exceptions\n"); /* purecov: tested */
    if (flush_io_cache(&tempfile_for_exceptions) ||
        reinit_io_cache(&tempfile_for_exceptions, READ_CACHE, 0L, 0, 0))
      goto err;

    while (!my_b_read(&tempfile_for_exceptions, (uchar *)&key_length,
                      sizeof(key_length)) &&
           !my_b_read(&tempfile_for_exceptions, (uchar *)sort_keys,
                      (uint)key_length)) {
      if (_mi_ck_write(idx, keyno, (uchar *)sort_keys, key_length - ref_length))
        goto err;
    }
  }

  error = 0;

err:
  my_free(sort_keys);
  delete_dynamic(&buffpek);
  close_cached_file(&tempfile);
  close_cached_file(&tempfile_for_exceptions);

  DBUG_RETURN(error ? -1 : 0);
} /* _create_index_by_sort */

/* Search after all keys and place them in a temp. file */

static ha_rows find_all_keys(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                             DYNAMIC_ARRAY *buffpek, int *maxbuffer,
                             IO_CACHE *tempfile,
                             IO_CACHE *tempfile_for_exceptions) {
  int error;
  uint idx;
  DBUG_ENTER("find_all_keys");

  idx = error = 0;
  sort_keys[0] = (uchar *)(sort_keys + keys);

  while (!(error = (*info->key_read)(info, sort_keys[idx]))) {
    if (info->real_key_length > info->key_length) {
      if (write_key(info, sort_keys[idx], tempfile_for_exceptions))
        DBUG_RETURN(HA_POS_ERROR); /* purecov: inspected */
      continue;
    }

    if (++idx == keys) {
      if (info->write_keys(info, sort_keys, idx - 1,
                           (BUFFPEK *)alloc_dynamic(buffpek), tempfile))
        DBUG_RETURN(HA_POS_ERROR); /* purecov: inspected */

      sort_keys[0] = (uchar *)(sort_keys + keys);
      memcpy(sort_keys[0], sort_keys[idx - 1], (size_t)info->key_length);
      idx = 1;
    }
    sort_keys[idx] = sort_keys[idx - 1] + info->key_length;
  }
  if (error > 0)
    DBUG_RETURN(HA_POS_ERROR); /* Aborted by get_key */ /* purecov: inspected */
  if (buffpek->elements) {
    if (info->write_keys(info, sort_keys, idx,
                         (BUFFPEK *)alloc_dynamic(buffpek), tempfile))
      DBUG_RETURN(HA_POS_ERROR); /* purecov: inspected */
    *maxbuffer = buffpek->elements - 1;
  } else
    *maxbuffer = 0;

  DBUG_RETURN((*maxbuffer) * (keys - 1) + idx);
} /* find_all_keys */

/* Search after all keys and place them in a temp. file */

void *thr_find_all_keys(void *arg) {
  MI_SORT_PARAM *sort_param = (MI_SORT_PARAM *)arg;
  int error;
  ulonglong memavl, old_memavl;
  uint keys = 0, sort_length;
  uint idx, maxbuffer;
  uchar **sort_keys = 0;
  st_keycache_thread_var thread_keycache_var;

  error = 1;

  if (my_thread_init()) goto err;

  memset(&thread_keycache_var, 0, sizeof(st_keycache_thread_var));
  mysql_cond_init(PSI_NOT_INSTRUMENTED, &thread_keycache_var.suspend);
  keycache_tls = &thread_keycache_var;

  { /* Add extra block since DBUG_ENTER declare variables */
    DBUG_ENTER("thr_find_all_keys");
    DBUG_PRINT("enter", ("master: %d", sort_param->master));
    if (sort_param->sort_info->got_error) goto err;

    if (sort_param->keyinfo->flag & HA_VAR_LENGTH_KEY) {
      sort_param->write_keys = write_keys_varlen;
      sort_param->read_to_buffer = read_to_buffer_varlen;
      sort_param->write_key = write_merge_key_varlen;
    } else {
      sort_param->write_keys = write_keys;
      sort_param->read_to_buffer = read_to_buffer;
      sort_param->write_key = write_merge_key;
    }

    my_b_clear(&sort_param->tempfile);
    my_b_clear(&sort_param->tempfile_for_exceptions);
    memset(&sort_param->buffpek, 0, sizeof(sort_param->buffpek));
    memset(&sort_param->unique, 0, sizeof(sort_param->unique));
    sort_keys = (uchar **)NULL;

    memavl = MY_MAX(sort_param->sortbuff_size, MIN_SORT_BUFFER);
    idx = (uint)sort_param->sort_info->max_records;
    sort_length = sort_param->key_length;
    maxbuffer = 1;

    if ((memavl - sizeof(BUFFPEK)) / (sort_length + sizeof(char *)) >
        UINT_MAX32)
      memavl = sizeof(BUFFPEK) + UINT_MAX32 * (sort_length + sizeof(char *));

    while (memavl >= MIN_SORT_BUFFER) {
      if ((my_off_t)(idx + 1) * (sort_length + sizeof(char *)) <=
          (my_off_t)memavl)
        keys = idx + 1;
      else {
        uint skr;
        do {
          skr = maxbuffer;
          if (memavl < sizeof(BUFFPEK) * maxbuffer ||
              (keys = (uint)((memavl - sizeof(BUFFPEK) * maxbuffer) /
                             (sort_length + sizeof(char *)))) <= 1 ||
              keys < (uint)maxbuffer) {
            mi_check_print_error(sort_param->sort_info->param,
                                 "myisam_sort_buffer_size is too small");
            goto err;
          }
        } while ((maxbuffer = (int)(idx / (keys - 1) + 1)) != skr);
      }
      if ((sort_keys = (uchar **)my_malloc(
               PSI_NOT_INSTRUMENTED,
               keys * (sort_length + sizeof(char *)) +
                   ((sort_param->keyinfo->flag & HA_FULLTEXT) ? HA_FT_MAXBYTELEN
                                                              : 0),
               MYF(0)))) {
        if (my_init_dynamic_array(&sort_param->buffpek, PSI_NOT_INSTRUMENTED,
                                  sizeof(BUFFPEK), NULL, maxbuffer,
                                  maxbuffer / 2)) {
          my_free(sort_keys);
          sort_keys = (uchar **)NULL; /* for err: label */
        } else
          break;
      }
      old_memavl = memavl;
      if ((memavl = memavl / 4 * 3) < MIN_SORT_BUFFER &&
          old_memavl > MIN_SORT_BUFFER)
        memavl = MIN_SORT_BUFFER;
    }
    if (memavl < MIN_SORT_BUFFER) {
      mi_check_print_error(sort_param->sort_info->param,
                           "MyISAM sort buffer too small");
      goto err; /* purecov: tested */
    }

    if (sort_param->sort_info->param->testflag & T_VERBOSE)
      printf("Key %d - Allocating buffer for %d keys\n", sort_param->key + 1,
             keys);
    sort_param->sort_keys = sort_keys;

    idx = error = 0;
    sort_keys[0] = (uchar *)(sort_keys + keys);

    DBUG_PRINT("info", ("reading keys"));
    while (!(error = sort_param->sort_info->got_error) &&
           !(error = (*sort_param->key_read)(sort_param, sort_keys[idx]))) {
      if (sort_param->real_key_length > sort_param->key_length) {
        if (write_key(sort_param, sort_keys[idx],
                      &sort_param->tempfile_for_exceptions))
          goto err;
        continue;
      }

      if (++idx == keys) {
        if (sort_param->write_keys(
                sort_param, sort_keys, idx - 1,
                (BUFFPEK *)alloc_dynamic(&sort_param->buffpek),
                &sort_param->tempfile))
          goto err;
        sort_keys[0] = (uchar *)(sort_keys + keys);
        memcpy(sort_keys[0], sort_keys[idx - 1],
               (size_t)sort_param->key_length);
        idx = 1;
      }
      sort_keys[idx] = sort_keys[idx - 1] + sort_param->key_length;
    }
    if (error > 0) goto err;
    if (sort_param->buffpek.elements) {
      if (sort_param->write_keys(sort_param, sort_keys, idx,
                                 (BUFFPEK *)alloc_dynamic(&sort_param->buffpek),
                                 &sort_param->tempfile))
        goto err;
      sort_param->keys = (sort_param->buffpek.elements - 1) * (keys - 1) + idx;
    } else
      sort_param->keys = idx;

    sort_param->sort_keys_length = keys;
    goto ok;

  err:
    DBUG_PRINT("error", ("got some error"));
    sort_param->sort_info->got_error = 1; /* no need to protect with a mutex */
    my_free(sort_keys);
    sort_param->sort_keys = 0;
    delete_dynamic(&sort_param->buffpek);
    close_cached_file(&sort_param->tempfile);
    close_cached_file(&sort_param->tempfile_for_exceptions);

  ok:
    free_root(&sort_param->wordroot, MYF(0));
    /*
      Detach from the share if the writer is involved. Avoid others to
      be blocked. This includes a flush of the write buffer. This will
      also indicate EOF to the readers.
      That means that a writer always gets here first and readers -
      only when they see EOF. But if a reader finishes prematurely
      because of an error it may reach this earlier - don't allow it
      to detach the writer thread.
    */
    if (sort_param->master && sort_param->sort_info->info->rec_cache.share)
      remove_io_thread(&sort_param->sort_info->info->rec_cache);

    /* Readers detach from the share if any. Avoid others to be blocked. */
    if (sort_param->read_cache.share) remove_io_thread(&sort_param->read_cache);

    mysql_mutex_lock(&sort_param->sort_info->mutex);
    if (!--sort_param->sort_info->threads_running)
      mysql_cond_signal(&sort_param->sort_info->cond);
    mysql_mutex_unlock(&sort_param->sort_info->mutex);
    DBUG_PRINT("exit", ("======== ending thread ========"));
  }
  mysql_cond_destroy(&thread_keycache_var.suspend);
  my_thread_end();
  return NULL;
}

int thr_write_keys(MI_SORT_PARAM *sort_param) {
  SORT_INFO *sort_info = sort_param->sort_info;
  MI_CHECK *param = sort_info->param;
  size_t length = 0;
  ulong keys;
  ulong *rec_per_key_part = param->rec_per_key_part;
  int got_error = sort_info->got_error;
  uint i;
  MI_INFO *info = sort_info->info;
  MYISAM_SHARE *share = info->s;
  MI_SORT_PARAM *sinfo;
  uchar *mergebuf = 0;
  DBUG_ENTER("thr_write_keys");

  for (i = 0, sinfo = sort_param; i < sort_info->total_keys; i++, sinfo++) {
    if (!sinfo->sort_keys) {
      got_error = 1;
      my_free(mi_get_rec_buff_ptr(info, sinfo->rec_buff));
      continue;
    }
    if (!got_error) {
      mi_set_key_active(share->state.key_map, sinfo->key);
      if (!sinfo->buffpek.elements) {
        if (param->testflag & T_VERBOSE) {
          printf("Key %d  - Dumping %u keys\n", sinfo->key + 1, sinfo->keys);
          fflush(stdout);
        }
        if (write_index(sinfo, sinfo->sort_keys, sinfo->keys) ||
            flush_ft_buf(sinfo) || flush_pending_blocks(sinfo))
          got_error = 1;
      }
    }
    my_free(sinfo->sort_keys);
    my_free(mi_get_rec_buff_ptr(info, sinfo->rec_buff));
    sinfo->sort_keys = 0;
  }

  for (i = 0, sinfo = sort_param; i < sort_info->total_keys; i++,
      delete_dynamic(&sinfo->buffpek), close_cached_file(&sinfo->tempfile),
      close_cached_file(&sinfo->tempfile_for_exceptions),
      rec_per_key_part += sinfo->keyinfo->keysegs, sinfo++) {
    if (got_error) continue;
    if (sinfo->keyinfo->flag & HA_VAR_LENGTH_KEY) {
      sinfo->write_keys = write_keys_varlen;
      sinfo->read_to_buffer = read_to_buffer_varlen;
      sinfo->write_key = write_merge_key_varlen;
    } else {
      sinfo->write_keys = write_keys;
      sinfo->read_to_buffer = read_to_buffer;
      sinfo->write_key = write_merge_key;
    }
    if (sinfo->buffpek.elements) {
      uint maxbuffer = sinfo->buffpek.elements - 1;
      if (!mergebuf) {
        length = (size_t)param->sort_buffer_length;
        while (length >= MIN_SORT_BUFFER) {
          if ((mergebuf =
                   (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, length, MYF(0))))
            break;
          length = length * 3 / 4;
        }
        if (!mergebuf) {
          got_error = 1;
          continue;
        }
      }
      keys = length / sinfo->key_length;
      if (maxbuffer >= MERGEBUFF2) {
        if (param->testflag & T_VERBOSE)
          printf("Key %d  - Merging %u keys\n", sinfo->key + 1, sinfo->keys);
        if (merge_many_buff(sinfo, keys, (uchar **)mergebuf,
                            dynamic_element(&sinfo->buffpek, 0, BUFFPEK *),
                            (int *)&maxbuffer, &sinfo->tempfile)) {
          got_error = 1;
          continue;
        }
      }
      if (flush_io_cache(&sinfo->tempfile) ||
          reinit_io_cache(&sinfo->tempfile, READ_CACHE, 0L, 0, 0)) {
        got_error = 1;
        continue;
      }
      if (param->testflag & T_VERBOSE)
        printf("Key %d  - Last merge and dumping keys\n", sinfo->key + 1);
      if (merge_index(sinfo, keys, (uchar **)mergebuf,
                      dynamic_element(&sinfo->buffpek, 0, BUFFPEK *), maxbuffer,
                      &sinfo->tempfile) ||
          flush_ft_buf(sinfo) || flush_pending_blocks(sinfo)) {
        got_error = 1;
        continue;
      }
    }
    if (my_b_inited(&sinfo->tempfile_for_exceptions)) {
      uint key_length;

      if (param->testflag & T_VERBOSE)
        printf("Key %d  - Dumping 'long' keys\n", sinfo->key + 1);

      if (flush_io_cache(&sinfo->tempfile_for_exceptions) ||
          reinit_io_cache(&sinfo->tempfile_for_exceptions, READ_CACHE, 0L, 0,
                          0)) {
        got_error = 1;
        continue;
      }

      while (!got_error &&
             !my_b_read(&sinfo->tempfile_for_exceptions, (uchar *)&key_length,
                        sizeof(key_length))) {
        uchar ft_buf[HA_FT_MAXBYTELEN + HA_FT_WLEN + 10];
        if (key_length > sizeof(ft_buf) ||
            my_b_read(&sinfo->tempfile_for_exceptions, (uchar *)ft_buf,
                      (uint)key_length) ||
            _mi_ck_write(info, sinfo->key, (uchar *)ft_buf,
                         key_length - info->s->rec_reflength))
          got_error = 1;
      }
    }
    if (!got_error && param->testflag & T_STATISTICS)
      update_key_parts(sinfo->keyinfo, rec_per_key_part, sinfo->unique,
                       param->stats_method == MI_STATS_METHOD_IGNORE_NULLS
                           ? sinfo->notnull
                           : NULL,
                       (ulonglong)info->state->records);
  }
  my_free(mergebuf);
  DBUG_RETURN(got_error);
}

/* Write all keys in memory to file for later merge */

static int write_keys(MI_SORT_PARAM *info, uchar **sort_keys, uint count,
                      BUFFPEK *buffpek, IO_CACHE *tempfile) {
  uchar **end;
  uint sort_length = info->key_length;
  DBUG_ENTER("write_keys");

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    DBUG_RETURN(1); /* purecov: inspected */

  buffpek->file_pos = my_b_tell(tempfile);
  buffpek->count = count;

  for (end = sort_keys + count; sort_keys != end; sort_keys++) {
    if (my_b_write(tempfile, (uchar *)*sort_keys, (uint)sort_length))
      DBUG_RETURN(1); /* purecov: inspected */
  }
  DBUG_RETURN(0);
} /* write_keys */

static inline int my_var_write(MI_SORT_PARAM *info, IO_CACHE *to_file,
                               uchar *bufs) {
  int err;
  uint16 len = _mi_keylength(info->keyinfo, (uchar *)bufs);

  /* The following is safe as this is a local file */
  if ((err = my_b_write(to_file, (uchar *)&len, sizeof(len)))) return (err);
  if ((err = my_b_write(to_file, bufs, (uint)len))) return (err);
  return (0);
}

static int write_keys_varlen(MI_SORT_PARAM *info, uchar **sort_keys, uint count,
                             BUFFPEK *buffpek, IO_CACHE *tempfile) {
  uchar **end;
  int err;
  DBUG_ENTER("write_keys_varlen");

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    DBUG_RETURN(1); /* purecov: inspected */

  buffpek->file_pos = my_b_tell(tempfile);
  buffpek->count = count;
  for (end = sort_keys + count; sort_keys != end; sort_keys++) {
    if ((err = my_var_write(info, tempfile, (uchar *)*sort_keys)))
      DBUG_RETURN(err);
  }
  DBUG_RETURN(0);
} /* write_keys_varlen */

static int write_key(MI_SORT_PARAM *info, uchar *key, IO_CACHE *tempfile) {
  uint key_length = info->real_key_length;
  DBUG_ENTER("write_key");

  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    DBUG_RETURN(1);

  if (my_b_write(tempfile, (uchar *)&key_length, sizeof(key_length)) ||
      my_b_write(tempfile, (uchar *)key, (uint)key_length))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
} /* write_key */

/* Write index */

static int write_index(MI_SORT_PARAM *info, uchar **sort_keys, uint count) {
  DBUG_ENTER("write_index");

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  while (count--) {
    if ((*info->key_write)(info, *sort_keys++))
      DBUG_RETURN(-1); /* purecov: inspected */
  }
  DBUG_RETURN(0);
} /* write_index */

/* Merge buffers to make < MERGEBUFF2 buffers */

static int merge_many_buff(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                           BUFFPEK *buffpek, int *maxbuffer, IO_CACHE *t_file) {
  int i;
  IO_CACHE t_file2, *from_file, *to_file, *temp;
  BUFFPEK *lastbuff;
  DBUG_ENTER("merge_many_buff");

  if (*maxbuffer < MERGEBUFF2) DBUG_RETURN(0); /* purecov: inspected */
  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    DBUG_RETURN(1); /* purecov: inspected */

  from_file = t_file;
  to_file = &t_file2;
  while (*maxbuffer >= MERGEBUFF2) {
    reinit_io_cache(from_file, READ_CACHE, 0L, 0, 0);
    reinit_io_cache(to_file, WRITE_CACHE, 0L, 0, 0);
    lastbuff = buffpek;
    for (i = 0; i <= *maxbuffer - MERGEBUFF * 3 / 2; i += MERGEBUFF) {
      if (merge_buffers(info, keys, from_file, to_file, sort_keys, lastbuff++,
                        buffpek + i, buffpek + i + MERGEBUFF - 1))
        goto cleanup;
    }
    if (merge_buffers(info, keys, from_file, to_file, sort_keys, lastbuff++,
                      buffpek + i, buffpek + *maxbuffer))
      break;                            /* purecov: inspected */
    if (flush_io_cache(to_file)) break; /* purecov: inspected */
    temp = from_file;
    from_file = to_file;
    to_file = temp;
    *maxbuffer = (int)(lastbuff - buffpek) - 1;
  }
cleanup:
  close_cached_file(to_file); /* This holds old result */
  if (to_file == t_file) {
    DBUG_ASSERT(t_file2.type == WRITE_CACHE);
    *t_file = t_file2; /* Copy result file */
    t_file->current_pos = &t_file->write_pos;
    t_file->current_end = &t_file->write_end;
  }

  DBUG_RETURN(*maxbuffer >= MERGEBUFF2); /* Return 1 if interrupted */
} /* merge_many_buff */

/*
   Read data to buffer

  SYNOPSIS
    read_to_buffer()
    fromfile		File to read from
    buffpek		Where to read from
    sort_length		max length to read
  RESULT
    > 0	Ammount of bytes read
    -1	Error
*/

static uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
                           uint sort_length) {
  uint count;
  uint length;

  if ((count = (uint)MY_MIN((ha_rows)buffpek->max_keys, buffpek->count))) {
    if (mysql_file_pread(fromfile->file, (uchar *)buffpek->base,
                         (length = sort_length * count), buffpek->file_pos,
                         MYF_RW))
      return ((uint)-1); /* purecov: inspected */
    buffpek->key = buffpek->base;
    buffpek->file_pos += length; /* New filepos */
    buffpek->count -= count;
    buffpek->mem_count = count;
  }
  return (count * sort_length);
} /* read_to_buffer */

static uint read_to_buffer_varlen(IO_CACHE *fromfile, BUFFPEK *buffpek,
                                  uint sort_length) {
  uint count;
  uint16 length_of_key = 0;
  uint idx;
  uchar *buffp;

  if ((count = (uint)MY_MIN((ha_rows)buffpek->max_keys, buffpek->count))) {
    buffp = buffpek->base;

    for (idx = 1; idx <= count; idx++) {
      if (mysql_file_pread(fromfile->file, (uchar *)&length_of_key,
                           sizeof(length_of_key), buffpek->file_pos, MYF_RW))
        return ((uint)-1);
      buffpek->file_pos += sizeof(length_of_key);
      if (mysql_file_pread(fromfile->file, (uchar *)buffp, length_of_key,
                           buffpek->file_pos, MYF_RW))
        return ((uint)-1);
      buffpek->file_pos += length_of_key;
      buffp = buffp + sort_length;
    }
    buffpek->key = buffpek->base;
    buffpek->count -= count;
    buffpek->mem_count = count;
  }
  return (count * sort_length);
} /* read_to_buffer_varlen */

static int write_merge_key_varlen(MI_SORT_PARAM *info, IO_CACHE *to_file,
                                  uchar *key, uint sort_length, uint count) {
  uint idx;
  uchar *bufs = key;

  for (idx = 1; idx <= count; idx++) {
    int err;
    if ((err = my_var_write(info, to_file, bufs))) return (err);
    bufs = bufs + sort_length;
  }
  return (0);
}

static int write_merge_key(MI_SORT_PARAM *info MY_ATTRIBUTE((unused)),
                           IO_CACHE *to_file, uchar *key, uint sort_length,
                           uint count) {
  return my_b_write(to_file, key, (size_t)sort_length * count);
}

/*
  Merge buffers to one buffer
  If to_file == 0 then use info->key_write
*/

static int merge_buffers(MI_SORT_PARAM *info, uint keys, IO_CACHE *from_file,
                         IO_CACHE *to_file, uchar **sort_keys,
                         BUFFPEK *lastbuff, BUFFPEK *Fb, BUFFPEK *Tb) {
  int error;
  uint sort_length, maxcount;
  ha_rows count;
  my_off_t to_start_filepos = 0;
  uchar *strpos;
  BUFFPEK *buffpek, **refpek;
  QUEUE queue;
  volatile int *killed = killed_ptr(info->sort_info->param);
  DBUG_ENTER("merge_buffers");

  count = error = 0;
  maxcount = keys / ((uint)(Tb - Fb) + 1);
  DBUG_ASSERT(maxcount > 0);
  if (to_file) to_start_filepos = my_b_tell(to_file);
  strpos = (uchar *)sort_keys;
  sort_length = info->key_length;

  if (init_queue(
          &queue, key_memory_QUEUE, (uint)(Tb - Fb) + 1, offsetof(BUFFPEK, key),
          0, (int (*)(void *, uchar *, uchar *))info->key_cmp, (void *)info))
    DBUG_RETURN(1); /* purecov: inspected */

  for (buffpek = Fb; buffpek <= Tb; buffpek++) {
    count += buffpek->count;
    buffpek->base = strpos;
    buffpek->max_keys = maxcount;
    strpos += (uint)(
        error = (int)info->read_to_buffer(from_file, buffpek, sort_length));
    if (error == -1) goto err; /* purecov: inspected */
    queue_insert(&queue, (uchar *)buffpek);
  }

  while (queue.elements > 1) {
    for (;;) {
      if (*killed) {
        error = 1;
        goto err;
      }
      buffpek = (BUFFPEK *)queue_top(&queue);
      if (to_file) {
        if (info->write_key(info, to_file, (uchar *)buffpek->key,
                            (uint)sort_length, 1)) {
          error = 1;
          goto err; /* purecov: inspected */
        }
      } else {
        if ((*info->key_write)(info, (void *)buffpek->key)) {
          error = 1;
          goto err; /* purecov: inspected */
        }
      }
      buffpek->key += sort_length;
      if (!--buffpek->mem_count) {
        if (!(error =
                  (int)info->read_to_buffer(from_file, buffpek, sort_length))) {
          uchar *base = buffpek->base;
          uint max_keys = buffpek->max_keys;

          (void)queue_remove(&queue, 0);

          /* Put room used by buffer to use in other buffer */
          for (refpek = (BUFFPEK **)&queue_top(&queue);
               refpek <= (BUFFPEK **)&queue_end(&queue); refpek++) {
            buffpek = *refpek;
            if (buffpek->base + buffpek->max_keys * sort_length == base) {
              buffpek->max_keys += max_keys;
              break;
            } else if (base + max_keys * sort_length == buffpek->base) {
              buffpek->base = base;
              buffpek->max_keys += max_keys;
              break;
            }
          }
          break; /* One buffer have been removed */
        }
      } else if (error == -1)
        goto err;             /* purecov: inspected */
      queue_replaced(&queue); /* Top element has been replaced */
    }
  }
  buffpek = (BUFFPEK *)queue_top(&queue);
  buffpek->base = (uchar *)sort_keys;
  buffpek->max_keys = keys;
  do {
    if (to_file) {
      if (info->write_key(info, to_file, (uchar *)buffpek->key, sort_length,
                          buffpek->mem_count)) {
        error = 1;
        goto err; /* purecov: inspected */
      }
    } else {
      uchar *end;
      strpos = buffpek->key;
      for (end = strpos + buffpek->mem_count * sort_length; strpos != end;
           strpos += sort_length) {
        if ((*info->key_write)(info, (void *)strpos)) {
          error = 1;
          goto err; /* purecov: inspected */
        }
      }
    }
  } while ((error = (int)info->read_to_buffer(from_file, buffpek,
                                              sort_length)) != -1 &&
           error != 0);

  lastbuff->count = count;
  if (to_file) lastbuff->file_pos = to_start_filepos;
err:
  delete_queue(&queue);
  DBUG_RETURN(error);
} /* merge_buffers */

/* Do a merge to output-file (save only positions) */

static int merge_index(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                       BUFFPEK *buffpek, int maxbuffer, IO_CACHE *tempfile) {
  DBUG_ENTER("merge_index");
  if (merge_buffers(info, keys, tempfile, (IO_CACHE *)0, sort_keys, buffpek,
                    buffpek, buffpek + maxbuffer))
    DBUG_RETURN(1); /* purecov: inspected */
  DBUG_RETURN(0);
} /* merge_index */

static int flush_ft_buf(MI_SORT_PARAM *info) {
  int err = 0;
  if (info->sort_info->ft_buf) {
    err = sort_ft_buf_flush(info);
    my_free(info->sort_info->ft_buf);
    info->sort_info->ft_buf = 0;
  }
  return err;
}
