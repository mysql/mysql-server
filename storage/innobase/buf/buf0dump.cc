/*****************************************************************************

Copyright (c) 2011, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file buf/buf0dump.cc
 Implements a buffer pool dump/load.

 Created April 08, 2011 Vasil Dimov
 *******************************************************/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>

#include "buf0buf.h"
#include "buf0dump.h"
#include "dict0dict.h"

#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_thread.h"
#include "mysql/psi/mysql_stage.h"
#include "os0file.h"
#include "os0thread-create.h"
#include "os0thread.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "sync0rw.h"
#include "univ.i"
#include "ut0byte.h"

enum status_severity { STATUS_VERBOSE, STATUS_INFO, STATUS_ERR };

static inline bool SHUTTING_DOWN() {
  return srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP;
}

/* Flags that tell the buffer pool dump/load thread which action should it
take after being waked up. */
static bool buf_dump_should_start = false;
static bool buf_load_should_start = false;

static bool buf_load_abort_flag = false;

/* Used to temporary store dump info in order to avoid IO while holding
buffer pool LRU list mutex during dump and also to sort the contents of the
dump before reading the pages from disk during load.
We store the space id in the high 32 bits and page no in low 32 bits. */
typedef uint64_t buf_dump_t;

/* Aux macros to create buf_dump_t and to extract space and page from it */
inline uint64_t BUF_DUMP_CREATE(space_id_t space, page_no_t page) {
  return ut_ull_create(space, page);
}
constexpr space_id_t BUF_DUMP_SPACE(uint64_t a) {
  return static_cast<space_id_t>((a) >> 32);
}
constexpr page_no_t BUF_DUMP_PAGE(uint64_t a) {
  return static_cast<page_no_t>((a)&0xFFFFFFFFUL);
}
/** Wakes up the buffer pool dump/load thread and instructs it to start
 a dump. This function is called by MySQL code via buffer_pool_dump_now()
 and it should return immediately because the whole MySQL is frozen during
 its execution. */
void buf_dump_start() {
  buf_dump_should_start = true;
  os_event_set(srv_buf_dump_event);
}

/** Wakes up the buffer pool dump/load thread and instructs it to start
 a load. This function is called by MySQL code via buffer_pool_load_now()
 and it should return immediately because the whole MySQL is frozen during
 its execution. */
void buf_load_start() {
  buf_load_should_start = true;
  os_event_set(srv_buf_dump_event);
}

/** Sets the global variable that feeds MySQL's innodb_buffer_pool_dump_status
 to the specified string. The format and the following parameters are the
 same as the ones used for printf(3). The value of this variable can be
 retrieved by:
 SELECT variable_value FROM performance_schema.global_status WHERE
 variable_name = 'INNODB_BUFFER_POOL_DUMP_STATUS';
 or by:
 SHOW STATUS LIKE 'innodb_buffer_pool_dump_status'; */
static MY_ATTRIBUTE((format(printf, 2, 3))) void buf_dump_status(
    enum status_severity severity, /*!< in: status severity */
    const char *fmt,               /*!< in: format */
    ...)                           /*!< in: extra parameters according
                                   to fmt */
{
  va_list ap;

  va_start(ap, fmt);

  ut_vsnprintf(export_vars.innodb_buffer_pool_dump_status,
               sizeof(export_vars.innodb_buffer_pool_dump_status), fmt, ap);

  switch (severity) {
    case STATUS_INFO:
      ib::info(ER_IB_MSG_119) << export_vars.innodb_buffer_pool_dump_status;
      break;

    case STATUS_ERR:
      ib::error(ER_IB_MSG_120) << export_vars.innodb_buffer_pool_dump_status;
      break;

    case STATUS_VERBOSE:
      break;
  }

  va_end(ap);
}

/** Sets the global variable that feeds MySQL's innodb_buffer_pool_load_status
 to the specified string. The format and the following parameters are the
 same as the ones used for printf(3). The value of this variable can be
 retrieved by:
 SELECT variable_value FROM performance_schema.global_status WHERE
 variable_name = 'INNODB_BUFFER_POOL_LOAD_STATUS';
 or by:
 SHOW STATUS LIKE 'innodb_buffer_pool_load_status'; */
static MY_ATTRIBUTE((format(printf, 2, 3))) void buf_load_status(
    enum status_severity severity, /*!< in: status severity */
    const char *fmt,               /*!< in: format */
    ...)                           /*!< in: extra parameters according to fmt */
{
  va_list ap;

  va_start(ap, fmt);

  ut_vsnprintf(export_vars.innodb_buffer_pool_load_status,
               sizeof(export_vars.innodb_buffer_pool_load_status), fmt, ap);

  switch (severity) {
    case STATUS_INFO:
      ib::info(ER_IB_MSG_121) << export_vars.innodb_buffer_pool_load_status;
      break;

    case STATUS_ERR:
      ib::error(ER_IB_MSG_122) << export_vars.innodb_buffer_pool_load_status;
      break;

    case STATUS_VERBOSE:
      break;
  }

  va_end(ap);
}

/** Returns the directory path where the buffer pool dump file will be created.
@return directory path */
static const char *get_buf_dump_dir() {
  const char *dump_dir;

  /* The dump file should be created in the default data directory if
  innodb_data_home_dir is set as an empty string. */
  if (strcmp(srv_data_home, "") == 0) {
    dump_dir = MySQL_datadir_path;
  } else {
    dump_dir = srv_data_home;
  }

  return (dump_dir);
}

/** Generate the path to the buffer pool dump/load file.
@param[out]     path            generated path
@param[in]      path_size       size of 'path', used as in snprintf(3). */
void buf_dump_generate_path(char *path, size_t path_size) {
  char buf[FN_REFLEN];

  snprintf(buf, sizeof(buf), "%s%c%s", get_buf_dump_dir(), OS_PATH_SEPARATOR,
           srv_buf_dump_filename);

  /* Use this file if it exists. */
  if (os_file_exists(buf)) {
    /* my_realpath() assumes the destination buffer is big enough
    to hold FN_REFLEN bytes. */
    ut_a(path_size >= FN_REFLEN);

    my_realpath(path, buf, 0);
  } else {
    /* If it does not exist, then resolve only srv_data_home
    and append srv_buf_dump_filename to it. */
    char srv_data_home_full[FN_REFLEN];

    my_realpath(srv_data_home_full, get_buf_dump_dir(), 0);

    if (srv_data_home_full[strlen(srv_data_home_full) - 1] ==
        OS_PATH_SEPARATOR) {
      snprintf(path, path_size, "%s%s", srv_data_home_full,
               srv_buf_dump_filename);
    } else {
      snprintf(path, path_size, "%s%c%s", srv_data_home_full, OS_PATH_SEPARATOR,
               srv_buf_dump_filename);
    }
  }
}

/** Perform a buffer pool dump into the file specified by
innodb_buffer_pool_filename. If any errors occur then the value of
innodb_buffer_pool_dump_status will be set accordingly, see buf_dump_status().
The dump filename can be specified by (relative to srv_data_home):
SET GLOBAL innodb_buffer_pool_filename='filename';
@param[in]      obey_shutdown   quit if we are in a shutting down state */
static void buf_dump(bool obey_shutdown) {
#define SHOULD_QUIT() (SHUTTING_DOWN() && obey_shutdown)

  char full_filename[OS_FILE_MAX_PATH];
  char tmp_filename[OS_FILE_MAX_PATH + 11];
  char now[32];
  FILE *f;
  ulint i;
  int ret;

  buf_dump_generate_path(full_filename, sizeof(full_filename));

  snprintf(tmp_filename, sizeof(tmp_filename), "%s.incomplete", full_filename);

  buf_dump_status(STATUS_INFO, "Dumping buffer pool(s) to %s", full_filename);

  f = fopen(tmp_filename, "w");
  if (f == nullptr) {
    buf_dump_status(STATUS_ERR, "Cannot open '%s' for writing: %s",
                    tmp_filename, strerror(errno));
    return;
  }
  /* else */

  /* walk through each buffer pool */
  for (i = 0; i < srv_buf_pool_instances && !SHOULD_QUIT(); i++) {
    buf_pool_t *buf_pool;
    buf_dump_t *dump;

    buf_pool = buf_pool_from_array(i);

    /* obtain buf_pool LRU list mutex before allocate, since
    UT_LIST_GET_LEN(buf_pool->LRU) could change */
    mutex_enter(&buf_pool->LRU_list_mutex);

    size_t n_pages = UT_LIST_GET_LEN(buf_pool->LRU);

    /* skip empty buffer pools */
    if (n_pages == 0) {
      mutex_exit(&buf_pool->LRU_list_mutex);
      continue;
    }

    if (srv_buf_pool_dump_pct != 100) {
      ut_ad(srv_buf_pool_dump_pct < 100);

      n_pages = n_pages * srv_buf_pool_dump_pct / 100;

      if (n_pages == 0) {
        n_pages = 1;
      }
    }

    dump = static_cast<buf_dump_t *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, n_pages * sizeof(*dump)));

    if (dump == nullptr) {
      mutex_exit(&buf_pool->LRU_list_mutex);
      fclose(f);
      buf_dump_status(STATUS_ERR, "Cannot allocate %zu bytes: %s",
                      n_pages * sizeof(*dump), strerror(errno));
      /* leave tmp_filename to exist */
      return;
    }
    {
      size_t j{0};
      for (auto bpage : buf_pool->LRU) {
        if (n_pages <= j) break;
        ut_a(buf_page_in_file(bpage));

        dump[j++] = BUF_DUMP_CREATE(bpage->id.space(), bpage->id.page_no());
      }

      ut_a(j == n_pages);
    }

    mutex_exit(&buf_pool->LRU_list_mutex);

    for (size_t j = 0; j < n_pages && !SHOULD_QUIT(); j++) {
      ret = fprintf(f, SPACE_ID_PF "," PAGE_NO_PF "\n", BUF_DUMP_SPACE(dump[j]),
                    BUF_DUMP_PAGE(dump[j]));
      if (ret < 0) {
        ut::free(dump);
        fclose(f);
        buf_dump_status(STATUS_ERR, "Cannot write to '%s': %s", tmp_filename,
                        strerror(errno));
        /* leave tmp_filename to exist */
        return;
      }

      if (j % 128 == 0) {
        buf_dump_status(
            STATUS_VERBOSE,
            "Dumping buffer pool " ULINTPF "/" ULINTPF ", page %zu/%zu", i + 1,
            static_cast<ulint>(srv_buf_pool_instances), j + 1, n_pages);
      }
    }

    ut::free(dump);
  }

  ret = fclose(f);
  if (ret != 0) {
    buf_dump_status(STATUS_ERR, "Cannot close '%s': %s", tmp_filename,
                    strerror(errno));
    return;
  }
  /* else */

  ret = unlink(full_filename);
  if (ret != 0 && errno != ENOENT) {
    buf_dump_status(STATUS_ERR, "Cannot delete '%s': %s", full_filename,
                    strerror(errno));
    /* leave tmp_filename to exist */
    return;
  }
  /* else */

  ret = rename(tmp_filename, full_filename);
  if (ret != 0) {
    buf_dump_status(STATUS_ERR, "Cannot rename '%s' to '%s': %s", tmp_filename,
                    full_filename, strerror(errno));
    /* leave tmp_filename to exist */
    return;
  }
  /* else */

  /* success */

  ut_sprintf_timestamp(now);

  buf_dump_status(STATUS_INFO, "Buffer pool(s) dump completed at %s", now);
}

/** Artificially delay the buffer pool loading if necessary. The idea of this
function is to prevent hogging the server with IO and slowing down too much
normal client queries.
@param[in,out]  last_check_time         milliseconds since epoch of the last
                                        time we did check if throttling is
                                        needed, we do the check every
                                        srv_io_capacity IO ops.
@param[in]      last_activity_count     activity count
@param[in]      n_io                    number of IO ops done since buffer
                                        pool load has started */
static inline void buf_load_throttle_if_needed(
    std::chrono::steady_clock::time_point *last_check_time,
    ulint *last_activity_count, ulint n_io) {
  if (n_io % srv_io_capacity < srv_io_capacity - 1) {
    return;
  }

  if (*last_check_time == std::chrono::steady_clock::time_point{} ||
      *last_activity_count == 0) {
    *last_check_time = std::chrono::steady_clock::now();
    *last_activity_count = srv_get_activity_count();
    return;
  }

  /* srv_io_capacity IO operations have been performed by buffer pool
  load since the last time we were here. */

  /* If no other activity, then keep going without any delay. */
  if (srv_get_activity_count() == *last_activity_count) {
    return;
  }

  /* There has been other activity, throttle. */

  const auto elapsed_time = std::chrono::steady_clock::now() - *last_check_time;

  /* Notice that elapsed_time is not the time for the last
  srv_io_capacity IO operations performed by BP load. It is the
  time elapsed since the last time we detected that there has been
  other activity. This has a small and acceptable deficiency, e.g.:
  1. BP load runs and there is no other activity.
  2. Other activity occurs, we run N IO operations after that and
     enter here (where 0 <= N < srv_io_capacity).
  3. last_check_time is very old and we do not sleep at this time, but
     only update last_check_time and last_activity_count.
  4. We run srv_io_capacity more IO operations and call this function
     again.
  5. There has been more other activity and thus we enter here.
  6. Now last_check_time is recent and we sleep if necessary to prevent
     more than srv_io_capacity IO operations per second.
  The deficiency is that we could have slept at 3., but for this we
  would have to update last_check_time before the
  "cur_activity_count == *last_activity_count" check and calling
  ut_time_monotonic_ms() that often may turn out to be too expensive. */

  if (elapsed_time < std::chrono::seconds{1}) {
    std::this_thread::sleep_for(std::chrono::seconds{1} - elapsed_time);
  }

  *last_check_time = std::chrono::steady_clock::now();
  *last_activity_count = srv_get_activity_count();
}

/** Perform a buffer pool load from the file specified by
 innodb_buffer_pool_filename. If any errors occur then the value of
 innodb_buffer_pool_load_status will be set accordingly, see buf_load_status().
 The dump filename can be specified by (relative to srv_data_home):
 SET GLOBAL innodb_buffer_pool_filename='filename'; */
static void buf_load() {
  char full_filename[OS_FILE_MAX_PATH];
  char now[32];
  FILE *f;
  buf_dump_t *dump;
  ulint dump_n;
  ulint total_buffer_pools_pages;
  ulint i;
  ulint space_id;
  ulint page_no;
  int fscanf_ret;

  /* Ignore any leftovers from before */
  buf_load_abort_flag = false;

  buf_dump_generate_path(full_filename, sizeof(full_filename));

  buf_load_status(STATUS_INFO, "Loading buffer pool(s) from %s", full_filename);

  f = fopen(full_filename, "r");
  if (f == nullptr) {
    buf_load_status(STATUS_ERR, "Cannot open '%s' for reading: %s",
                    full_filename, strerror(errno));
    return;
  }
  /* else */

  /* First scan the file to estimate how many entries are in it.
  This file is tiny (approx 500KB per 1GB buffer pool), reading it
  two times is fine. */
  dump_n = 0;
  while (fscanf(f, ULINTPF "," ULINTPF, &space_id, &page_no) == 2 &&
         !SHUTTING_DOWN()) {
    dump_n++;
  }

  if (!SHUTTING_DOWN() && !feof(f)) {
    /* fscanf() returned != 2 */
    const char *what;
    if (ferror(f)) {
      what = "reading";
    } else {
      what = "parsing";
    }
    fclose(f);
    buf_load_status(STATUS_ERR,
                    "Error %s '%s',"
                    " unable to load buffer pool (stage 1)",
                    what, full_filename);
    return;
  }

  /* If dump is larger than the buffer pool(s), then we ignore the
  extra trailing. This could happen if a dump is made, then buffer
  pool is shrunk and then load is attempted. */
  total_buffer_pools_pages = buf_pool_get_n_pages() * srv_buf_pool_instances;
  if (dump_n > total_buffer_pools_pages) {
    dump_n = total_buffer_pools_pages;
  }

  if (dump_n != 0) {
    dump = static_cast<buf_dump_t *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, dump_n * sizeof(*dump)));
  } else {
    fclose(f);
    ut_sprintf_timestamp(now);
    buf_load_status(STATUS_INFO,
                    "Buffer pool(s) load completed at %s"
                    " (%s was empty)",
                    now, full_filename);
    return;
  }

  if (dump == nullptr) {
    fclose(f);
    buf_load_status(STATUS_ERR, "Cannot allocate " ULINTPF " bytes: %s",
                    (ulint)(dump_n * sizeof(*dump)), strerror(errno));
    return;
  }

  rewind(f);

  for (i = 0; i < dump_n && !SHUTTING_DOWN(); i++) {
    fscanf_ret = fscanf(f, ULINTPF "," ULINTPF, &space_id, &page_no);

    if (fscanf_ret != 2) {
      if (feof(f)) {
        break;
      }
      /* else */

      ut::free(dump);
      fclose(f);
      buf_load_status(STATUS_ERR,
                      "Error parsing '%s', unable"
                      " to load buffer pool (stage 2)",
                      full_filename);
      return;
    }

    if (space_id > UINT32_MASK || page_no > UINT32_MASK) {
      ut::free(dump);
      fclose(f);
      buf_load_status(STATUS_ERR,
                      "Error parsing '%s': bogus"
                      " space,page " ULINTPF "," ULINTPF " at line " ULINTPF
                      ","
                      " unable to load buffer pool",
                      full_filename, space_id, page_no, i);
      return;
    }

    dump[i] = BUF_DUMP_CREATE(space_id, page_no);
  }

  /* Set dump_n to the actual number of initialized elements,
  i could be smaller than dump_n here if the file got truncated after
  we read it the first time. */
  dump_n = i;

  fclose(f);

  if (dump_n == 0) {
    ut::free(dump);
    ut_sprintf_timestamp(now);
    buf_load_status(STATUS_INFO,
                    "Buffer pool(s) load completed at %s"
                    " (%s was empty)",
                    now, full_filename);
    return;
  }

  if (!SHUTTING_DOWN()) {
    std::sort(dump, dump + dump_n);
  }

  std::chrono::steady_clock::time_point last_check_time;
  ulint last_activity_cnt = 0;

  /* Avoid calling the expensive fil_space_acquire_silent() for each
  page within the same tablespace. dump[] is sorted by (space, page),
  so all pages from a given tablespace are consecutive. */
  space_id_t cur_space_id = BUF_DUMP_SPACE(dump[0]);
  fil_space_t *space = fil_space_acquire_silent(cur_space_id);
  page_size_t page_size(space ? space->flags : 0);

#ifdef HAVE_PSI_STAGE_INTERFACE
  PSI_stage_progress *pfs_stage_progress =
      mysql_set_stage(srv_stage_buffer_pool_load.m_key);
#endif /* HAVE_PSI_STAGE_INTERFACE */

  mysql_stage_set_work_estimated(pfs_stage_progress, dump_n);
  mysql_stage_set_work_completed(pfs_stage_progress, 0);

  for (i = 0; i < dump_n && !SHUTTING_DOWN(); i++) {
    /* space_id for this iteration of the loop */
    const space_id_t this_space_id = BUF_DUMP_SPACE(dump[i]);

    if (this_space_id != cur_space_id) {
      if (space != nullptr) {
        fil_space_release(space);
      }

      cur_space_id = this_space_id;
      space = fil_space_acquire_silent(cur_space_id);

      if (space != nullptr) {
        const page_size_t cur_page_size(space->flags);
        page_size.copy_from(cur_page_size);
      }
    }

    if (space == nullptr) {
      continue;
    }

    buf_read_page_background(page_id_t(this_space_id, BUF_DUMP_PAGE(dump[i])),
                             page_size, true);

    if (i % 64 == 63) {
      os_aio_simulated_wake_handler_threads();
    }

    /* Update the progress every 32 MiB, which is every Nth page,
    where N = 32*1024^2 / page_size. */
    static const ulint update_status_every_n_mb = 32;
    static const ulint update_status_every_n_pages =
        update_status_every_n_mb * 1024 * 1024 / page_size.physical();

    if (i % update_status_every_n_pages == 0) {
      buf_load_status(STATUS_VERBOSE, "Loaded " ULINTPF "/" ULINTPF " pages",
                      i + 1, dump_n);
      mysql_stage_set_work_completed(pfs_stage_progress, i);
    }

    if (buf_load_abort_flag) {
      if (space != nullptr) {
        fil_space_release(space);
      }
      buf_load_abort_flag = false;
      ut::free(dump);
      buf_load_status(STATUS_INFO, "Buffer pool(s) load aborted on request");
      /* Premature end, set estimated = completed = i and
      end the current stage event. */
      mysql_stage_set_work_estimated(pfs_stage_progress, i);
      mysql_stage_set_work_completed(pfs_stage_progress, i);
#ifdef HAVE_PSI_STAGE_INTERFACE
      mysql_end_stage();
#endif /* HAVE_PSI_STAGE_INTERFACE */
      return;
    }

    buf_load_throttle_if_needed(&last_check_time, &last_activity_cnt, i);
  }

  if (space != nullptr) {
    fil_space_release(space);
  }

  ut::free(dump);

  ut_sprintf_timestamp(now);

  buf_load_status(STATUS_INFO, "Buffer pool(s) load completed at %s", now);

  /* Make sure that estimated = completed when we end. */
  mysql_stage_set_work_completed(pfs_stage_progress, dump_n);
  /* End the stage progress event. */
#ifdef HAVE_PSI_STAGE_INTERFACE
  mysql_end_stage();
#endif /* HAVE_PSI_STAGE_INTERFACE */
}

/** Aborts a currently running buffer pool load. This function is called by
 MySQL code via buffer_pool_load_abort() and it should return immediately
 because the whole MySQL is frozen during its execution. */
void buf_load_abort() { buf_load_abort_flag = true; }

/** This is the main thread for buffer pool dump/load. It waits for an
event and when waked up either performs a dump or load and sleeps
again. */
void buf_dump_thread() {
  ut_ad(!srv_read_only_mode);

  buf_dump_status(STATUS_VERBOSE, "Dumping of buffer pool not started");
  buf_load_status(STATUS_VERBOSE, "Loading of buffer pool not started");

  if (srv_buffer_pool_load_at_startup) {
    buf_load();
  }

  while (!SHUTTING_DOWN()) {
    os_event_wait(srv_buf_dump_event);

    if (buf_dump_should_start) {
      buf_dump_should_start = false;
      buf_dump(true /* quit on shutdown */);
    }

    if (buf_load_should_start) {
      buf_load_should_start = false;
      buf_load();
    }

    os_event_reset(srv_buf_dump_event);
  }

  if (srv_buffer_pool_dump_at_shutdown && srv_fast_shutdown != 2) {
    buf_dump(false /* ignore shutdown down flag,
                keep going even if we are in a shutdown state */);
  }
}
