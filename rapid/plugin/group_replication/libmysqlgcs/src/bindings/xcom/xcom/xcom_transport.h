/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_TRANSPORT_H
#define XCOM_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define XDR_INT_SIZE 4
#define MSG_HDR_SIZE (3*XDR_INT_SIZE)

/* Definition of message with fixed size header and variable size payload */

/* This is version 1_0 of the header. It is OK to change the header in
   other versions as long as the version field is the first field.
*/
/* version[4] length[4] type[1] tag[2] UNUSED[1] message-body[length] */
/*
  The version is used both for protocol negotiations and to discard messages with wrong version.
  The length field is the length of the message, not including the header itself.
  The type field is 0 if a normal message, otherwise it is a protocol control message.
  The tag field will be used during protocol negotiation to uniquely identify the request. The reply will
  contain the same tag.
  The message-body contains xdr-serialized data.
 */

#define SERIALIZED_BUFLEN(x) ((x) + MSG_HDR_SIZE)

#define VERS_PTR(buf) (buf)
#define LENGTH_PTR(buf) &((buf)[XDR_INT_SIZE])
#define X_TYPE (2*XDR_INT_SIZE)
#define X_TAG (X_TYPE + 1)
#define X_TAG_PTR(buf)&((buf)[X_TAG])
#ifdef NOTDEF
#define CHECK_PTR(buf) &((buf)[3*XDR_INT_SIZE])
#endif
#define MSG_PTR(buf) &((buf)[MSG_HDR_SIZE])

/* Transport level message types */
enum x_msg_type {
	x_normal = 0, 		/* Normal message */
	x_version_req = 1,	/* Negotiate protocol version */
	x_version_reply = 2	/* Protocol version reply */
};
typedef enum x_msg_type x_msg_type;


struct envelope {
	char	*srv;
	xcom_port	port;
	pax_msg * p;
	int	crash_on_error;
};

typedef struct envelope envelope;

int	check_protoversion(xcom_proto x_proto, xcom_proto negotiated);
int	flush_srv_buf(server *s, int64_t *ret);

/**
  Reads message from connection rfd with buffering reads.

  @param[in]     rfd Pointer to open connection.
  @param[in,out] buf Used for buffering reads.
  @param[out]    p   Output buffer.
  @param[out]    s   Pointer to server. Server timestamp updated if not 0.
  @param[out]    ret Number of bytes read, or -1 if failure.

  @return
    @retval 0 if task should terminate.
    @retval 1 if it should continue.
*/

int buffered_read_msg(connection_descriptor *rfd, srv_buf *buf,
                      pax_msg *p, server *s, int64_t *ret);

/**
  Reads message from connection rfd without buffering reads.

  @param[in]     rfd Pointer to open connection.
  @param[out]    p   Output buffer.
  @param[in,out] s   Pointer to server. Server timestamp updated if not 0.
  @param[in,out] ret Number of bytes read, or -1 if failure.

  @return
    @retval 0 if task should terminate.
    @retval 1 if it should continue.
*/
int read_msg(connection_descriptor *rfd, pax_msg *p, server *s, int64_t *ret);

int	send_to_acceptors(pax_msg *p, const char *dbg);
int	send_to_all(pax_msg *p, const char *dbg);
int	send_to_all_site(site_def const *s, pax_msg *p, const char *dbg);
int	send_to_others(site_def const *s, pax_msg *p, const char *dbg);
int	send_to_someone(site_def const *s, pax_msg *p, const char *dbg);

int	sender_task(task_arg arg);
int	local_sender_task(task_arg arg);
int	shutdown_servers();
int	srv_ref(server *s);
int	srv_unref(server *s);
int	tcp_reaper_task(task_arg arg);
int	tcp_server(task_arg arg);
uint32_t crc32c_hash(char *buf, char *end);
int  apply_xdr(xcom_proto x_proto, gpointer buff, uint32_t bufflen,
               xdrproc_t xdrfunc, void *xdrdata, enum xdr_op op);
void init_crc32c();
void init_xcom_transport(xcom_port listen_port);
void reset_srv_buf(srv_buf *sb);
char	*xcom_get_name(char *a);
xcom_port xcom_get_port(char *a);
int	send_server_msg(site_def const *s, node_no i, pax_msg *p);
double server_active(site_def const *s, node_no i);
void update_servers(site_def *s, cargo_type operation);
void garbage_collect_servers();
int	client_task(task_arg arg);
int	send_msg(server *s, node_no from, node_no to, uint32_t group_id, pax_msg *p);
/**
  Updates timestamp of server.

  @param[in]     s  Pointer to server.
*/
void server_detected(server *s);

void invalidate_servers(const site_def* old_site_def,
                        const site_def* new_site_def);

void shutdown_connection(connection_descriptor *con);
void reset_connection(connection_descriptor *con);
void close_connection(connection_descriptor *con);

#ifdef XCOM_HAVE_OPENSSL
void ssl_free_con(connection_descriptor *con);
void ssl_shutdown_con(connection_descriptor *con);
#endif

char const *xcom_proto_name(xcom_proto proto_vers);
xcom_proto negotiate_protocol(xcom_proto proto_vers);
void get_header_1_0(unsigned char header_buf[], uint32_t *msgsize,
                    x_msg_type *x_type, unsigned int *tag) ;
void put_header_1_0(unsigned char header_buf[], uint32_t msgsize,
                    x_msg_type x_type, unsigned int tag);

int	send_proto(connection_descriptor *con, xcom_proto x_proto,
               x_msg_type x_type, unsigned int tag, int64_t *ret);
int	recv_proto(connection_descriptor const * rfd, xcom_proto *x_proto,
               x_msg_type *x_type, unsigned int *tag, int64_t *ret);

void write_protoversion(unsigned char *buf, xcom_proto proto_vers);
xcom_proto read_protoversion(unsigned char *p);

int serialize_msg(pax_msg *p, xcom_proto x_proto, uint32_t *buflen, char **buf);
int deserialize_msg(pax_msg *p, xcom_proto x_proto,  char *buf,
                    uint32_t buflen);
xcom_proto common_xcom_version(site_def const *site);
xcom_proto get_latest_common_proto();
xcom_proto set_latest_common_proto(xcom_proto x_proto);

#ifdef __cplusplus
}
#endif

#endif

