/***********************************************************************

Copyright (c) 2010, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

***********************************************************************/

/**************************************************//**
@file include/srv0mon.h
Server monitor counter related defines

Created 12/15/2009	Jimmy Yang
*******************************************************/

#ifndef srv0mon_h
#define srv0mon_h

#include "univ.i"


/** Possible status values for "mon_status" in "struct monitor_value" */
enum monitor_running_status {
	MONITOR_STARTED = 1,	/*!< Monitor has been turned on */
	MONITOR_STOPPED = 2	/*!< Monitor has been turned off */
};

typedef enum monitor_running_status	monitor_running_t;

/** Two monitor structures are defined in this file. One is
"monitor_value_t" which contains dynamic counter values for each
counter. The other is "monitor_info_t", which contains
static information (counter name, desc etc.) for each counter.
In addition, an enum datatype "monitor_id_t" is also defined,
it identifies each monitor with an internally used symbol, whose
integer value indexes into above two structure for its dynamic
and static information.
Developer who intend to add new counters would require to
fill in counter information as described in "monitor_info_t" and
create the internal counter ID in "monitor_id_t". */

/** Structure containing the actual values of a monitor counter. */
struct monitor_value_struct {
	ib_time_t	mon_start_time;	/*!< Start time of monitoring  */
	ib_time_t	mon_stop_time;	/*!< Stop time of monitoring */
	ib_time_t	mon_reset_time;	/*!< Time counter resetted */
	lint		mon_value;	/*!< Current counter Value */
	lint		mon_max_value;	/*!< Current Max value */
	lint		mon_min_value;	/*!< Current Min value */
	lint		mon_value_reset;/*!< value at last reset */
	lint		mon_max_value_start; /*!< Max value since start */
	lint		mon_min_value_start; /*!< Min value since start */
	lint		mon_start_value;/*!< Value at the start time */
	lint		mon_last_value;	/*!< Last set of values */
	monitor_running_t mon_status;	/* whether monitor still running */
};

typedef struct monitor_value_struct	monitor_value_t;

/** Follwoing defines are possible values for "monitor_type" field in
"struct monitor_info" */
enum monitor_type_value {
	MONITOR_MODULE = 1,	/*!< This is a monitor module type,
				not a counter */
	MONITOR_EXISTING = 2,	/*!< The monitor carries information from
				an existing system status variable */
	MONITOR_AVERAGE = 4,	/*!< Set this status if we want to
				calculate the average value (over time)
				for the counter. */
	MONITOR_DISPLAY_CURRENT = 8, /*!< Display current value of the
				counter, rather than incremental value
				over the period. Mostly for counters
				displaying current resource usage */
	MONITOR_GROUP_MODULE = 16 /*!< Monitor can be turned on/off
				only as a module, but not individually */
};

typedef enum monitor_type_value	monitor_type_t;

/** Counter minimum value is initialized to be max value of lint */
#define	MIN_RESERVED		((lint)(ULINT_MAX >> 1))
#define	MAX_RESERVED		(~MIN_RESERVED)

/** This enumeration defines internal monitor identifier used internally
to identify each particular counter. Its value indexes into two arrays,
one is the "innodb_counter_value" array which records actual monitor
counter values, the other is "innodb_counter_info" array which describes
each counter's basic information (name, desc etc.). A couple of
naming rules here:
1) If the monitor defines a module, it starts with MONITOR_MODULE
2) If the monitor uses exisitng counters from "status variable", its ID
name shall start with MONITOR_OVLD

Please refer to "innodb_counter_info" in srv/srv0mon.c for detail
information for each monitor counter */

enum monitor_id_value {
	/* This is to identify the default value set by the metrics
	control global variables */
	MONITOR_DEFAULT_START = 0,

	/* Start of Metadata counter */
	MONITOR_MODULE_METADATA,
	MONITOR_TABLE_OPEN,
	MONITOR_TABLE_CLOSE,

	/* Lock manager related counters */
	MONITOR_MODULE_LOCK,
	MONITOR_DEADLOCK,
	MONITOR_TIMEOUT,
	MONITOR_LOCKREC_WAIT,
	MONITOR_NUM_RECLOCK_REQ,
	MONITOR_RECLOCK_CREATED,
	MONITOR_RECLOCK_REMOVED,
	MONITOR_NUM_RECLOCK,
	MONITOR_TABLELOCK_CREATED,
	MONITOR_TABLELOCK_REMOVED,
	MONITOR_NUM_TABLELOCK,
	MONITOR_ROW_LOCK_CURRENT_WAIT,
	MONITOR_LOCK_WAIT_TIME,
	MONITOR_OVLD_ROW_LOCK_WAIT,

	/* Buffer and I/O realted counters. */
	MONITOR_MODULE_BUFFER,
	MONITOR_OVLD_BUF_POOL_READS,
	MONITOR_OVLD_BUF_POOL_READ_REQUESTS,
	MONITOR_OVLD_BUF_POOL_WRITE_REQUEST,
	MONITOR_PAGE_INFLUSH,
	MONITOR_OVLD_BUF_POOL_WAIT_FREE,
	MONITOR_OVLD_BUF_POOL_READ_AHEAD,
	MONITOR_OVLD_BUF_POOL_READ_AHEAD_EVICTED,
	MONITOR_OVLD_BUF_POOL_PAGE_TOTAL,
	MONITOR_OVLD_BUF_POOL_PAGE_MISC,
	MONITOR_OVLD_BUF_POOL_PAGES_DATA,
	MONITOR_OVLD_BUF_POOL_PAGES_DIRTY,
	MONITOR_OVLD_BUF_POOL_PAGES_FREE,
	MONITOR_OVLD_PAGE_CREATED,
	MONITOR_OVLD_PAGES_WRITTEN,
	MONITOR_OVLD_PAGES_READ,
	MONITOR_OVLD_BYTE_READ,
	MONITOR_OVLD_BYTE_WRITTEN,
	MONITOR_NUM_ADAPTIVE_FLUSHES,
	MONITOR_FLUSH_ADAPTIVE_PAGES,
	MONITOR_NUM_ASYNC_FLUSHES,
	MONITOR_FLUSH_ASYNC_PAGES,
	MONITOR_NUM_SYNC_FLUSHES,
	MONITOR_FLUSH_SYNC_PAGES,
	MONITOR_NUM_MAX_DIRTY_FLUSHES,
	MONITOR_FLUSH_MAX_DIRTY_PAGES,
	MONITOR_FLUSH_IO_CAPACITY_PCT,

	/* Buffer Page I/O specific counters. */
	MONITOR_MODULE_BUF_PAGE,
	MONITOR_INDEX_LEAF_PAGE_READ,
	MONITOR_INDEX_NON_LEAF_PAGE_READ,
	MONITOR_INDEX_IBUF_LEAF_PAGE_READ,
	MONITOR_INDEX_IBUF_NON_LEAF_PAGE_READ,
	MONITOR_UNDO_LOG_PAGE_READ,
	MONITOR_INODE_PAGE_READ,
	MONITOR_IBUF_FREELIST_PAGE_READ,
	MONITOR_IBUF_BITMAP_PAGE_READ,
	MONITOR_SYSTEM_PAGE_READ,
	MONITOR_TRX_SYSTEM_PAGE_READ,
	MONITOR_FSP_HDR_PAGE_READ,
	MONITOR_XDES_PAGE_READ,
	MONITOR_BLOB_PAGE_READ,
	MONITOR_ZBLOB_PAGE_READ,
	MONITOR_ZBLOB2_PAGE_READ,
	MONITOR_OTHER_PAGE_READ,
	MONITOR_INDEX_LEAF_PAGE_WRITTEN,
	MONITOR_INDEX_NON_LEAF_PAGE_WRITTEN,
	MONITOR_INDEX_IBUF_LEAF_PAGE_WRITTEN,
	MONITOR_INDEX_IBUF_NON_LEAF_PAGE_WRITTEN,
	MONITOR_UNDO_LOG_PAGE_WRITTEN,
	MONITOR_INODE_PAGE_WRITTEN,
	MONITOR_IBUF_FREELIST_PAGE_WRITTEN,
	MONITOR_IBUF_BITMAP_PAGE_WRITTEN,
	MONITOR_SYSTEM_PAGE_WRITTEN,
	MONITOR_TRX_SYSTEM_PAGE_WRITTEN,
	MONITOR_FSP_HDR_PAGE_WRITTEN,
	MONITOR_XDES_PAGE_WRITTEN,
	MONITOR_BLOB_PAGE_WRITTEN,
	MONITOR_ZBLOB_PAGE_WRITTEN,
	MONITOR_ZBLOB2_PAGE_WRITTEN,
	MONITOR_OTHER_PAGE_WRITTEN,

	/* OS level counters (I/O) */
	MONITOR_MODULE_OS,
	MONITOR_OVLD_OS_FILE_READ,
	MONITOR_OVLD_OS_FILE_WRITE,
	MONITOR_OVLD_OS_FSYNC,
	MONITOR_OS_PENDING_READS,
	MONITOR_OS_PENDING_WRITES,
	MONITOR_OVLD_OS_LOG_WRITTEN,
	MONITOR_OVLD_OS_LOG_FSYNC,
	MONITOR_OVLD_OS_LOG_PENDING_FSYNC,
	MONITOR_OVLD_OS_LOG_PENDING_WRITES,

	/* Transaction related counters */
	MONITOR_MODULE_TRX,
	MONITOR_TRX_COMMIT,
	MONITOR_TRX_ABORT,
	MONITOR_TRX_ACTIVE,
	MONITOR_NUM_ROW_PURGE,
	MONITOR_DML_PURGE_DELAY,
	MONITOR_RSEG_HISTORY_LEN,
	MONITOR_NUM_UNDO_SLOT_USED,
	MONITOR_NUM_UNDO_SLOT_CACHED,
	MONITOR_RSEG_CUR_SIZE,

	/* Recovery related counters */
	MONITOR_MODULE_RECOVERY,
	MONITOR_NUM_CHECKPOINT,
	MONITOR_LSN_FLUSHDISK,
	MONITOR_LSN_CHECKPOINT,
	MONITOR_LSN_CURRENT,
	MONITOR_PENDING_LOG_WRITE,
	MONITOR_PENDING_CHECKPOINT_WRITE,
	MONITOR_LOG_IO,
	MONITOR_OVLD_LOG_WAITS,
	MONITOR_OVLD_LOG_WRITE_REQUEST,
	MONITOR_OVLD_LOG_WRITES,

	MONITOR_FLUSH_DIRTY_PAGE_EXCEED,

	/* Page Manager related counters */
	MONITOR_MODULE_PAGE,
	MONITOR_PAGE_COMPRESS,
	MONITOR_PAGE_DECOMPRESS,

	/* Index related counters */
	MONITOR_MODULE_INDEX,
	MONITOR_INDEX_SPLIT,
	MONITOR_INDEX_MERGE,

	/* Tablespace related counters */
	MONITOR_MODULE_FIL_SYSTEM,
	MONITOR_OVLD_N_FILE_OPENED,

	/* Data DML related counters */
	MONITOR_MODULE_DMLSTATS,
	MONITOR_OLVD_ROW_READ,
	MONITOR_OLVD_ROW_INSERTED,
	MONITOR_OLVD_ROW_DELETED,
	MONITOR_OLVD_ROW_UPDTATED,

	/* This is used only for control system to turn
	on/off and reset all monitor counters */
	MONITOR_ALL_COUNTER,

	/* This must be the last member */
	NUM_MONITOR
};

typedef enum monitor_id_value		monitor_id_t;

/** This informs the monitor control system to turn
on/off and reset monitor counters through wild card match */
#define	MONITOR_WILDCARD_MATCH		(NUM_MONITOR + 1)

/** Cannot find monitor counter with a specified name */
#define	MONITOR_NO_MATCH		(NUM_MONITOR + 2)

/** struct monitor_info describes the basic/static information
about each monitor counter. */
struct monitor_info_struct {
	const char*	monitor_name;	/*!< Monitor name */
	const char*	monitor_module;	/*!< Sub Module the monitor
					belongs to */
	const char*	monitor_desc;	/*!< Brief desc of monitor counter */
	monitor_type_t	monitor_type;	/*!< Type of Monitor Info */
	monitor_id_t	monitor_id;	/*!< Monitor ID as defined in enum
					monitor_id_t */
};

typedef struct monitor_info_struct	monitor_info_t;

/** Following are the "set_option" values allowed for
srv_mon_process_existing_counter() and srv_mon_process_existing_counter()
functions. To turn on/off/reset the monitor counters. */
enum mon_set_option {
	MONITOR_TURN_ON = 1,		/*!< Turn on the counter */
	MONITOR_TURN_OFF,		/*!< Turn off the counter */
	MONITOR_RESET_VALUE,		/*!< Reset current values */
	MONITOR_RESET_ALL_VALUE,	/*!< Reset all values */
	MONITOR_GET_VALUE		/*!< Option for
					srv_mon_process_existing_counter()
					function */
};

typedef enum mon_set_option		mon_option_t;

/** Number of bit in a ulint datatype */
#define	NUM_BITS_ULINT	(sizeof(ulint) * CHAR_BIT)

/** This "monitor_set_tbl" is a bitmap records whether a particular monitor
counter has been turned on or off */
extern ulint		monitor_set_tbl[(NUM_MONITOR + NUM_BITS_ULINT - 1) /
					NUM_BITS_ULINT];

/** Macros to turn on/off the control bit in monitor_set_tbl for a monitor
counter option. */
#define MONITOR_ON(monitor)				\
	(monitor_set_tbl[monitor / NUM_BITS_ULINT] |=	\
			((ulint)1 << (monitor % NUM_BITS_ULINT)))

#define MONITOR_OFF(monitor)				\
	(monitor_set_tbl[monitor / NUM_BITS_ULINT] &=	\
			~((ulint)1 << (monitor % NUM_BITS_ULINT)))

/** Check whether the requested monitor is turned on/off */
#define MONITOR_IS_ON(monitor)				\
	(monitor_set_tbl[monitor / NUM_BITS_ULINT] &	\
			((ulint)1 << (monitor % NUM_BITS_ULINT)))

/** The actual monitor counter array that records each monintor counter
value */
extern monitor_value_t	 innodb_counter_value[NUM_MONITOR];

/** Following are macro defines for basic montior counter manipulations.
Please note we do not provide any synchronization for these monitor
operations due to performance consideration. Most counters can
be placed under existing mutex protections in respective code
module. */

/** Macros to access various fields of a monitor counters */
#define MONITOR_FIELD(monitor, field)			\
		(innodb_counter_value[monitor].field)

#define MONITOR_VALUE(monitor)				\
		MONITOR_FIELD(monitor, mon_value)

#define MONITOR_MAX_VALUE(monitor)			\
		MONITOR_FIELD(monitor, mon_max_value)

#define MONITOR_MIN_VALUE(monitor)			\
		MONITOR_FIELD(monitor, mon_min_value)

#define MONITOR_VALUE_RESET(monitor)			\
		MONITOR_FIELD(monitor, mon_value_reset)

#define MONITOR_MAX_VALUE_START(monitor)		\
		MONITOR_FIELD(monitor, mon_max_value_start)

#define MONITOR_MIN_VALUE_START(monitor)		\
		MONITOR_FIELD(monitor, mon_min_value_start)

#define MONITOR_LAST_VALUE(monitor)			\
		MONITOR_FIELD(monitor, mon_last_value)

#define MONITOR_START_VALUE(monitor)			\
		MONITOR_FIELD(monitor, mon_start_value)

#define MONITOR_VALUE_SINCE_START(monitor)		\
		(MONITOR_VALUE(monitor) + MONITOR_VALUE_RESET(monitor))

#define MONITOR_STATUS(monitor)				\
		MONITOR_FIELD(monitor, mon_status)

#define MONITOR_SET_START(monitor)					\
	do {								\
		MONITOR_STATUS(monitor) = MONITOR_STARTED;		\
		MONITOR_FIELD((monitor), mon_start_time) = time(NULL);	\
	} while (0)

#define MONITOR_SET_OFF(monitor)					\
	do {								\
		MONITOR_STATUS(monitor) = MONITOR_STOPPED;		\
		MONITOR_FIELD((monitor), mon_stop_time) = time(NULL);	\
	} while (0)

#ifdef HAVE_ATOMIC_BUILTINS
#define INC_VALUE(value, amount)					\
		(value = os_atomic_increment_lint(&value, amount))
#else
#define INC_VALUE(value, amount)	((value) += (amount))
#endif /* HAVE_ATOMIC_BUILTINS */

#define	MONITOR_INIT_ZERO_VALUE		0

/** Max and min values are initialized when we first turn on the monitor
counter, and set the MONITOR_STATUS. */
#define MONITOR_MAX_MIN_NOT_INIT(monitor)				\
		(MONITOR_STATUS(monitor) == MONITOR_INIT_ZERO_VALUE	\
		 && MONITOR_MIN_VALUE(monitor) == MONITOR_INIT_ZERO_VALUE \
		 && MONITOR_MAX_VALUE(monitor) == MONITOR_INIT_ZERO_VALUE)

#define MONITOR_INIT(monitor)						\
	if (MONITOR_MAX_MIN_NOT_INIT(monitor)) {			\
		MONITOR_MIN_VALUE(monitor) = MIN_RESERVED;		\
		MONITOR_MIN_VALUE_START(monitor) = MIN_RESERVED;	\
		MONITOR_MAX_VALUE(monitor) = MAX_RESERVED;		\
		MONITOR_MAX_VALUE_START(monitor) = MAX_RESERVED;	\
	}

/** Macros to increment/decrement the counters. The normal
monitor counter operation expects appropriate synchronization
already exists. No additional mutex is necessary when operating
on the counters */
#define	MONITOR_INC(monitor)						\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor)++;				\
		if (MONITOR_VALUE(monitor) > MONITOR_MAX_VALUE(monitor)) {  \
			MONITOR_MAX_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

#define	MONITOR_DEC(monitor)						\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor)--;				\
		if (MONITOR_VALUE(monitor) < MONITOR_MIN_VALUE(monitor)) {  \
			MONITOR_MIN_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

#define	MONITOR_INC_VALUE(monitor, value)				\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor) += (value);			\
		if (MONITOR_VALUE(monitor) > MONITOR_MAX_VALUE(monitor)) {  \
			MONITOR_MAX_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

#define	MONITOR_DEC_VALUE(monitor, value)				\
	if (MONITOR_IS_ON(monitor)) {					\
		ut_ad(MONITOR_VALUE(monitor) >= (value);		\
		MONITOR_VALUE(monitor) -= (value);			\
		if (MONITOR_VALUE(monitor) < MONITOR_MIN_VALUE(monitor)) {  \
			MONITOR_MIN_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

/* Increment/decrement counter without check the monitor on/off bit, which
could already be checked as a module group */
#define	MONITOR_INC_NOCHECK(monitor)					\
	do {								\
		MONITOR_VALUE(monitor)++;				\
		if (MONITOR_VALUE(monitor) > MONITOR_MAX_VALUE(monitor)) {  \
			MONITOR_MAX_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	} while (0)							\

#define	MONITOR_DEC_NOCHECK(monitor)					\
	do {								\
		MONITOR_VALUE(monitor)--;				\
		if (MONITOR_VALUE(monitor) < MONITOR_MIN_VALUE(monitor)) {  \
			MONITOR_MIN_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	} while (0)

/** Directly set a monitor counter's value */
#define	MONITOR_SET(monitor, value)					\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor) = (value);			\
		if (MONITOR_VALUE(monitor) > MONITOR_MAX_VALUE(monitor)) {  \
			MONITOR_MAX_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
		if (MONITOR_VALUE(monitor) < MONITOR_MIN_VALUE(monitor)) {  \
			MONITOR_MIN_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

/** Directly set a monitor counter's value, and if the value
is monotonically increasing, only max value needs to be updated */
#define	MONITOR_SET_UPD_MAX_ONLY(monitor, value)			\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor) = (value);			\
		if (MONITOR_VALUE(monitor) > MONITOR_MAX_VALUE(monitor)) {  \
			MONITOR_MAX_VALUE(monitor) = MONITOR_VALUE(monitor);\
		}							\
	}

/** Some values such as log sequence number are montomically increasing
number, do not need to record max/min values */
#define MONITOR_SET_SIMPLE(monitor, value)				\
	if (MONITOR_IS_ON(monitor)) {					\
		MONITOR_VALUE(monitor) = (lint)(value);			\
	}

/** Reset the monitor value and max/min value to zero. The reset
operation would only be conducted when the counter is turned off */
#define MONITOR_RESET_ALL(monitor)					\
	do {								\
		MONITOR_VALUE(monitor) = MONITOR_INIT_ZERO_VALUE;	\
		MONITOR_MAX_VALUE(monitor) = MAX_RESERVED;		\
		MONITOR_MIN_VALUE(monitor) = MIN_RESERVED;		\
		MONITOR_VALUE_RESET(monitor) = MONITOR_INIT_ZERO_VALUE;	\
		MONITOR_MAX_VALUE_START(monitor) = MAX_RESERVED;	\
		MONITOR_MIN_VALUE_START(monitor) = MIN_RESERVED;	\
		MONITOR_LAST_VALUE(monitor) = MONITOR_INIT_ZERO_VALUE;	\
		MONITOR_FIELD(monitor, mon_start_time) =		\
					MONITOR_INIT_ZERO_VALUE;	\
		MONITOR_FIELD(monitor, mon_stop_time) =			\
					MONITOR_INIT_ZERO_VALUE;	\
	} while (0)

/** Following four macros defines necessary operations to fetch and
consolidate information from existing system status variables. */

/** Save the passed-in value to mon_start_value field of monitor
counters */
#define MONITOR_SAVE_START(monitor, value)				\
	(MONITOR_START_VALUE(monitor) =					\
		 (value) - MONITOR_VALUE_RESET(monitor))

/** Save the passed-in value to mon_last_value field of monitor
counters */
#define MONITOR_SAVE_LAST(monitor)					\
	do {								\
		MONITOR_LAST_VALUE(monitor) = MONITOR_VALUE(monitor);	\
		MONITOR_START_VALUE(monitor) += MONITOR_VALUE(monitor);	\
	} while (0)

/** Set monitor value to the difference of value and mon_start_value
compensated by mon_last_value if accumulated value is required. */
#define MONITOR_SET_DIFF(monitor, value)				\
	MONITOR_SET_UPD_MAX_ONLY(monitor, ((value)			\
	- MONITOR_VALUE_RESET(monitor)					\
	- MONITOR_FIELD(monitor, mon_start_value)			\
	+ MONITOR_FIELD(monitor, mon_last_value)))

/****************************************************************//**
Get monitor's monitor_info_t by its monitor id (index into the
innodb_counter_info array
@return	Point to corresponding monitor_info_t, or NULL if no such
monitor */
UNIV_INTERN
monitor_info_t*
srv_mon_get_info(
/*=============*/
	monitor_id_t	monitor_id);	/*!< id index into the
					innodb_counter_info array */
/****************************************************************//**
Get monitor's name by its monitor id (index into the
innodb_counter_info array
@return	corresponding monitor name, or NULL if no such
monitor */
UNIV_INTERN
const char*
srv_mon_get_name(
/*=============*/
	monitor_id_t	monitor_id);	/*!< id index into the
					innodb_counter_info array */

/****************************************************************//**
Turn on/off/reset monitor counters in a module. If module_value
is NUM_MONITOR then turn on all monitor counters.
@return	0 if successful, or the first monitor that cannot be
turned on because it is already turned on. */
UNIV_INTERN
void
srv_mon_set_module_control(
/*=======================*/
	monitor_id_t	module_id,	/*!< in: Module ID as in
					monitor_counter_id. If it is
					set to NUM_MONITOR, this means
					we shall turn on all the counters */
	mon_option_t	set_option);	/*!< in: Turn on/off reset the
					counter */
/****************************************************************//**
This function consolidates some existing server counters used
by "system status variables". These existing system variables do not have
mechanism to start/stop and reset the counters, so we simulate these
controls by remembering the corresponding counter values when the
corresponding monitors are turned on/off/reset, and do appropriate
mathematics to deduct the actual value. */
UNIV_INTERN
void
srv_mon_process_existing_counter(
/*=============================*/
	monitor_id_t	monitor_id,	/*!< in: the monitor's ID as in
					monitor_counter_id */
	mon_option_t	set_option);	/*!< in: Turn on/off reset the
					counter */
/*************************************************************//**
This function is used to calculate the maximum counter value
since the start of monitor counter
@return	max counter value since start. */
UNIV_INLINE
lint
srv_mon_calc_max_since_start(
/*=========================*/
	monitor_id_t	monitor);	/*!< in: monitor id */
/*************************************************************//**
This function is used to calculate the minimum counter value
since the start of monitor counter
@return	min counter value since start. */
UNIV_INLINE
lint
srv_mon_calc_min_since_start(
/*=========================*/
	monitor_id_t	monitor);	/*!< in: monitor id*/
/*************************************************************//**
Reset a monitor, create a new base line with the current monitor
value. This baseline is recorded by MONITOR_VALUE_RESET(monitor) */
UNIV_INTERN
void
srv_mon_reset(
/*==========*/
	monitor_id_t	monitor);	/*!< in: monitor id*/
/*************************************************************//**
This function resets all values of a monitor counter */
UNIV_INLINE
void
srv_mon_reset_all(
/*==============*/
	monitor_id_t	monitor);	/*!< in: monitor id*/

#ifndef UNIV_NONINL
#include "srv0mon.ic"
#endif

#endif
