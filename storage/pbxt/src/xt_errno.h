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
 * Author: Paul McCullagh
 *
 * H&G2JCtL
 */

#define XT_NO_ERR					0
#define XT_SYSTEM_ERROR				-1
#define XT_ERR_STACK_OVERFLOW		-2
#define XT_ASSERTION_FAILURE		-3
#define XT_SIGNAL_CAUGHT			-4
#define XT_ERR_JUMP_OVERFLOW		-5
#define XT_ERR_BAD_HANDLE			-6
#define XT_ERR_TABLE_EXISTS			-7
#define XT_ERR_NAME_TOO_LONG		-8
#define XT_ERR_TABLE_NOT_FOUND		-9
#define XT_ERR_SESSION_NOT_FOUND	-10
#define XT_ERR_BAD_ADDRESS			-11
#define XT_ERR_UNKNOWN_SERVICE		-12
#define XT_ERR_UNKNOWN_HOST			-13
#define XT_ERR_TOKEN_EXPECTED		-14
#define XT_ERR_PROPERTY_REQUIRED	-15
#define XT_ERR_BAD_XACTION			-16
#define XT_ERR_INVALID_SLOT			-17
#define XT_ERR_DEADLOCK				-18
#define XT_ERR_CANNOT_CHANGE_DB		-19
#define XT_ERR_ILLEGAL_CHAR			-20
#define XT_ERR_UNTERMINATED_STRING	-21
#define XT_ERR_SYNTAX				-22
#define XT_ERR_ILLEGAL_INSTRUCTION	-23
#define XT_ERR_OUT_OF_BOUNDS		-24
#define XT_ERR_STACK_UNDERFLOW		-25
#define XT_ERR_TYPE_MISMATCH		-26
#define XT_ERR_ILLEGAL_TYPE			-27
#define XT_ERR_ID_TOO_LONG			-28
#define XT_ERR_TYPE_OVERFLOW		-29
#define XT_ERR_TABLE_IN_USE			-30
#define XT_ERR_NO_DATABASE_IN_USE	-31
#define XT_ERR_CANNOT_RESOLVE_TYPE	-32
#define XT_ERR_BAD_INDEX_DESC		-33
#define XT_ERR_WRONG_NO_OF_VALUES	-34
#define XT_ERR_CANNOT_OUTPUT_VALUE	-35
#define XT_ERR_COLUMN_NOT_FOUND		-36
#define XT_ERR_NOT_IMPLEMENTED		-37
#define XT_ERR_UNEXPECTED_EOS		-38
#define XT_ERR_BAD_TOKEN			-39
#define XT_ERR_RES_STACK_OVERFLOW	-40
#define XT_ERR_BAD_INDEX_TYPE		-41
#define XT_ERR_INDEX_EXISTS			-42
#define XT_ERR_INDEX_STRUC_EXISTS	-43
#define XT_ERR_INDEX_NOT_FOUND		-44
#define XT_ERR_INDEX_CORRUPT		-45
#define XT_ERR_DUPLICATE_KEY		-46
#define XT_ERR_TYPE_NOT_SUPPORTED	-47
#define XT_ERR_BAD_TABLE_VERSION	-48
#define XT_ERR_BAD_RECORD_FORMAT	-49
#define XT_ERR_BAD_EXT_RECORD		-50
#define XT_ERR_RECORD_CHANGED		-51			// Record has already been updated by some other transaction
#define XT_ERR_XLOG_WAS_CORRUPTED	-52
#define XT_ERR_NO_DICTIONARY		-53
#define XT_ERR_TOO_MANY_TABLES		-54			// Maximum number of table exceeded.
#define XT_ERR_KEY_TOO_LARGE		-55			// Maximum size of an index key exceeded
#define XT_ERR_MULTIPLE_DATABASES	-56
#define XT_ERR_NO_TRANSACTION		-57
#define XT_ERR_A_EXPECTED_NOT_B		-58
#define XT_ERR_NO_MATCHING_INDEX	-59
#define XT_ERR_TABLE_LOCKED			-60
#define XT_ERR_NO_REFERENCED_ROW	-61
#define XT_ERR_BAD_DICTIONARY		-62
#define XT_ERR_LOADING_MYSQL_DIC	-63
#define XT_ERR_ROW_IS_REFERENCED	-64
#define XT_ERR_COLUMN_IS_NOT_NULL	-65
#define XT_ERR_INCORRECT_NO_OF_COLS	-66
#define XT_ERR_FK_ON_TEMP_TABLE		-67
#define XT_ERR_REF_TABLE_NOT_FOUND	-68
#define XT_ERR_REF_TYPE_WRONG		-69
#define XT_ERR_DUPLICATE_FKEY		-70
#define XT_ERR_INDEX_FILE_TO_LARGE	-71
#define XT_ERR_UPGRADE_TABLE		-72
#define XT_ERR_INDEX_NEW_VERSION	-73
#define XT_ERR_LOCK_TIMEOUT			-74
#define XT_ERR_CONVERSION			-75
#define XT_ERR_NO_ROWS				-76
#define XT_ERR_MYSQL_ERROR			-77
#define XT_ERR_DATA_LOG_NOT_FOUND	-78
#define XT_ERR_LOG_MAX_EXCEEDED		-79
#define XT_ERR_MAX_ROW_COUNT		-80
#define XT_ERR_FILE_TOO_LONG		-81
#define XT_ERR_BAD_IND_BLOCK_SIZE	-82
#define XT_ERR_INDEX_CORRUPTED		-83
#define XT_ERR_NO_INDEX_CACHE		-84
#define XT_ERR_INDEX_LOG_CORRUPT	-85
#define XT_ERR_TOO_MANY_THREADS		-86
#define XT_ERR_TOO_MANY_WAITERS		-87
#define XT_ERR_INDEX_OLD_VERSION	-88
#define XT_ERR_PBXT_TABLE_EXISTS	-89
#define XT_ERR_SERVER_RUNNING		-90
#define XT_ERR_INDEX_MISSING		-91
#define XT_ERR_RECORD_DELETED		-92
#define XT_ERR_NEW_TYPE_OF_XLOG		-93
#define XT_ERR_NO_BEFORE_IMAGE		-94
#define XT_ERR_FK_REF_TEMP_TABLE	-95
#define XT_ERR_MYSQL_SHUTDOWN		-98
#define XT_ERR_MYSQL_NO_THREAD		-99

#ifdef XT_WIN
#define XT_ENOMEM					ERROR_NOT_ENOUGH_MEMORY
#define XT_EAGAIN					ERROR_RETRY
#define XT_EBUSY					ERROR_BUSY
#else
#define XT_ENOMEM					ENOMEM
#define XT_EAGAIN					EAGAIN
#define XT_EBUSY					EBUSY
#endif
