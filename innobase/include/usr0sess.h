/******************************************************
Sessions

(c) 1996 Innobase Oy

Created 6/25/1996 Heikki Tuuri
*******************************************************/

#ifndef usr0sess_h
#define usr0sess_h

#include "univ.i"
#include "ut0byte.h"
#include "hash0hash.h"
#include "trx0types.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "usr0types.h"
#include "que0types.h"
#include "data0data.h"
#include "rem0rec.h"
#include "com0com.h"

/* The session system global data structure */
extern sess_sys_t*	sess_sys;

/*************************************************************************
Sets the session id in a client message. */

void
sess_cli_msg_set_sess(
/*==================*/
	byte*	str,	/* in/out: message string */
	dulint	sess_id);/* in: session id */
/***************************************************************************
Sets the message type of a message from the client. */
UNIV_INLINE
void
sess_cli_msg_set_type(
/*==================*/
	byte*	str,	/* in: message string */
	ulint	type);	/* in: message type */
/***************************************************************************
Gets the message type of a message from the server. */
UNIV_INLINE
ulint
sess_srv_msg_get_type(
/*==================*/
			/* out: message type */
	byte*	str);	/* in: message string */
/***************************************************************************
Creates a session sytem at database start. */

void
sess_sys_init_at_db_start(void);
/*===========================*/
/*************************************************************************
Opens a session. */

sess_t*
sess_open(
/*======*/
					/* out, own: session object */
	com_endpoint_t*	endpoint,	/* in: communication endpoint used
					for communicating with the client */
	byte*		addr_buf,	/* in: client address */
	ulint		addr_len);	/* in: client address length */
/*************************************************************************
Closes a session, freeing the memory occupied by it. */

void
sess_close(
/*=======*/
	sess_t*		sess);	/* in, own: session object */
/*************************************************************************
Raises an SQL error. */

void
sess_raise_error_low(
/*=================*/
	trx_t*		trx,	/* in: transaction */
	ulint		err_no,	/* in: error number */
	ulint		type,	/* in: more info of the error, or 0 */
	dict_table_t*	table,	/* in: dictionary table or NULL */
	dict_index_t*	index,	/* in: table index or NULL */
	dtuple_t*	tuple,	/* in: tuple to insert or NULL */
	rec_t*		rec,	/* in: record or NULL */
	char*		err_str);/* in: arbitrary null-terminated error string,
				or NULL */
/*************************************************************************
Closes a session, freeing the memory occupied by it, if it is in a state
where it should be closed. */

ibool
sess_try_close(
/*===========*/
				/* out: TRUE if closed */
	sess_t*		sess);	/* in, own: session object */
/*************************************************************************
Initializes the first fields of a message to client. */

void
sess_srv_msg_init(
/*==============*/
	sess_t*	sess,	/* in: session object */
	byte*	buf,	/* in: message buffer, must be at least of size
			SESS_SRV_MSG_DATA */
	ulint	type);	/* in: message type */
/*************************************************************************
Sends a simple message to client. */

void
sess_srv_msg_send_simple(
/*=====================*/
	sess_t*	sess,		/* in: session object */
	ulint	type,		/* in: message type */
	ulint	rel_kernel);	/* in: SESS_RELEASE_KERNEL or
				SESS_NOT_RELEASE_KERNEL */
/***************************************************************************
Processes a message from a client. NOTE: May release the kernel mutex
temporarily. */

void
sess_receive_msg_rel_kernel(
/*========================*/
	sess_t*	sess,	/* in: session */
	byte*	str,	/* in: message string */
	ulint	len);	/* in: message length */
/***************************************************************************
When a command has been completed, this function sends the message about it
to the client. */

void
sess_command_completed_message(
/*===========================*/
	sess_t*	sess,	/* in: session */
	byte*	msg,	/* in: message buffer */
	ulint	len);	/* in: message data length */
/***********************************************************************
Starts a new connection and a session, or starts a query based on a client
message. This is called by a SRV_COM thread. */

void
sess_process_cli_msg(
/*=================*/
	byte*	str,	/* in: message string */
	ulint	len,	/* in: string length */
	byte*	addr,	/* in: address string */
	ulint	alen);	/* in: address length */


/* The session handle. All fields are protected by the kernel mutex */
struct sess_struct{
	dulint		id;		/* session id */
	dulint		usr_id;		/* user id */
	hash_node_t	hash;		/* hash chain node */
	ulint		refer_count;	/* reference count to the session
					object: when this drops to zero
					and the session has no query graphs
					left, discarding the session object
					is allowed */
	dulint		error_count;	/* if this counter has increased while
					a thread is parsing an SQL command,
					its graph should be discarded */
	ibool		disconnecting;	/* TRUE if the session is to be
					disconnected when its reference
					count drops to 0 */
	ulint		state;		/* state of the session */
	dulint		msgs_sent;	/* count of messages sent to the
					client */
	dulint		msgs_recv;	/* count of messages received from the
					client */
	ibool		client_waits;	/* when the session receives a message
					from the client, this set to TRUE, and
					when the session sends a message to
					the client this is set to FALSE */
	trx_t*		trx;		/* transaction object permanently
					assigned for the session: the
					transaction instance designated by the
					trx id changes, but the memory
					structure is preserved */
	ulint		next_graph_id;	/* next query graph id to assign */
	UT_LIST_BASE_NODE_T(que_t)
			graphs;		/* query graphs belonging to this
					session */
	/*------------------------------*/
	ulint		err_no;		/* latest error number, 0 if none */
	char*		err_str;	/* latest error string */
	ulint		err_len;	/* error string length */
	/*------------------------------*/
	com_endpoint_t*	endpoint;	/* server communications endpoint used
					to communicate with the client */
	char*		addr_buf;	/* client address string */
	ulint		addr_len;	/* client address string length */
	/*------------------------------*/
	byte*		big_msg;	/* if the client sends a message which
					does not fit in a single packet,
					it is assembled in this buffer; if
					this field is not NULL, it is assumed
					that the message should be catenated
					here */
	ulint		big_msg_size;	/* size of the big message buffer */
	ulint		big_msg_len;	/* length of data in the big message
					buffer */
};

/* The session system; this is protected by the kernel mutex */
struct sess_sys_struct{
	ulint		state;		/* state of the system:
					SESS_SYS_RUNNING or
					SESS_SYS_SHUTTING_DOWN */
	sess_t*		shutdown_req;	/* if shutdown was requested by some
					session, confirmation of shutdown
					completion should be sent to this
					session */	
	dulint		free_sess_id;	/* first unused session id */
	hash_table_t*	hash;		/* hash table of the sessions */
};


/*---------------------------------------------------*/
/* The format of an incoming message from a client */
#define SESS_CLI_MSG_CHECKSUM	0	/* the checksum should be the first
					field in the message */
#define SESS_CLI_MSG_SESS_ID	4	/* this is set to 0 if the client
					wants to connect and establish
					a new session */
#define	SESS_CLI_MSG_SESS_ID_CHECK 12	/* checksum of the sess id field */
#define SESS_CLI_MSG_TYPE	16
#define SESS_CLI_MSG_NO		20
#define	SESS_CLI_MSG_CONTINUE	28	/* 0, or SESS_MSG_FIRST_PART
					SESS_MSG_MIDDLE_PART, or
					SESS_MSG_LAST_PART */
#define	SESS_CLI_MSG_CONT_SIZE	32	/* size of a multipart message in
					kilobytes (rounded upwards) */
#define SESS_CLI_MSG_DATA	36
/*---------------------------------------------------*/

/* Client-to-session message types */
#define SESS_CLI_CONNECT	1
#define	SESS_CLI_PREPARE	2
#define	SESS_CLI_EXECUTE	3
#define	SESS_CLI_BREAK_EXECUTION 4

/* Client-to-session statement command types */
#define SESS_COMM_FETCH_NEXT	1
#define SESS_COMM_FETCH_PREV	2
#define SESS_COMM_FETCH_FIRST	3
#define SESS_COMM_FETCH_LAST	4
#define SESS_COMM_FETCH_NTH	5
#define SESS_COMM_FETCH_NTH_LAST 6
#define	SESS_COMM_EXECUTE	7
#define	SESS_COMM_NO_COMMAND	8

/*---------------------------------------------------*/
/* The format of an outgoing message from a session to the client */
#define SESS_SRV_MSG_CHECKSUM	0	/* the checksum should be the first
					field in the message */
#define SESS_SRV_MSG_SESS_ID	4
#define SESS_SRV_MSG_TYPE	12
#define SESS_SRV_MSG_NO		16
#define	SESS_SRV_MSG_CONTINUE	24	/* 0, or SESS_MSG_FIRST_PART
					SESS_MSG_MIDDLE_PART, or
					SESS_MSG_LAST_PART */
#define	SESS_SRV_MSG_CONT_SIZE	28	/* size of a multipart message
					in kilobytes (rounded upward) */
#define SESS_SRV_MSG_DATA	32
/*---------------------------------------------------*/

/* Session-to-client message types */
#define	SESS_SRV_ACCEPT_CONNECT	1
#define	SESS_SRV_SUCCESS	2
#define	SESS_SRV_ERROR		3

/* Multipart messages */
#define SESS_MSG_SINGLE_PART	0
#define SESS_MSG_FIRST_PART	1
#define	SESS_MSG_MIDDLE_PART	2
#define	SESS_MSG_LAST_PART	3

/* Error numbers */
#define SESS_ERR_NONE			0
#define SESS_ERR_TRX_COMMITTED		1
#define SESS_ERR_TRX_ROLLED_BACK	2
#define	SESS_ERR_SESSION_DISCONNECTED	3
#define	SESS_ERR_REPLY_FAILED		4
#define	SESS_ERR_CANNOT_BREAK_OP	5
#define	SESS_ERR_MSG_LOST		6
#define	SESS_ERR_MSG_CORRUPTED		7
#define	SESS_ERR_EXTRANEOUS_MSG		8
#define	SESS_ERR_OUT_OF_MEMORY		9
#define SESS_ERR_SQL_ERROR		10
#define SESS_ERR_STMT_NOT_FOUND		11
#define SESS_ERR_STMT_NOT_READY		12
#define	SESS_ERR_EXTRANEOUS_SRV_MSG	13
#define	SESS_ERR_BREAK_BY_CLIENT	14

/* Session states */
#define SESS_ACTIVE		1
#define SESS_ERROR		2	/* session contains an error message
					which has not yet been communicated
					to the client */
/* Session system states */
#define SESS_SYS_RUNNING	1
#define SESS_SYS_SHUTTING_DOWN	2

/* Session hash table size */
#define SESS_HASH_SIZE		1024

/* Flags used in sess_srv_msg_send */
#define SESS_RELEASE_KERNEL	1
#define SESS_NOT_RELEASE_KERNEL	2

#ifndef UNIV_NONINL
#include "usr0sess.ic"
#endif

#endif 
