/******************************************************
Innobase ODBC client library

(c) 1998 Innobase Oy

Created 2/22/1998 Heikki Tuuri
*******************************************************/

#include "odbc0odbc.h"

#include "mem0mem.h"
#include "com0com.h"
#include "usr0sess.h"

#define ODBC_STAT_INITIAL	1
#define ODBC_STAT_PREPARED	2
#define ODBC_STAT_EXECUTED	3


typedef struct odbc_conn_struct	odbc_conn_t;
typedef struct odbc_env_struct	odbc_env_t;

/* ODBC parameter description struct */

typedef struct odbc_param_struct	odbc_param_t;
struct odbc_param_struct{
	ulint	data_type;	/* SQL_CHAR, ... */
	ibool	is_input;	/* TRUE if an input parameter of a stored
				procedure, FALSE if an output parameter */
	byte*	buf;		/* buffer where the value is stored before
				SQLExecute, or where it comes after SQLExecute
				in the case of an output parameter */
	lint*	data_len;	/* pointer to where the data len or the value
				SQL_NULL_DATA is stored */
	ulint	buf_len;	/* buffer size */
};

/* ODBC statement data structure */

typedef struct odbc_stat_struct	odbc_stat_t;
struct odbc_stat_struct{
	ulint		state;		/* ODBC_STAT_INITIAL,
					ODBC_STAT_PREPARED,
					ODBC_STAT_EXECUTED */
	ulint		id;		/* statement id */
	ulint		n_params;	/* number of parameters */
	odbc_param_t*	params;		/* pointer to an array describing
					the parameters, if any */
	ulint		n_params_bound;	/* number of parameters that have
					been bound: the statement cannot be
					executed before n_params_bound
					== n_params */
	byte*		error_msg;	/* possible error message, or NULL;
					allocated separately from dynamic
					memory */
	ulint		error_msg_len;	/* error mesage length if it is
					non-NULL */
	odbc_conn_t*	conn;		/* connection */
	UT_LIST_NODE_T(odbc_stat_t)
			stat_list;	/* list of the statements of the
					connection */
};

/* ODBC connection data structure */

struct odbc_conn_struct{
	ibool		connected;	/* TRUE if connected */
	char*		server_name;	/* server name where connected
					(= server address) */
	ulint		server_name_len;/* length of the server name */
	com_endpoint_t*	com_endpoint;	/* connection endpoint for this client
					connection */
	dulint		out_msg_count;	/* count of outgoing messages */
	byte*		out_datagram_buf;/* buffer for outgoing datagrams to
					the server */
	byte*		in_datagram_buf;/* buffer for incoming datagrams from
					the server */
	byte*		addr_buf;	/* buffer for the address from which
					an incoming datagram came; in practice,
					this will be the server address */
	dulint		sess_id;	/* user session id, once the
					connection to the server is
					established */
	odbc_env_t*	env;		/* environment */
	UT_LIST_BASE_NODE_T(odbc_stat_t)
			stat_list;	/* list of the statements of the
					connection */
	UT_LIST_NODE_T(odbc_conn_t)
			conn_list;	/* list of the connections of the
					environment */
};

/* ODBC environment data structure */

struct odbc_env_struct{
	UT_LIST_BASE_NODE_T(odbc_conn_t)	conn_list;
					/* list of the connections of the
					environment */
};
	
/**************************************************************************
Gets the nth parameter description struct for a statement. */
UNIV_INLINE
odbc_param_t*
stat_get_nth_param(
/*===============*/
				/* out: nth parameter */
	odbc_stat_t*	stat,	/* in: pointer to statement handle */
	ulint		i)	/* in: parameter index */
{
	ut_ad(stat->n_params > i);

	return(stat->params + i);
}

/**************************************************************************
Allocates an SQL environment. */

RETCODE
SQLAllocEnv(
/*========*/
			/* out: SQL_SUCCESS */
	HENV*	phenv)	/* out: pointer to an environment handle */
{
	odbc_env_t*	env;

	if (!sync_initialized) {
		sync_init();
		mem_init(2000000);
	}

	env = mem_alloc(sizeof(odbc_env_t));

	UT_LIST_INIT(env->conn_list);

	*phenv = env;

	return(SQL_SUCCESS);
}

/**************************************************************************
Allocates an SQL connection. */

RETCODE
SQLAllocConnect(
/*============*/
			/* out: SQL_SUCCESS */
	HENV	henv,	/* in: pointer to an environment handle */
	HDBC*	phdbc)	/* out: pointer to a connection handle */
{
	odbc_conn_t*	conn;
	odbc_env_t*	env;

	ut_a(henv);

	env = henv;
	conn = mem_alloc(sizeof(odbc_conn_t));

	conn->connected = FALSE;
	conn->env = env;

	UT_LIST_INIT(conn->stat_list);

	UT_LIST_ADD_LAST(conn_list, env->conn_list, conn);

	*phdbc = conn;

	return(SQL_SUCCESS);
}

/**************************************************************************
Allocates an SQL statement. */

RETCODE
SQLAllocStmt(
/*=========*/
	HDBC	hdbc,	/* in: SQL connection */
	HSTMT*	phstmt)	/* out: pointer to a statement handle */
{
	odbc_conn_t*	conn;
	odbc_stat_t*	stat;

	ut_a(hdbc);

	conn = hdbc;

	stat = mem_alloc(sizeof(odbc_stat_t));

	stat->state = ODBC_STAT_INITIAL;
	stat->error_msg = NULL;
	stat->conn = conn;

	UT_LIST_ADD_LAST(stat_list, conn->stat_list, stat);

	*phstmt = stat;

	return(SQL_SUCCESS);
}

/**************************************************************************
Sends the message in datagram_buf to the server. */
static
void
odbc_send_cli_msg(
/*==============*/
	odbc_conn_t*	conn,	/* in: connection, does not have to be
				connected yet */
	ulint 		len)	/* in: message length (excluding the standard
				header of size SESS_CLI_MSG_DATA) */
{
	ulint	ret;
	ulint	fold;
	byte*	msg;

	ut_a(len + SESS_CLI_MSG_DATA <= ODBC_DATAGRAM_SIZE);

	msg = conn->out_datagram_buf;

	mach_write_to_8(msg + SESS_CLI_MSG_NO, conn->out_msg_count);

	UT_DULINT_INC(conn->out_msg_count);

	fold = ut_fold_binary(msg + 4, len + SESS_CLI_MSG_DATA - 4);

 	ut_ad(SESS_CLI_MSG_CHECKSUM == 0);

	mach_write_to_4(msg + SESS_CLI_MSG_CHECKSUM, fold);

	ret = com_sendto(conn->com_endpoint, msg, SESS_CLI_MSG_DATA + len,
				conn->server_name, conn->server_name_len);
	ut_a(ret == 0);
}

/**************************************************************************
Receives a message in datagram_buf from the server. */
static
void
odbc_recv_srv_msg(
/*==============*/
	odbc_conn_t*	conn,	/* in: connection, does not have to be
				connected yet */
	ulint* 		len)	/* out: received message length (excluding the
				standard header of size SESS_SRV_MSG_DATA) */
{
	ulint	total_len;
	ulint	addr_len;
	ulint	ret;

	ret = com_recvfrom(conn->com_endpoint, conn->in_datagram_buf,
			ODBC_DATAGRAM_SIZE, &total_len, (char*)conn->addr_buf,
			ODBC_ADDRESS_SIZE, &addr_len);
	ut_a(ret == 0);
	ut_a(total_len >= SESS_SRV_MSG_DATA);

	*len = total_len - SESS_SRV_MSG_DATA;
}
	
/**************************************************************************
Connects to a database server process (establishes a connection and a
session). */

RETCODE
SQLConnect(
/*=======*/
				/* out: SQL_SUCCESS */
	HDBC	hdbc,		/* in: SQL connection handle */
	UCHAR*	szDSN,		/* in: data source name (server name) */
	SWORD	cbDSN,		/* in: data source name length */
	UCHAR*	szUID,		/* in: user name */
	SWORD	cbUID,		/* in: user name length */
	UCHAR*	szAuthStr,	/* in: password */
	SWORD	cbAuthStr)	/* in: password length */
{
	com_endpoint_t*	ep;
	odbc_conn_t*	conn;
	ulint		err;
	ulint		size;
	byte*		msg;
	ulint		len;
	UCHAR		catenated_name[100];
	
	ut_a(hdbc && szDSN);

	UT_NOT_USED(szUID);
	UT_NOT_USED(cbUID);
	UT_NOT_USED(szAuthStr);
	UT_NOT_USED(cbAuthStr);

	conn = hdbc;
	
	ut_a(!conn->connected);

	conn->server_name = mem_alloc(cbDSN);
	ut_memcpy(conn->server_name, szDSN, cbDSN);

	conn->server_name_len = cbDSN;

	ep = com_endpoint_create(COM_SHM);
	
	ut_a(ep);

	conn->com_endpoint = ep;

	conn->out_msg_count = ut_dulint_zero;
	
	size = ODBC_DATAGRAM_SIZE;
	
	err = com_endpoint_set_option(ep, COM_OPT_MAX_DGRAM_SIZE,
							(byte*)&size, 4);
	ut_a(err == 0);

	/* Make the data source name catenated to user name as the
	address of the communications endpoint */

	ut_a((ulint)cbDSN + (ulint)cbUID < 100);
	
	ut_memcpy(catenated_name, szDSN, (ulint)cbDSN);
	ut_memcpy(catenated_name + (ulint)cbDSN, szUID, (ulint)cbUID);
	
	err = com_bind(ep, (char*)catenated_name, (ulint)cbDSN
							+ (ulint)cbUID);
	ut_a(err == 0);

	conn->in_datagram_buf = mem_alloc(ODBC_DATAGRAM_SIZE);

	msg = mem_alloc(ODBC_DATAGRAM_SIZE);

	conn->out_datagram_buf = msg;
	conn->addr_buf = mem_alloc(ODBC_ADDRESS_SIZE);

	/* Set the session id to dulint 0 as we are not yet connected */
	
	sess_cli_msg_set_sess(msg, ut_dulint_zero);
	sess_cli_msg_set_type(msg, SESS_CLI_CONNECT);

	/*------------------------------------------*/
	
	odbc_send_cli_msg(conn, 0);

	odbc_recv_srv_msg(conn, &len);

	/*------------------------------------------*/
	
	ut_a(len == 0);
	ut_a(sess_srv_msg_get_type(conn->in_datagram_buf)
						== SESS_SRV_ACCEPT_CONNECT);
	
	conn->sess_id = mach_read_from_8(conn->in_datagram_buf
						+ SESS_SRV_MSG_SESS_ID);

	/* Write the session id to out_datagram_buf: it will not be rewritten
	until the connection is closed, as the session id will stay the same */
							
	sess_cli_msg_set_sess(msg, conn->sess_id);

	/* We currently only send single part messages: the following will
	stay 0 during the connection */

	mach_write_to_4(msg + SESS_CLI_MSG_CONTINUE, 0);
	mach_write_to_4(msg + SESS_CLI_MSG_CONT_SIZE, 0);
	
	conn->connected = TRUE;

	return(SQL_SUCCESS);
}

/**************************************************************************
Stores an error message to a statement handle, so that it can be later
queried with SQLError. */
static
void
odbc_stat_store_error_msg(
/*======================*/
	odbc_stat_t*	stat,	/* in: statement handle */
	byte*		msg,	/* in: error message sent by the server */
	ulint		len)	/* in: length of msg */
{
	if (stat->error_msg) {
		mem_free(stat->error_msg);
	}

	stat->error_msg_len = len;

	len += SESS_SRV_MSG_DATA;

	stat->error_msg = mem_alloc(len);
	ut_memcpy(stat->error_msg, msg, len);
}

/**************************************************************************
Queries an error message. */

RETCODE
SQLError(
/*=====*/
				/* out: SQL_SUCCESS or SQL_NO_DATA_FOUND */
	HENV	henv,		/* in: SQL_NULL_HENV */
	HDBC	hdbc,		/* in: SQL_NULL_HDBC */
	HSTMT	hstmt,		/* in: statement handle */
	UCHAR*	szSqlState,	/* in/out: SQLSTATE as a null-terminated string,
				(currently, always == "S1000") */
	SDWORD*	pfNativeError,	/* out: native error code */
	UCHAR*	szErrorMsg,	/* in/out: buffer for an error message as a
				null-terminated string */
	SWORD	cbErrorMsgMax,	/* in: buffer size for szErrorMsg */
	SWORD*	pcbErrorMsg)	/* out: error message length */
{
	odbc_stat_t*	stat;
	ulint		len;
	
	ut_a(henv == SQL_NULL_HENV);
	ut_a(hdbc == SQL_NULL_HDBC);
	ut_a(hstmt);
	ut_a(cbErrorMsgMax > 1);
	
	stat = hstmt;

	if (stat->error_msg == NULL) {

		return(SQL_NO_DATA_FOUND);
	}

	*pfNativeError = 0;
	ut_memcpy(szSqlState, "S1000", 6);

	len = (ulint)cbErrorMsgMax - 1;

	if (stat->error_msg_len < len) {
		len = stat->error_msg_len;
	}
	
	ut_memcpy(szErrorMsg, stat->error_msg + SESS_SRV_MSG_DATA, len);

	*(szErrorMsg + len) = '\0';
	
	*pcbErrorMsg = (SWORD)len;

	if (stat->error_msg) {
		mem_free(stat->error_msg);
		stat->error_msg = NULL;
	}

	return(SQL_SUCCESS);
}

/**************************************************************************
Makes the server to parse and optimize an SQL string. */

RETCODE
SQLPrepare(
/*=======*/
				/* out: SQL_SUCCESS or SQL_ERROR */
	HSTMT	hstmt,		/* in: statement handle */
	UCHAR*	szSqlStr,	/* in: SQL string */
	SDWORD	cbSqlStr)	/* in: SQL string length */
{
	odbc_stat_t*	stat;
	odbc_conn_t*	conn;
	odbc_param_t*	param;
	ulint		len;
	byte*		msg;
	ulint		i;

	stat = hstmt;
	conn = stat->conn;

	if (stat->error_msg) {
		mem_free(stat->error_msg);
		stat->error_msg = NULL;
	}

	ut_memcpy(conn->out_datagram_buf + SESS_CLI_MSG_DATA, szSqlStr,
							1 + (ulint)cbSqlStr);
							
	sess_cli_msg_set_type(conn->out_datagram_buf, SESS_CLI_PREPARE);

	/* The client message will be decoded in sess_receive_prepare */

	/*------------------------------------------*/	

	odbc_send_cli_msg(conn, 1 + (ulint)cbSqlStr);

	odbc_recv_srv_msg(conn, &len);

	/*------------------------------------------*/	

	/* The server message was coded in sess_receive_prepare */

	ut_a(len >= 8);

	msg = conn->in_datagram_buf;

	if (sess_srv_msg_get_type(msg) != SESS_SRV_SUCCESS) {

		ut_a(sess_srv_msg_get_type(msg) == SESS_SRV_ERROR);

		odbc_stat_store_error_msg(stat, msg, len);

		return(SQL_ERROR);
	}

	stat->id = mach_read_from_4(msg + SESS_SRV_MSG_DATA);

	stat->n_params = mach_read_from_4(msg + SESS_SRV_MSG_DATA + 4);

	stat->n_params_bound = 0;

	ut_a(len == 8 + stat->n_params);

	if (stat->n_params > 0) {

		stat->params = mem_alloc(stat->n_params
						* sizeof(odbc_param_t));
		for (i = 0; i < stat->n_params; i++) {
			param = stat_get_nth_param(stat, i);
			
			param->is_input = mach_read_from_1(
					    msg + SESS_SRV_MSG_DATA + 8 + i);
			/* Set buf to NULL so that we know when the parameter
			has been bound */

			param->buf = NULL;
		}
	}
	
	stat->state = ODBC_STAT_PREPARED;

	return(SQL_SUCCESS);
}

/**************************************************************************
Binds a parameter in a prepared statement. */

RETCODE
SQLBindParameter(
/*=============*/
				/* out: SQL_SUCCESS */
	HSTMT	hstmt,		/* in: statement handle */
	UWORD	ipar,		/* in: parameter index, starting from 1 */
	SWORD	fParamType,	/* in: SQL_PARAM_INPUT or SQL_PARAM_OUTPUT */
	SWORD	fCType,		/* in: SQL_C_CHAR, ... */
	SWORD	fSqlType,	/* in: SQL_CHAR, ... */
	UDWORD	cbColDef,	/* in: precision: ignored */
	SWORD	ibScale,	/* in: scale: ignored */
	PTR	rgbValue,	/* in: pointer to a buffer for the data */
	SDWORD	cbValueMax,	/* in: buffer size */
	SDWORD*	pcbValue)	/* in: pointer to a buffer for the data
				length or SQL_NULL_DATA */
{
	odbc_stat_t*	stat;
	odbc_param_t*	param;

	stat = hstmt;

	ut_a(stat->state != ODBC_STAT_INITIAL);
	ut_a(rgbValue);
	ut_a(ipar <= stat->n_params);
	ut_a(ipar > 0);
	ut_a(cbValueMax >= 0);
	ut_a(pcbValue);

	UT_NOT_USED(ibScale);
	UT_NOT_USED(fCType);
	UT_NOT_USED(cbColDef);

	if (stat->error_msg) {
		mem_free(stat->error_msg);
		stat->error_msg = NULL;
	}

	param = stat_get_nth_param(stat, ipar - 1);

	if (param->buf == NULL) {
		stat->n_params_bound++;
	}

	param->data_type = fSqlType;

	ut_a((fParamType != SQL_PARAM_INPUT) || param->is_input);
	ut_a((fParamType == SQL_PARAM_INPUT) || !param->is_input);
	
	param->buf = rgbValue;
	param->buf_len = cbValueMax;
	param->data_len = pcbValue;

	return(SQL_SUCCESS);
}

/**************************************************************************
Executes a prepared statement where all parameters have been bound. */

RETCODE
SQLExecute(
/*=======*/
			/* out: SQL_SUCCESS or SQL_ERROR */
	HSTMT	hstmt)	/* in: statement handle */
{
	odbc_stat_t*	stat;
	odbc_conn_t*	conn;
	odbc_param_t*	param;
	lint		len;
	ulint		msg_len;
	byte*		msg;
	byte*		ptr;
	lint		int_val;
	ulint		i;

	stat = hstmt;

	ut_a(stat->state != ODBC_STAT_INITIAL);
	ut_a(stat->n_params == stat->n_params_bound);
	
	if (stat->error_msg) {
		mem_free(stat->error_msg);
		stat->error_msg = NULL;
	}

	conn = stat->conn;
	msg = conn->out_datagram_buf;

	sess_cli_msg_set_type(msg, SESS_CLI_EXECUTE);

	ptr = msg + SESS_CLI_MSG_DATA;

	mach_write_to_4(ptr, stat->id);

	ptr += 4;

	for (i = 0; i < stat->n_params; i++) {

		param = stat_get_nth_param(stat, i);

		if (param->is_input) {
			/* Copy its length and data to the message buffer */

			len = *(param->data_len);			

			mach_write_to_4(ptr, (ulint)len);

			ptr += 4;

			if (len != SQL_NULL_DATA) {
				if (param->data_type == SQL_INTEGER) {
					ut_ad(len == 4);
					int_val = *((lint*)(param->buf));

					mach_write_to_4(ptr, (ulint)int_val);
				} else {
					ut_memcpy(ptr, param->buf, len);
				}

				ptr += len;
			}
		}
	}

	/* The client message will be decoded in sess_receive_command */
	
	/*------------------------------------------*/
	
	odbc_send_cli_msg(conn, ptr - (msg + SESS_CLI_MSG_DATA));

	odbc_recv_srv_msg(conn, &msg_len);

	/*------------------------------------------*/

	/* The server message was coded in sess_command_completed_message */

	msg = conn->in_datagram_buf;

	if (sess_srv_msg_get_type(msg) != SESS_SRV_SUCCESS) {

		ut_a(sess_srv_msg_get_type(msg) == SESS_SRV_ERROR);

		odbc_stat_store_error_msg(stat, msg, msg_len);
		
		return(SQL_ERROR);
	}

	ptr = msg + SESS_SRV_MSG_DATA;
	
	for (i = 0; i < stat->n_params; i++) {

		param = stat_get_nth_param(stat, i);

		if (!param->is_input) {
			/* Copy its length and data from the message buffer */

			len = (lint)mach_read_from_4(ptr);

			ptr += 4;

			*(param->data_len) = len;

			if (len != SQL_NULL_DATA) {
				if (param->data_type == SQL_INTEGER) {
					ut_ad(len == 4);

					int_val = (lint)mach_read_from_4(ptr);

					*((lint*)(param->buf)) = int_val;
				} else {
					ut_memcpy(param->buf, ptr, (ulint)len);
				}

				ptr += len;
			}
		}
	}

	ut_ad(msg + SESS_SRV_MSG_DATA + msg_len == ptr);	

	return(SQL_SUCCESS);
}
