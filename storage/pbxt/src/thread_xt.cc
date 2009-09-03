/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-03	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#ifndef XT_WIN
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "xt_defs.h"
#include "strutil_xt.h"
#include "pthread_xt.h"
#include "thread_xt.h"
#include "memory_xt.h"
#include "sortedlist_xt.h"
#include "trace_xt.h"
#include "myxt_xt.h"
#include "database_xt.h"

void xt_db_init_thread(XTThreadPtr self, XTThreadPtr new_thread);
void xt_db_exit_thread(XTThreadPtr self);

static void thr_accumulate_statistics(XTThreadPtr self);

/*
 * -----------------------------------------------------------------------
 * THREAD GLOBALS
 */

xtPublic u_int			xt_thr_maximum_threads;
xtPublic u_int			xt_thr_current_thread_count;
xtPublic u_int			xt_thr_current_max_threads;

/* This structure is a double linked list of thread, with a wait
 * condition on it.
 */
static XTLinkedListPtr	thr_list;

/* This structure maps thread ID's to thread pointers. */
xtPublic XTThreadPtr	*xt_thr_array;
static xt_mutex_type	thr_array_lock;

/* Global accumulated statistics: */
static XTStatisticsRec	thr_statistics;

/*
 * -----------------------------------------------------------------------
 * Error logging
 */

static xt_mutex_type	log_mutex;
static int				log_level = 0;
static FILE				*log_file = NULL;
static xtBool			log_newline = TRUE;

xtPublic xtBool xt_init_logging(void)
{
	int err;

	log_file = stdout;
	log_level = XT_LOG_TRACE;
	err = xt_p_mutex_init_with_autoname(&log_mutex, NULL);
	if (err) {
		xt_log_errno(XT_NS_CONTEXT, err);
		log_file = NULL;
		log_level = 0;
		return FALSE;
	}
	if (!xt_init_trace()) {
		xt_exit_logging();
		return FALSE;
	}
	return TRUE;
}

xtPublic void xt_exit_logging(void)
{
	if (log_file) {
		xt_free_mutex(&log_mutex);
		log_file = NULL;
	}
	xt_exit_trace();
}

xtPublic void xt_get_now(char *buffer, size_t len)
{
	time_t		ticks;
	struct tm	ltime;

	ticks = time(NULL);
	if (ticks == (time_t) -1) {
#ifdef XT_WIN
		printf(buffer, "** error %d getting time **", errno);
#else
		snprintf(buffer, len, "** error %d getting time **", errno);
#endif
		return;
	}
	localtime_r(&ticks, &ltime);
	strftime(buffer, len, "%y%m%d %H:%M:%S", &ltime);
}

static void thr_log_newline(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level)
{
	c_char	*level_str;
	char	time_str[200];
	char	thr_name[XT_THR_NAME_SIZE+3];

	xt_get_now(time_str, 200);
	if (self && *self->t_name) {
		xt_strcpy(XT_THR_NAME_SIZE+3, thr_name, " ");
		xt_strcat(XT_THR_NAME_SIZE+3, thr_name, self->t_name);
	}
	else
		thr_name[0] = 0;
	switch (level) {
		case XT_LOG_FATAL: level_str = " [Fatal]"; break;
		case XT_LOG_ERROR: level_str = " [Error]"; break;
		case XT_LOG_WARNING: level_str = " [Warning]"; break;
		case XT_LOG_INFO: level_str = " [Note]"; break;
		case XT_LOG_TRACE: level_str = " [Trace]"; break;
		default: level_str = " "; break;
	}
	if (func && *func && *func != '-') {
		char func_name[XT_MAX_FUNC_NAME_SIZE];

		xt_strcpy_term(XT_MAX_FUNC_NAME_SIZE, func_name, func, '(');
		if (file && *file)
			fprintf(log_file, "%s%s%s %s(%s:%d) ", time_str, level_str, thr_name, func_name, xt_last_name_of_path(file), line);
		else
			fprintf(log_file, "%s%s%s %s() ", time_str, level_str, thr_name, func_name);
	}
	else {
		if (file && *file)
			fprintf(log_file, "%s%s%s [%s:%d] ", time_str, level_str, thr_name, xt_last_name_of_path(file), line);
		else
			fprintf(log_file, "%s%s%s ", time_str, level_str, thr_name);
	}
}

#ifdef XT_WIN
/* Windows uses printf()!! */
#define DEFAULT_LOG_BUFFER_SIZE			2000
#else
#ifdef DEBUG
#define DEFAULT_LOG_BUFFER_SIZE			10
#else
#define DEFAULT_LOG_BUFFER_SIZE			2000
#endif
#endif

void xt_log_flush(XTThreadPtr XT_UNUSED(self))
{
	fflush(log_file);
}

/*
 * Log the given formated string information to the log file.
 * Before each new line, this function writes the
 * log header, which includes the time, log level,
 * and source file and line number (optional).
 */
static void thr_log_va(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, c_char *fmt, va_list ap)
{
	char buffer[DEFAULT_LOG_BUFFER_SIZE];
	char *log_string = NULL;

	if (level > log_level)
		return;

	xt_lock_mutex_ns(&log_mutex);

#ifdef XT_WIN
	vsprintf(buffer, fmt, ap);
	log_string = buffer;
#else
#if !defined(va_copy) || defined(XT_SOLARIS)
	int len;

	len = vsnprintf(buffer, DEFAULT_LOG_BUFFER_SIZE-1, fmt, ap);
	if (len > DEFAULT_LOG_BUFFER_SIZE-1)
		len = DEFAULT_LOG_BUFFER_SIZE-1;
	buffer[len] = 0;
	log_string = buffer;
#else
	/* Use the buffer, unless it is too small */
	va_list ap2;

	va_copy(ap2, ap);
	if (vsnprintf(buffer, DEFAULT_LOG_BUFFER_SIZE, fmt, ap) >= DEFAULT_LOG_BUFFER_SIZE) {
		if (vasprintf(&log_string, fmt, ap2) == -1)
			log_string = NULL;
	}
	else
		log_string = buffer;
#endif
#endif

	if (log_string) {
		char *str, *str_end, tmp_ch;

		str = log_string;
		while (*str) {
			if (log_newline) {
				thr_log_newline(self, func, file, line, level);
				log_newline = FALSE;
			}
			str_end = strchr(str, '\n');
			if (str_end) {
				str_end++;
				tmp_ch = *str_end;
				*str_end = 0;
				log_newline = TRUE;
			}
			else {
				str_end = str + strlen(str);
				tmp_ch = 0;
			}
			fprintf(log_file, "%s", str);
			fflush(log_file);
			*str_end = tmp_ch;
			str = str_end;
		}

		if (log_string != buffer)
			free(log_string);
	}

	xt_unlock_mutex_ns(&log_mutex);
}

xtPublic void xt_logf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, c_char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	thr_log_va(self, func, file, line, level, fmt, ap);
	va_end(ap);
}

xtPublic void xt_log(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, c_char *string)
{
	xt_logf(self, func, file, line, level, "%s", string);
}

static int thr_log_error_va(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, int xt_err, int sys_err, c_char *fmt, va_list ap)
{
	int		default_level;
	char	xt_err_string[50];

	*xt_err_string = 0;
	switch (xt_err) {
		case XT_ASSERTION_FAILURE:
			strcpy(xt_err_string, "Assertion");
			default_level = XT_LOG_FATAL;
			break;
		case XT_SYSTEM_ERROR:
			strcpy(xt_err_string, "errno");
			default_level = XT_LOG_ERROR;
			break;
		case XT_SIGNAL_CAUGHT:
			strcpy(xt_err_string, "Signal");
			default_level = XT_LOG_ERROR;
			break;
		default:
			sprintf(xt_err_string, "%d", xt_err);
			default_level = XT_LOG_ERROR;
			break;
	}
	if (level == XT_LOG_DEFAULT)
		level = default_level;

	if (*xt_err_string) {
		if (sys_err)
			xt_logf(self, func, file, line, level, "%s (%d): ", xt_err_string, sys_err);
		else
			xt_logf(self, func, file, line, level, "%s: ", xt_err_string);
	}
	thr_log_va(self, func, file, line, level, fmt, ap);
	xt_logf(self, func, file, line, level, "\n");
	return level;
}

/* The function returns the actual log level used. */
xtPublic int xt_log_errorf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, int xt_err, int sys_err, c_char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	level = thr_log_error_va(self, func, file, line, level, xt_err, sys_err, fmt, ap);
	va_end(ap);
	return level;
}

/* The function returns the actual log level used. */
xtPublic int xt_log_error(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, int xt_err, int sys_err, c_char *string)
{
	return xt_log_errorf(self, func, file, line, level, xt_err, sys_err, "%s", string);
}

xtPublic void xt_log_exception(XTThreadPtr self, XTExceptionPtr e, int level)
{
	level = xt_log_error(
		self,
		e->e_func_name,
		e->e_source_file,
		e->e_source_line,
		level,
		e->e_xt_err,
		e->e_sys_err,
		e->e_err_msg);
	/* Dump the catch trace: */
	if (*e->e_catch_trace)
		xt_logf(self, NULL, NULL, 0, level, "%s", e->e_catch_trace);
}

xtPublic void xt_log_and_clear_exception(XTThreadPtr self)
{
	xt_log_exception(self, &self->t_exception, XT_LOG_DEFAULT);
	xt_clear_exception(self);
}

xtPublic void xt_log_and_clear_exception_ns(void)
{
	xt_log_and_clear_exception(xt_get_self());
}

xtPublic void xt_log_and_clear_warning(XTThreadPtr self)
{
	xt_log_exception(self, &self->t_exception, XT_LOG_WARNING);
	xt_clear_exception(self);
}

xtPublic void xt_log_and_clear_warning_ns(void)
{
	xt_log_and_clear_warning(xt_get_self());
}

/*
 * -----------------------------------------------------------------------
 * Exceptions
 */

static void thr_add_catch_trace(XTExceptionPtr e, c_char *func, c_char *file, u_int line)
{
	if (func && *func && *func != '-') {
		xt_strcat_term(XT_CATCH_TRACE_SIZE, e->e_catch_trace, func, '(');
		xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, "(");
	}
	if (file && *file) {
		xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, xt_last_name_of_path(file));
		if (line) {
			char buffer[40];

			sprintf(buffer, "%u", line);
			xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, ":");
			xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, buffer);
		}
	}
	if (func && *func && *func != '-')
		xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, ")");
	xt_strcat(XT_CATCH_TRACE_SIZE, e->e_catch_trace, "\n");
}

static void thr_save_error_va(XTExceptionPtr e, XTThreadPtr self, xtBool throw_it, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, va_list ap)
{
	int i;

	if (!e)
		return;

	e->e_xt_err = xt_err;
	e->e_sys_err = sys_err;
	vsnprintf(e->e_err_msg, XT_ERR_MSG_SIZE, fmt, ap);

	/* Make the first character of the message upper case: */
	if (isalpha(e->e_err_msg[0]) && islower(e->e_err_msg[0]))
		e->e_err_msg[0] = (char) toupper(e->e_err_msg[0]);

	if (func && *func && *func != '-')
		xt_strcpy_term(XT_MAX_FUNC_NAME_SIZE, e->e_func_name, func, '(');
	else
		*e->e_func_name = 0;
	if (file && *file) {
		xt_strcpy(XT_SOURCE_FILE_NAME_SIZE, e->e_source_file, xt_last_name_of_path(file));
		e->e_source_line = line;
	}
	else {
		*e->e_source_file = 0;
		e->e_source_line = 0;
	}
	*e->e_catch_trace = 0;

	if (!self)
		return;

	/* Create a stack trace for this exception: */
	thr_add_catch_trace(e, func, file, line);
	for (i=self->t_call_top-1; i>=0; i--)
		thr_add_catch_trace(e, self->t_call_stack[i].cs_func, self->t_call_stack[i].cs_file, self->t_call_stack[i].cs_line);

	if (throw_it)
		xt_throw(self);
}

/*
 * -----------------------------------------------------------------------
 * THROWING EXCEPTIONS
 */

/* If we have to allocate resources and the hold them temporarily during which
 * time an exception could occur, then these functions provide a holding
 * place for the data, which will be freed in the case of an exception.
 *
 * Note: the free functions could themselves allocated resources.
 * to make sure all things work out we only remove the resource from
 * then stack when it is freed.
 */
static void thr_free_resources(XTThreadPtr self, XTResourcePtr top)
{
	XTResourcePtr		rp;
	XTThreadFreeFunc	free_func;

	if (!top)
		return;
	while (self->t_res_top > top) {
		/* Pop the top resource: */
		rp = (XTResourcePtr) (((char *) self->t_res_top) - self->t_res_top->r_prev_size);

		/* Free the resource: */
		if (rp->r_free_func) {
			free_func = rp->r_free_func;
			rp->r_free_func = NULL;
			free_func(self, rp->r_data);
		}

		self->t_res_top = rp;
	}
}

xtPublic void xt_bug(XTThreadPtr XT_UNUSED(self))
{
	static int *bug_ptr = NULL;
	
	bug_ptr = NULL;
}

/*
 * This function is called when an exception is caught.
 * It restores the function call top and frees
 * any resource allocated by lower levels.
 */
xtPublic void xt_caught(XTThreadPtr self)
{
	/* Restore the call top: */
	self->t_call_top = self->t_jmp_env[self->t_jmp_depth].jb_call_top;

	/* Free the temporary data that would otherwize be lost
	 * This should do nothing, because we actually free things on throw
	 * (se below).
	 */
	thr_free_resources(self, self->t_jmp_env[self->t_jmp_depth].jb_res_top);
}

/* Throw an already registered error: */
xtPublic void xt_throw(XTThreadPtr self)
{
	if (self) {
		ASSERT_NS(self->t_exception.e_xt_err);
		if (self->t_jmp_depth > 0 && self->t_jmp_depth <= XT_MAX_JMP) {
			/* As recommended by Barry: rree the resources before the stack is invalid! */
			thr_free_resources(self, self->t_jmp_env[self->t_jmp_depth-1].jb_res_top);

			/* Then do the longjmp: */
			longjmp(self->t_jmp_env[self->t_jmp_depth-1].jb_buffer, 1);
		}
	}

	/*
	 * We cannot throw an error, because it will not be caught.
	 * This means there is no try ... catch block above.
	 * In this case, we just return.
	 * The calling functions must handle errors...
	xt_caught(self);
	xt_log(XT_CONTEXT, XT_LOG_FATAL, "Uncaught exception\n");
	xt_exit_thread(self, NULL);
	*/
}

xtPublic void xt_throwf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, ...)
{
	va_list		ap;
	XTThreadPtr	thread = self ? self : xt_get_self();

	va_start(ap, fmt);
	thr_save_error_va(thread ? &thread->t_exception : NULL, thread, self ? TRUE : FALSE, func, file, line, xt_err, sys_err, fmt, ap);
	va_end(ap);
}

xtPublic void xt_throw_error(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *msg)
{
	xt_throwf(self, func, file, line, xt_err, sys_err, "%s", msg);
}

#define XT_SYS_ERR_SIZE		300

#ifdef XT_WIN
static c_char *thr_get_sys_error(int err, char *err_msg)
#else
static c_char *thr_get_sys_error(int err, char *XT_UNUSED(err_msg))
#endif
{
#ifdef XT_WIN
	char *ptr;

	if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL,
		err, 0, err_msg, XT_SYS_ERR_SIZE, NULL)) {
		return strerror(err);
	}

	ptr = &err_msg[strlen(err_msg)];
	while (ptr-1 > err_msg) {
		if (*(ptr-1) != '\n' && *(ptr-1) != '\r' && *(ptr-1) != '.')
			break;
		ptr--;
	}
	*ptr = 0;
return err_msg;
#else
	return strerror(err);
#endif
}

static c_char *thr_get_err_string(int xt_err)
{
	c_char *str;

	switch (xt_err) {
		case XT_ERR_STACK_OVERFLOW:		str = "Stack overflow"; break;
		case XT_ERR_JUMP_OVERFLOW:		str = "Jump overflow"; break;
		case XT_ERR_TABLE_EXISTS:		str = "Table `%s` already exists"; break;
		case XT_ERR_NAME_TOO_LONG:		str = "Name '%s' is too long"; break;
		case XT_ERR_TABLE_NOT_FOUND:	str = "Table `%s` not found"; break;
		case XT_ERR_SESSION_NOT_FOUND:	str = "Session %s not found"; break;
		case XT_ERR_BAD_ADDRESS:		str = "Incorrect address '%s'"; break;
		case XT_ERR_UNKNOWN_SERVICE:	str = "Unknown service '%s'"; break;
		case XT_ERR_UNKNOWN_HOST:		str = "Host '%s' not found"; break;
		case XT_ERR_TOKEN_EXPECTED:		str = "%s expected in place of %s"; break;
		case XT_ERR_PROPERTY_REQUIRED:	str = "Property '%s' required"; break;
		case XT_ERR_DEADLOCK:			str = "Deadlock, transaction aborted"; break;
		case XT_ERR_CANNOT_CHANGE_DB:	str = "Cannot change database while transaction is in progress"; break;
		case XT_ERR_ILLEGAL_CHAR:		str = "Illegal character: '%s'"; break;
		case XT_ERR_UNTERMINATED_STRING:str = "Unterminated string: %s"; break;
		case XT_ERR_SYNTAX:				str = "Syntax error near %s"; break;
		case XT_ERR_ILLEGAL_INSTRUCTION:str = "Illegal instruction"; break;
		case XT_ERR_OUT_OF_BOUNDS:		str = "Memory reference out of bounds"; break;
		case XT_ERR_STACK_UNDERFLOW:	str = "Stack underflow"; break;
		case XT_ERR_TYPE_MISMATCH:		str = "Type mismatch"; break;
		case XT_ERR_ILLEGAL_TYPE:		str = "Illegal type for operator"; break;
		case XT_ERR_ID_TOO_LONG:		str = "Identifier too long: %s"; break;
		case XT_ERR_TYPE_OVERFLOW:		str = "Type overflow: %s"; break;
		case XT_ERR_TABLE_IN_USE:		str = "Table `%s` in use"; break;
		case XT_ERR_NO_DATABASE_IN_USE:	str = "No database in use"; break;
		case XT_ERR_CANNOT_RESOLVE_TYPE:str = "Cannot resolve type with ID: %s"; break;
		case XT_ERR_BAD_INDEX_DESC:		str = "Unsupported index description: %s"; break;
		case XT_ERR_WRONG_NO_OF_VALUES:	str = "Incorrect number of values"; break;
		case XT_ERR_CANNOT_OUTPUT_VALUE:str = "Cannot output given type"; break;
		case XT_ERR_COLUMN_NOT_FOUND:	str = "Column `%s.%s` not found"; break;
		case XT_ERR_NOT_IMPLEMENTED:	str = "Not implemented"; break;
		case XT_ERR_UNEXPECTED_EOS:		str = "Connection unexpectedly lost"; break;
		case XT_ERR_BAD_TOKEN:			str = "Incorrect binary token"; break;
		case XT_ERR_RES_STACK_OVERFLOW:	str = "Internal error: resource stack overflow"; break;
		case XT_ERR_BAD_INDEX_TYPE:		str = "Unsupported index type: %s"; break;
		case XT_ERR_INDEX_EXISTS:		str = "Index '%s' already exists"; break;
		case XT_ERR_INDEX_STRUC_EXISTS:	str = "Index '%s' has an identical structure"; break;
		case XT_ERR_INDEX_NOT_FOUND:	str = "Index '%s' not found"; break;
		case XT_ERR_INDEX_CORRUPT:		str = "Cannot read index '%s'"; break;
		case XT_ERR_TYPE_NOT_SUPPORTED:	str = "Data type %s not supported"; break;
		case XT_ERR_BAD_TABLE_VERSION:	str = "Table `%s` version not supported, upgrade required"; break;
		case XT_ERR_BAD_RECORD_FORMAT:	str = "Record format unknown, either corrupted or upgrade required"; break;
		case XT_ERR_BAD_EXT_RECORD:		str = "Extended record part does not match reference"; break;
		case XT_ERR_RECORD_CHANGED:		str = "Record already updated, transaction aborted"; break;
		case XT_ERR_XLOG_WAS_CORRUPTED:	str = "Corrupted transaction log has been truncated"; break;
		case XT_ERR_DUPLICATE_KEY:		str = "Duplicate unique key"; break;
		case XT_ERR_NO_DICTIONARY:		str = "Table `%s` has not yet been opened by MySQL"; break;
		case XT_ERR_TOO_MANY_TABLES:	str = "Limit of %s tables per database exceeded"; break;
		case XT_ERR_KEY_TOO_LARGE:		str = "Index '%s' exceeds the key size limit of %s"; break;
		case XT_ERR_MULTIPLE_DATABASES:	str = "Multiple database in a single transaction is not permitted"; break;
		case XT_ERR_NO_TRANSACTION:		str = "Internal error: no transaction running"; break;
		case XT_ERR_A_EXPECTED_NOT_B:	str = "%s expected in place of %s"; break;
		case XT_ERR_NO_MATCHING_INDEX:	str = "Matching index required for '%s'"; break;
		case XT_ERR_TABLE_LOCKED:		str = "Table `%s` locked"; break;
		case XT_ERR_NO_REFERENCED_ROW:		str = "Constraint: `%s`"; break;  // "Foreign key '%s', referenced row does not exist"
		case XT_ERR_ROW_IS_REFERENCED:		str = "Constraint: `%s`"; break;  // "Foreign key '%s', has a referencing row"
		case XT_ERR_BAD_DICTIONARY:			str = "Internal dictionary does not match MySQL dictionary"; break;
		case XT_ERR_LOADING_MYSQL_DIC:		str = "Error loading %s.frm file, MySQL error: %s"; break;
		case XT_ERR_COLUMN_IS_NOT_NULL:		str = "Column `%s` is NOT NULL"; break;
		case XT_ERR_INCORRECT_NO_OF_COLS:	str = "Incorrect number of columns near %s"; break;
		case XT_ERR_FK_ON_TEMP_TABLE:		str = "Cannot create foreign key on temporary table"; break;
		case XT_ERR_REF_TABLE_NOT_FOUND:	str = "Referenced table `%s` not found"; break;
		case XT_ERR_REF_TYPE_WRONG:			str = "Incorrect data type on referenced column `%s`"; break;
		case XT_ERR_DUPLICATE_FKEY:			str = "Duplicate unique foreign key, contraint: %s"; break;
		case XT_ERR_INDEX_FILE_TO_LARGE:	str = "Index file has grown too large: %s"; break;
		case XT_ERR_UPGRADE_TABLE:			str = "Table `%s` must be upgraded from PBXT version %s"; break;
		case XT_ERR_INDEX_NEW_VERSION:		str = "Table `%s` index created by a newer version, upgrade required"; break;
		case XT_ERR_LOCK_TIMEOUT:			str = "Lock timeout on table `%s`"; break;
		case XT_ERR_CONVERSION:				str = "Error converting value for column `%s.%s`"; break;
		case XT_ERR_NO_ROWS:				str = "No matching row found in table `%s`"; break;
		case XT_ERR_DATA_LOG_NOT_FOUND:		str = "Data log not found: '%s'"; break;
		case XT_ERR_LOG_MAX_EXCEEDED:		str = "Maximum log count, %s, exceeded"; break;
		case XT_ERR_MAX_ROW_COUNT:			str = "Maximum row count reached"; break;
		case XT_ERR_FILE_TOO_LONG:			str = "File cannot be mapped, too large: '%s'"; break;
		case XT_ERR_BAD_IND_BLOCK_SIZE:		str = "Table `%s`, incorrect index block size: %s"; break;
		case XT_ERR_INDEX_CORRUPTED:		str = "Table `%s` index is corrupted, REPAIR TABLE required"; break;
		case XT_ERR_NO_INDEX_CACHE:			str = "Not enough index cache memory to handle concurrent updates"; break;
		case XT_ERR_INDEX_LOG_CORRUPT:		str = "Index log corrupt: '%s'"; break;
		case XT_ERR_TOO_MANY_THREADS:		str = "Too many threads: %s, increase pbxt_max_threads"; break;
		case XT_ERR_TOO_MANY_WAITERS:		str = "Too many waiting threads: %s"; break;
		case XT_ERR_INDEX_OLD_VERSION:		str = "Table `%s` index created by an older version, REPAIR TABLE required"; break;
		case XT_ERR_PBXT_TABLE_EXISTS:		str = "System table cannot be dropped because PBXT table still exists"; break;
		case XT_ERR_SERVER_RUNNING:			str = "A server is possibly already running"; break;
		case XT_ERR_INDEX_MISSING:			str = "Index file of table '%s' is missing"; break;
		case XT_ERR_RECORD_DELETED:			str = "Record was deleted"; break;
		case XT_ERR_NEW_TYPE_OF_XLOG:		str = "Transaction log %s, is using a newer format, upgrade required"; break;
		case XT_ERR_NO_BEFORE_IMAGE:		str = "Internal error: no before image"; break;
		case XT_ERR_FK_REF_TEMP_TABLE:		str = "Foreign key may not reference temporary table"; break;
		case XT_ERR_MYSQL_SHUTDOWN:			str = "Cannot open table, MySQL has shutdown"; break;
		case XT_ERR_MYSQL_NO_THREAD:		str = "Cannot create thread, MySQL has shutdown"; break;
		default:							str = "Unknown XT error"; break;
	}
	return str;
}

xtPublic void xt_throw_i2xterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item, c_char *item2)
{
	xt_throwf(self, func, file, line, xt_err, 0, thr_get_err_string(xt_err), item, item2);
}

xtPublic void xt_throw_ixterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item)
{
	xt_throw_i2xterr(self, func, file, line, xt_err, item, NULL);
}

xtPublic void xt_throw_tabcolerr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item, c_char *item2)
{
	char buffer[XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + 3];

	xt_2nd_last_name_of_path(sizeof(buffer), buffer, tab_item->ps_path);
	xt_strcat(sizeof(buffer), buffer, ".");
	xt_strcat(sizeof(buffer), buffer, xt_last_name_of_path(tab_item->ps_path));

	xt_throw_i2xterr(self, func, file, line, xt_err, buffer, item2);
}

xtPublic void xt_throw_taberr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item)
{
	char buffer[XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + 3];

	xt_2nd_last_name_of_path(sizeof(buffer), buffer, tab_item->ps_path);
	xt_strcat(sizeof(buffer), buffer, ".");
	xt_strcat(sizeof(buffer), buffer, xt_last_name_of_path(tab_item->ps_path));

	xt_throw_ixterr(self, func, file, line, xt_err, buffer);
}

xtPublic void xt_throw_ulxterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, u_long value)
{
	char buffer[100];

	sprintf(buffer, "%lu", value);
	xt_throw_ixterr(self, func, file, line, xt_err, buffer);
}

xtPublic void xt_throw_sulxterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item, u_long value)
{
	char buffer[100];

	sprintf(buffer, "%lu", value);
	xt_throw_i2xterr(self, func, file, line, xt_err, item, buffer);
}

xtPublic void xt_throw_xterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err)
{
	xt_throw_ixterr(self, func, file, line, xt_err, NULL);
}

xtPublic void xt_throw_errno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err)
{
	char err_msg[XT_SYS_ERR_SIZE];

	xt_throw_error(self, func, file, line, XT_SYSTEM_ERROR, err, thr_get_sys_error(err, err_msg));
}

xtPublic void xt_throw_ferrno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err, c_char *path)
{
	char err_msg[XT_SYS_ERR_SIZE];

	xt_throwf(self, func, file, line, XT_SYSTEM_ERROR, err, "%s: '%s'", thr_get_sys_error(err, err_msg), path);
}

xtPublic void xt_throw_assertion(XTThreadPtr self, c_char *func, c_char *file, u_int line, c_char *str)
{
	xt_throw_error(self, func, file, line, XT_ASSERTION_FAILURE, 0, str);
}

static void xt_log_assertion(XTThreadPtr self, c_char *func, c_char *file, u_int line, c_char *str)
{
	xt_log_error(self, func, file, line, XT_LOG_DEFAULT, XT_ASSERTION_FAILURE, 0, str);
}

xtPublic void xt_throw_signal(XTThreadPtr self, c_char *func, c_char *file, u_int line, int sig)
{
#ifdef XT_WIN
	char buffer[100];

	sprintf(buffer, "Signal #%d", sig);
	xt_throw_error(self, func, file, line, XT_SIGNAL_CAUGHT, sig, buffer);
#else
	xt_throw_error(self, func, file, line, XT_SIGNAL_CAUGHT, sig, strsignal(sig));
#endif
}

/*
 * -----------------------------------------------------------------------
 * REGISTERING EXCEPTIONS
 */

xtPublic void xt_registerf(c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, ...)
{
	va_list		ap;
	XTThreadPtr	thread = xt_get_self();

	va_start(ap, fmt);
	thr_save_error_va(thread ? &thread->t_exception : NULL, thread, FALSE, func, file, line, xt_err, sys_err, fmt, ap);
	va_end(ap);
}

xtPublic void xt_register_i2xterr(c_char *func, c_char *file, u_int line, int xt_err, c_char *item, c_char *item2)
{
	xt_registerf(func, file, line, xt_err, 0, thr_get_err_string(xt_err), item, item2);
}

xtPublic void xt_register_ixterr(c_char *func, c_char *file, u_int line, int xt_err, c_char *item)
{
	xt_register_i2xterr(func, file, line, xt_err, item, NULL);
}

xtPublic void xt_register_tabcolerr(c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item, c_char *item2)
{
	char buffer[XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + 3];

	xt_2nd_last_name_of_path(sizeof(buffer), buffer, tab_item->ps_path);
	xt_strcat(sizeof(buffer), buffer, ".");
	xt_strcpy(sizeof(buffer), buffer, xt_last_name_of_path(tab_item->ps_path));
	xt_strcat(sizeof(buffer), buffer, ".");
	xt_strcat(sizeof(buffer), buffer, item2);

	xt_register_ixterr(func, file, line, xt_err, buffer);
}

xtPublic void xt_register_taberr(c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item)
{
	char buffer[XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + XT_IDENTIFIER_NAME_SIZE + 3];

	xt_2nd_last_name_of_path(sizeof(buffer), buffer, tab_item->ps_path);
	xt_strcat(sizeof(buffer), buffer, ".");
	xt_strcpy(sizeof(buffer), buffer, xt_last_name_of_path(tab_item->ps_path));

	xt_register_ixterr(func, file, line, xt_err, buffer);
}

xtPublic void xt_register_ulxterr(c_char *func, c_char *file, u_int line, int xt_err, u_long value)
{
	char buffer[100];

	sprintf(buffer, "%lu", value);
	xt_register_ixterr(func, file, line, xt_err, buffer);
}

xtPublic xtBool xt_register_ferrno(c_char *func, c_char *file, u_int line, int err, c_char *path)
{
	char err_msg[XT_SYS_ERR_SIZE];

	xt_registerf(func, file, line, XT_SYSTEM_ERROR, err, "%s: '%s'", thr_get_sys_error(err, err_msg), path);
	return FAILED;
}

xtPublic void xt_register_error(c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *msg)
{
	xt_registerf(func, file, line, xt_err, sys_err, "%s", msg);
}

xtPublic xtBool xt_register_errno(c_char *func, c_char *file, u_int line, int err)
{
	char err_msg[XT_SYS_ERR_SIZE];

	xt_register_error(func, file, line, XT_SYSTEM_ERROR, err, thr_get_sys_error(err, err_msg));
	return FAILED;
}

xtPublic void xt_register_xterr(c_char *func, c_char *file, u_int line, int xt_err)
{
	xt_register_error(func, file, line, xt_err, 0, thr_get_err_string(xt_err));
}

/*
 * -----------------------------------------------------------------------
 * CREATING EXCEPTIONS
 */

xtPublic void xt_exceptionf(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	thr_save_error_va(e, self, FALSE, func, file, line, xt_err, sys_err, fmt, ap);
	va_end(ap);
}

xtPublic void xt_exception_error(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *msg)
{
	xt_exceptionf(e, self, func, file, line, xt_err, sys_err, "%s", msg);
}

xtPublic xtBool xt_exception_errno(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int err)
{
	char err_msg[XT_SYS_ERR_SIZE];

	xt_exception_error(e, self, func, file, line, XT_SYSTEM_ERROR, err, thr_get_sys_error(err, err_msg));
	return FAILED;
}

/*
 * -----------------------------------------------------------------------
 * LOG ERRORS
 */

xtPublic void xt_log_errno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err)
{
	XTExceptionRec e;

	xt_exception_errno(&e, self, func, file, line, err);
	xt_log_exception(self, &e, XT_LOG_DEFAULT);
}

/*
 * -----------------------------------------------------------------------
 * Assertions and failures (one breakpoints for all failures)
 */
//#define CRASH_ON_ASSERT

xtPublic xtBool xt_assert(XTThreadPtr self, c_char *expr, c_char *func, c_char *file, u_int line)
{
	(void) self;
#ifdef DEBUG
	//xt_set_fflush(TRUE);
	//xt_dump_trace();
	printf("%s(%s:%d) %s\n", func, file, (int) line, expr);
#ifdef CRASH_ON_ASSERT
	abort();
#endif
#ifdef XT_WIN
	FatalAppExit(0, "Assertion Failed!");
#endif
#else
	xt_throw_assertion(self, func, file, line, expr);
#endif
	return FALSE;
}

xtPublic xtBool xt_assume(XTThreadPtr self, c_char *expr, c_char *func, c_char *file, u_int line)
{
	xt_log_assertion(self, func, file, line, expr);
	return FALSE;
}

/*
 * -----------------------------------------------------------------------
 * Create and destroy threads
 */

typedef struct ThreadData {
	xtBool			td_started;
	XTThreadPtr		td_thr;
	void			*(*td_start_routine)(XTThreadPtr self);
} ThreadDataRec, *ThreadDataPtr;

#ifdef XT_WIN
pthread_key(void *, thr_key);
#else
static pthread_key_t thr_key;
#endif

#ifdef HANDLE_SIGNALS
static void thr_ignore_signal(int sig)
{
#pragma unused(sig)
}

static void thr_throw_signal(int sig)
{
	XTThreadPtr	self;

	self = xt_get_self();

	if (self->t_main) {
		/* The main thread will pass on a signal to all threads: */
		xt_signal_all_threads(self, sig);
		if (sig != SIGTERM) {
			if (self->t_disable_interrupts) {
				self->t_delayed_signal = sig;
				self->t_disable_interrupts = FALSE;	/* Prevent infinite loop */
			}
			else {
				self->t_delayed_signal = 0;
				xt_throw_signal(self, "thr_throw_signal", NULL, 0, sig);
			}
		}
	}
	else {
		if (self->t_disable_interrupts) {
			self->t_delayed_signal = sig;
			self->t_disable_interrupts = FALSE;	/* Prevent infinite loop */
		}
		else {
			self->t_delayed_signal = 0;
			xt_throw_signal(self, "thr_throw_signal", NULL, 0, sig);
		}
	}
}

static xtBool thr_setup_signals(void)
{
	struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = thr_ignore_signal;

	if (sigaction(SIGPIPE, &action, NULL) == -1)
		goto error_occurred;
	if (sigaction(SIGHUP, &action, NULL) == -1)
		goto error_occurred;

    action.sa_handler = thr_throw_signal;

	if (sigaction(SIGQUIT, &action, NULL) == -1)
		goto error_occurred;
	if (sigaction(SIGTERM, &action, NULL) == -1)
		goto error_occurred;
#ifndef DEBUG
	if (sigaction(SIGILL, &action, NULL) == -1)
		goto error_occurred;
	if (sigaction(SIGBUS, &action, NULL) == -1)
		goto error_occurred;
	if (sigaction(SIGSEGV, &action, NULL) == -1)
		goto error_occurred;
#endif
	return TRUE;

	error_occurred:
	xt_log_errno(XT_NS_CONTEXT, errno);
	return FALSE;
}
#endif

typedef void *(*ThreadMainFunc)(XTThreadPtr self);

extern "C" void *thr_main(void *data)
{
	ThreadDataPtr	td = (ThreadDataPtr) data;
	XTThreadPtr		self = td->td_thr;
	ThreadMainFunc		start_routine;
	void			*return_data;

	enter_();
	self->t_pthread = pthread_self();
	start_routine = td->td_start_routine;
	return_data = NULL;

#ifdef HANDLE_SIGNALS
	if (!thr_setup_signals())
		return NULL;
#endif

	try_(a) {
		if (!xt_set_key(thr_key, self, &self->t_exception))
			throw_();
		td->td_started = TRUE;
		return_data = (*start_routine)(self);
	}
	catch_(a) {
		xt_log_and_clear_exception(self);
	}
	cont_(a);

	outer_();
	xt_free_thread(self);
	
	/* {MYSQL-THREAD-KILL}
	 * Clean up any remaining MySQL thread!
	 */
	myxt_delete_remaining_thread();
	return return_data;
}

static void thr_free_data(XTThreadPtr self)
{
	if (self->t_free_data) {
		(*self->t_free_data)(self, self->t_data);
		self->t_data = NULL;
	}
}

xtPublic void xt_set_thread_data(XTThreadPtr self, void *data, XTThreadFreeFunc free_func)
{
	thr_free_data(self);
	self->t_free_data = free_func;
	self->t_data = data;
}

static void thr_exit(XTThreadPtr self)
{
	/* Free the thread temporary data. */
	thr_free_resources(self, (XTResourcePtr) self->x.t_res_stack);
	xt_db_exit_thread(self);
	thr_free_data(self);					/* Free custom user data. */

	if (self->t_id > 0) {
		ASSERT(self->t_id < xt_thr_current_max_threads);
		xt_lock_mutex(self, &thr_array_lock);
		pushr_(xt_unlock_mutex, &thr_array_lock);
		thr_accumulate_statistics(self);
		xt_thr_array[self->t_id] = NULL;
		xt_thr_current_thread_count--;
		if (self->t_id+1 == xt_thr_current_max_threads) {
			/* We can reduce the current maximum,
			 * this makes operations that scan the array faster!
			 */
			u_int i;

			i = self->t_id;
			for(;;) {
				if (xt_thr_array[i])
					break;
				if (!i)
					break;
				i--;
			}
			xt_thr_current_max_threads = i+1;
		}
		freer_(); // xt_unlock_mutex(&thr_array_lock)
	}

	xt_free_cond(&self->t_cond);
	xt_free_mutex(&self->t_lock);

	self->st_thread_list_count = 0;
	self->st_thread_list_size = 0;
	if (self->st_thread_list) {
		xt_free_ns(self->st_thread_list);
		self->st_thread_list = NULL;
	}
}

static void thr_init(XTThreadPtr self, XTThreadPtr new_thread)
{
	new_thread->t_res_top = (XTResourcePtr) new_thread->x.t_res_stack;

	new_thread->st_thread_list_count = 0;
	new_thread->st_thread_list_size = 0;
	new_thread->st_thread_list = NULL;
	try_(a) {
		xt_init_cond(self, &new_thread->t_cond);
		xt_init_mutex_with_autoname(self, &new_thread->t_lock);

		xt_lock_mutex(self, &thr_array_lock);
		pushr_(xt_unlock_mutex, &thr_array_lock);

		ASSERT(xt_thr_current_thread_count <= xt_thr_current_max_threads);
		ASSERT(xt_thr_current_max_threads <= xt_thr_maximum_threads);
		if (xt_thr_current_thread_count == xt_thr_maximum_threads)
			xt_throw_ulxterr(XT_CONTEXT, XT_ERR_TOO_MANY_THREADS, (u_long) xt_thr_maximum_threads+1);
		if (xt_thr_current_thread_count == xt_thr_current_max_threads) {
			new_thread->t_id = xt_thr_current_thread_count;
			xt_thr_array[new_thread->t_id] = new_thread;
			xt_thr_current_max_threads++;
		}
		else {
			/* There must be a free slot: */
			for (u_int i=0; i<xt_thr_current_max_threads; i++) {
				if (!xt_thr_array[i]) {
					new_thread->t_id = i;
					xt_thr_array[i] = new_thread;
					break;
				}
			}
		}
		xt_thr_current_thread_count++;
		freer_(); // xt_unlock_mutex(&thr_array_lock)

		xt_db_init_thread(self, new_thread);
	}
	catch_(a) {
		thr_exit(new_thread);
		throw_();
	}
	cont_(a);
	
}

/*
 * The caller of this function automatically becomes the main thread.
 */
xtPublic XTThreadPtr xt_init_threading(u_int max_threads)
{
	volatile XTThreadPtr	self = NULL;
	XTExceptionRec			e;
	int						err;

	/* Align the number of threads: */
	xt_thr_maximum_threads = xt_align_size(max_threads, XT_XS_LOCK_ALIGN);

#ifdef HANDLE_SIGNALS
	if (!thr_setup_signals())
		return NULL;
#endif

	xt_p_init_threading();

	err = pthread_key_create(&thr_key, NULL);
	if (err) {
		xt_log_errno(XT_NS_CONTEXT, err);
		return NULL;
	}

	if ((err = xt_p_mutex_init_with_autoname(&thr_array_lock, NULL))) {
		xt_log_errno(XT_NS_CONTEXT, err);
		goto failed;
	}
	
	if (!(xt_thr_array = (XTThreadPtr *) malloc(xt_thr_maximum_threads * sizeof(XTThreadPtr)))) {
		xt_log_errno(XT_NS_CONTEXT, XT_ENOMEM);
		goto failed;
	}

	xt_thr_array[0] = (XTThreadPtr) 1; // Dummy, not used
	xt_thr_current_thread_count = 1;
	xt_thr_current_max_threads = 1;

	/* Create the main thread: */
	self = xt_create_thread("MainThread", TRUE, FALSE, &e);
	if (!self) {
		xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
		goto failed;
	}

	try_(a) {
		XTThreadPtr	thread = self;
		thr_list = xt_new_linkedlist(thread, NULL, NULL, TRUE);
	}
	catch_(a) {
		XTThreadPtr	thread = self;
		xt_log_and_clear_exception(thread);
		xt_exit_threading(thread);
	}
	cont_(a);

	return self;
	
	failed:
	xt_exit_threading(NULL);
	return NULL;
}

xtPublic void xt_exit_threading(XTThreadPtr self)
{
	if (thr_list) {
		xt_free_linkedlist(self, thr_list);
		thr_list = NULL;
	}

	/* This should be the main thread! */
	if (self) {
		ASSERT(self->t_main);
		xt_free_thread(self);
	}

	if (xt_thr_array) {
		free(xt_thr_array);
		xt_thr_array = NULL;
		xt_free_mutex(&thr_array_lock);
	}

	xt_thr_current_thread_count = 0;
	xt_thr_current_max_threads = 0;

	/* I no longer delete 'thr_key' because
	 * functions that call xt_get_self() after this
	 * point will get junk back if we delete
	 * thr_key. In particular the XT_THREAD_LOCK_INFO
	 * code fails
	if (thr_key) {
		pthread_key_delete(thr_key);
		thr_key = (pthread_key_t) 0;
	}
	*/
}

xtPublic void xt_wait_for_all_threads(XTThreadPtr self)
{
	if (thr_list)
		xt_ll_wait_till_empty(self, thr_list);
}

/*
 * Call this function in a busy wait loop!
 * Use if for wait loops that are not
 * time critical.
 */
xtPublic void xt_busy_wait(void)
{
#ifdef XT_WIN
	Sleep(1);
#else
	usleep(10);
#endif
}

xtPublic void xt_critical_wait(void)
{
	/* NOTE: On Mac xt_busy_wait() works better than xt_yield()
	 */
#if defined(XT_MAC) || defined(XT_WIN)
	xt_busy_wait();
#else
	xt_yield();
#endif
}


/*
 * Use this for loops that time critical.
 * Time critical means we need to get going
 * as soon as possible!
 */
xtPublic void xt_yield(void)
{
#ifdef XT_WIN
	Sleep(0);
#elif defined(XT_MAC) || defined(XT_SOLARIS)
	usleep(0);
#elif defined(XT_NETBSD)
	sched_yield();
#else
	pthread_yield();
#endif
}

xtPublic void xt_sleep_milli_second(u_int t)
{
#ifdef XT_WIN
	Sleep(t);
#else
	usleep(t * 1000);
#endif
}

xtPublic void xt_signal_all_threads(XTThreadPtr self, int sig)
{
	XTLinkedItemPtr li;
	XTThreadPtr		sig_thr;

	xt_ll_lock(self, thr_list);
	try_(a) {
		li = thr_list->ll_items;
		while (li) {
			sig_thr = (XTThreadPtr) li;
			if (sig_thr != self)
				pthread_kill(sig_thr->t_pthread, sig);
			li = li->li_next;
		}
	}
	catch_(a) {
		xt_ll_unlock(self, thr_list);
		throw_();
	}
	cont_(a);
	xt_ll_unlock(self, thr_list);
}

/*
 * Apply the given function to all threads except self!
 */
xtPublic void xt_do_to_all_threads(XTThreadPtr self, void (*do_func_ptr)(XTThreadPtr self, XTThreadPtr to_thr, void *thunk), void *thunk)
{
	XTLinkedItemPtr li;
	XTThreadPtr		to_thr;

	xt_ll_lock(self, thr_list);
	pushr_(xt_ll_unlock, thr_list);

	li = thr_list->ll_items;
	while (li) {
		to_thr = (XTThreadPtr) li;
		if (to_thr != self)
			(*do_func_ptr)(self, to_thr, thunk);
		li = li->li_next;
	}

	freer_(); // xt_ll_unlock(thr_list)
}

xtPublic XTThreadPtr xt_get_self(void)
{
	XTThreadPtr self;

	/* First check if the handler has the data: */
	if ((self = myxt_get_self()))
		return self;
	/* Then it must be a background process, and the 
	 * thread info is stored in the local key: */
	return (XTThreadPtr) xt_get_key(thr_key);
}

xtPublic void xt_set_self(XTThreadPtr self)
{
	xt_set_key(thr_key, self, NULL);
}

xtPublic void xt_clear_exception(XTThreadPtr thread)
{
	thread->t_exception.e_xt_err = 0;
	thread->t_exception.e_sys_err = 0;
	*thread->t_exception.e_err_msg = 0;
	*thread->t_exception.e_func_name = 0;
	*thread->t_exception.e_source_file = 0;
	thread->t_exception.e_source_line = 0;
	*thread->t_exception.e_catch_trace = 0;
}

/*
 * Create a thread without requiring thread to do it (as in xt_create_daemon()).
 *
 * This function returns NULL on error.
 */
xtPublic XTThreadPtr xt_create_thread(c_char *name, xtBool main_thread, xtBool user_thread, XTExceptionPtr e)
{
	volatile XTThreadPtr self;
	
	self = (XTThreadPtr) xt_calloc_ns(sizeof(XTThreadRec));
	if (!self) {
		xt_exception_errno(e, XT_CONTEXT, ENOMEM);
		return NULL;
	}

	if (!xt_set_key(thr_key, self, e)) {
		xt_free_ns(self);
		return NULL;
	}

	xt_strcpy(XT_THR_NAME_SIZE, self->t_name, name);
	self->t_main = main_thread;
	self->t_daemon = FALSE;

	try_(a) {
		thr_init(self, self);
	}
	catch_(a) {
		*e = self->t_exception;
		xt_set_key(thr_key, NULL, NULL);
		xt_free_ns(self);
		self = NULL;
	}
	cont_(a);

	if (self && user_thread) {
		/* Add non-temporary threads to the thread list. */
		try_(b) {
			xt_ll_add(self, thr_list, &self->t_links, TRUE);
		}
		catch_(b) {
			*e = self->t_exception;
			xt_free_thread(self);
			self = NULL;
		}
		cont_(b);
	}

	return self;
}

/*
 * Create a daemon thread.
 */
xtPublic XTThreadPtr xt_create_daemon(XTThreadPtr self, c_char *name)
{
	XTThreadPtr new_thread;

	/* NOTE: thr_key will be set when this thread start running. */

	new_thread = (XTThreadPtr) xt_calloc(self, sizeof(XTThreadRec));
	xt_strcpy(XT_THR_NAME_SIZE, new_thread->t_name, name);
	new_thread->t_main = FALSE;
	new_thread->t_daemon = TRUE;

	try_(a) {
		thr_init(self, new_thread);
	}
	catch_(a) {
		xt_free(self, new_thread);
		throw_();
	}
	cont_(a);
	return new_thread;
}

void xt_free_thread(XTThreadPtr self)
{
	thr_exit(self);
	if (!self->t_daemon && thr_list)
		xt_ll_remove(self, thr_list, &self->t_links, TRUE);
	/* Note, if I move this before thr_exit() then self = xt_get_self(); will fail in 
	 * xt_close_file_ns() which is called by xt_unuse_database()!
	 */

	 /*
	  * Do not clear the pthread's key value unless it is the same as the thread just released.
	  * This can happen during shutdown when the engine is deregistered with the PBMS engine.
	  *
	  * What happens is that during deregistration the PBMS engine calls close to close all
	  * PBXT resources on all MySQL THDs created by PBMS for it's own pthreads. So the 'self' 
	  * being freed is not the same 'self' associated with the PBXT 'thr_key'.
	  */
	if (thr_key && (self == ((XTThreadPtr) xt_get_key(thr_key)))) {
		xt_set_key(thr_key, NULL, NULL);
	}
	xt_free_ns(self);
}

xtPublic pthread_t xt_run_thread(XTThreadPtr self, XTThreadPtr child, void *(*start_routine)(XTThreadPtr))
{
	ThreadDataRec	data;
	int				err;
	pthread_t		child_thread;

	enter_();
	
	// 'data' can be on the stack because we are waiting for the thread to start
	// before exiting the function.
	data.td_started = FALSE;
	data.td_thr = child;
	data.td_start_routine = start_routine;
#ifdef XT_WIN
	{
		pthread_attr_t	attr = { 0, 0, 0 };

		attr.priority = THREAD_PRIORITY_NORMAL;
		err = pthread_create(&child_thread, &attr, thr_main, &data);
	}
#else
	err = pthread_create(&child_thread, NULL, thr_main, &data);
#endif
	if (err) {
		xt_free_thread(child);
		xt_throw_errno(XT_CONTEXT, err);
	}
	while (!data.td_started) {
		/* Check that the self is still alive: */
		if (pthread_kill(child_thread, 0))
			break;
		xt_busy_wait();
	}
	return_(child_thread);
}

xtPublic void xt_exit_thread(XTThreadPtr self, void *result)
{
	xt_free_thread(self);
	pthread_exit(result);
}

xtPublic void *xt_wait_for_thread(xtThreadID tid, xtBool ignore_error)
{
	int			err;
	void		*value_ptr = NULL;
	xtBool		ok = FALSE;
	XTThreadPtr thread;
	pthread_t	t1 = 0;

	xt_lock_mutex_ns(&thr_array_lock);
	if (tid < xt_thr_maximum_threads) {
		if ((thread = xt_thr_array[tid])) {
			t1 = thread->t_pthread;
			ok = TRUE;
		}
	}
	xt_unlock_mutex_ns(&thr_array_lock);
	if (ok) {
		err = xt_p_join(t1, &value_ptr);
		if (err && !ignore_error)
			xt_log_errno(XT_NS_CONTEXT, err);
	}
	return value_ptr;
}

/*
 * Kill the given thead, and wait for it to terminate.
 * This function just returns if the self is already dead.
 */
xtPublic void xt_kill_thread(pthread_t t1)
{
	int		err;
	void	*value_ptr = NULL;

	err = pthread_kill(t1, SIGTERM);
	if (err)
		return;
	err = xt_p_join(t1, &value_ptr);
	if (err)
		xt_log_errno(XT_NS_CONTEXT, err);
}

/*
 * -----------------------------------------------------------------------
 * Read/write locking
 */

#ifdef XT_THREAD_LOCK_INFO
xtPublic xtBool xt_init_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock, const char *name)
#else
xtPublic xtBool xt_init_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock)
#endif
{
	int err;

#ifdef XT_THREAD_LOCK_INFO
	err = xt_p_rwlock_init_with_name(rwlock, NULL, name);
#else
	err = xt_p_rwlock_init(rwlock, NULL);
#endif

	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FAILED;
	}
	return OK;
}

xtPublic void xt_free_rwlock(xt_rwlock_type *rwlock)
{
	int err;

	for (;;) {
		err = xt_p_rwlock_destroy(rwlock);
		if (err != XT_EBUSY)
			break;
		xt_busy_wait();
	}
	/* PMC - xt_xn_exit_db() is called even when xt_xn_init_db() is not fully completed!
	 * This generates a lot of log entries. But I have no desire to only call
	 * free for those articles that I have init'ed!
	if (err)
		xt_log_errno(XT_NS_CONTEXT, err);
	*/
}

xtPublic xt_rwlock_type *xt_slock_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock)
{
	int err;

	for (;;) {
		err = xt_slock_rwlock_ns(rwlock);
		if (err != XT_EAGAIN)
			break;
		xt_busy_wait();
	}
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return NULL;
	}
	return rwlock;
}

xtPublic xt_rwlock_type *xt_xlock_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock)
{
	int err;

	for (;;) {
		err = xt_xlock_rwlock_ns(rwlock);
		if (err != XT_EAGAIN)
			break;
		xt_busy_wait();
	}

	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return NULL;
	}
	return rwlock;
}

xtPublic void xt_unlock_rwlock(XTThreadPtr XT_UNUSED(self), xt_rwlock_type *rwlock)
{
	int err;

	err = xt_unlock_rwlock_ns(rwlock);
	if (err)
		xt_log_errno(XT_NS_CONTEXT, err);
}

/*
 * -----------------------------------------------------------------------
 * Mutex locking
 */

xtPublic xt_mutex_type *xt_new_mutex(XTThreadPtr self)
{
	xt_mutex_type *mx;

	if (!(mx = (xt_mutex_type *) xt_calloc(self, sizeof(xt_mutex_type))))
		return NULL;
	pushr_(xt_free, mx);
	if (!xt_init_mutex_with_autoname(self, mx)) {
		freer_();
		return NULL;
	}
	popr_();
	return mx;
}

xtPublic void xt_delete_mutex(XTThreadPtr self, xt_mutex_type *mx)
{
	if (mx) {
		xt_free_mutex(mx);
		xt_free(self, mx);
	}
}

#ifdef XT_THREAD_LOCK_INFO
xtPublic xtBool xt_init_mutex(XTThreadPtr self, xt_mutex_type *mx, const char *name)
#else
xtPublic xtBool xt_init_mutex(XTThreadPtr self, xt_mutex_type *mx)
#endif
{
	int err;

	err = xt_p_mutex_init_with_name(mx, NULL, name);
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FALSE;
	}
	return TRUE;
}

void xt_free_mutex(xt_mutex_type *mx)
{
	int err;

	for (;;) {
		err = xt_p_mutex_destroy(mx);
		if (err != XT_EBUSY)
			break;
		xt_busy_wait();
	}
	/* PMC - xt_xn_exit_db() is called even when xt_xn_init_db() is not fully completed!
	if (err)
		xt_log_errno(XT_NS_CONTEXT, err);
	*/
}

xtPublic xtBool xt_lock_mutex(XTThreadPtr self, xt_mutex_type *mx)
{
	int err;

	for (;;) {
		err = xt_lock_mutex_ns(mx);
		if (err != XT_EAGAIN)
			break;
		xt_busy_wait();
	}

	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FALSE;
	}
	return TRUE;
}

xtPublic void xt_unlock_mutex(XTThreadPtr self, xt_mutex_type *mx)
{
	int err;

	err = xt_unlock_mutex_ns(mx);
	if (err)
		xt_throw_errno(XT_CONTEXT, err);
}

xtPublic xtBool xt_set_key(pthread_key_t key, const void *value, XTExceptionPtr e)
{
#ifdef XT_WIN
	my_pthread_setspecific_ptr(thr_key, (void *) value);
#else
	int err;

	err = pthread_setspecific(key, value);
	if (err) {
		if (e)
			xt_exception_errno(e, XT_NS_CONTEXT, err);
		return FALSE;
	}
#endif
	return TRUE;
}

xtPublic void *xt_get_key(pthread_key_t key)
{
#ifdef XT_WIN
	return my_pthread_getspecific_ptr(void *, thr_key);
#else
	return pthread_getspecific(key);
#endif
}

xtPublic xt_cond_type *xt_new_cond(XTThreadPtr self)
{
	xt_cond_type *cond;

	if (!(cond = (xt_cond_type *) xt_calloc(self, sizeof(xt_cond_type))))
		return NULL;
	pushr_(xt_free, cond);
	if (!xt_init_cond(self, cond)) {
		freer_();
		return NULL;
	}
	popr_();
	return cond;
}

xtPublic void xt_delete_cond(XTThreadPtr self, xt_cond_type *cond)
{
	if (cond) {
		xt_free_cond(cond);
		xt_free(self, cond);
	}
}

xtPublic xtBool xt_init_cond(XTThreadPtr self, xt_cond_type *cond)
{
	int err;

	err = pthread_cond_init(cond, NULL);
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FALSE;
	}
	return TRUE;
}

xtPublic void xt_free_cond(xt_cond_type *cond)
{
	int err;

	for (;;) {
		err = pthread_cond_destroy(cond);
		if (err != XT_EBUSY)
			break;
		xt_busy_wait();
	}
	/* PMC - xt_xn_exit_db() is called even when xt_xn_init_db() is not fully completed!
	if (err)
		xt_log_errno(XT_NS_CONTEXT, err);
	*/
}

xtPublic xtBool xt_throw_delayed_signal(XTThreadPtr self, c_char *func, c_char *file, u_int line)
{
	XTThreadPtr me = self ? self : xt_get_self();

	if (me->t_delayed_signal) {
		int sig = me->t_delayed_signal;
		
		me->t_delayed_signal = 0;
		xt_throw_signal(self, func, file, line, sig);
		return FAILED;
	}
	return OK;
}

xtPublic xtBool xt_wait_cond(XTThreadPtr self, xt_cond_type *cond, xt_mutex_type *mutex)
{
	int			err;
	XTThreadPtr	me = self ? self : xt_get_self();

	/* PMC - In my tests, if I throw an exception from within the wait
	 * the condition and the mutex remain locked.
	 */
	me->t_disable_interrupts = TRUE;
	err = xt_p_cond_wait(cond, mutex);
	me->t_disable_interrupts = FALSE;
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FALSE;
	}
	if (me->t_delayed_signal) {
		xt_throw_delayed_signal(XT_CONTEXT);
		return FALSE;
	}
	return TRUE;
}

xtPublic xtBool xt_suspend(XTThreadPtr thread)
{
	xtBool ok;

	// You can only suspend yourself. 
	ASSERT_NS(pthread_equal(thread->t_pthread, pthread_self()));
	
	xt_lock_mutex_ns(&thread->t_lock);
	ok = xt_wait_cond(NULL, &thread->t_cond, &thread->t_lock);
	xt_unlock_mutex_ns(&thread->t_lock);
	return ok;
}

xtPublic xtBool xt_unsuspend(XTThreadPtr target)
{
	return xt_broadcast_cond_ns(&target->t_cond);
}

xtPublic void xt_lock_thread(XTThreadPtr thread)
{
	xt_lock_mutex_ns(&thread->t_lock);
}

xtPublic void xt_unlock_thread(XTThreadPtr thread)
{
	xt_unlock_mutex_ns(&thread->t_lock);
}

xtPublic xtBool xt_wait_thread(XTThreadPtr thread)
{
	return xt_wait_cond(NULL, &thread->t_cond, &thread->t_lock);
}

xtPublic void xt_signal_thread(XTThreadPtr target)
{
	xt_broadcast_cond_ns(&target->t_cond);
}

xtPublic void xt_terminate_thread(XTThreadPtr XT_UNUSED(self), XTThreadPtr target)
{
	target->t_quit = TRUE;
	target->t_delayed_signal = SIGTERM;
}

xtPublic xtProcID xt_getpid()
{
#ifdef XT_WIN
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

xtPublic xtBool xt_process_exists(xtProcID pid)
{
	xtBool found;

#ifdef XT_WIN
	HANDLE	h;
	DWORD	code;

	found = FALSE;
	h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (h) {
		if (GetExitCodeProcess(h, &code)) {
			if (code == STILL_ACTIVE)
				found = TRUE;
		}
		CloseHandle(h);
	}
	else {
		int err;

		err = HRESULT_CODE(GetLastError());
		if (err != ERROR_INVALID_PARAMETER)
			found = TRUE;
	}
#else
	found = TRUE;
	if (kill(pid, 0) == -1) {
		if (errno == ESRCH)
			found = FALSE;
	}
#endif
	return found;	
}

xtPublic xtBool xt_timed_wait_cond(XTThreadPtr self, xt_cond_type *cond, xt_mutex_type *mutex, u_long milli_sec)
{
	int				err;
	struct timespec	abstime;
	XTThreadPtr		me = self ? self : xt_get_self();

#ifdef XT_WIN
	union ft64		now;
  
	GetSystemTimeAsFileTime(&now.ft);

	/* System time is measured in 100ns units.
	 * This calculation will be reversed by the Windows implementation
	 * of pthread_cond_timedwait(), in order to extract the
	 * milli-second timeout!
	 */
	abstime.tv.i64 = now.i64 + (milli_sec * 10000);
  
	abstime.max_timeout_msec = milli_sec;
#else
	struct timeval	now;
	u_llong			micro_sec;

	/* Get the current time in microseconds: */
	gettimeofday(&now, NULL);
	micro_sec = (u_llong) now.tv_sec * (u_llong) 1000000 + (u_llong) now.tv_usec;
	
	/* Add the timeout which is in milli seconds */
	micro_sec += (u_llong) milli_sec * (u_llong) 1000;

	/* Setup the end time, which is in nano-seconds. */
	abstime.tv_sec = (long) (micro_sec / 1000000);				/* seconds */
	abstime.tv_nsec = (long) ((micro_sec % 1000000) * 1000);	/* and nanoseconds */
#endif

	me->t_disable_interrupts = TRUE;
	err = xt_p_cond_timedwait(cond, mutex, &abstime);
	me->t_disable_interrupts = FALSE;
	if (err && err != ETIMEDOUT) {
		xt_throw_errno(XT_CONTEXT, err);
		return FALSE;
	}
	if (me->t_delayed_signal) {
		xt_throw_delayed_signal(XT_CONTEXT);
		return FALSE;
	}
	return TRUE;
}

xtPublic xtBool xt_signal_cond(XTThreadPtr self, xt_cond_type *cond)
{
	int err;

	err = pthread_cond_signal(cond);
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return FAILED;
	}
	return OK;
}

xtPublic void xt_broadcast_cond(XTThreadPtr self, xt_cond_type *cond)
{
	int err;

	err = pthread_cond_broadcast(cond);
	if (err)
		xt_throw_errno(XT_CONTEXT, err);
}

xtPublic xtBool xt_broadcast_cond_ns(xt_cond_type *cond)
{
	int err;

	err = pthread_cond_broadcast(cond);
	if (err) {
		xt_register_errno(XT_REG_CONTEXT, err);
		return FAILED;
	}
	return OK;
}

static int prof_setjmp_count = 0;

xtPublic int prof_setjmp(void)
{
	prof_setjmp_count++;
	return 0;
}

xtPublic void xt_set_low_priority(XTThreadPtr self)
{
	int err = xt_p_set_low_priority(self->t_pthread);
	if (err) {
		self = NULL; /* Will cause logging, instead of throwing exception */
		xt_throw_errno(XT_CONTEXT, err);
	}
}

xtPublic void xt_set_normal_priority(XTThreadPtr self)
{
	int err = xt_p_set_normal_priority(self->t_pthread);
	if (err) {
		self = NULL; /* Will cause logging, instead of throwing exception */
		xt_throw_errno(XT_CONTEXT, err);
	}
}

xtPublic void xt_set_high_priority(XTThreadPtr self)
{
	int err = xt_p_set_high_priority(self->t_pthread);
	if (err) {
		self = NULL; /* Will cause logging, instead of throwing exception */
		xt_throw_errno(XT_CONTEXT, err);
	}
}

xtPublic void xt_set_priority(XTThreadPtr self, int priority)
{
	if (priority < XT_PRIORITY_NORMAL)
		xt_set_low_priority(self);
	else if (priority > XT_PRIORITY_NORMAL)
		xt_set_high_priority(self);
	else
		xt_set_normal_priority(self);
}

/*
 * -----------------------------------------------------------------------
 * STATISTICS
 */

xtPublic void xt_gather_statistics(XTStatisticsPtr stats)
{
	XTThreadPtr *thr;
	xtWord8		s;

	xt_lock_mutex_ns(&thr_array_lock);
	*stats = thr_statistics;
	// Ignore index 0, it is not used!
	thr = &xt_thr_array[1];
	for (u_int i=1; i<xt_thr_current_max_threads; i++) {
		if (*thr) {
			stats->st_commits += (*thr)->st_statistics.st_commits;
			stats->st_rollbacks += (*thr)->st_statistics.st_rollbacks;
			stats->st_stat_read += (*thr)->st_statistics.st_stat_read;
			stats->st_stat_write += (*thr)->st_statistics.st_stat_write;

			XT_ADD_STATS(stats->st_rec, (*thr)->st_statistics.st_rec);
			if ((s = (*thr)->st_statistics.st_rec.ts_flush_start))
				stats->st_rec.ts_flush_time += xt_trace_clock() - s;
			stats->st_rec_cache_hit += (*thr)->st_statistics.st_rec_cache_hit;
			stats->st_rec_cache_miss += (*thr)->st_statistics.st_rec_cache_miss;
			stats->st_rec_cache_frees += (*thr)->st_statistics.st_rec_cache_frees;

			XT_ADD_STATS(stats->st_ind, (*thr)->st_statistics.st_ind);
			if ((s = (*thr)->st_statistics.st_ind.ts_flush_start))
				stats->st_ind.ts_flush_time += xt_trace_clock() - s;
			stats->st_ind_cache_hit += (*thr)->st_statistics.st_ind_cache_hit;
			stats->st_ind_cache_miss += (*thr)->st_statistics.st_ind_cache_miss;
			XT_ADD_STATS(stats->st_ilog, (*thr)->st_statistics.st_ilog);

			XT_ADD_STATS(stats->st_xlog, (*thr)->st_statistics.st_xlog);
			if ((s = (*thr)->st_statistics.st_xlog.ts_flush_start))
				stats->st_xlog.ts_flush_time += xt_trace_clock() - s;
			stats->st_xlog_cache_hit += (*thr)->st_statistics.st_xlog_cache_hit;
			stats->st_xlog_cache_miss += (*thr)->st_statistics.st_xlog_cache_miss;

			XT_ADD_STATS(stats->st_data, (*thr)->st_statistics.st_data);
			if ((s = (*thr)->st_statistics.st_data.ts_flush_start))
				stats->st_data.ts_flush_time += xt_trace_clock() - s;

			stats->st_scan_index += (*thr)->st_statistics.st_scan_index;
			stats->st_scan_table += (*thr)->st_statistics.st_scan_table;
			stats->st_row_select += (*thr)->st_statistics.st_row_select;
			stats->st_row_insert += (*thr)->st_statistics.st_row_insert;
			stats->st_row_update += (*thr)->st_statistics.st_row_update;
			stats->st_row_delete += (*thr)->st_statistics.st_row_delete;

			stats->st_wait_for_xact += (*thr)->st_statistics.st_wait_for_xact;
			stats->st_retry_index_scan += (*thr)->st_statistics.st_retry_index_scan;
			stats->st_reread_record_list += (*thr)->st_statistics.st_reread_record_list;
		}
		thr++;
	}
	xt_unlock_mutex_ns(&thr_array_lock);
}

static void thr_accumulate_statistics(XTThreadPtr self)
{
	thr_statistics.st_commits += self->st_statistics.st_commits;
	thr_statistics.st_rollbacks += self->st_statistics.st_rollbacks;
	thr_statistics.st_stat_read += self->st_statistics.st_stat_read;
	thr_statistics.st_stat_write += self->st_statistics.st_stat_write;

	XT_ADD_STATS(thr_statistics.st_rec, self->st_statistics.st_rec);
	thr_statistics.st_rec_cache_hit += self->st_statistics.st_rec_cache_hit;
	thr_statistics.st_rec_cache_miss += self->st_statistics.st_rec_cache_miss;
	thr_statistics.st_rec_cache_frees += self->st_statistics.st_rec_cache_frees;

	XT_ADD_STATS(thr_statistics.st_ind, self->st_statistics.st_ind);
	thr_statistics.st_ind_cache_hit += self->st_statistics.st_ind_cache_hit;
	thr_statistics.st_ind_cache_miss += self->st_statistics.st_ind_cache_miss;
	XT_ADD_STATS(thr_statistics.st_ilog, self->st_statistics.st_ilog);

	XT_ADD_STATS(thr_statistics.st_xlog, self->st_statistics.st_xlog);
	thr_statistics.st_xlog_cache_hit += self->st_statistics.st_xlog_cache_hit;
	thr_statistics.st_xlog_cache_miss += self->st_statistics.st_xlog_cache_miss;

	XT_ADD_STATS(thr_statistics.st_data, self->st_statistics.st_data);

	thr_statistics.st_scan_index += self->st_statistics.st_scan_index;
	thr_statistics.st_scan_table += self->st_statistics.st_scan_table;
	thr_statistics.st_row_select += self->st_statistics.st_row_select;
	thr_statistics.st_row_insert += self->st_statistics.st_row_insert;
	thr_statistics.st_row_update += self->st_statistics.st_row_update;
	thr_statistics.st_row_delete += self->st_statistics.st_row_delete;

	thr_statistics.st_wait_for_xact += self->st_statistics.st_wait_for_xact;
	thr_statistics.st_retry_index_scan += self->st_statistics.st_retry_index_scan;
	thr_statistics.st_reread_record_list += self->st_statistics.st_reread_record_list;
}

xtPublic u_llong xt_get_statistic(XTStatisticsPtr stats, XTDatabaseHPtr db, u_int rec_id)
{
	u_llong stat_value;

	switch (rec_id) {
		case XT_STAT_TIME_CURRENT:
			stat_value = (u_llong) time(NULL);
			break;
		case XT_STAT_TIME_PASSED:
			stat_value = (u_llong) xt_trace_clock();
			break;
		case XT_STAT_COMMITS:
			stat_value = stats->st_commits;
			break;
		case XT_STAT_ROLLBACKS:
			stat_value = stats->st_rollbacks;
			break;
		case XT_STAT_STAT_READS:
			stat_value = stats->st_stat_read;
			break;
		case XT_STAT_STAT_WRITES:
			stat_value = stats->st_stat_write;
			break;

		case XT_STAT_REC_BYTES_IN:
			stat_value = stats->st_rec.ts_read;
			break;
		case XT_STAT_REC_BYTES_OUT:
			stat_value = stats->st_rec.ts_write;
			break;
		case XT_STAT_REC_SYNC_COUNT:
			stat_value = stats->st_rec.ts_flush;
			break;
		case XT_STAT_REC_SYNC_TIME:
			stat_value = stats->st_rec.ts_flush_time;
			break;
		case XT_STAT_REC_CACHE_HIT:
			stat_value = stats->st_rec_cache_hit;
			break;
		case XT_STAT_REC_CACHE_MISS:
			stat_value = stats->st_rec_cache_miss;
			break;
		case XT_STAT_REC_CACHE_FREES:
			stat_value = stats->st_rec_cache_frees;
			break;
		case XT_STAT_REC_CACHE_USAGE:
			stat_value = (u_llong) xt_tc_get_usage();
			break;

		case XT_STAT_IND_BYTES_IN:
			stat_value = stats->st_ind.ts_read;
			break;
		case XT_STAT_IND_BYTES_OUT:
			stat_value = stats->st_ind.ts_write;
			break;
		case XT_STAT_IND_SYNC_COUNT:
			stat_value = stats->st_ind.ts_flush;
			break;
		case XT_STAT_IND_SYNC_TIME:
			stat_value = stats->st_ind.ts_flush_time;
			break;
		case XT_STAT_IND_CACHE_HIT:
			stat_value = stats->st_ind_cache_hit;
			break;
		case XT_STAT_IND_CACHE_MISS:
			stat_value = stats->st_ind_cache_miss;
			break;
		case XT_STAT_IND_CACHE_USAGE:
			stat_value = (u_llong) xt_ind_get_usage();
			break;
		case XT_STAT_ILOG_BYTES_IN:
			stat_value = stats->st_ilog.ts_read;
			break;
		case XT_STAT_ILOG_BYTES_OUT:
			stat_value = stats->st_ilog.ts_write;
			break;
		case XT_STAT_ILOG_SYNC_COUNT:
			stat_value = stats->st_ilog.ts_flush;
			break;
		case XT_STAT_ILOG_SYNC_TIME:
			stat_value = stats->st_ilog.ts_flush_time;
			break;

		case XT_STAT_XLOG_BYTES_IN:
			stat_value = stats->st_xlog.ts_read;
			break;
		case XT_STAT_XLOG_BYTES_OUT:
			stat_value = stats->st_xlog.ts_write;
			break;
		case XT_STAT_XLOG_SYNC_COUNT:
			stat_value = stats->st_xlog.ts_flush;
			break;
		case XT_STAT_XLOG_SYNC_TIME:
			stat_value = stats->st_xlog.ts_flush_time;
			break;
		case XT_STAT_XLOG_CACHE_HIT:
			stat_value = stats->st_xlog_cache_hit;
			break;
		case XT_STAT_XLOG_CACHE_MISS:
			stat_value = stats->st_xlog_cache_miss;
			break;
		case XT_STAT_XLOG_CACHE_USAGE:
			stat_value = (u_llong) xt_xlog_get_usage();
			break;

		case XT_STAT_DATA_BYTES_IN:
			stat_value = stats->st_data.ts_read;
			break;
		case XT_STAT_DATA_BYTES_OUT:
			stat_value = stats->st_data.ts_write;
			break;
		case XT_STAT_DATA_SYNC_COUNT:
			stat_value = stats->st_data.ts_flush;
			break;
		case XT_STAT_DATA_SYNC_TIME:
			stat_value = stats->st_data.ts_flush_time;
			break;

		case XT_STAT_BYTES_TO_CHKPNT:
			stat_value = db ? xt_bytes_since_last_checkpoint(db, db->db_xlog.xl_write_log_id, db->db_xlog.xl_write_log_offset) : 0;
			break;
		case XT_STAT_LOG_BYTES_TO_WRITE:
			stat_value = db ? db->db_xlog.xl_log_bytes_written - db->db_xlog.xl_log_bytes_read : 0;//db->db_xlog.xlog_bytes_to_write();
			break;
		case XT_STAT_BYTES_TO_SWEEP:
			/* This stat is potentially very expensive: */
			stat_value = db ? xt_xn_bytes_to_sweep(db, xt_get_self()) : 0;
			break;
		case XT_STAT_WAIT_FOR_XACT:
			stat_value = stats->st_wait_for_xact;
			break;
		case XT_STAT_XACT_TO_CLEAN:
			stat_value = db ? db->db_xn_curr_id + 1 - db->db_xn_to_clean_id : 0;
			break;
		case XT_STAT_SWEEPER_WAITS:
			stat_value = db ? db->db_stat_sweep_waits : 0;
			break;

		case XT_STAT_SCAN_INDEX:
			stat_value = stats->st_scan_index;
			break;
		case XT_STAT_SCAN_TABLE:
			stat_value = stats->st_scan_table;
			break;
		case XT_STAT_ROW_SELECT:
			stat_value = stats->st_row_select;
			break;
		case XT_STAT_ROW_INSERT:
			stat_value = stats->st_row_insert;
			break;
		case XT_STAT_ROW_UPDATE:
			stat_value = stats->st_row_update;
			break;
		case XT_STAT_ROW_DELETE:
			stat_value = stats->st_row_delete;
			break;

		case XT_STAT_RETRY_INDEX_SCAN:
			stat_value = stats->st_retry_index_scan;
			break;
		case XT_STAT_REREAD_REC_LIST:
			stat_value = stats->st_reread_record_list;
			break;
		default:
			stat_value = 0;
			break;
	}
	return stat_value;
}
