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

#ifndef __xt_strutil_h__
#define __xt_strutil_h__

#include <string.h>

#include "xt_defs.h"

#ifdef XT_WIN
#define XT_DIR_CHAR					'\\'
#define XT_IS_DIR_CHAR(c)			((c) == '/' || (c) == '\\')
#else
#define XT_DIR_CHAR					'/'
#define XT_IS_DIR_CHAR(c)			((c) == '/')
#endif

#define MAX_INT8_STRING_SIZE		100

void	xt_strcpy(size_t size, char *to, c_char *from);
void	xt_strncpy(size_t size, char *to, c_char *from, size_t len_from);
void	xt_strcat(size_t size, char *to, c_char *from);
void	xt_strcati(size_t size, char *to, int i);
void	xt_strcpy_term(size_t size, char *to, c_char *from, char term);
void	xt_strcat_term(size_t size, char *to, c_char *from, char term);

xtBool	xt_ends_with(c_char *str, c_char *sub);
xtBool	xt_starts_with(c_char *str, c_char *sub);

char	*xt_last_2_names_of_path(c_char *path);
char	*xt_last_name_of_path(c_char *path);
void	xt_2nd_last_name_of_path(size_t size, char *dest, c_char *path);
c_char	*xt_last_directory_of_path(c_char *path);
xtBool	xt_remove_dir_char(char *dir_name);
xtBool	xt_add_dir_char(size_t max, char *path);
void	xt_remove_last_name_of_path(char *path);
char	*xt_find_extension(c_char *file_name);
void	xt_remove_extension(char *file_name);
xtBool	xt_is_extension(c_char *file_name, c_char *ext);

xtInt8	xt_str_to_int8(c_char *ptr, xtBool *overflow);
void	xt_int8_to_str(xtInt8 value, char *string);
void	xt_double_to_str(double value, int scale, char *string);

xtInt8	xt_byte_size_to_int8(c_char *ptr);
void	xt_int8_to_byte_size(xtInt8 value, char *string);

c_char	*xt_get_version(void);

void xt_strcpy_url(size_t size, char *to, c_char *from);
void xt_strncpy_url(size_t size, char *to, c_char *from, size_t len_from);

const char		*xt_strchr(const char *str, char ch);
unsigned char	xt_hex_digit(char ch);

#define XT_STAT_TIME_CURRENT		0
#define XT_STAT_TIME_PASSED			1

#define XT_STAT_COMMITS				2
#define XT_STAT_ROLLBACKS			3
#define XT_STAT_WAIT_FOR_XACT		4
#define XT_STAT_XACT_TO_CLEAN		5

#define XT_STAT_STAT_READS			6
#define XT_STAT_STAT_WRITES			7

#define XT_STAT_REC_BYTES_IN		8
#define XT_STAT_REC_BYTES_OUT		9
#define XT_STAT_REC_SYNC_COUNT		10
#define XT_STAT_REC_SYNC_TIME		11
#define XT_STAT_REC_CACHE_HIT		12
#define XT_STAT_REC_CACHE_MISS		13
#define XT_STAT_REC_CACHE_FREES		14
#define XT_STAT_REC_CACHE_USAGE		15

#define XT_STAT_IND_BYTES_IN		16
#define XT_STAT_IND_BYTES_OUT		17
#define XT_STAT_IND_SYNC_COUNT		18
#define XT_STAT_IND_SYNC_TIME		19
#define XT_STAT_IND_CACHE_HIT		20
#define XT_STAT_IND_CACHE_MISS		21
#define XT_STAT_IND_CACHE_USAGE		22
#define XT_STAT_ILOG_BYTES_IN		23
#define XT_STAT_ILOG_BYTES_OUT		24
#define XT_STAT_ILOG_SYNC_COUNT		25
#define XT_STAT_ILOG_SYNC_TIME		26

#define XT_STAT_XLOG_BYTES_IN		27
#define XT_STAT_XLOG_BYTES_OUT		28
#define XT_STAT_XLOG_SYNC_COUNT		29
#define XT_STAT_XLOG_SYNC_TIME		30
#define XT_STAT_XLOG_CACHE_HIT		31
#define XT_STAT_XLOG_CACHE_MISS		32
#define XT_STAT_XLOG_CACHE_USAGE	33

#define XT_STAT_DATA_BYTES_IN		34
#define XT_STAT_DATA_BYTES_OUT		35
#define XT_STAT_DATA_SYNC_COUNT		36
#define XT_STAT_DATA_SYNC_TIME		37

#define XT_STAT_BYTES_TO_CHKPNT		38
#define XT_STAT_LOG_BYTES_TO_WRITE	39
#define XT_STAT_BYTES_TO_SWEEP		40
#define XT_STAT_SWEEPER_WAITS		41

#define XT_STAT_SCAN_INDEX			42
#define XT_STAT_SCAN_TABLE			43
#define XT_STAT_ROW_SELECT			44
#define XT_STAT_ROW_INSERT			45
#define XT_STAT_ROW_UPDATE			46
#define XT_STAT_ROW_DELETE			47

#define XT_STAT_CURRENT_MAX			48

#define XT_STAT_RETRY_INDEX_SCAN	48
#define XT_STAT_REREAD_REC_LIST		49
#define XT_STAT_MAXIMUM				50

#define XT_STAT_ACCUMULATIVE		1
#define XT_STAT_BYTE_COUNT			2
#define XT_STAT_PERCENTAGE			4
#define XT_STAT_COMBO_FIELD			8				/* Field is short, 2 chars instead of 5. */
#define XT_STAT_COMBO_FIELD_2		16				/* Field is short, 2 chars instead of 5. */
#define XT_STAT_TIME_VALUE			32
#define XT_STAT_DATE				64

typedef struct XTStatMetaData {
	int				sm_id;
	const char		*sm_name;
	const char		*sm_short_line_1;
	const char		*sm_short_line_2;
	int				sm_flags;
	const char		*sm_description;
} XTStatMetaDataRec, *XTStatMetaDataPtr;

XTStatMetaDataPtr	xt_get_stat_meta_data(int i);
void				xt_set_time_unit(const char *u);

#ifdef XT_WIN
void	xt_win_dialog(char *message);
#endif

#endif
