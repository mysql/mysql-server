/***********************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/
/**************************************************//**
@file innodb_engine.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef INNODB_ENGINE_H
#define INNODB_ENGINE_H

#include "config.h"

#include <pthread.h>

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>
#include <innodb_config.h>

/** default settings that determine the number of write operation for
a connection before committing the transaction */
#define	CONN_NUM_WRITE_COMMIT	1

/** default settings that determine the number of read operation for
a connection before committing the transaction */
#define CONN_NUM_READ_COMMIT	1048510

/** structure contains the cursor information for each connection */
typedef struct innodb_conn_data		innodb_conn_data_t;

#define UT_LIST_NODE_T(TYPE)						\
struct {								\
        TYPE *  prev;   /*!< pointer to the previous node,		\
                        NULL if start of list */			\
        TYPE *  next;   /*!< pointer to next node, NULL if end of list */\
}									\

#define UT_LIST_BASE_NODE_T(TYPE)					\
struct {								\
        int   count;  /*!< count of nodes in list */			\
        TYPE *  start;  /*!< pointer to list start, NULL if empty */	\
        TYPE *  end;    /*!< pointer to list end, NULL if empty */	\
}
									\
/** Connection specific data */
struct innodb_conn_data {
	ib_trx_t	c_ro_crsr_trx;	/*!< transaction for read only crsr */
	ib_crsr_t	c_ro_crsr;	/*!< read only cursor for the
					connection */
	ib_crsr_t	c_ro_idx_crsr;	/*!< index cursor for read */
	ib_trx_t	c_crsr_trx;	/*!< transaction for write cursor */
	ib_crsr_t       c_crsr;         /*!< data cursor */
	ib_crsr_t       c_idx_crsr;     /*!< index cursor */
	bool            c_in_use;       /*!< whether they are in use */
	void*		c_cookie;	/*!< connection cookie */
	uint64_t	c_r_count;	/*!< number of reads */
	uint64_t	c_r_count_commit;/*!< number of reads since
					last commit */
	uint64_t        c_w_count;	/*!< number of updates, including
					write/update/delete */
	uint64_t	c_w_count_commit;/*!< number of updates since
					last commit */
	void*		thd;		/*!< MySQL THD, used for binlog */
	void*		mysql_tbl;	/*!< MySQL TABLE, used for binlog */
	UT_LIST_NODE_T(innodb_conn_data_t) c_list; /*!< list ptr */
};

typedef UT_LIST_BASE_NODE_T(innodb_conn_data_t)	conn_base_t;

/** The InnoDB engine global data. Some layout are common to NDB memcached
engine and InnoDB memcached engine */
typedef struct innodb_engine {
	ENGINE_HANDLE_V1	engine;
	SERVER_HANDLE_V1	server;
	GET_SERVER_API		get_server_api;
	ENGINE_HANDLE*		m_default_engine;

	struct {
		size_t		nthreads;
		bool		cas_enabled;
	} server_options;

	union {
		engine_info	info;
		char		buffer[sizeof(engine_info)
				       * (LAST_REGISTERED_ENGINE_FEATURE + 1)];
	} info;

	/** following are InnoDB specific variables */
	bool			initialized;
	bool			connected;
	bool			enable_binlog;
	meta_info_t		meta_info;
	conn_base_t		conn_data;
	pthread_mutex_t		conn_mutex;
	ib_cb_t*		innodb_cb;
	uint64_t		r_batch_size;
	uint64_t		w_batch_size;
} innodb_engine_t;

/**********************************************************************//**
Unlock a table and commit the transaction
return 0 if fail to commit the transaction */
extern
int
handler_unlock_table(
/*=================*/
	void*	my_thd,			/*!< in: thread */
	void*	my_table,		/*!< in: Table metadata */
	int	my_lock_mode);		/*!< in: lock mode */


/** Some Macros to manipulate the list, extracted from "ut0lst.h" */
#define UT_LIST_INIT(BASE)						\
{									\
        (BASE).count = 0;						\
        (BASE).start = NULL;						\
        (BASE).end   = NULL;						\
}									\

#define UT_LIST_ADD_LAST(NAME, BASE, N)					\
{									\
        ((BASE).count)++;						\
        ((N)->NAME).prev = (BASE).end;					\
        ((N)->NAME).next = NULL;					\
        if ((BASE).end != NULL) {					\
                (((BASE).end)->NAME).next = (N);			\
        }								\
        (BASE).end = (N);						\
        if ((BASE).start == NULL) {					\
                (BASE).start = (N);					\
        }								\
}									\

#define UT_LIST_ADD_FIRST(NAME, BASE, N)				\
{									\
        ((BASE).count)++;						\
        ((N)->NAME).next = (BASE).start;				\
        ((N)->NAME).prev = NULL;					\
        if (UNIV_LIKELY((BASE).start != NULL)) {			\
                (((BASE).start)->NAME).prev = (N);			\
        }								\
        (BASE).start = (N);						\
        if (UNIV_UNLIKELY((BASE).end == NULL)) {			\
                (BASE).end = (N);					\
        }								\
}									\

# define UT_LIST_REMOVE_CLEAR(NAME, N)					\
((N)->NAME.prev = (N)->NAME.next = (void*) -1)

/** Removes a node from a linked list. */
#define UT_LIST_REMOVE(NAME, BASE, N)                                   \
do {                                                                    \
        ((BASE).count)--;                                               \
        if (((N)->NAME).next != NULL) {                                 \
                ((((N)->NAME).next)->NAME).prev = ((N)->NAME).prev;     \
        } else {                                                        \
                (BASE).end = ((N)->NAME).prev;                          \
        }                                                               \
        if (((N)->NAME).prev != NULL) {                                 \
                ((((N)->NAME).prev)->NAME).next = ((N)->NAME).next;     \
        } else {                                                        \
                (BASE).start = ((N)->NAME).next;                        \
        }                                                               \
        UT_LIST_REMOVE_CLEAR(NAME, N);                                  \
} while (0)

#define UT_LIST_GET_NEXT(NAME, N)					\
        (((N)->NAME).next)

#define UT_LIST_GET_LEN(BASE)						\
        (BASE).count

#define UT_LIST_GET_FIRST(BASE)						\
        (BASE).start

#endif /* INNODB_ENGINE_H */
