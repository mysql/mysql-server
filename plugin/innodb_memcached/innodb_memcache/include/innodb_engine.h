#ifndef NDBMEMCACHE_NDB_ENGINE_H
#define NDBMEMCACHE_NDB_ENGINE_H

#include "config.h"

#include <pthread.h>

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>
#include <innodb_config.h>

/** The InnoDB engine global data.

Inside memcached, a pointer to this is treated as simply an ENGINE_HANDLE_V1 
pointer.  But inside the NDB engine that pointer is cast up to point to the
whole private structure. 

This structure also contains a pointer to the default engine's 
private structure, since all caching is delegated to the default 
engine. */

 
/** structure contains the cursor information for each connection */
typedef struct innodb_conn_data		innodb_conn_data_t;

#define	CONN_NUM_WRITE_COMMIT	1
#define CONN_NUM_READ_COMMIT	1048510

/** Macros to manipulate the list */
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
}									\

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

# define UT_LIST_REMOVE_CLEAR(NAME, N)          			\
((N)->NAME.prev = (N)->NAME.next = (void*) -1)

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


/* Connection specific data */
struct innodb_conn_data {
	ib_trx_t	c_r_trx;	/*!< read transaction */
	ib_trx_t	c_trx;		/*!< write transaction */
	ib_crsr_t	c_r_crsr;	/*!< data cursor for read */
	ib_crsr_t	c_r_idx_crsr;	/*!< index cursor for read */
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
	UT_LIST_NODE_T(innodb_conn_data_t) c_list; /*!< list ptr */
};

typedef UT_LIST_BASE_NODE_T(innodb_conn_data_t)	conn_base_t;

typedef struct innodb_engine {
	ENGINE_HANDLE_V1	engine;
	SERVER_HANDLE_V1 	server;
	GET_SERVER_API 		get_server_api;
	ENGINE_HANDLE*		m_default_engine;

	struct {
		size_t nthreads;
		bool cas_enabled;  
	} server_options;

	union {
	engine_info info;
	char buffer[sizeof(engine_info) * (LAST_REGISTERED_ENGINE_FEATURE + 1)];
	} info;

	bool		initialized;
	bool		connected;

	meta_info_t	meta_info;

	conn_base_t	conn_data;
	pthread_mutex_t conn_mutex;
	ib_cb_t*	innodb_cb;
	uint64_t	r_batch_size;
	uint64_t	w_batch_size;	
} innodb_engine_t;

#endif
