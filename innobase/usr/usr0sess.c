/******************************************************
Sessions

(c) 1996 Innobase Oy

Created 6/25/1996 Heikki Tuuri
*******************************************************/

#include "usr0sess.h"

#ifdef UNIV_NONINL
#include "usr0sess.ic"
#endif

#include "ut0rnd.h"
#include "mach0data.h"
#include "ha0ha.h"
#include "trx0trx.h"
#include "que0que.h"
#include "pars0pars.h"
#include "pars0sym.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "odbc0odbc.h"

#define SESS_ERR_BUF_SIZE	8192

/* The session system global data structure */
sess_sys_t*	sess_sys	= NULL;

/*************************************************************************
Closes a session, freeing the memory occupied by it. */
static
void
sess_close(
/*=======*/
	sess_t*		sess);	/* in, own: session object */
/*************************************************************************
Communicates an error message to the client. If sess->client_waits is not
TRUE, puts the session to error state and does not try to send the error
message. */
static
void
sess_srv_msg_send_error(
/*====================*/
	sess_t*	sess);	/* in: session object */
/*************************************************************************
Copies error info to a session. Sends to the transaction a signal which will
rollback the latest incomplete SQL statement and then send the error message
to the client. NOTE: This function will take care of the freeing of the error
string, thus the caller must supply a copy of the error string. */
static
void
sess_error_low(
/*===========*/
	sess_t*	sess,	/* in: session object */
	ulint	err_no,	/* in: error number */
	char*	err_str);/* in, own: error string or NULL;
			NOTE: the function will take care of freeing of the
			string! */

/*************************************************************************
Folds a session id to a ulint. Because this function is used also in
calculating a checksum for the id to write in the message, it is performs
also a XOR operation to mix the values more thoroughly. */
UNIV_INLINE
ulint
sess_id_fold(
/*=========*/
			/* out: folded value; can be used also as the checksum
			for id */
	dulint	id)	/* in: session id */
{
	return(ut_fold_dulint(id) ^ 2945794411U);
}

/*************************************************************************
Sets the session id in a client message. */

void
sess_cli_msg_set_sess(
/*==================*/
	byte*	str,	/* in/out: message string */
	dulint	sess_id)/* in: session id */
{
	ulint	fold;
	
	mach_write_to_8(str + SESS_CLI_MSG_SESS_ID, sess_id);	

	fold = sess_id_fold(sess_id);

	mach_write_to_4(str + SESS_CLI_MSG_SESS_ID_CHECK, fold);
}

/***************************************************************************
Decrements the reference count of a session and closes it, if desired. */
UNIV_INLINE
void
sess_refer_count_dec(
/*=================*/
	sess_t*	sess)	/* in: session */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(sess->refer_count > 0);

	sess->refer_count--;

	if (sess->disconnecting && (sess->refer_count == 0)) {

	    	sess_close(sess);
	}
}

/***************************************************************************
Increments the reference count of a session. */
UNIV_INLINE
void
sess_refer_count_inc(
/*=================*/
	sess_t*	sess)	/* in: session */
{
	ut_ad(mutex_own(&kernel_mutex));

	sess->refer_count++;
}

/***************************************************************************
Creates a session system at a database start. */

void
sess_sys_init_at_db_start(void)
/*===========================*/
{
	sess_sys = mem_alloc(sizeof(sess_sys_t));

	sess_sys->state = SESS_SYS_RUNNING;
	sess_sys->free_sess_id = ut_dulint_create(0, 1);
	sess_sys->hash = hash_create(SESS_HASH_SIZE);
}

/***************************************************************************
Gets the message type of a message from client. */
UNIV_INLINE
ulint
sess_cli_msg_get_type(
/*==================*/
			/* out: message type */
	byte*	str)	/* in: message string */
{
	ut_ad(mutex_own(&kernel_mutex));

	return(mach_read_from_4(str + SESS_CLI_MSG_TYPE));
}

/***************************************************************************
Gets the message number of a message from client. */
UNIV_INLINE
dulint
sess_cli_msg_get_msg_no(
/*====================*/
			/* out: message number */
	byte*	str)	/* in: message string */
{
	ut_ad(mutex_own(&kernel_mutex));

	return(mach_read_from_8(str + SESS_CLI_MSG_NO));
}

/***************************************************************************
Gets the continue field of a message from client. */
UNIV_INLINE
ulint
sess_cli_msg_get_continue(
/*======================*/
			/* out: SESS_MSG_SINGLE_PART, ... */
	byte*	str)	/* in: message string */
{
	ut_ad(mutex_own(&kernel_mutex));

	return(mach_read_from_4(str + SESS_CLI_MSG_CONTINUE));
}

/***************************************************************************
Gets the size of a big message in kilobytes. */
UNIV_INLINE
ulint
sess_cli_msg_get_cont_size(
/*=======================*/
			/* out: size in kilobytes */
	byte*	str)	/* in: message string */
{
	ut_ad(mutex_own(&kernel_mutex));

	return(mach_read_from_4(str + SESS_CLI_MSG_CONT_SIZE));
}

/*************************************************************************
Checks the consistency of a message from a client. */
UNIV_INLINE
ibool
sess_cli_msg_check_consistency(
/*===========================*/
			/* out: TRUE if ok */
	byte*	str,	/* in: message string */
	ulint	len)	/* in: message string length */
{	
	ulint	fold;

	ut_ad(mutex_own(&kernel_mutex));

	if (len < SESS_CLI_MSG_DATA) {

		return(FALSE);
	}

	ut_ad(SESS_CLI_MSG_CHECKSUM == 0);

	fold = ut_fold_binary(str + 4, len - 4);

	if (mach_read_from_4(str + SESS_CLI_MSG_CHECKSUM) != fold) {

		return(FALSE);
	}

	return(TRUE);
}

/*************************************************************************
Opens a session. */

sess_t*
sess_open(
/*======*/
					/* out, own: session object */
	com_endpoint_t*	endpoint,	/* in: communication endpoint used
					for receiving messages from the client,
					or NULL if no client */
	byte*		addr_buf,	/* in: client address (= user name) */
	ulint		addr_len)	/* in: client address length */
{	
	sess_t*	sess;
	ulint	fold;

	ut_ad(mutex_own(&kernel_mutex));

	sess = mem_alloc(sizeof(sess_t));

	sess->id = sess_sys->free_sess_id;
	UT_DULINT_INC(sess_sys->free_sess_id);

	sess->state = SESS_ACTIVE;
	sess->disconnecting = FALSE;
	sess->msgs_sent = ut_dulint_zero;
	sess->msgs_recv = ut_dulint_zero;
	sess->client_waits = TRUE;
	sess->err_no = 0;
	sess->err_str = NULL;
	sess->error_count = ut_dulint_zero;
	
	sess->big_msg = NULL;

	sess->trx = trx_create(sess);

	sess->next_graph_id = 0;

	UT_LIST_INIT(sess->graphs);

	fold = sess_id_fold(sess->id);

	HASH_INSERT(sess_t, hash, sess_sys->hash, fold, sess);
							
	sess->endpoint = endpoint;
	sess->addr_buf = mem_alloc(addr_len);

	ut_memcpy(sess->addr_buf, addr_buf, addr_len);

	sess->addr_len = addr_len;
	
	return(sess);
}

/*************************************************************************
Closes a session, freeing the memory occupied by it. */

static
void
sess_close(
/*=======*/
	sess_t*	sess)	/* in, own: session object */
{	
	ulint	fold;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(sess->disconnecting);
	ut_ad(sess->trx == NULL);
	ut_ad(sess->refer_count == 0);

	fold = ut_fold_dulint(sess->id);
	HASH_DELETE(sess_t, hash, sess_sys->hash, fold, sess);

/*	sess_reply_to_client_rel_kernel(sess); */
	
	if (sess->err_str != NULL) {
		mem_free(sess->err_str);
	}

	mem_free(sess->addr_buf);
	mem_free(sess);
}

/*************************************************************************
Closes a session, freeing the memory occupied by it, if it is in a state
where it should be closed. */

ibool
sess_try_close(
/*===========*/
			/* out: TRUE if closed */
	sess_t*	sess)	/* in, own: session object */
{
	ut_ad(mutex_own(&kernel_mutex));

	if (sess->disconnecting && (UT_LIST_GET_LEN(sess->graphs) == 0)
						&& (sess->refer_count == 0)) {
		sess_close(sess);

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Initializes the first fields of a message to client. */

void
sess_srv_msg_init(
/*==============*/
	sess_t*	sess,	/* in: session object */
	byte*	buf,	/* in: message buffer, must be at least of size
			SESS_SRV_MSG_DATA */
	ulint	type)	/* in: message type */
{
	ut_ad(mutex_own(&kernel_mutex));

	sess->msgs_sent = ut_dulint_add(sess->msgs_sent, 1);
		
	mach_write_to_8(buf + SESS_SRV_MSG_SESS_ID, sess->id);
	mach_write_to_4(buf + SESS_SRV_MSG_TYPE, type);
	mach_write_to_8(buf + SESS_SRV_MSG_NO, sess->msgs_sent);

	ut_ad(com_endpoint_get_max_size(sess->endpoint) >= SESS_SRV_MSG_DATA);
}		

/*************************************************************************
Sends a message to the client. */
static
ulint
sess_srv_msg_send_low(
/*==================*/
			/* out: 0 if success, else error number */
	sess_t*	sess,	/* in: session object */
	byte*	buf,	/* in: message buffer */
	ulint	len,	/* in: message length */
	ulint	rel_ker)/* in: SESS_RELEASE_KERNEL if the kernel mutex should
			be temporarily released in the call; otherwise
			SESS_NOT_RELEASE_KERNEL */
{
	ulint	ret;

	ut_ad((rel_ker == SESS_NOT_RELEASE_KERNEL)
					|| (rel_ker == SESS_RELEASE_KERNEL));
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(len <= com_endpoint_get_max_size(sess->endpoint));
	ut_ad(len >= SESS_SRV_MSG_DATA);

	if (sess->client_waits == FALSE) {
		sess_error_low(sess, SESS_ERR_EXTRANEOUS_SRV_MSG, NULL);
		
		return(1);
	}
	
	/* The client will now receive an error message: if the session is
	in the error state, we can reset it to the normal state */
	
	if (sess->state == SESS_ERROR) {
		sess->state = SESS_ACTIVE;
	}
	
	/* We reset the client_waits flag to FALSE, regardless of whether the
	message gets delivered to the client or not. This convention makes
	things simpler. */

	sess->client_waits = FALSE;

	if (rel_ker == SESS_RELEASE_KERNEL) {

		mutex_exit(&kernel_mutex);
	}

	ret = com_sendto(sess->endpoint, buf, len, sess->addr_buf,
							sess->addr_len);
	if (rel_ker == SESS_RELEASE_KERNEL) {

		mutex_enter(&kernel_mutex);
	}

	if (ret != 0) {
		sess_error_low(sess, SESS_ERR_REPLY_FAILED, NULL);
	}
	
	return(ret);
}

/*************************************************************************
Sends a message to the client. If the session is in the error state, sends
the error message instead of buf. */
static
ulint
sess_srv_msg_send(
/*==============*/
			/* out: 0 if success, else error number */
	sess_t*	sess,	/* in: session object */
	byte*	buf,	/* in: message buffer */
	ulint	len,	/* in: message length */
	ulint	rel_ker)/* in: SESS_RELEASE_KERNEL if the kernel mutex should
			be temporarily released in the call; otherwise
			SESS_NOT_RELEASE_KERNEL */
{
	ulint	ret;

	ut_ad(mutex_own(&kernel_mutex));

	if (sess->state == SESS_ERROR) {

		sess_srv_msg_send_error(sess);

		return(2);
	}

	ret = sess_srv_msg_send_low(sess, buf, len, rel_ker);

	return(ret);
}

/*************************************************************************
Sends a simple message to client. */

void
sess_srv_msg_send_simple(
/*=====================*/
	sess_t*	sess,		/* in: session object */
	ulint	type,		/* in: message type */
	ulint	rel_kernel)	/* in: SESS_RELEASE_KERNEL or
				SESS_NOT_RELEASE_KERNEL */
{
	byte	buf[SESS_SRV_MSG_DATA];

	ut_ad(mutex_own(&kernel_mutex));

	sess_srv_msg_init(sess, buf, type);

	sess_srv_msg_send(sess, buf, SESS_SRV_MSG_DATA, rel_kernel);
}

/*************************************************************************
Communicates an error message to the client. If sess->client_waits is not
TRUE, puts the session to error state and does not try to send the error
message. */
static
void
sess_srv_msg_send_error(
/*====================*/
	sess_t*	sess)	/* in: session object */
{
	ulint	err_no;
	byte*	err_str;
	ulint	err_len;
	ulint	max_len;
	byte	buf[SESS_ERR_BUF_SIZE];
	ulint	ret;
	
	ut_ad(sess->client_waits);
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(sess->state == SESS_ERROR);
	ut_ad(!UT_LIST_GET_FIRST((sess->trx)->signals));

	if (!sess->client_waits) {
		/* Cannot send the error message now: leave the session to
		the error state and send it later */

		return;
	}

	err_no = sess->err_no;
	err_str = (byte*)sess->err_str;
	err_len = sess->err_len;

	max_len = ut_min(SESS_ERR_BUF_SIZE,
				com_endpoint_get_max_size(sess->endpoint));
	
	sess_srv_msg_init(sess, buf, SESS_SRV_ERROR);

	if (err_len + SESS_SRV_MSG_DATA > max_len) {

		err_len = max_len - SESS_SRV_MSG_DATA;
	}

	ut_memcpy(buf + SESS_SRV_MSG_DATA, err_str, err_len);

	ret = sess_srv_msg_send_low(sess, buf, SESS_SRV_MSG_DATA + err_len,
						SESS_NOT_RELEASE_KERNEL);
}

/*************************************************************************
Copies error info to a session. Sends to the transaction a signal which will
rollback the latest incomplete SQL statement and then send the error message
to the client. NOTE: This function will take care of the freeing of the error
string, thus the caller must supply a copy of the error string. */
static
void
sess_error_low(
/*===========*/
	sess_t*	sess,	/* in: session object */
	ulint	err_no,	/* in: error number */
	char*	err_str)/* in, own: error string or NULL;
			NOTE: the function will take care of freeing of the
			string! */
{
	ut_ad(mutex_own(&kernel_mutex));

	UT_DULINT_INC(sess->error_count);

	printf("Error string::: %s\n", err_str);

	if (sess->state == SESS_ERROR) {
		/* Ignore the error because the session is already in the
		error state */

		if (err_str) {
			mem_free(err_str);
		}

		return;
	}
	
	sess->err_no = err_no;

	if (sess->err_str) {
		mem_free(sess->err_str);
	}

	sess->err_str = err_str;
	sess->err_len = ut_strlen(err_str);
	sess->state = SESS_ERROR;

	if (sess->big_msg) {

		mem_free(sess->big_msg);
	}

	/* Send a signal which will roll back the latest incomplete SQL
	statement: the error message will be sent to the client by the error
	handling mechanism after the rollback is completed. */
	
	trx_sig_send(sess->trx, TRX_SIG_ERROR_OCCURRED, TRX_SIG_SELF, FALSE,
							NULL, NULL, NULL);
}

/***************************************************************************
When a command has been completed, this function sends the message about it
to the client. */

void
sess_command_completed_message(
/*===========================*/
	sess_t*	sess,	/* in: session */
	byte*	msg,	/* in: message buffer */
	ulint	len)	/* in: message data length */
{
	mutex_enter(&kernel_mutex);

	sess_srv_msg_send(sess, msg, SESS_SRV_MSG_DATA + len,
							SESS_RELEASE_KERNEL);
	mutex_exit(&kernel_mutex);
}
