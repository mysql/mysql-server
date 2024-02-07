/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Creates a index for a database by reading keys, sorting them and outputting
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
  DBUG_TRACE;
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
  sort_keys = (uchar **)nullptr;
  error = 1;
  maxbuffer = 1;

  memavl = std::max(sortbuff_size, MIN_SORT_BUFFER);
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
                                nullptr, maxbuffer, maxbuffer / 2)) {
        my_free(sort_keys);
        sort_keys = nullptr;
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
        reinit_io_cache(&tempfile, READ_CACHE, 0L, false, false))
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
        reinit_io_cache(&tempfile_for_exceptions, READ_CACHE, 0L, false, false))
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

  return error ? -1 : 0;
} /* _create_index_by_sort */

/* Search after all keys and place them in a temp. file */

static ha_rows find_all_keys(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                             DYNAMIC_ARRAY *buffpek, int *maxbuffer,
                             IO_CACHE *tempfile,
                             IO_CACHE *tempfile_for_exceptions) {
  int error;
  uint idx;
  DBUG_TRACE;

  idx = error = 0;
  sort_keys[0] = (uchar *)(sort_keys + keys);

  while (!(error = (*info->key_read)(info, sort_keys[idx]))) {
    if (info->real_key_length > info->key_length) {
      if (write_key(info, sort_keys[idx], tempfile_for_exceptions))
        return HA_POS_ERROR; /* purecov: inspected */
      continue;
    }

    if (++idx == keys) {
      if (info->write_keys(info, sort_keys, idx - 1,
                           (BUFFPEK *)alloc_dynamic(buffpek), tempfile))
        return HA_POS_ERROR; /* purecov: inspected */

      sort_keys[0] = (uchar *)(sort_keys + keys);
      memcpy(sort_keys[0], sort_keys[idx - 1], (size_t)info->key_length);
      idx = 1;
    }
    sort_keys[idx] = sort_keys[idx - 1] + info->key_length;
  }
  if (error > 0)
    return HA_POS_ERROR; /* Aborted by get_key */ /* purecov: inspected */
  if (buffpek->elements) {
    if (info->write_keys(info, sort_keys, idx,
                         (BUFFPEK *)alloc_dynamic(buffpek), tempfile))
      return HA_POS_ERROR; /* purecov: inspected */
    *maxbuffer = buffpek->elements - 1;
  } else
    *maxbuffer = 0;

  return (*maxbuffer) * (keys - 1) + idx;
} /* find_all_keys */

/* Write all keys in memory to file for later merge */

static int write_keys(MI_SORT_PARAM *info, uchar **sort_keys, uint count,
                      BUFFPEK *buffpek, IO_CACHE *tempfile) {
  uchar **end;
  uint sort_length = info->key_length;
  DBUG_TRACE;

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return 1; /* purecov: inspected */

  buffpek->file_pos = my_b_tell(tempfile);
  buffpek->count = count;

  for (end = sort_keys + count; sort_keys != end; sort_keys++) {
    if (my_b_write(tempfile, (uchar *)*sort_keys, (uint)sort_length))
      return 1; /* purecov: inspected */
  }
  return 0;
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
  DBUG_TRACE;

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return 1; /* purecov: inspected */

  buffpek->file_pos = my_b_tell(tempfile);
  buffpek->count = count;
  for (end = sort_keys + count; sort_keys != end; sort_keys++) {
    if ((err = my_var_write(info, tempfile, (uchar *)*sort_keys))) return err;
  }
  return 0;
} /* write_keys_varlen */

static int write_key(MI_SORT_PARAM *info, uchar *key, IO_CACHE *tempfile) {
  uint key_length = info->real_key_length;
  DBUG_TRACE;

  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return 1;

  if (my_b_write(tempfile, (uchar *)&key_length, sizeof(key_length)) ||
      my_b_write(tempfile, (uchar *)key, (uint)key_length))
    return 1;
  return 0;
} /* write_key */

/* Write index */

static int write_index(MI_SORT_PARAM *info, uchar **sort_keys, uint count) {
  DBUG_TRACE;

  std::sort(sort_keys, sort_keys + count, [info](uchar *a, uchar *b) {
    return info->key_cmp(info, pointer_cast<unsigned char *>(&a),
                         pointer_cast<unsigned char *>(&b)) < 0;
  });
  while (count--) {
    if ((*info->key_write)(info, *sort_keys++))
      return -1; /* purecov: inspected */
  }
  return 0;
} /* write_index */

/* Merge buffers to make < MERGEBUFF2 buffers */

static int merge_many_buff(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                           BUFFPEK *buffpek, int *maxbuffer, IO_CACHE *t_file) {
  int i;
  IO_CACHE t_file2, *from_file, *to_file, *temp;
  BUFFPEK *lastbuff;
  DBUG_TRACE;

  if (*maxbuffer < MERGEBUFF2) return 0; /* purecov: inspected */
  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2, my_tmpdir(info->tmpdir), "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return 1; /* purecov: inspected */

  from_file = t_file;
  to_file = &t_file2;
  while (*maxbuffer >= MERGEBUFF2) {
    reinit_io_cache(from_file, READ_CACHE, 0L, false, false);
    reinit_io_cache(to_file, WRITE_CACHE, 0L, false, false);
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
    assert(t_file2.type == WRITE_CACHE);
    *t_file = t_file2; /* Copy result file */
    t_file->current_pos = &t_file->write_pos;
    t_file->current_end = &t_file->write_end;
  }

  return *maxbuffer >= MERGEBUFF2; /* Return 1 if interrupted */
} /* merge_many_buff */

/*
   Read data to buffer

  SYNOPSIS
    read_to_buffer()
    fromfile		File to read from
    buffpek		Where to read from
    sort_length		max length to read
  RESULT
    > 0	Number of bytes read
    -1	Error
*/

static uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
                           uint sort_length) {
  uint count;
  uint length;

  if ((count = std::min<ha_rows>(buffpek->max_keys, buffpek->count))) {
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

  if ((count = std::min<ha_rows>(buffpek->max_keys, buffpek->count))) {
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

static int write_merge_key(MI_SORT_PARAM *info [[maybe_unused]],
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
  DBUG_TRACE;

  count = error = 0;
  maxcount = keys / ((uint)(Tb - Fb) + 1);
  assert(maxcount > 0);
  if (to_file) to_start_filepos = my_b_tell(to_file);
  strpos = (uchar *)sort_keys;
  sort_length = info->key_length;

  if (init_queue(&queue, key_memory_QUEUE, (uint)(Tb - Fb) + 1,
                 offsetof(BUFFPEK, key), false,
                 (int (*)(void *, uchar *, uchar *))info->key_cmp,
                 (void *)info))
    return 1; /* purecov: inspected */

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
  return error;
} /* merge_buffers */

/* Do a merge to output-file (save only positions) */

static int merge_index(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
                       BUFFPEK *buffpek, int maxbuffer, IO_CACHE *tempfile) {
  DBUG_TRACE;
  if (merge_buffers(info, keys, tempfile, (IO_CACHE *)nullptr, sort_keys,
                    buffpek, buffpek, buffpek + maxbuffer))
    return 1; /* purecov: inspected */
  return 0;
} /* merge_index */

static int flush_ft_buf(MI_SORT_PARAM *info) {
  int err = 0;
  if (info->sort_info->ft_buf) {
    err = sort_ft_buf_flush(info);
    my_free(info->sort_info->ft_buf);
    info->sort_info->ft_buf = nullptr;
  }
  return err;
}
