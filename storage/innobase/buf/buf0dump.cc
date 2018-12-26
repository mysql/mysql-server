/*****************************************************************************

Copyright (c) 2011, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file buf/buf0dump.cc
Implements a buffer pool dump/load.

Created April 08, 2011 Vasil Dimov
*******************************************************/

#include "my_global.h"
#include "my_sys.h"
#include "my_thread.h"

#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/psi.h"

#include "univ.i"

#include "buf0buf.h"
#include "buf0dump.h"
#include "dict0dict.h"
#include "os0file.h"
#include "os0thread.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "sync0rw.h"
#include "ut0byte.h"

#include <algorithm>

enum status_severity {
	STATUS_VERBOSE,
	STATUS_INFO,
	STATUS_ERR
};

#define SHUTTING_DOWN()	(srv_shutdown_state != SRV_SHUTDOWN_NONE)

/* Flags that tell the buffer pool dump/load thread which action should it
take after being waked up. */
static ibool	buf_dump_should_start = FALSE;
static ibool	buf_load_should_start = FALSE;

static ibool	buf_load_abort_flag = FALSE;

/* Used to temporary store dump info in order to avoid IO while holding
buffer pool mutex during dump and also to sort the contents of the dump
before reading the pages from disk during load.
We store the space id in the high 32 bits and page no in low 32 bits. */
typedef ib_uint64_t	buf_dump_t;

/* Aux macros to create buf_dump_t and to extract space and page from it */
#define BUF_DUMP_CREATE(space, page)	ut_ull_create(space, page)
#define BUF_DUMP_SPACE(a)		((ulint) ((a) >> 32))
#define BUF_DUMP_PAGE(a)		((ulint) ((a) & 0xFFFFFFFFUL))

/*****************************************************************//**
Wakes up the buffer pool dump/load thread and instructs it to start
a dump. This function is called by MySQL code via buffer_pool_dump_now()
and it should return immediately because the whole MySQL is frozen during
its execution. */
void
buf_dump_start()
/*============*/
{
	buf_dump_should_start = TRUE;
	os_event_set(srv_buf_dump_event);
}

/*****************************************************************//**
Wakes up the buffer pool dump/load thread and instructs it to start
a load. This function is called by MySQL code via buffer_pool_load_now()
and it should return immediately because the whole MySQL is frozen during
its execution. */
void
buf_load_start()
/*============*/
{
	buf_load_should_start = TRUE;
	os_event_set(srv_buf_dump_event);
}

/*****************************************************************//**
Sets the global variable that feeds MySQL's innodb_buffer_pool_dump_status
to the specified string. The format and the following parameters are the
same as the ones used for printf(3). The value of this variable can be
retrieved by:
SELECT variable_value FROM information_schema.global_status WHERE
variable_name = 'INNODB_BUFFER_POOL_DUMP_STATUS';
or by:
SHOW STATUS LIKE 'innodb_buffer_pool_dump_status'; */
static MY_ATTRIBUTE((nonnull, format(printf, 2, 3)))
void
buf_dump_status(
/*============*/
	enum status_severity	severity,/*!< in: status severity */
	const char*		fmt,	/*!< in: format */
	...)				/*!< in: extra parameters according
					to fmt */
{
	va_list	ap;

	va_start(ap, fmt);

	ut_vsnprintf(
		export_vars.innodb_buffer_pool_dump_status,
		sizeof(export_vars.innodb_buffer_pool_dump_status),
		fmt, ap);

	switch (severity) {
	case STATUS_INFO:
		ib::info() << export_vars.innodb_buffer_pool_dump_status;
		break;

	case STATUS_ERR:
		ib::error() << export_vars.innodb_buffer_pool_dump_status;
		break;

	case STATUS_VERBOSE:
		break;
	}

	va_end(ap);
}

/*****************************************************************//**
Sets the global variable that feeds MySQL's innodb_buffer_pool_load_status
to the specified string. The format and the following parameters are the
same as the ones used for printf(3). The value of this variable can be
retrieved by:
SELECT variable_value FROM information_schema.global_status WHERE
variable_name = 'INNODB_BUFFER_POOL_LOAD_STATUS';
or by:
SHOW STATUS LIKE 'innodb_buffer_pool_load_status'; */
static MY_ATTRIBUTE((nonnull, format(printf, 2, 3)))
void
buf_load_status(
/*============*/
	enum status_severity	severity,/*!< in: status severity */
	const char*	fmt,	/*!< in: format */
	...)			/*!< in: extra parameters according to fmt */
{
	va_list	ap;

	va_start(ap, fmt);

	ut_vsnprintf(
		export_vars.innodb_buffer_pool_load_status,
		sizeof(export_vars.innodb_buffer_pool_load_status),
		fmt, ap);

	switch (severity) {
	case STATUS_INFO:
		ib::info() << export_vars.innodb_buffer_pool_load_status;
		break;

	case STATUS_ERR:
		ib::error() << export_vars.innodb_buffer_pool_load_status;
		break;

	case STATUS_VERBOSE:
		break;
	}

	va_end(ap);
}

/** Returns the directory path where the buffer pool dump file will be created.
@return directory path */
static
const char*
get_buf_dump_dir()
{
	const char*	dump_dir;

	/* The dump file should be created in the default data directory if
	innodb_data_home_dir is set as an empty string. */
	if (strcmp(srv_data_home, "") == 0) {
		dump_dir = fil_path_to_mysql_datadir;
	} else {
		dump_dir = srv_data_home;
	}

	return(dump_dir);
}

/** Generate the path to the buffer pool dump/load file.
@param[out]	path		generated path
@param[in]	path_size	size of 'path', used as in snprintf(3). */
static
void
buf_dump_generate_path(
	char*	path,
	size_t	path_size)
{
	char	buf[FN_REFLEN];

	ut_snprintf(buf, sizeof(buf), "%s%c%s", get_buf_dump_dir(),
		    OS_PATH_SEPARATOR, srv_buf_dump_filename);

	os_file_type_t	type;
	bool		exists = false;
	bool		ret;

	ret = os_file_status(buf, &exists, &type);

	/* For realpath() to succeed the file must exist. */

	if (ret && exists) {
		/* my_realpath() assumes the destination buffer is big enough
		to hold FN_REFLEN bytes. */
		ut_a(path_size >= FN_REFLEN);

		my_realpath(path, buf, 0);
	} else {
		/* If it does not exist, then resolve only srv_data_home
		and append srv_buf_dump_filename to it. */
		char	srv_data_home_full[FN_REFLEN];

		my_realpath(srv_data_home_full, get_buf_dump_dir(), 0);

		if (srv_data_home_full[strlen(srv_data_home_full) - 1]
		    == OS_PATH_SEPARATOR) {

			ut_snprintf(path, path_size, "%s%s",
				    srv_data_home_full,
				    srv_buf_dump_filename);
		} else {
			ut_snprintf(path, path_size, "%s%c%s",
				    srv_data_home_full,
				    OS_PATH_SEPARATOR,
				    srv_buf_dump_filename);
		}
	}
}

/*****************************************************************//**
Perform a buffer pool dump into the file specified by
innodb_buffer_pool_filename. If any errors occur then the value of
innodb_buffer_pool_dump_status will be set accordingly, see buf_dump_status().
The dump filename can be specified by (relative to srv_data_home):
SET GLOBAL innodb_buffer_pool_filename='filename'; */
static
void
buf_dump(
/*=====*/
	ibool	obey_shutdown)	/*!< in: quit if we are in a shutting down
				state */
{
#define SHOULD_QUIT()	(SHUTTING_DOWN() && obey_shutdown)

	char	full_filename[OS_FILE_MAX_PATH];
	char	tmp_filename[OS_FILE_MAX_PATH + 11];
	char	now[32];
	FILE*	f;
	ulint	i;
	int	ret;

	buf_dump_generate_path(full_filename, sizeof(full_filename));

	ut_snprintf(tmp_filename, sizeof(tmp_filename),
		    "%s.incomplete", full_filename);

	buf_dump_status(STATUS_INFO, "Dumping buffer pool(s) to %s",
			full_filename);

	f = fopen(tmp_filename, "w");
	if (f == NULL) {
		buf_dump_status(STATUS_ERR,
				"Cannot open '%s' for writing: %s",
				tmp_filename, strerror(errno));
		return;
	}
	/* else */

	/* walk through each buffer pool */
	for (i = 0; i < srv_buf_pool_instances && !SHOULD_QUIT(); i++) {
		buf_pool_t*		buf_pool;
		const buf_page_t*	bpage;
		buf_dump_t*		dump;
		ulint			n_pages;
		ulint			j;

		buf_pool = buf_pool_from_array(i);

		/* obtain buf_pool mutex before allocate, since
		UT_LIST_GET_LEN(buf_pool->LRU) could change */
		buf_pool_mutex_enter(buf_pool);

		n_pages = UT_LIST_GET_LEN(buf_pool->LRU);

		/* skip empty buffer pools */
		if (n_pages == 0) {
			buf_pool_mutex_exit(buf_pool);
			continue;
		}

		if (srv_buf_pool_dump_pct != 100) {
			ut_ad(srv_buf_pool_dump_pct < 100);

			n_pages = n_pages * srv_buf_pool_dump_pct / 100;

			if (n_pages == 0) {
				n_pages = 1;
			}
		}

		dump = static_cast<buf_dump_t*>(ut_malloc_nokey(
				n_pages * sizeof(*dump)));

		if (dump == NULL) {
			buf_pool_mutex_exit(buf_pool);
			fclose(f);
			buf_dump_status(STATUS_ERR,
					"Cannot allocate " ULINTPF " bytes: %s",
					(ulint) (n_pages * sizeof(*dump)),
					strerror(errno));
			/* leave tmp_filename to exist */
			return;
		}

		for (bpage = UT_LIST_GET_FIRST(buf_pool->LRU), j = 0;
		     bpage != NULL && j < n_pages;
		     bpage = UT_LIST_GET_NEXT(LRU, bpage), j++) {

			ut_a(buf_page_in_file(bpage));

			dump[j] = BUF_DUMP_CREATE(bpage->id.space(),
						  bpage->id.page_no());
		}

		ut_a(j == n_pages);

		buf_pool_mutex_exit(buf_pool);

		for (j = 0; j < n_pages && !SHOULD_QUIT(); j++) {
			ret = fprintf(f, ULINTPF "," ULINTPF "\n",
				      BUF_DUMP_SPACE(dump[j]),
				      BUF_DUMP_PAGE(dump[j]));
			if (ret < 0) {
				ut_free(dump);
				fclose(f);
				buf_dump_status(STATUS_ERR,
						"Cannot write to '%s': %s",
						tmp_filename, strerror(errno));
				/* leave tmp_filename to exist */
				return;
			}

			if (j % 128 == 0) {
				buf_dump_status(
					STATUS_VERBOSE,
					"Dumping buffer pool"
					" " ULINTPF "/" ULINTPF ","
					" page " ULINTPF "/" ULINTPF,
					i + 1, srv_buf_pool_instances,
					j + 1, n_pages);
			}
		}

		ut_free(dump);
	}

	ret = fclose(f);
	if (ret != 0) {
		buf_dump_status(STATUS_ERR,
				"Cannot close '%s': %s",
				tmp_filename, strerror(errno));
		return;
	}
	/* else */

	ret = unlink(full_filename);
	if (ret != 0 && errno != ENOENT) {
		buf_dump_status(STATUS_ERR,
				"Cannot delete '%s': %s",
				full_filename, strerror(errno));
		/* leave tmp_filename to exist */
		return;
	}
	/* else */

	ret = rename(tmp_filename, full_filename);
	if (ret != 0) {
		buf_dump_status(STATUS_ERR,
				"Cannot rename '%s' to '%s': %s",
				tmp_filename, full_filename,
				strerror(errno));
		/* leave tmp_filename to exist */
		return;
	}
	/* else */

	/* success */

	ut_sprintf_timestamp(now);

	buf_dump_status(STATUS_INFO,
			"Buffer pool(s) dump completed at %s", now);
}

/*****************************************************************//**
Artificially delay the buffer pool loading if necessary. The idea of
this function is to prevent hogging the server with IO and slowing down
too much normal client queries. */
UNIV_INLINE
void
buf_load_throttle_if_needed(
/*========================*/
	ulint*	last_check_time,	/*!< in/out: milliseconds since epoch
					of the last time we did check if
					throttling is needed, we do the check
					every srv_io_capacity IO ops. */
	ulint*	last_activity_count,
	ulint	n_io)			/*!< in: number of IO ops done since
					buffer pool load has started */
{
	if (n_io % srv_io_capacity < srv_io_capacity - 1) {
		return;
	}

	if (*last_check_time == 0 || *last_activity_count == 0) {
		*last_check_time = ut_time_ms();
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

	ulint	now = ut_time_ms();
	ulint	elapsed_time = now - *last_check_time;

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
	ut_time_ms() that often may turn out to be too expensive. */

	if (elapsed_time < 1000 /* 1 sec (1000 milli secs) */) {
		os_thread_sleep((1000 - elapsed_time) * 1000 /* micro secs */);
	}

	*last_check_time = ut_time_ms();
	*last_activity_count = srv_get_activity_count();
}

/*****************************************************************//**
Perform a buffer pool load from the file specified by
innodb_buffer_pool_filename. If any errors occur then the value of
innodb_buffer_pool_load_status will be set accordingly, see buf_load_status().
The dump filename can be specified by (relative to srv_data_home):
SET GLOBAL innodb_buffer_pool_filename='filename'; */
static
void
buf_load()
/*======*/
{
	char		full_filename[OS_FILE_MAX_PATH];
	char		now[32];
	FILE*		f;
	buf_dump_t*	dump;
	ulint		dump_n;
	ulint		total_buffer_pools_pages;
	ulint		i;
	ulint		space_id;
	ulint		page_no;
	int		fscanf_ret;

	/* Ignore any leftovers from before */
	buf_load_abort_flag = FALSE;

	buf_dump_generate_path(full_filename, sizeof(full_filename));

	buf_load_status(STATUS_INFO,
			"Loading buffer pool(s) from %s", full_filename);

	f = fopen(full_filename, "r");
	if (f == NULL) {
		buf_load_status(STATUS_ERR,
				"Cannot open '%s' for reading: %s",
				full_filename, strerror(errno));
		return;
	}
	/* else */

	/* First scan the file to estimate how many entries are in it.
	This file is tiny (approx 500KB per 1GB buffer pool), reading it
	two times is fine. */
	dump_n = 0;
	while (fscanf(f, ULINTPF "," ULINTPF, &space_id, &page_no) == 2
	       && !SHUTTING_DOWN()) {
		dump_n++;
	}

	if (!SHUTTING_DOWN() && !feof(f)) {
		/* fscanf() returned != 2 */
		const char*	what;
		if (ferror(f)) {
			what = "reading";
		} else {
			what = "parsing";
		}
		fclose(f);
		buf_load_status(STATUS_ERR, "Error %s '%s',"
				" unable to load buffer pool (stage 1)",
				what, full_filename);
		return;
	}

	/* If dump is larger than the buffer pool(s), then we ignore the
	extra trailing. This could happen if a dump is made, then buffer
	pool is shrunk and then load is attempted. */
	total_buffer_pools_pages = buf_pool_get_n_pages()
		* srv_buf_pool_instances;
	if (dump_n > total_buffer_pools_pages) {
		dump_n = total_buffer_pools_pages;
	}

	if(dump_n != 0) {
		dump = static_cast<buf_dump_t*>(ut_malloc_nokey(
				dump_n * sizeof(*dump)));
	} else {
		fclose(f);
		ut_sprintf_timestamp(now);
		buf_load_status(STATUS_INFO,
				"Buffer pool(s) load completed at %s"
				" (%s was empty)", now, full_filename);
		return;
	}

	if (dump == NULL) {
		fclose(f);
		buf_load_status(STATUS_ERR,
				"Cannot allocate " ULINTPF " bytes: %s",
				(ulint) (dump_n * sizeof(*dump)),
				strerror(errno));
		return;
	}

	rewind(f);

	for (i = 0; i < dump_n && !SHUTTING_DOWN(); i++) {
		fscanf_ret = fscanf(f, ULINTPF "," ULINTPF,
				    &space_id, &page_no);

		if (fscanf_ret != 2) {
			if (feof(f)) {
				break;
			}
			/* else */

			ut_free(dump);
			fclose(f);
			buf_load_status(STATUS_ERR,
					"Error parsing '%s', unable"
					" to load buffer pool (stage 2)",
					full_filename);
			return;
		}

		if (space_id > ULINT32_MASK || page_no > ULINT32_MASK) {
			ut_free(dump);
			fclose(f);
			buf_load_status(STATUS_ERR,
					"Error parsing '%s': bogus"
					" space,page " ULINTPF "," ULINTPF
					" at line " ULINTPF ","
					" unable to load buffer pool",
					full_filename,
					space_id, page_no,
					i);
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
		ut_free(dump);
		ut_sprintf_timestamp(now);
		buf_load_status(STATUS_INFO,
				"Buffer pool(s) load completed at %s"
				" (%s was empty)", now, full_filename);
		return;
	}

	if (!SHUTTING_DOWN()) {
		std::sort(dump, dump + dump_n);
	}

	ulint		last_check_time = 0;
	ulint		last_activity_cnt = 0;

	/* Avoid calling the expensive fil_space_acquire_silent() for each
	page within the same tablespace. dump[] is sorted by (space, page),
	so all pages from a given tablespace are consecutive. */
	ulint		cur_space_id = BUF_DUMP_SPACE(dump[0]);
	fil_space_t*	space = fil_space_acquire_silent(cur_space_id);
	page_size_t	page_size(space ? space->flags : 0);

#ifdef HAVE_PSI_STAGE_INTERFACE
	PSI_stage_progress*	pfs_stage_progress
		= mysql_set_stage(srv_stage_buffer_pool_load.m_key);
#endif /* HAVE_PSI_STAGE_INTERFACE */

	mysql_stage_set_work_estimated(pfs_stage_progress, dump_n);
	mysql_stage_set_work_completed(pfs_stage_progress, 0);

	for (i = 0; i < dump_n && !SHUTTING_DOWN(); i++) {

		/* space_id for this iteration of the loop */
		const ulint	this_space_id = BUF_DUMP_SPACE(dump[i]);

		if (this_space_id != cur_space_id) {
			if (space != NULL) {
				fil_space_release(space);
			}

			cur_space_id = this_space_id;
			space = fil_space_acquire_silent(cur_space_id);

			if (space != NULL) {
				const page_size_t	cur_page_size(
					space->flags);
				page_size.copy_from(cur_page_size);
			}
		}

		if (space == NULL) {
			continue;
		}

		buf_read_page_background(
			page_id_t(this_space_id, BUF_DUMP_PAGE(dump[i])),
			page_size, true);

		if (i % 64 == 63) {
			os_aio_simulated_wake_handler_threads();
		}

		/* Update the progress every 32 MiB, which is every Nth page,
		where N = 32*1024^2 / page_size. */
		static const ulint	update_status_every_n_mb = 32;
		static const ulint	update_status_every_n_pages
			= update_status_every_n_mb * 1024 * 1024
			/ page_size.physical();

		if (i % update_status_every_n_pages == 0) {
			buf_load_status(STATUS_VERBOSE,
					"Loaded " ULINTPF "/" ULINTPF " pages",
					i + 1, dump_n);
			mysql_stage_set_work_completed(pfs_stage_progress, i);
		}

		if (buf_load_abort_flag) {
			if (space != NULL) {
				fil_space_release(space);
			}
			buf_load_abort_flag = FALSE;
			ut_free(dump);
			buf_load_status(
				STATUS_INFO,
				"Buffer pool(s) load aborted on request");
			/* Premature end, set estimated = completed = i and
			end the current stage event. */
			mysql_stage_set_work_estimated(pfs_stage_progress, i);
			mysql_stage_set_work_completed(pfs_stage_progress, i);
#ifdef HAVE_PSI_STAGE_INTERFACE
			mysql_end_stage();
#endif /* HAVE_PSI_STAGE_INTERFACE */
			return;
		}

		buf_load_throttle_if_needed(
			&last_check_time, &last_activity_cnt, i);
	}

	if (space != NULL) {
		fil_space_release(space);
	}

	ut_free(dump);

	ut_sprintf_timestamp(now);

	buf_load_status(STATUS_INFO,
			"Buffer pool(s) load completed at %s", now);

	/* Make sure that estimated = completed when we end. */
	mysql_stage_set_work_completed(pfs_stage_progress, dump_n);
	/* End the stage progress event. */
#ifdef HAVE_PSI_STAGE_INTERFACE
	mysql_end_stage();
#endif /* HAVE_PSI_STAGE_INTERFACE */
}

/*****************************************************************//**
Aborts a currently running buffer pool load. This function is called by
MySQL code via buffer_pool_load_abort() and it should return immediately
because the whole MySQL is frozen during its execution. */
void
buf_load_abort()
/*============*/
{
	buf_load_abort_flag = TRUE;
}

/*****************************************************************//**
This is the main thread for buffer pool dump/load. It waits for an
event and when waked up either performs a dump or load and sleeps
again.
@return this function does not return, it calls os_thread_exit() */
extern "C"
os_thread_ret_t
DECLARE_THREAD(buf_dump_thread)(
/*============================*/
	void*	arg MY_ATTRIBUTE((unused)))	/*!< in: a dummy parameter
						required by os_thread_create */
{
	my_thread_init();
	ut_ad(!srv_read_only_mode);

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(buf_dump_thread_key);
#endif /* UNIV_PFS_THREAD */

	srv_buf_dump_thread_active = TRUE;

	buf_dump_status(STATUS_VERBOSE, "Dumping of buffer pool not started");
	buf_load_status(STATUS_VERBOSE, "Loading of buffer pool not started");

	if (srv_buffer_pool_load_at_startup) {
		buf_load();
	}

	while (!SHUTTING_DOWN()) {

		os_event_wait(srv_buf_dump_event);

		if (buf_dump_should_start) {
			buf_dump_should_start = FALSE;
			buf_dump(TRUE /* quit on shutdown */);
		}

		if (buf_load_should_start) {
			buf_load_should_start = FALSE;
			buf_load();
		}

		os_event_reset(srv_buf_dump_event);
	}

	if (srv_buffer_pool_dump_at_shutdown && srv_fast_shutdown != 2) {
		buf_dump(FALSE /* ignore shutdown down flag,
		keep going even if we are in a shutdown state */);
	}

	srv_buf_dump_thread_active = FALSE;

	my_thread_end();
	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;
}
