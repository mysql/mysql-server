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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "strutil_xt.h"

xtPublic void xt_strcpy(size_t size, char *to, c_char *from)
{
	if (size > 0) {
		size--;
		while (*from && size--)
			*to++ = *from++;
		*to = 0;
	}
}

xtPublic void xt_strncpy(size_t size, char *to, c_char *from, size_t len_from)
{
	if (size > 0) {
		size--;
		while (len_from-- && size--)
			*to++ = *from++;
		*to = 0;
	}
}

xtPublic void xt_strcpy_term(size_t size, char *to, c_char *from, char term)
{
	if (size > 0) {
		size--;
		while (*from && *from != term && size--)
			*to++ = *from++;
		*to = 0;
	}
}

xtPublic void xt_strcat_term(size_t size, char *to, c_char *from, char term)
{
	while (*to && size--) to++;
	if (size > 0) {
		size--;
		while (*from && *from != term && size--)
			*to++ = *from++;
		*to = 0;
	}
}

xtPublic void xt_strcat(size_t size, char *to, c_char *from)
{
	while (*to && size--) to++;
	xt_strcpy(size, to, from);
}

xtPublic void xt_strcati(size_t size, char *to, int i)
{
	char buffer[50];
	
	sprintf(buffer, "%d", i);
	xt_strcat(size, to, buffer);
}

xtPublic xtBool xt_ends_with(c_char *str, c_char *sub)
{
	unsigned long len = strlen(str);
	
	if (len >= strlen(sub))
		return strcmp(&str[len-strlen(sub)], sub) == 0;
	return FALSE;
}

xtPublic xtPublic xtBool xt_starts_with(c_char *str, c_char *sub)
{
	return (strstr(str, sub) == str);
}

/* This function returns "" if the path ends with a dir char */
xtPublic void xt_2nd_last_name_of_path(size_t size, char *dest, c_char *path)
{
	size_t	len;
	c_char	*ptr, *pend;

	len = strlen(path);
	if (!len) {
		*dest = 0;
		return;
	}
	ptr = path + len - 1;
	while (ptr != path && !XT_IS_DIR_CHAR(*ptr))
		ptr--;
	if (!XT_IS_DIR_CHAR(*ptr)) {
		*dest = 0;
		return;
	}
	pend = ptr;
	ptr--;
	while (ptr != path && !XT_IS_DIR_CHAR(*ptr))
		ptr--;
	if (XT_IS_DIR_CHAR(*ptr))
		ptr++;
	len = (size_t) (pend - ptr);
	if (len > size-1)
		len = size-1;
	memcpy(dest, ptr, len);
	dest[len] = 0;
}

/* This function returns "" if the path ends with a dir char */
xtPublic char *xt_last_name_of_path(c_char *path)
{
	size_t	length;
	c_char	*ptr;

	length = strlen(path);
	if (!length)
		return (char *) path;
	ptr = path + length - 1;
	while (ptr != path && !XT_IS_DIR_CHAR(*ptr)) ptr--;
	if (XT_IS_DIR_CHAR(*ptr)) ptr++;
	return (char *) ptr;
}

xtPublic char *xt_last_2_names_of_path(c_char *path)
{
	size_t	length;
	c_char	*ptr;

	length = strlen(path);
	if (!length)
		return (char *) path;
	ptr = path + length - 1;
	while (ptr != path && !XT_IS_DIR_CHAR(*ptr)) ptr--;
	if (XT_IS_DIR_CHAR(*ptr)) {
		ptr--;
		while (ptr != path && !XT_IS_DIR_CHAR(*ptr)) ptr--;
		if (XT_IS_DIR_CHAR(*ptr))
			ptr++;
	}
	return (char *) ptr;
}

xtPublic c_char *xt_last_directory_of_path(c_char *path)
/* This function returns the last name component, even if the path ends with a dir char */
{
	size_t	length;
	c_char	*ptr;

	length = strlen(path);
	if (!length)
		return(path);
	ptr = path + length - 1;
	/* Path may end with multiple slashes: */
	while (ptr != path && XT_IS_DIR_CHAR(*ptr))
		ptr--;
	while (ptr != path && !XT_IS_DIR_CHAR(*ptr))
		ptr--;
	if (XT_IS_DIR_CHAR(*ptr)) ptr++;
	return(ptr);
}

xtPublic char *xt_find_extension(c_char *file_name)
{
	c_char	*ptr;

	for (ptr = file_name + strlen(file_name) - 1; ptr >= file_name; ptr--) {
		if (XT_IS_DIR_CHAR(*ptr))
			break;
		if (*ptr == '.')
			return (char *) (ptr + 1);
	}
	return NULL;
}

xtPublic void xt_remove_extension(char *file_name)
{
	char *ptr = xt_find_extension(file_name);

	if (ptr)
		*(ptr - 1) = 0;
}

xtPublic xtBool xt_is_extension(c_char *file_name, c_char *ext)
{
	char *ptr;
	
	if (!(ptr = xt_find_extension(file_name)))
		return FALSE;
	return strcmp(ptr, ext) == 0;
}

/*
 * Optionally remove trailing directory delimiters (If the directory name consists of one
 * character, the directory delimiter is not removed).
 */
xtPublic xtBool xt_remove_dir_char(char *dir_name)
{
	size_t	length;
	xtBool	removed = FALSE;
	
	length = strlen(dir_name);
	while (length > 1 && XT_IS_DIR_CHAR(dir_name[length - 1])) {
		dir_name[length - 1] = '\0';
		length--;
		removed = TRUE;
	}
	return removed;
}

xtPublic void xt_remove_last_name_of_path(char *path)
{
	char *ptr;

	if ((ptr = xt_last_name_of_path(path)))
		*ptr = 0;
}

xtBool xt_add_dir_char(size_t max, char *path)
{
	size_t slen = strlen(path);

	if (slen >= max)
		return FALSE;

	if (slen == 0) {
		/* If no path is given we will be at the current working directory, under UNIX we must
		 * NOT add a directory delimiter character:
		 */
		return FALSE;
	}

	if (!XT_IS_DIR_CHAR(path[slen - 1])) {
		path[slen] = XT_DIR_CHAR;
		path[slen + 1] = '\0';
		return TRUE;
	}
	return FALSE;
}

xtPublic xtInt8 xt_str_to_int8(c_char *ptr, xtBool *overflow)
{
	xtInt8 value = 0;

	if (overflow)
		*overflow = FALSE;
	while (*ptr == '0') ptr++;
	if (!*ptr)
		value = (xtInt8) 0;
	else {
		sscanf(ptr, "%"PRId64, &value);
		if (!value && overflow)
			*overflow = TRUE;
	}
	return value;
}

xtPublic void xt_int8_to_str(xtInt8 value, char *string)
{
	sprintf(string, "%"PRId64, value);
}

xtPublic void xt_double_to_str(double value, int scale, char *string)
{
	char *ptr;

	sprintf(string, "%.*f", scale, value);
	ptr = string + strlen(string) - 1;
	
	if (strchr(string, '.') && (*ptr == '0' || *ptr == '.')) {
		while (ptr-1 > string && *(ptr-1) == '0') ptr--;
		if (ptr-1 > string && *(ptr-1) == '.') ptr--;
		*ptr = 0;
	}
}

/*
 * This function understand GB, MB, KB.
 */
xtPublic xtInt8 xt_byte_size_to_int8(c_char *ptr)
{
	char	number[101], *num_ptr;
	xtInt8	size;

	while (*ptr && isspace(*ptr))
		ptr++;

	num_ptr = number;
	while (*ptr && isdigit(*ptr)) {
		if (num_ptr < number+100) {
			*num_ptr = *ptr;
			num_ptr++;
		}
		ptr++;
	}
	*num_ptr = 0;
	size = xt_str_to_int8(number, NULL);

	while (*ptr && isspace(*ptr))
		ptr++;
	
	switch (toupper(*ptr)) {
		case 'P':
			size *= 1024;
		case 'T':
			size *= 1024;
		case 'G':
			size *= 1024;
		case 'M':
			size *= 1024;
		case 'K':
			size *= 1024;
			break;
	}
	
	return size;
}

xtPublic void xt_int8_to_byte_size(xtInt8 value, char *string)
{
	double	v;
	c_char	*unit;
	char	val_str[100];

	if (value >= (xtInt8) (1024 * 1024 * 1024)) {
		v = (double) value / (double) (1024 * 1024 * 1024);
		unit = "GB";
	}
	else if (value >= (xtInt8) (1024 * 1024)) {
		v = (double) value / (double) (1024 * 1024);
		unit = "MB";
	}
	else if (value >= (xtInt8) 1024) {
		v = (double) value / (double) (1024);
		unit = "Kb";
	}
	else {
		v = (double) value;
		unit = "bytes";
	}
	
	xt_double_to_str(v, 2, val_str);
	sprintf(string, "%s %s (%"PRId64" bytes)", val_str, unit, value);
}

/* Version number must also be set in configure.in! */
xtPublic c_char *xt_get_version(void)
{
	return "1.0.08d RC";
}

/* Copy and URL decode! */
xtPublic void xt_strcpy_url(size_t size, char *to, c_char *from)
{
	if (size > 0) {
		size--;
		while (*from && size--) {
			if (*from == '%' && isxdigit(*(from+1)) && isxdigit(*(from+2))) {
				unsigned char a = xt_hex_digit(*(from+1));
				unsigned char b = xt_hex_digit(*(from+2));
				*to++ = a << 4 | b;
				from += 3;
			}
			else
				*to++ = *from++;
		}
		*to = 0;
	}
}

/* Copy and URL decode! */
xtPublic void xt_strncpy_url(size_t size, char *to, c_char *from, size_t len_from)
{
	if (size > 0) {
		size--;
		while (len_from-- && size--) {
			if (*from == '%' && len_from >= 2 && isxdigit(*(from+1)) && isxdigit(*(from+2))) {
				unsigned char a = xt_hex_digit(*(from+1));
				unsigned char b = xt_hex_digit(*(from+2));
				*to++ = a << 4 | b;
				from += 3;
			}
			else
				*to++ = *from++;
		}
		*to = 0;
	}
}

/* Returns a pointer to the end of the string if nothing found! */
const char *xt_strchr(const char *str, char ch)
{
	while (*str && *str != ch) str++;
	return str;
}

unsigned char xt_hex_digit(char ch)
{
	if (isdigit(ch))
		return((unsigned char) ch - (unsigned char) '0');

	ch = toupper(ch);
	if (ch >= 'A' && ch <= 'F')
		return((unsigned char) ch - (unsigned char) 'A' + (unsigned char) 10);

	return((unsigned char) 0);
}

#ifdef XT_WIN
xtPublic void xt_win_dialog(char *message)
{
	MessageBoxA(NULL, message, "Debug Me!", MB_ICONWARNING | MB_OK);
}
#endif

/*
 * --------------- SYSTEM STATISTICS ------------------
 */

static char					su_t_unit[10] = "usec";
/*
 * Note times, are return in microseconds, but the display in xtstat is currently
 * in milliseconds.
 */
static XTStatMetaDataRec	pbxt_stat_meta_data[XT_STAT_MAXIMUM] = {
	{ XT_STAT_TIME_CURRENT,	"Current Time",				"time",	"curr",		XT_STAT_DATE,
		"The current time in seconds" },
	{ XT_STAT_TIME_PASSED,	"Time Since Last Call",		"time",	su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE,
		"Time passed in %sseconds since last statistics call" },

	{ XT_STAT_COMMITS,			"Commit Count",			"xact", "commt",	XT_STAT_ACCUMULATIVE,
		"Number of transactions committed" },
	{ XT_STAT_ROLLBACKS,		"Rollback Count",		"xact", "rollb",	XT_STAT_ACCUMULATIVE,
		"Number of transactions rolled back" },
	{ XT_STAT_WAIT_FOR_XACT,	"Wait for Xact Count",	"xact", "waits",	XT_STAT_ACCUMULATIVE,
		"Number of times waited for another transaction" },
	{ XT_STAT_XACT_TO_CLEAN,	"Dirty Xact Count",		"xact", "dirty",	0,
		"Number of transactions still to be cleaned up" },

	{ XT_STAT_STAT_READS,		"Read Statements",		"stat", "read",		XT_STAT_ACCUMULATIVE,
		"Number of SELECT statements" },
	{ XT_STAT_STAT_WRITES,		"Write Statements",		"stat", "write",	XT_STAT_ACCUMULATIVE,
		"Number of UPDATE/INSERT/DELETE statements" },

	{ XT_STAT_REC_BYTES_IN,		"Record Bytes Read",	"rec", "in",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes read from the record/row files" },
	{ XT_STAT_REC_BYTES_OUT,	"Record Bytes Written",	"rec", "out",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes written from the record/row files" },
	{ XT_STAT_REC_SYNC_COUNT,	"Record File Flushes",	"rec", "syncs",		XT_STAT_ACCUMULATIVE | XT_STAT_COMBO_FIELD,
		"Number of flushes to record/row files" },
	{ XT_STAT_REC_SYNC_TIME,	"Record Flush Time",	"rec", su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE | XT_STAT_COMBO_FIELD_2,
		"The time in %sseconds to flush record/row files" },
	{ XT_STAT_REC_CACHE_HIT,	"Record Cache Hits",	"rec", "hits",		XT_STAT_ACCUMULATIVE,
		"Hits when accessing the record cache" },
	{ XT_STAT_REC_CACHE_MISS,	"Record Cache Misses",	"rec", "miss",		XT_STAT_ACCUMULATIVE,
		"Misses when accessing the record cache" },
	{ XT_STAT_REC_CACHE_FREES,	"Record Cache Frees",	"rec", "frees",		XT_STAT_ACCUMULATIVE,
		"Number of record cache pages freed" },
	{ XT_STAT_REC_CACHE_USAGE,	"Record Cache Usage",	"rec", "%use",		XT_STAT_PERCENTAGE,
		"Percentage of record cache in use" },

	{ XT_STAT_IND_BYTES_IN,		"Index Bytes Read",		"ind", "in",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes read from the index files" },
	{ XT_STAT_IND_BYTES_OUT,	"Index Bytes Written",	"ind", "out",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes written from the index files" },
	{ XT_STAT_IND_SYNC_COUNT,	"Index File Flushes",	"ind", "syncs",		XT_STAT_ACCUMULATIVE | XT_STAT_COMBO_FIELD,
		"Number of flushes to index files" },
	{ XT_STAT_IND_SYNC_TIME,	"Index Flush Time",		"ind", su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE | XT_STAT_COMBO_FIELD_2,
		"The time in %sseconds to flush index files" },
	{ XT_STAT_IND_CACHE_HIT,	"Index Cache Hits",		"ind", "hits",		XT_STAT_ACCUMULATIVE,
		"Hits when accessing the index cache" },
	{ XT_STAT_IND_CACHE_MISS,	"Index Cache Misses",	"ind", "miss",		XT_STAT_ACCUMULATIVE,
		"Misses when accessing the index cache" },
	{ XT_STAT_IND_CACHE_USAGE,	"Index Cache Usage",	"ind", "%use",		XT_STAT_PERCENTAGE,
		"Percentage of index cache used" },
	{ XT_STAT_ILOG_BYTES_IN,	"Index Log Bytes In",	"ilog", "in",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes read from the index log files" },
	{ XT_STAT_ILOG_BYTES_OUT,	"Index Log Bytes Out",	"ilog", "out",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes written from the index log files" },
	{ XT_STAT_ILOG_SYNC_COUNT,	"Index Log File Syncs",	"ilog", "syncs",	XT_STAT_ACCUMULATIVE | XT_STAT_COMBO_FIELD,
		"Number of flushes to index log files" },
	{ XT_STAT_ILOG_SYNC_TIME,	"Index Log Sync Time",	"ilog", su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE | XT_STAT_COMBO_FIELD_2,
		"The time in %sseconds to flush index log files" },

	{ XT_STAT_XLOG_BYTES_IN,	"Xact Log Bytes In",	"xlog", "in",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes read from the transaction log files" },
	{ XT_STAT_XLOG_BYTES_OUT,	"Xact Log Bytes Out",	"xlog", "out",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes written from the transaction log files" },
	{ XT_STAT_XLOG_SYNC_COUNT,	"Xact Log File Syncs",	"xlog", "syncs",	XT_STAT_ACCUMULATIVE,
		"Number of flushes to transaction log files" },
	{ XT_STAT_XLOG_SYNC_TIME,	"Xact Log Sync Time",	"xlog", su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE,
		"The time in %sseconds to flush transaction log files" },
	{ XT_STAT_XLOG_CACHE_HIT,	"Xact Log Cache Hits",	"xlog", "hits",		XT_STAT_ACCUMULATIVE,
		"Hits when accessing the transaction log cache" },
	{ XT_STAT_XLOG_CACHE_MISS,	"Xact Log Cache Misses","xlog", "miss",		XT_STAT_ACCUMULATIVE,
		"Misses when accessing the transaction log cache" },
	{ XT_STAT_XLOG_CACHE_USAGE,	"Xact Log Cache Usage",	"xlog", "%use",		XT_STAT_PERCENTAGE,
		"Percentage of transaction log cache used" },

	{ XT_STAT_DATA_BYTES_IN,	"Data Log Bytes In",	"data", "in",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes read from the data log files" },
	{ XT_STAT_DATA_BYTES_OUT,	"Data Log Bytes Out",	"data", "out",		XT_STAT_ACCUMULATIVE | XT_STAT_BYTE_COUNT,
		"Bytes written from the data log files" },
	{ XT_STAT_DATA_SYNC_COUNT,	"Data Log File Syncs",	"data", "syncs",	XT_STAT_ACCUMULATIVE,
		"Number of flushes to data log files" },
	{ XT_STAT_DATA_SYNC_TIME,	"Data Log Sync Time",	"data", su_t_unit,	XT_STAT_ACCUMULATIVE | XT_STAT_TIME_VALUE,
		"The time in %sseconds to flush data log files" },

	{ XT_STAT_BYTES_TO_CHKPNT,	"Bytes to Checkpoint",	"to", "chkpt",		XT_STAT_BYTE_COUNT,
		"Bytes written to the log since the last checkpoint" },
	{ XT_STAT_LOG_BYTES_TO_WRITE, "Log Bytes to Write",	"to", "write",		XT_STAT_BYTE_COUNT,
		"Bytes written to the log, still to be written to the database" },
	{ XT_STAT_BYTES_TO_SWEEP,	"Log Bytes to Sweep",	"to", "sweep",		XT_STAT_BYTE_COUNT,
		"Bytes written to the log, still to be read by the sweeper" },
	{ XT_STAT_SWEEPER_WAITS,	"Sweeper Wait on Xact",	"sweep", "waits",	XT_STAT_ACCUMULATIVE,
		"Attempts to cleanup a transaction" },

	{ XT_STAT_SCAN_INDEX,		"Index Scan Count",		"scan", "index",	XT_STAT_ACCUMULATIVE,
		"Number of index scans" },
	{ XT_STAT_SCAN_TABLE,		"Table Scan Count",		"scan", "table",	XT_STAT_ACCUMULATIVE,
		"Number of table scans" },
	{ XT_STAT_ROW_SELECT,		"Select Row Count",		"row", "sel",		XT_STAT_ACCUMULATIVE,
		"Number of rows selected" },
	{ XT_STAT_ROW_INSERT,		"Insert Row Count",		"row", "ins",		XT_STAT_ACCUMULATIVE,
		"Number of rows inserted" },
	{ XT_STAT_ROW_UPDATE,		"Update Row Count",		"row", "upd",		XT_STAT_ACCUMULATIVE,
		"Number of rows updated" },
	{ XT_STAT_ROW_DELETE,		"Delete Row Count",		"row", "del",		XT_STAT_ACCUMULATIVE,
		"Number of rows deleted" },

	{ XT_STAT_RETRY_INDEX_SCAN,	"Index Scan Retries",	"retry", "iscan",	XT_STAT_ACCUMULATIVE,
		"Index scans restarted because of locked record" },
	{ XT_STAT_REREAD_REC_LIST,	"Record List Rereads",	"retry", "rlist",	XT_STAT_ACCUMULATIVE,
		"Record list rescanned due to lock" }
};

xtPublic XTStatMetaDataPtr xt_get_stat_meta_data(int i)
{
	return &pbxt_stat_meta_data[i];
}

xtPublic void xt_set_time_unit(const char *u)
{
	xt_strcpy(10, su_t_unit, u);
}

