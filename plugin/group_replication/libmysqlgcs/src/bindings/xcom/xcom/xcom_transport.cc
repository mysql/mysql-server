/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <rpc/rpc.h>
#include <stdlib.h>
#include <string.h>

#include "xcom/xcom_profile.h"
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "xcom/node_connection.h"
#include "xcom/node_list.h"
#include "xcom/node_no.h"
#include "xcom/retry.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/site_struct.h"
#include "xcom/sock_probe.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_msg_queue.h"
#include "xcom/xcom_statistics.h"
#include "xcom/xcom_transport.h"
#include "xcom/xcom_vp_str.h"
#include "xcom/xdr_utils.h"
#include "xdr_gen/xcom_vp.h"

#include "xcom/network/network_provider_manager.h"
#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif /* _WIN32 */

#include <openssl/err.h>
#include <openssl/ssl.h>

#endif

#define MY_XCOM_PROTO x_1_9

xcom_proto const my_min_xcom_version =
    x_1_0; /* The minimum protocol version I am able to understand */
xcom_proto const my_xcom_version =
    MY_XCOM_PROTO; /* The maximum protocol version I am able to understand */

/* #define XCOM_ECM */

#define SERVER_MAX (2 * NSERVERS)

/* Turn Nagle's algorithm on or off */
static int const NAGLE = 0;

extern int xcom_shutdown;

static void shut_srv(server *s);

static xcom_port xcom_listen_port = 0; /* Port used by xcom */

/* purecov: begin deadcode */
static int pm(xcom_port port) { return port == xcom_listen_port; }
/* purecov: end */

int close_open_connection(connection_descriptor *conn) {
  return Network_provider_manager::getInstance().close_xcom_connection(conn);
}

connection_descriptor *open_new_connection(const char *server, xcom_port port,
                                           int connection_timeout) {
  return Network_provider_manager::getInstance().open_xcom_connection(
      server, port, Network_provider_manager::getInstance().is_xcom_using_ssl(),
      connection_timeout);
}

/* purecov: begin deadcode */
connection_descriptor *open_new_local_connection(const char *server,
                                                 xcom_port port) {
  // Local connection must avoid SSL at all costs.
  // Nevertheless, we will keep the service running with local signalling,
  // trying to make a connection without SSL, and afterwards, with SSL.
  connection_descriptor *retval = nullptr;
  retval = Network_provider_manager::getInstance().open_xcom_connection(
      server, port, false);

  if (retval->fd == -1) {
    free(retval);
    retval = nullptr;
    retval = open_new_connection(server, port);
  }

  return retval;
}
/* purecov: end */

result set_nodelay(int fd) {
  int n = 1;
  result ret = {0, 0};

  do {
    SET_OS_ERR(0);
    ret.val =
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (xcom_buf *)&n, sizeof n);
    ret.funerr = to_errno(GET_OS_ERR);
    IFDBG(D_NONE, FN; NDBG(from_errno(ret.funerr), d));
  } while (ret.val < 0 && can_retry(ret.funerr));
  return ret;
}

void init_xcom_transport(xcom_port listen_port) {
  xcom_listen_port = listen_port;
  if (get_port_matcher() == nullptr) /* purecov: begin deadcode */
    set_port_matcher(pm);
  /* purecov: end */
}

void reset_srv_buf(srv_buf *sb) {
  sb->start = 0;
  sb->n = 0;
}

/* Note that channel is alive */
static void alive(server *s) {
  if (s) {
    s->active = task_now();
  }
}

static u_int srv_buf_capacity(srv_buf *sb) { return sizeof(sb->buf); }

static u_int srv_buf_free_space(srv_buf *sb) {
  return ((u_int)sizeof(sb->buf)) - sb->n;
}

static u_int srv_buf_buffered(srv_buf *sb) { return sb->n - sb->start; }

static char *srv_buf_extract_ptr(srv_buf *sb) { return &sb->buf[sb->start]; }

static char *srv_buf_insert_ptr(srv_buf *sb) { return &sb->buf[sb->n]; }

static inline void advance_extract_ptr(srv_buf *sb, u_int len) {
  sb->start += len;
}

static u_int get_srv_buf(srv_buf *sb, char *data, u_int len) {
  if (len > srv_buf_buffered(sb)) {
    len = srv_buf_buffered(sb);
  }

  memcpy(data, srv_buf_extract_ptr(sb), (size_t)len);
  advance_extract_ptr(sb, len);
  return len;
}

static inline void advance_insert_ptr(srv_buf *sb, u_int len) { sb->n += len; }

static u_int put_srv_buf(srv_buf *sb, char *data, u_int len) {
  assert(sb->n + len <= sizeof(sb->buf));
  memcpy(srv_buf_insert_ptr(sb), data, (size_t)len);
  advance_insert_ptr(sb, len);
  return len;
}

int flush_srv_buf(server *s, int64_t *ret) {
  DECL_ENV
  u_int buflen;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t sent{0};
  TASK_BEGIN
  ep->buflen = s->out_buf.n;
  reset_srv_buf(&s->out_buf);
  if (s->con->fd >= 0) {
    if (ep->buflen) {
      IFDBG(D_TRANSPORT, FN; PTREXP(stack); NDBG(ep->buflen, u));
      /* LOCK_FD(s->con.fd, 'w'); */
      TASK_CALL(task_write(s->con, s->out_buf.buf, ep->buflen, &sent));
      /* UNLOCK_FD(s->fd, 'w'); */
      if (sent <= 0) {
        shutdown_connection(s->con);
      }
      TASK_RETURN(sent);
    }
    TASK_RETURN(0);
  } else {
    TASK_FAIL;
  }

  FINALLY
  TASK_END;
}

/*
This function checks if a new node entering the system that is from a lower
version than us, is able to speak to every node in the system. This is used
for rolling downgrade purposes.

For this, we will check if every node in the current configuration is reachable
via IPv4 either by address or by name.

If there is at least one single node that is reachable ONLY through IPv6, this
should fail. A DBA then must ensure that all nodes are configured with an
address that is reachable via IPv4 old nodes.
*/
int is_new_node_eligible_for_ipv6(xcom_proto incoming_proto,
                                  const site_def *current_site_def) {
  if (incoming_proto >= MY_XCOM_PROTO) return 0; /* For sure it will speak V6 */

  /* I must check if all nodes are IPv4-reachable.
     This means that:
     - They are configured with an IPv4 raw address
     - They have a name that is reachable via IPv4 name resolution */
  if (current_site_def == nullptr)
    return 0; /* This means that we are the ones entering a group */

  {
    node_address *na = current_site_def->nodes.node_list_val;
    u_int node;

    /* For each node in the current configuration */
    for (node = 0; node < current_site_def->nodes.node_list_len; node++) {
      int has_ipv4_address = 0;
      struct addrinfo *node_addr = nullptr;
      struct addrinfo *node_addr_cycle = nullptr;
      char ip[IP_MAX_SIZE];
      xcom_port port;

      if (get_ip_and_port(na[node].address, ip, &port)) {
        G_DEBUG("Error parsing IP and Port. Returning an error");
        return 1;
      }

      /* Query the name server */
      checked_getaddrinfo(ip, nullptr, nullptr, &node_addr);

      /* Lets cycle through all returned addresses and check if at least one
         address is reachable via IPv4. */
      node_addr_cycle = node_addr;
      while (!has_ipv4_address && node_addr_cycle) {
        if (node_addr_cycle->ai_family == AF_INET) {
          has_ipv4_address = 1;
        }
        node_addr_cycle = node_addr_cycle->ai_next;
      }

      if (node_addr) freeaddrinfo(node_addr);

      if (!has_ipv4_address) return 1;
    }
  }

  return 0;
}

/* Send a message to server s */
static int _send_msg(server *s, pax_msg *p, node_no to, int64_t *ret) {
  DECL_ENV
  uint32_t buflen;
  char *buf;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t sent{0};
  TASK_BEGIN
  p->to = to;
  IFDBG(D_NONE, FN; PTREXP(stack); PTREXP(s); PTREXP(p); NDBG(s->con.fd, d));
  IFDBG(D_NONE, FN; STREXP(s->srv); NDBG(s->port, d); NDBG(task_now(), f);
        COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
  if (to == p->from) {
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
    dispatch_op(find_site_def(p->synode), p, nullptr);
    TASK_RETURN(sizeof(*p));
  } else {
    p->max_synode = get_max_synode();
    if (s->con->fd >= 0) {
      /* LOCK_FD(s->con.fd, 'w'); */
      serialize_msg(p, s->con->x_proto, &ep->buflen, &ep->buf);
      IFDBG(D_TRANSPORT, FN; NDBG(ep->buflen, u));
      if (ep->buflen) {
        /* Not enough space? Flush the buffer */
        if (ep->buflen > srv_buf_free_space(&s->out_buf)) {
          TASK_CALL(flush_srv_buf(s, ret));
          if (s->con->fd < 0) {
            TASK_FAIL;
          }
          /* Still not enough? Message must be huge, send without buffering */
          if (ep->buflen > srv_buf_free_space(&s->out_buf)) {
            IFDBG(D_TRANSPORT, FN; STRLIT("task_write "); NDBG(ep->buflen, u));
            TASK_CALL(task_write(s->con, ep->buf, ep->buflen, &sent));
            if (s->con->fd < 0) {
              TASK_FAIL;
            }
          } else { /* Buffer the write */
            put_srv_buf(&s->out_buf, ep->buf, ep->buflen);
            sent = ep->buflen;
          }
        } else { /* Buffer the write */
          put_srv_buf(&s->out_buf, ep->buf, ep->buflen);
          sent = ep->buflen;
        }
        send_count[p->op]++;
        send_bytes[p->op] += ep->buflen;
        alive(s); /* Note activity */
        /* IFDBG(D_NONE, STRLIT("sent message "); STRLIT(pax_op_to_str(p->op));
         */
        /* NDBG(p->from,d); NDBG(p->to,d); */
        /* SYCEXP(p->synode);  */
        /* BALCEXP(p->proposal)); */
        X_FREE(ep->buf);
        /* UNLOCK_FD(s->con.fd, 'w'); */
        if (sent <= 0) {
          shutdown_connection(s->con);
        }
        TASK_RETURN(sent);
      }
      TASK_RETURN(0);
    } else
      TASK_FAIL;
  }
  FINALLY
  if (ep->buf) X_FREE(ep->buf);
  TASK_END;
}

void write_protoversion(unsigned char *buf, xcom_proto proto_vers) {
  put_32(VERS_PTR(buf), proto_vers);
}

xcom_proto read_protoversion(unsigned char *p) { return (xcom_proto)get_32(p); }

int check_protoversion(xcom_proto x_proto, xcom_proto negotiated) {
  if (x_proto != negotiated) {
    IFDBG(D_NONE, FN; STRLIT(" found XCOM protocol version "); NDBG(x_proto, d);
          STRLIT(" need version "); NDBG(negotiated, d););

    return 0;
  }
  return 1;
}

/* Send a protocol negotiation message on connection con */
int send_proto(connection_descriptor *con, xcom_proto x_proto,
               x_msg_type x_type, unsigned int tag, int64_t *ret) {
  DECL_ENV
  char buf[MSG_HDR_SIZE];
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t sent{0};
  TASK_BEGIN
  if (con->fd >= 0) {
    con->snd_tag = tag;
    write_protoversion(VERS_PTR((unsigned char *)ep->buf), x_proto);
    put_header_1_0((unsigned char *)ep->buf, 0, x_type, tag);
    TASK_CALL(task_write(con, ep->buf, MSG_HDR_SIZE, &sent));
    if (con->fd < 0) {
      TASK_FAIL;
    }
    if (sent <= 0) {
      shutdown_connection(con);
    }
    TASK_RETURN(sent);
  } else {
    TASK_FAIL;
  }
  FINALLY

  TASK_END;
}

/*
 * xdrfunc() has different sigatures for __cplusplus
 */
#ifdef __APPLE__
#define XDRFUNC(a, b) xdrfunc((a), (b), 0)
#else
#define XDRFUNC xdrfunc
#endif

int apply_xdr(void *buff, uint32_t bufflen, xdrproc_t xdrfunc, void *xdrdata,
              enum xdr_op op) {
  XDR xdr;
  [[maybe_unused]] int s = 0;

  xdr.x_ops = nullptr;
  xdrmem_create(&xdr, (char *)buff, bufflen, op);
  /*
    Mac OSX changed the xdrproc_t prototype to take
    three parameters instead of two.

    The argument is that it has the potential to break
    the ABI due to compiler optimizations.

    The recommended value for the third parameter is
    0 for those that are not making use of it (which
    is the case). This will keep this code cross-platform
    and cross-version compatible.
  */
  if (xdr.x_ops) {
    s = XDRFUNC(&xdr, xdrdata);
    xdr_destroy(&xdr);
  }
  return s;
}

#if TASK_DBUG_ON
static void dump_header(char *buf) {
  char *end = buf + MSG_HDR_SIZE;
  GET_GOUT;
  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
  STRLIT("message header ");
  PTREXP(buf);
  while (buf < end) {
    NPUT(*buf, x);
    buf++;
  }
  PRINT_GOUT;
  FREE_GOUT;
}
#endif

void dbg_app_data(app_data_ptr a);

/* Return 0 if it fails to serialize the message, otherwise 1 is returned. */
static int serialize(void *p, xcom_proto x_proto, uint32_t *out_len,
                     xdrproc_t xdrfunc, char **out_buf) {
  unsigned char *buf = nullptr;
  uint64_t msg_buflen = 0;
  uint64_t tot_buflen = 0;
  unsigned int tag = 666;
  x_msg_type x_type = x_normal;
  int retval = 0;

  /* Find length of serialized message */
  msg_buflen = xdr_sizeof(xdrfunc, p);
  if (!msg_buflen) return 0;
  assert(msg_buflen);
  tot_buflen = SERIALIZED_BUFLEN(msg_buflen);
  IFDBG(D_TRANSPORT, FN; PTREXP(p); NDBG64(msg_buflen); NDBG64(tot_buflen));
  /*
    Paxos message size is limited in UINT32 range. It will return an
    error if the serialized message is bigger than UINT32_MAX bytes.
  */
  if (tot_buflen > UINT32_MAX) {
    G_ERROR("Serialized message exceeds 4GB limit.");
    return retval;
  }

  /*
    Allocate space for version number, length field, type, tag, and serialized
    message. Explicit type case suppress the warnings on 32bits.
  */
  buf = (unsigned char *)xcom_calloc((size_t)1, (size_t)tot_buflen);
  if (buf) {
    /* Write protocol version */
    write_protoversion(buf, x_proto);

    /* Serialize message */
    retval =
        apply_xdr(MSG_PTR(buf), (uint32_t)msg_buflen, xdrfunc, p, XDR_ENCODE);
    if (retval) {
      /* Serialize header into buf */
      put_header_1_0(buf, (uint32_t)msg_buflen, x_type, tag);
    }

    *out_len = (uint32_t)tot_buflen;
    *out_buf = (char *)buf;
  }
  IFDBG(D_TRANSPORT, FN; NDBG(retval, d); NDBG(*out_len, u); PTREXP(*out_buf);
        dump_header(*out_buf));
  return retval;
}

/* Version 1 has no new messages, only modified, so all should be sent */
static inline int old_proto_knows(xcom_proto x_proto [[maybe_unused]],
                                  pax_op op [[maybe_unused]]) {
  return 1;
}

static xdrproc_t pax_msg_func[] = {
    reinterpret_cast<xdrproc_t>(0),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_0),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_1),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_2),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_3),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_4),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_5),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_6),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_7),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_8),
    reinterpret_cast<xdrproc_t>(xdr_pax_msg_1_9)};

int serialize_msg(pax_msg *p, xcom_proto x_proto, uint32_t *buflen,
                  char **buf) {
  *buflen = 0;
  *buf = nullptr;
  return (x_proto >= x_1_0 && x_proto <= MY_XCOM_PROTO) &&
         old_proto_knows(x_proto, p->op) &&
         serialize((void *)p, x_proto, buflen, pax_msg_func[x_proto], buf);
}

int deserialize_msg(pax_msg *p, xcom_proto x_proto, char *buf,
                    uint32_t buflen) {
  if (x_proto >= x_1_0 && x_proto <= MY_XCOM_PROTO) {
    int apply_ok =
        apply_xdr(buf, buflen, pax_msg_func[x_proto], (void *)p, XDR_DECODE);
    if (!apply_ok) {
      xdr_free((xdrproc_t)xdr_pax_msg, (char *)p);
      memset(p, 0, sizeof(*p));
    }
    return apply_ok;
  }
  return 0;
}

/* Better checksum */
static uint32_t crc_table[256];

void init_crc32c() {
  uint32_t i;
  for (i = 0; i < 256; i++) {
    int j;
    uint32_t c = i;
    for (j = 0; j < 8; j++) {
      c = (c & 1) ? (0x82F63B78 ^ (c >> 1)) : (c >> 1);
    }
    crc_table[i] = c;
  }
}

#define CRC32CSTART 0xFFFFFFFF

/* Paxos servers (nodes) */

/* Array of servers, only maxservers entries actually used */
static server *all_servers[SERVER_MAX];
static int maxservers = 0;

/* Create a new server */
static server *mksrv(char *srv, xcom_port port) {
  server *s;

  s = (server *)xcom_calloc((size_t)1, sizeof(*s));

  IFDBG(D_NONE, FN; PTREXP(s); STREXP(srv));
  if (s == nullptr) {
    g_critical("out of memory");
    abort();
  }
  s->garbage = 0;
  s->invalid = 0;
  s->refcnt = 0;
  s->srv = srv;
  s->port = port;
  s->con = new_connection(-1, nullptr);
  s->active = 0.0;
  s->detected = 0.0;
  s->last_ping_received = 0.0;
  s->number_of_pings_received = 0;
#if defined(_WIN32)
  s->reconnect = false;
#endif

  channel_init(&s->outgoing, TYPE_HASH("msg_link"));
  IFDBG(D_NONE, FN; STREXP(srv); NDBG(port, d));

  if (xcom_mynode_match(srv, port)) { /* Short-circuit local messages */
    IFDBG(D_NONE, FN; STRLIT("creating local sender"); STREXP(srv);
          NDBG(port, d));
    s->sender = task_new(local_sender_task, void_arg(s), "local_sender_task",
                         XCOM_THREAD_DEBUG);
  } else {
    s->sender =
        task_new(sender_task, void_arg(s), "sender_task", XCOM_THREAD_DEBUG);
    IFDBG(D_NONE, FN; STRLIT("creating sender and reply_handler"); STREXP(srv);
          NDBG(port, d));
    s->reply_handler = task_new(reply_handler_task, void_arg(s),
                                "reply_handler_task", XCOM_THREAD_DEBUG);
  }
  reset_srv_buf(&s->out_buf);
  return s;
}

static server *addsrv(char *srv, xcom_port port) {
  server *s = mksrv(srv, port);
  assert(all_servers[maxservers] == nullptr);
  assert(maxservers < SERVER_MAX);
  all_servers[maxservers] = s;
  /*
   Keep the server from being freed if the acceptor_learner_task calls
   srv_unref on the server before the {local_,}server_task and
   reply_handler_task begin.
  */
  srv_ref(s);
  IFDBG(D_NONE, FN; PTREXP(all_servers[maxservers]);
        STREXP(all_servers[maxservers]->srv);
        NDBG(all_servers[maxservers]->port, d); NDBG(maxservers, d));
  maxservers++;
  return s;
}

static void rmsrv(int i) {
  assert(all_servers[i]);
  assert(maxservers > 0);
  assert(i < maxservers);
  IFDBG(D_NONE, FN; PTREXP(all_servers[i]); STREXP(all_servers[i]->srv);
        NDBG(all_servers[i]->port, d); NDBG(i, d));
  maxservers--;

  /* Allow the server to be freed. This unref pairs with the ref from addsrv. */
  srv_unref(all_servers[i]);

  all_servers[i] = all_servers[maxservers];
  all_servers[maxservers] = nullptr;
}

static void init_collect() {
  int i;

  for (i = 0; i < maxservers; i++) {
    assert(all_servers[i]);
    all_servers[i]->garbage = 1;
  }
}

extern void get_all_site_defs(site_def ***s, uint32_t *n);

static void mark_site_servers(site_def *site) {
  u_int i;
  for (i = 0; i < get_maxnodes(site); i++) {
    server *s = site->servers[i];
    assert(s);
    s->garbage = 0;
  }
}

static void mark() {
  site_def **site;
  uint32_t n;
  uint32_t i;

  get_all_site_defs(&site, &n);

  for (i = 0; i < n; i++) {
    if (site[i]) {
      mark_site_servers(site[i]);
    }
  }
}

static void sweep() {
  int i = 0;
  while (i < maxservers) {
    server *s = all_servers[i];
    assert(s);
    if (s->garbage) {
      IFDBG(D_NONE, FN; STREXP(s->srv));
      shut_srv(s);
      rmsrv(i);
    } else {
      i++;
    }
  }
}

void garbage_collect_servers() {
  IFDBG(D_NONE, FN);
  init_collect();
  mark();
  sweep();
  IFDBG(D_NONE, FN);
}

/* Free a server */
static void freesrv(server *s) {
  X_FREE(s->con);
  X_FREE(s->srv);
  X_FREE(s);
}

double server_active(site_def const *s, node_no i) {
  if (s->servers[i])
    return s->servers[i]->active;
  else
    return 0.0;
}

/* Shutdown server */
static void shut_srv(server *s) {
  if (!s) return;
  IFDBG(D_NONE, FN; PTREXP(s); STREXP(s->srv));

  shutdown_connection(s->con);

  /* Tasks will free the server object when they terminate */
  if (s->sender) task_terminate(s->sender);
  if (s->reply_handler) task_terminate(s->reply_handler);
}

int srv_ref(server *s) {
  assert(s->refcnt >= 0);
  s->refcnt++;
  return s->refcnt;
}

int srv_unref(server *s) {
  assert(s->refcnt >= 0);
  s->refcnt--;
  if (s->refcnt == 0) {
    freesrv(s);
    return 0;
  }
  return s->refcnt;
}

/* Listen for connections on socket and create a handler task */
int incoming_connection_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  connection_descriptor *new_conn;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  TASK_BEGIN

  do {
    ep->new_conn =
        Network_provider_manager::getInstance().incoming_connection();
    if (ep->new_conn == nullptr) {
      TASK_DELAY(0.1);
    } else {
      task_new(acceptor_learner_task, void_arg(ep->new_conn),
               "acceptor_learner_task", XCOM_THREAD_DEBUG);
    }
  } while (!xcom_shutdown);
  FINALLY
  // Cleanup connection if this is stuck
  connection_descriptor *clean_conn =
      Network_provider_manager::getInstance().incoming_connection();
  if (clean_conn) {
    close_connection(clean_conn);
  }
  free(clean_conn);

  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}

void server_detected(server *s) { s->detected = task_now(); }

/* Try to connect to another node */
static int dial(server *s) {
  DECL_ENV
  int dummy;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN
  IFDBG(D_BUG, FN; STRLIT(" dial "); NPUT(get_nodeno(get_site_def()), u);
        STRLIT(s->srv); NDBG(s->port, u));

  // Delete old connection
  reset_connection(s->con);
  X_FREE(s->con);
  s->con = nullptr;

  s->con = open_new_connection(s->srv, s->port, 1000);
  if (!s->con) {
    s->con = new_connection(-1, nullptr);
  }

  if (s->con->fd < 0) {
    IFDBG(D_NONE, FN; STRLIT("could not dial "); STRLIT(s->srv);
          NDBG(s->port, u););
  } else {
    if (NAGLE == 0) {
      set_nodelay(s->con->fd);
    }

    unblock_fd(s->con->fd);
    IFDBG(D_BUG, FN; STRLIT(" dial connected ");
          NPUT(get_nodeno(get_site_def()), u); STRLIT(s->srv);
          NDBG(s->port, u));
    set_connected(s->con, CON_FD);
    alive(s);
    update_detected(get_site_def_rw());
  }
  FINALLY
  TASK_END;
}

/* Send message by putting it in the server queue */
int send_msg(server *s, node_no from, node_no to, uint32_t group_id,
             pax_msg *p) {
  assert(p);
  assert(s);
  {
    msg_link *link = msg_link_new(p, to);
    IFDBG(D_NONE, FN; PTREXP(&s->outgoing);
          COPY_AND_FREE_GOUT(dbg_msg_link(link)););
    p->from = from;
    p->group_id = group_id;
    p->max_synode = get_max_synode();
    p->delivered_msg = get_delivered_msg();
    IFDBG(D_NONE, FN; PTREXP(p); STREXP(s->srv); NDBG(p->from, d);
          NDBG(p->to, d); NDBG(p->group_id, u));
    channel_put(&s->outgoing, &link->l);
  }
  return 0;
}

static inline int _send_server_msg(site_def const *s, node_no to, pax_msg *p) {
  assert(s);
  assert(s->servers[to]);
  if (s->servers[to] && s->servers[to]->invalid == 0 && p) {
    send_msg(s->servers[to], s->nodeno, to, get_group_id(s), p);
  }
  return 0;
}

int send_server_msg(site_def const *s, node_no to, pax_msg *p) {
  return _send_server_msg(s, to, p);
}

/* Selector function to see if we should send message */
typedef int (*node_set_selector)(site_def const *s, node_no node);

static int all(site_def const *s, node_no node) {
  (void)s;
  (void)node;
  return 1;
}

/* purecov: begin deadcode */
static int not_self(site_def const *s, node_no node) {
  return s->nodeno != node;
}
/* purecov: end */

/* Send message to set of nodes determined by test_func */
static inline int send_to_node_set(site_def const *s, node_no max, pax_msg *p,
                                   node_set_selector test_func,
                                   const char *dbg [[maybe_unused]]) {
  int retval = 0;
  assert(s);
  if (s) {
    node_no i = 0;
    for (i = 0; i < max; i++) {
      if (test_func(s, i)) {
        IFDBG(D_NONE, FN; STRLIT(dbg); STRLIT(" "); NDBG(i, u); NDBG(max, u);
              PTREXP(p));
        retval = _send_server_msg(s, i, p);
      }
    }
  }
  return retval;
}

/* Send to all servers in site */
int send_to_all_site(site_def const *s, pax_msg *p, const char *dbg) {
  int retval = 0;
  retval = send_to_node_set(s, get_maxnodes(s), p, all, dbg);
  return retval;
}

/* Send to all servers in site except self */
/* purecov: begin deadcode */
int send_to_all_except_self(site_def const *s, pax_msg *p, const char *dbg) {
  int retval = 0;
  retval = send_to_node_set(s, get_maxnodes(s), p, not_self, dbg);
  return retval;
}
/* purecov: end */

/* Send to all servers */
int send_to_all(pax_msg *p, const char *dbg) {
  return send_to_all_site(find_site_def(p->synode), p, dbg);
}

static inline int send_other_loop(site_def const *s, pax_msg *p,
                                  const char *dbg [[maybe_unused]]) {
  int retval = 0;
  node_no i = 0;
#ifdef MAXACCEPT
  node_no max = MIN(get_maxnodes(s), MAXACCEPT);
#else
  node_no max;
  assert(s);
  max = get_maxnodes(s);
#endif
  for (i = 0; i < max; i++) {
    if (i != s->nodeno) {
      IFDBG(D_NONE, FN; STRLIT(dbg); STRLIT(" "); NDBG(i, u); NDBG(max, u);
            PTREXP(p));
      retval = _send_server_msg(s, i, p);
    }
  }
  return retval;
}

/* Send to other servers */
int send_to_others(site_def const *s, pax_msg *p, const char *dbg) {
  int retval = 0;
  retval = send_other_loop(s, p, dbg);
  return retval;
}

/* Send to some other live server, round robin */
int send_to_someone(site_def const *s, pax_msg *p,
                    const char *dbg [[maybe_unused]]) {
  int retval = 0;
  static node_no i = 0;
  node_no prev = 0;
#ifdef MAXACCEPT
  node_no max = MIN(get_maxnodes(s), MAXACCEPT);
#else
  node_no max;
  assert(s);
  max = get_maxnodes(s);
#endif
  /* IFDBG(D_NONE, FN; NDBG(max,u); NDBG(s->maxnodes,u)); */
  assert(max > 0);
  prev = i % max;
  i = (i + 1) % max;
  while (i != prev) {
    /* IFDBG(D_NONE, FN; NDBG(i,u); NDBG(prev,u)); */
    if (i != s->nodeno && !may_be_dead(s->detected, i, task_now())) {
      IFDBG(D_NONE, FN; STRLIT(dbg); NDBG(i, u); NDBG(max, u); PTREXP(p));
      retval = _send_server_msg(s, i, p);
      break;
    }
    i = (i + 1) % max;
  }
  return retval;
}

#ifdef MAXACCEPT
/* Send to all acceptors */
int send_to_acceptors(pax_msg *p, const char *dbg) {
  site_def const *s = find_site_def(p->synode);
  int retval = 0;
  int i;
  retval = send_to_node_set(s, MIN(MAXACCEPT, s->maxnodes), p, all, dbg);
  return retval;
}

#else
/* Send to all acceptors */
int send_to_acceptors(pax_msg *p, const char *dbg) {
  return send_to_all(p, dbg);
}

#endif

/* Used by :/int.*read_msg */
/**
  Reads n bytes from connection rfd without buffering reads.

  @param[in]     rfd Pointer to open connection.
  @param[out]    p   Output buffer.
  @param[in]     n   Number of bytes to read.
  @param[out]    s   Pointer to server.
  @param[out]    ret Number of bytes read, or -1 if failure.

  @retval 0 if task should terminate.
  @retval 1 if it should continue.
*/
static int read_bytes(connection_descriptor const *rfd, char *p, uint32_t n,
                      server *s, int64_t *ret) {
  DECL_ENV
  uint32_t left;
  char *bytes;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t nread = 0;

  IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); PTREXP(s); NDBG(n, u));
  TASK_BEGIN

      (void)
  s;
  ep->left = n;
  ep->bytes = (char *)p;
  while (ep->left > 0) {
    TASK_CALL(task_read(rfd, ep->bytes,
                        ep->left >= INT_MAX ? INT_MAX : (int)ep->left, &nread));
    IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); NDBG64(nread); NDBG(ep->left, u));
    if (nread == 0) {
      TASK_RETURN(0);
    } else if (nread < 0) {
      IFDBG(D_NONE, FN; NDBG64(nread));
      TASK_FAIL;
    } else {
      ep->bytes += nread;
      ep->left -= (uint32_t)nread;
    }
  }
  assert(ep->left == 0);
  TASK_RETURN(n);
  FINALLY
  TASK_END;
}

/**
  Reads n bytes from connection rfd with buffering reads.

  @param[in]     rfd Pointer to open connection.
  @param[in,out] buf Used for buffering reads.
                     Originally initialized by caller, maintained by
  buffered_read_bytes.
  @param[out]    p   Output buffer.
  @param[in]     n   Number of bytes to read
  @param[out]    s   Pointer to server.
  @param[out]    ret Number of bytes read, or -1 if failure.

  @retval 0 if task should terminate.
  @retval 1 if it should continue.
*/
static int buffered_read_bytes(connection_descriptor const *rfd, srv_buf *buf,
                               char *p, uint32_t n, server *s, int64_t *ret) {
  DECL_ENV
  uint32_t left;
  char *bytes;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  uint32_t nget = 0;

  IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); PTREXP(s); NDBG(n, u));

  int64_t nread{0};
  TASK_BEGIN(void) s;
  ep->left = n;
  ep->bytes = (char *)p;

  /* First, try to get bytes from buffer */
  nget = get_srv_buf(buf, ep->bytes, n);
  ep->bytes += nget;
  ep->left -= nget;

  if (ep->left >= srv_buf_capacity(buf)) {
    /* Too big, do direct read of rest */
    IFDBG(D_TRANSPORT, FN; STRLIT("Too big, do direct read of rest"));
    TASK_CALL(read_bytes(rfd, ep->bytes, ep->left, s, ret));
    if (*ret <= 0) {
      TASK_FAIL;
    }
    ep->left -= (uint32_t)(*ret);
  } else {
    /* Buffered read makes sense */
    while (ep->left > 0) {
      /* Buffer is empty, reset and read */
      reset_srv_buf(buf);

      TASK_CALL(task_read(rfd, srv_buf_insert_ptr(buf),
                          (int)srv_buf_free_space(buf), &nread));
      IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); NDBG64(nread););
      if (nread == 0) {
        TASK_RETURN(0);
      } else if (nread < 0) {
        IFDBG(D_NONE, FN; NDBG64(nread));
        TASK_FAIL;
      } else {
        /* Update buffer to reflect number of bytes read */
        advance_insert_ptr(buf, (uint)nread);
        nget = get_srv_buf(buf, ep->bytes, ep->left);
        ep->bytes += nget;
        ep->left -= nget;
      }
    }
  }
  assert(ep->left == 0);
  TASK_RETURN(n);
  FINALLY
  TASK_END;
}

void get_header_1_0(unsigned char header_buf[], uint32_t *msgsize,
                    x_msg_type *x_type, unsigned int *tag) {
  *msgsize = get_32(LENGTH_PTR(header_buf));
  *x_type = (x_msg_type)header_buf[X_TYPE];
  *tag = get_16(X_TAG_PTR(header_buf));
  IFDBG(D_TRANSPORT, FN; NDBG(*msgsize, u); NDBG(*x_type, d); NDBG(*tag, u));
}

void put_header_1_0(unsigned char header_buf[], uint32_t msgsize,
                    x_msg_type x_type, unsigned int tag) {
  IFDBG(D_TRANSPORT, FN; NDBG(msgsize, u); NDBG(x_type, d); NDBG(tag, u));
  put_32(LENGTH_PTR(header_buf), msgsize);
  header_buf[X_TYPE] = (unsigned char)x_type;
  put_16(X_TAG_PTR(header_buf), tag);
}

/* See also :/static .*read_bytes */
int read_msg(connection_descriptor *rfd, pax_msg *p, server *s, int64_t *ret) {
  int deserialize_ok = 0;

  DECL_ENV
  int64_t n;
  char *bytes;
  unsigned char header_buf[MSG_HDR_SIZE];
  xcom_proto x_version;
  uint32_t msgsize;
  x_msg_type x_type;
  unsigned int tag;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN
  do {
    ep->bytes = nullptr;
    /* Read length field, protocol version, and checksum */
    ep->n = 0;
    IFDBG(D_TRANSPORT, FN; STRLIT("reading header"));
    TASK_CALL(read_bytes(rfd, (char *)ep->header_buf, MSG_HDR_SIZE, s, &ep->n));

    if (ep->n != MSG_HDR_SIZE) {
      G_INFO("Failure reading from fd=%d n=%" PRIu64 " from %s:%d", rfd->fd,
             ep->n, s->srv, s->port);
      IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); NDBG64(ep->n));
      TASK_FAIL;
    }

    /* Check the protocol version before doing anything else */
    ep->x_version = read_protoversion(VERS_PTR(ep->header_buf));
    get_header_1_0(ep->header_buf, &ep->msgsize, &ep->x_type, &ep->tag);
    if (ep->x_type == x_version_req) {
      /* Negotiation request. See what we can offer */
      rfd->x_proto = negotiate_protocol(ep->x_version);
      IFDBG(D_TRANSPORT,
            STRLIT("incoming connection will use protcol version ");
            NDBG(rfd->x_proto, u); STRLIT(xcom_proto_to_str(rfd->x_proto));
            NDBG(rfd->fd, d));
      ADD_DBG(
          D_TRANSPORT,
          add_event(EVENT_DUMP_PAD,
                    string_arg("incoming connection will use protcol version"));
          add_event(EVENT_DUMP_PAD,
                    string_arg(xcom_proto_to_str(rfd->x_proto))););
      if (rfd->x_proto > my_xcom_version) TASK_FAIL;
      if (is_new_node_eligible_for_ipv6(ep->x_version, get_site_def())) {
        G_WARNING(
            "Incoming node is not eligible to enter the group due to lack "
            "of IPv6 support. There is at least one group member that is "
            "reachable only via IPv6. Please configure the whole group with "
            "IPv4 addresses and try again");
        TASK_FAIL;
      }
      set_connected(rfd, CON_PROTO);
      TASK_CALL(send_proto(rfd, rfd->x_proto, x_version_reply, ep->tag, ret));
    } else if (ep->x_type == x_version_reply) {
      /* Mark connection with negotiated protocol version */
      if (rfd->snd_tag == ep->tag) {
        rfd->x_proto = ep->x_version;
        IFDBG(D_TRANSPORT, STRLIT("peer connection will use protcol version ");
              NDBG(rfd->x_proto, u); STRLIT(xcom_proto_to_str(rfd->x_proto));
              NDBG(rfd->fd, d));

        ADD_DBG(D_TRANSPORT,
                add_event(
                    0, string_arg("peer connection will use protcol version"));
                add_event(EVENT_DUMP_PAD,
                          string_arg(xcom_proto_to_str(rfd->x_proto))););
        if (rfd->x_proto > my_xcom_version || rfd->x_proto == x_unknown_proto)
          TASK_FAIL;

        set_connected(rfd, CON_PROTO);
      }
    }
  } while (ep->x_type != x_normal);

#ifdef XCOM_PARANOID
  assert(check_protoversion(ep->x_version, rfd->x_proto));
#endif
  if (!check_protoversion(ep->x_version, rfd->x_proto)) {
    TASK_FAIL;
  }

  /* OK, we can grok this version */

  /* Allocate buffer space for message */
  ep->bytes = (char *)xcom_calloc((size_t)1, (size_t)ep->msgsize);
  if (!ep->bytes) {
    TASK_FAIL;
  }

  /* Read message */
  ep->n = 0;
  IFDBG(D_TRANSPORT, FN; STRLIT("reading message"));
  TASK_CALL(read_bytes(rfd, ep->bytes, ep->msgsize, s, &ep->n));

  if (ep->n > 0) {
    /* Deserialize message */
    deserialize_ok = deserialize_msg(p, rfd->x_proto, ep->bytes, ep->msgsize);
    IFDBG(D_NONE, FN; STRLIT(" deserialized message"));
  }
  /* Deallocate buffer */
  X_FREE(ep->bytes);
  if (ep->n <= 0 || !deserialize_ok) {
    IFDBG(D_NONE, FN; NDBG(rfd->fd, d); NDBG64(ep->n); NDBG(deserialize_ok, d));
    TASK_FAIL;
  }
  TASK_RETURN(ep->n);
  FINALLY
  TASK_END;
}

int buffered_read_msg(connection_descriptor *rfd, srv_buf *buf, pax_msg *p,
                      server *s, int64_t *ret) {
  int deserialize_ok = 0;

  DECL_ENV
  int64_t n;
  char *bytes;
  unsigned char header_buf[MSG_HDR_SIZE];
  xcom_proto x_version;
  uint32_t msgsize;
  x_msg_type x_type;
  unsigned int tag;
#ifdef NOTDEF
  unsigned int check;
#endif
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN
  do {
    ep->bytes = nullptr;
    /* Read length field, protocol version, and checksum */
    ep->n = 0;
    IFDBG(D_TRANSPORT, FN; STRLIT("reading header"));
    TASK_CALL(buffered_read_bytes(rfd, buf, (char *)ep->header_buf,
                                  MSG_HDR_SIZE, s, &ep->n));

    if (ep->n != MSG_HDR_SIZE) {
      IFDBG(D_TRANSPORT, FN; NDBG(rfd->fd, d); NDBG64(ep->n));
      TASK_FAIL;
    }

    /* Check the protocol version before doing anything else */
    ep->x_version = read_protoversion(VERS_PTR(ep->header_buf));
    get_header_1_0(ep->header_buf, &ep->msgsize, &ep->x_type, &ep->tag);
    if (ep->x_type == x_version_req) {
      /* Negotiation request. See what we can offer */
      rfd->x_proto = negotiate_protocol(ep->x_version);
      IFDBG(D_TRANSPORT,
            STRLIT("incoming connection will use protcol version ");
            NDBG(rfd->x_proto, u); STRLIT(xcom_proto_to_str(rfd->x_proto)));
      ADD_DBG(
          D_TRANSPORT,
          add_event(EVENT_DUMP_PAD,
                    string_arg("incoming connection will use protcol version"));
          add_event(EVENT_DUMP_PAD,
                    string_arg(xcom_proto_to_str(rfd->x_proto))););
      if (rfd->x_proto > my_xcom_version) TASK_FAIL;
      if (is_new_node_eligible_for_ipv6(ep->x_version, get_site_def())) {
        G_WARNING(
            "Incoming node is not eligible to enter the group due to lack "
            "of IPv6 support. There is at least one group member that is "
            "reachable only via IPv6. Please configure the whole group with "
            "IPv4 addresses and try again");
        TASK_FAIL;
      }

      set_connected(rfd, CON_PROTO);
      TASK_CALL(send_proto(rfd, rfd->x_proto, x_version_reply, ep->tag, ret));
    } else if (ep->x_type == x_version_reply) {
      /* Mark connection with negotiated protocol version */
      if (rfd->snd_tag == ep->tag) {
        rfd->x_proto = ep->x_version;
        IFDBG(D_TRANSPORT, STRLIT("peer connection will use protcol version ");
              NDBG(rfd->x_proto, u); STRLIT(xcom_proto_to_str(rfd->x_proto)));
        ADD_DBG(D_TRANSPORT,
                add_event(
                    0, string_arg("peer connection will use protcol version"));
                add_event(EVENT_DUMP_PAD,
                          string_arg(xcom_proto_to_str(rfd->x_proto))););
        if (rfd->x_proto > my_xcom_version || rfd->x_proto == x_unknown_proto)
          TASK_FAIL;

        set_connected(rfd, CON_PROTO);
      }
    }
  } while (ep->x_type != x_normal);

#ifdef XCOM_PARANOID
  assert(check_protoversion(ep->x_version, rfd->x_proto));
#endif
  if (!check_protoversion(ep->x_version, rfd->x_proto)) {
    TASK_FAIL;
  }

  /* OK, we can grok this version */

  /* Allocate buffer space for message */
  ep->bytes = (char *)xcom_calloc((size_t)1, (size_t)ep->msgsize);
  if (!ep->bytes) {
    TASK_FAIL;
  }
  /* Read message */
  ep->n = 0;
  IFDBG(D_TRANSPORT, FN; STRLIT("reading message"));
  TASK_CALL(buffered_read_bytes(rfd, buf, ep->bytes, ep->msgsize, s, &ep->n));

  if (ep->n > 0) {
    /* Deserialize message */
    deserialize_ok = deserialize_msg(p, rfd->x_proto, ep->bytes, ep->msgsize);
    IFDBG(D_NONE, FN; STRLIT(" deserialized message"));
  }
  /* Deallocate buffer */
  X_FREE(ep->bytes);
  if (ep->n <= 0 || !deserialize_ok) {
    IFDBG(D_NONE, FN; NDBG(rfd->fd, d); NDBG64(ep->n); NDBG(deserialize_ok, d));
    TASK_FAIL;
  }
  TASK_RETURN(ep->n);
  FINALLY
  TASK_END;
}

#if 0
int recv_proto(connection_descriptor const *rfd, xcom_proto *x_proto,
               x_msg_type *x_type, unsigned int *tag, int64_t *ret) {
  DECL_ENV
  int64_t n;
  unsigned char header_buf[MSG_HDR_SIZE];
  uint32_t msgsize;
  END_ENV;

  TASK_BEGIN

  /* Read length field, protocol version, and checksum */
  ep->n = 0;
  IFDBG(D_TRANSPORT, FN; STRLIT("reading header"));
  TASK_CALL(read_bytes(rfd, (char *)ep->header_buf, MSG_HDR_SIZE, 0, &ep->n));

  if (ep->n != MSG_HDR_SIZE) {
    IFDBG(D_NONE, FN; NDBG64(ep->n));
    TASK_FAIL;
  }

  *x_proto = read_protoversion(VERS_PTR(ep->header_buf));
  get_header_1_0(ep->header_buf, &ep->msgsize, x_type, tag);
  TASK_RETURN(ep->n);
  FINALLY
  TASK_END;
}
#endif

/* Sender task */

static inline unsigned int incr_tag(unsigned int tag) {
  ++tag;
  return tag & 0xffff;
}

static void start_protocol_negotiation(channel *outgoing) {
  msg_link *link = msg_link_new(nullptr, VOID_NODE_NO);
  IFDBG(D_NONE, FN; PTREXP(outgoing); COPY_AND_FREE_GOUT(dbg_msg_link(link)););
  channel_put_front(outgoing, &link->l);
}

linkage connect_wait = {
    0, &connect_wait,
    &connect_wait}; /* sender_task sleeps here while waiting to connect */

void wakeup_sender() { task_wakeup(&connect_wait); }

#define TAG_START 313

/* Fetch messages from queue and send to other server.  Having a
   separate queue and task for doing this simplifies the logic since we
   never need to wait to send. */
int sender_task(task_arg arg) {
  DECL_ENV
  server *s;
  msg_link *link;
  unsigned int tag;
  double dtime;
  double channel_empty_time;
#if defined(_WIN32)
  bool was_connected;
#endif
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t ret_code{0};
  TASK_BEGIN

  ep->channel_empty_time = task_now();
  ep->dtime = INITIAL_CONNECT_WAIT; /* Initial wait is short, to avoid
                                       unnecessary waiting */
#if defined(_WIN32)
  ep->was_connected = false;
#endif
  ep->s = (server *)get_void_arg(arg);
  ep->link = nullptr;
  ep->tag = TAG_START;
  srv_ref(ep->s);

  while (!xcom_shutdown) {
    /* Loop until connected */
    G_MESSAGE("Connecting to %s:%d", ep->s->srv, ep->s->port);
    for (;;) {
#if defined(_WIN32)
      if (!ep->was_connected) {
#endif
        TASK_CALL(dial(ep->s));
#if defined(_WIN32)
      } else {
        ep->s->reconnect = true;
      }
#endif
      if (is_connected(ep->s->con)) break;

      if (ep->dtime < MAX_CONNECT_WAIT) {
        G_MESSAGE("Connection to %s:%d failed", ep->s->srv, ep->s->port);
      }

      TIMED_TASK_WAIT(&connect_wait, ep->dtime);
      if (xcom_shutdown) TERMINATE;
      /* Delay cleanup of messages to avoid unnecessary loss when connecting */
      if (task_now() > ep->channel_empty_time + 2.0) {
        empty_msg_channel(&ep->s->outgoing);
        ep->channel_empty_time = task_now();
      }

      ep->dtime += CONNECT_WAIT_INCREASE; /* Increase wait time for next try */
      if (ep->dtime > MAX_CONNECT_WAIT) {
        ep->dtime = MAX_CONNECT_WAIT;
      }
    }

    G_MESSAGE("Connected to %s:%d", ep->s->srv, ep->s->port);
    ep->dtime = INITIAL_CONNECT_WAIT;
#if defined(_WIN32)
    ep->was_connected = true;
    ep->s->reconnect = false;
#endif
    reset_srv_buf(&ep->s->out_buf);

    /* We are ready to start sending messages.
       Insert a message in the input queue to negotiate the protocol.
    */
    start_protocol_negotiation(&ep->s->outgoing);
    while (is_connected(ep->s->con)) {
      int64_t ret;
      assert(!ep->link);
      if (false && link_empty(&ep->s->outgoing.data)) {
        TASK_DELAY(0.1 * xcom_drand48());
      }
      /* FWD_ITER(&ep->s->outgoing.data, msg_link, IFDBG(D_NONE, FN;
       * PTREXP(link_iter));); */
      if (link_empty(&ep->s->outgoing.data)) {
        TASK_CALL(flush_srv_buf(ep->s, &ret));
      }
      CHANNEL_GET(&ep->s->outgoing, &ep->link, msg_link);
      {
        /* IFDBG(D_NONE, FN; PTREXP(stack); PTREXP(ep->link));
        IFDBG(D_NONE, FN; PTREXP(&ep->s->outgoing);
               COPY_AND_FREE_GOUT(dbg_msg_link(ep->link)););
        IFDBG(D_NONE, FN; STRLIT(" extracted ");
                        COPY_AND_FREE_GOUT(dbg_linkage(&ep->link->l));
            ); */

        /* If ep->link->p is 0, it is a protocol (re)negotiation request */
        /* IFDBG(D_NONE, FN;
                NDBG(ep->s->con.x_proto,u);
           STRLIT(xcom_proto_to_str(ep->s->con.x_proto));
                NDBG(get_latest_common_proto(),u);
           STRLIT(xcom_proto_to_str(get_latest_common_proto()));
                NDBG(ep->s->con.fd,d)); */
        if (ep->link->p) {
          ADD_DBG(
              D_TRANSPORT, add_event(EVENT_DUMP_PAD,
                                     string_arg("sending ep->link->p->synode"));
              add_synode_event(ep->link->p->synode);
              add_event(EVENT_DUMP_PAD, string_arg("to"));
              add_event(EVENT_DUMP_PAD, uint_arg(ep->link->to)); add_event(
                  EVENT_DUMP_PAD, string_arg(pax_op_to_str(ep->link->p->op))););
          TASK_CALL(_send_msg(ep->s, ep->link->p, ep->link->to, &ret_code));
          if (ret_code < 0) {
            goto next;
          }
          ADD_DBG(
              D_TRANSPORT,
              add_event(EVENT_DUMP_PAD, string_arg("sent ep->link->p->synode"));
              add_synode_event(ep->link->p->synode);
              add_event(EVENT_DUMP_PAD, string_arg("to"));
              add_event(EVENT_DUMP_PAD, uint_arg(ep->link->p->to)); add_event(
                  EVENT_DUMP_PAD, string_arg(pax_op_to_str(ep->link->p->op))););
        } else {
          set_connected(ep->s->con, CON_FD);
          /* Send protocol negotiation request */
          do {
            TASK_CALL(send_proto(ep->s->con, my_xcom_version, x_version_req,
                                 ep->tag, &ret_code));
            if (!is_connected(ep->s->con)) {
              goto next;
            }
            ep->tag = incr_tag(ep->tag);
          } while (ret_code < 0);
          G_DEBUG("sent negotiation request for protocol %d fd %d",
                  my_xcom_version, ep->s->con->fd);
          ADD_DBG(
              D_TRANSPORT,
              add_event(EVENT_DUMP_PAD,
                        string_arg("sent negotiation request for protocol"));
              add_event(EVENT_DUMP_PAD,
                        string_arg(xcom_proto_to_str(my_xcom_version))););

          /* Wait until negotiation done.
             reply_handler_task will catch reply and change state */
          while (!proto_done(ep->s->con)) {
            TASK_DELAY(0.1);
            if (!is_connected(ep->s->con)) {
              goto next;
            }
          }
          ADD_DBG(
              D_TRANSPORT,
              add_event(EVENT_DUMP_PAD, string_arg("will use protocol"));
              add_event(EVENT_DUMP_PAD,
                        string_arg(xcom_proto_to_str(ep->s->con.x_proto))););
        }
      }
    next:
      msg_link_delete(&ep->link);
      /* TASK_YIELD; */
    }
    G_MESSAGE("Sender task disconnected from %s:%d", ep->s->srv, ep->s->port);
  }
  FINALLY
  empty_msg_channel(&ep->s->outgoing);
  ep->s->sender = nullptr;
  srv_unref(ep->s);
  if (ep->link) msg_link_delete(&ep->link);
  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}

#if defined(_WIN32)
/* Reconnect tcp connections on windows to avoid task starvation */
int tcp_reconnection_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  int dummy;
  server *s;
  int i;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  TASK_BEGIN

  ep->s = nullptr;
  ep->i = 0;

  while (!xcom_shutdown) {
    {
      ep->s = nullptr;
      for (ep->i = 0; ep->i < maxservers; ep->i++) {
        ep->s = all_servers[ep->i];
        if (ep->s && ep->s->reconnect && !is_connected(ep->s->con)) {
          TASK_CALL(dial(ep->s));
          if (is_connected(ep->s->con)) {
            ep->s->reconnect = false;
          }
          TASK_DELAY(2.0)
        }
      }
    }
    TASK_DELAY(2.0);
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}
#endif

/* Fetch messages from queue and send to self.
   Having a separate mechanism for internal communication
   avoids SSL blocking when trying to connect to same thread. */
int local_sender_task(task_arg arg) {
  DECL_ENV
  server *s;
  msg_link *link;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

  ep->s = (server *)get_void_arg(arg);
  ep->link = nullptr;
  srv_ref(ep->s);

  reset_srv_buf(&ep->s->out_buf);

  while (!xcom_shutdown) {
    assert(!ep->link);
    CHANNEL_GET(&ep->s->outgoing, &ep->link, msg_link);
    {
      /* IFDBG(D_NONE, FN; PTREXP(stack); PTREXP(ep->link)); */
      IFDBG(D_NONE, FN; PTREXP(&ep->s->outgoing);
            COPY_AND_FREE_GOUT(dbg_msg_link(ep->link)););
      IFDBG(D_NONE, FN; STRLIT(" extracted ");
            COPY_AND_FREE_GOUT(dbg_linkage(&ep->link->l)););
      assert(ep->link->p);
      ep->link->p->to = ep->link->p->from;
      dispatch_op(find_site_def(ep->link->p->synode), ep->link->p, nullptr);
    }
    msg_link_delete(&ep->link);
  }
  FINALLY
  empty_msg_channel(&ep->s->outgoing);
  ep->s->sender = nullptr;
  srv_unref(ep->s);
  if (ep->link) msg_link_delete(&ep->link);
  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}

static server *find_server(server *table[], int n, char *name, xcom_port port) {
  int i;
  for (i = 0; i < n; i++) {
    server *s = table[i];
    if (s && strcmp(s->srv, name) == 0 &&
        s->port == port) /* FIXME should use IP address */
      return s;
  }
  return nullptr;
}

void update_servers(site_def *s, cargo_type operation) {
  u_int n;

  if (s) {
    u_int i = 0;
    n = s->nodes.node_list_len;

    IFDBG(D_NONE, FN; NDBG(get_maxnodes(s), u); NDBG(n, d); PTREXP(s));

    G_INFO("Updating physical connections to other servers");

    for (i = 0; i < n; i++) {
      char *addr = s->nodes.node_list_val[i].address;
      char *name = nullptr;
      xcom_port port = 0;

      name = (char *)xcom_malloc(IP_MAX_SIZE);

      /* In this specific place, addr must have been validated elsewhere,
         specifically when the node is added. */
      if (get_ip_and_port(addr, name, &port)) {
        G_INFO("Error parsing ip:port for new server. Incorrect value is %s",
               addr ? addr : "unknown");
        free(name);
        continue;
      }

      {
        server *sp = find_server(all_servers, maxservers, name, port);

        if (sp) {
          G_INFO("Using existing server node %d host %s:%d", i, name, port);
          s->servers[i] = sp;

          /*
          Reset ping counters to make sure that we clear this state between
          different configurations. It is an assumption that at least in this
          point, every node is working fine.
          */
          s->servers[i]->last_ping_received = 0.0;
          s->servers[i]->number_of_pings_received = 0;

          free(name);
          if (sp->invalid) sp->invalid = 0;
        } else { /* No server? Create one */
          G_INFO("Creating new server node %d host %s:%d", i, name, port);
          if (port > 0) {
            s->servers[i] = addsrv(name, port);
          } else {
            /* purecov: begin deadcode */
            s->servers[i] = addsrv(name, xcom_listen_port);
            /* purecov: end */
          }
        }
        IFDBG(D_BUG, FN; PTREXP(s->servers[i]));
      }
    }
    /* Zero the rest */
    for (i = n; i < NSERVERS; i++) {
      s->servers[i] = nullptr;
    }

    /*
     If we have a force config, mark the servers that do not belong to this
     configuration as invalid
     */
    if (operation == force_config_type) {
      const site_def *old_site_def = get_prev_site_def();
      invalidate_servers(old_site_def, s);
    }
  }
}

/*
  Make a diff between 2 site_defs and mark as invalid servers
  that do not belong to the new site_def.

  This is only to be used if we are forcing a configuration.
 */
void invalidate_servers(const site_def *old_site_def,
                        const site_def *new_site_def) {
  u_int node = 0;
  for (; node < get_maxnodes(old_site_def); node++) {
    node_address *node_addr_from_old_site_def =
        &old_site_def->nodes.node_list_val[node];

    if (!node_exists(node_addr_from_old_site_def, &new_site_def->nodes)) {
      char *addr = node_addr_from_old_site_def->address;
      char name[IP_MAX_SIZE];
      xcom_port port = 0;

      /* Not processing any error here since it belongs to an already validated
         configuration. */
      get_ip_and_port(addr, name, &port);

      {
        server *sp = find_server(all_servers, maxservers, name, port);
        if (sp) {
          sp->invalid = 1;
        }
      }
    }
  }
}

/* Remove tcp connections which seem to be idle */
int tcp_reaper_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  int dummy;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  TASK_BEGIN
  while (!xcom_shutdown) {
    {
      int i;
      double now = task_now();
      for (i = 0; i < maxservers; i++) {
        server *s = all_servers[i];
        if (s && s->con->fd != -1 && (s->active + 10.0) < now) {
          shutdown_connection(s->con);
        }
      }
    }
    TASK_DELAY(1.0);
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}

#define TERMINATE_CLIENT(ep)            \
  {                                     \
    if (ep->s->crash_on_error) abort(); \
    TERMINATE;                          \
  }

#ifndef XCOM_WITHOUT_OPENSSL
void ssl_free_con(connection_descriptor *con) {
  SSL_free(con->ssl_fd);
  con->ssl_fd = nullptr;
}

#endif

void close_connection(connection_descriptor *con) {
  close_open_connection(con);
  set_connected(con, CON_NULL);
}

void shutdown_connection(connection_descriptor *con) {
  /* printstack(1); */
  ADD_DBG(D_TRANSPORT, add_event(EVENT_DUMP_PAD, string_arg("con->fd"));
          add_event(EVENT_DUMP_PAD, int_arg(con->fd)););
  close_connection(con);

  remove_and_wakeup(con->fd);
  con->fd = -1;
}

void reset_connection(connection_descriptor *con) {
  if (con) {
    con->fd = -1;
#ifndef XCOM_WITHOUT_OPENSSL
    con->ssl_fd = nullptr;
#endif
    set_connected(con, CON_NULL);
  }
}

/* The protocol version used by the group as a whole is the minimum of the
 maximum protocol versions in the config. */
xcom_proto common_xcom_version(site_def const *site) {
  u_int i;
  xcom_proto min_proto = my_xcom_version;
  for (i = 0; i < site->nodes.node_list_len; i++) {
    min_proto = MIN(min_proto, site->nodes.node_list_val[i].proto.max_proto);
  }
  return min_proto;
}

static xcom_proto latest_common_proto = MY_XCOM_PROTO;

xcom_proto set_latest_common_proto(xcom_proto x_proto) {
  return latest_common_proto = x_proto;
}

xcom_proto get_latest_common_proto() { return latest_common_proto; }

/* See which protocol we can use.
   Needs to be redefined as the protocol changes */
xcom_proto negotiate_protocol(xcom_proto proto_vers) {
  /* Ensure that protocol will not be greater than
  my_xcom_version */
  if (proto_vers < my_min_xcom_version) {
    return x_unknown_proto;
  } else if (proto_vers > my_xcom_version) {
    return my_xcom_version;
  } else {
    return proto_vers;
  }
}

xcom_proto minimum_ipv6_version() { return x_1_5; }

/*
  Parse name, IPv4, or IPv6 address with optional :port.
  If no port, *port is set to 0. Skips leading whitespace.
*/

struct parse_buf {
  char const *address;
  char const *in;
  char *out;
  char *end;
};

typedef struct parse_buf parse_buf;

static int emit(parse_buf *p) {
  if (p->out < p->end) {
    if (!isspace(*(p->in))) *(p->out)++ = *(p->in);
    return 1;
  } else {
    G_DEBUG(
        "Address including terminating null char is "
        "bigger than IP_MAX_SIZE which is "
        "%d",
        IP_MAX_SIZE);
    return 0;
  }
}

#define EMIT \
  if (!emit(p)) return 0

static int match_port(parse_buf *p, xcom_port *port) {
  if (*(p->in) == 0) goto err;
  {
    char *end_ptr = nullptr;
    long int port_to_int = strtol(p->in, &end_ptr, 10);
    if (end_ptr == nullptr || strlen(end_ptr) != 0) {
      goto err;
    }
    *port = (xcom_port)port_to_int;
    return 1;
  }
err:
  G_DEBUG("Malformed port number '%s'", p->in);
  return 0;
}

static int match_ipv6(parse_buf *p) {
  int has_colon = 0;
  p->in++;
  while (*(p->in)) {
    if (isspace(*(p->in))) {
      G_DEBUG("Malformed IPv6 address '%s'", p->address);
      return 0;
    }
    if (*(p->in) == ']') { /* End of IPv6 address */
      return has_colon > 0;
    }
    EMIT;
    if (*(p->in) == ':') {
      has_colon++;
      if (has_colon > 7) {
        G_DEBUG("Malformed IPv6 address '%s'", p->address);
        return 0;
      }
    } else if (!isxdigit(*(p->in))) {
      G_DEBUG("Malformed IPv6 address '%s'", p->address);
      return 0;
    }
    p->in++;
  }
  /* If we get here, there is no ] */
  p->in--;
  return 0;
}

static int match_ipv4_or_name(parse_buf *p) {
  while (*(p->in) && *(p->in) != ':') {
    if (isspace(*(p->in))) {
      G_DEBUG("Malformed IPv4 address or hostname '%s'", p->address);
      return 0;
    }
    EMIT;
    p->in++;
  }
  p->in--;
  return 1;
}

static int match_address(parse_buf *p) {
  if (*(p->in) == '[') /* Start of IPv6 address */
    return match_ipv6(p);
  else /* IPv4 address or name */
    return match_ipv4_or_name(p);
}

/* Return 1 if address is well-formed, 0 if not */
static int match_ip_and_port(char const *address, char ip[IP_MAX_SIZE],
                             xcom_port *port) {
  parse_buf p;
  // Sanity check before return
  auto ok_ip = [&ip]() { return ip[0] != 0; };

  /* Sanity checks */
  if (address == nullptr || (strlen(address) == 0)) {
    return 0;
  }

  /* Zero the output buffer and port */
  if (ip)
    memset(ip, 0, IP_MAX_SIZE);
  else
    return 0;

  if (port)
    *port = 0;
  else
    return 0;

  p.in = p.address = address;
  p.out = ip;
  p.end = ip + IP_MAX_SIZE - 1;

  /* Skip leading whitespace */
  while (*(p.in) && isspace(*(p.in))) p.in++;

  if (*(p.in) == 0) return 0; /* Nothing here */

  if (!match_address(&p)) return 0;
  p.in++;
  if (*(p.in) == ':') { /* We have a port */
    p.in++;
    return ok_ip() && match_port(&p, port);
  }
  return ok_ip() && NO_PORT_IN_ADDRESS; /* No :port, but that may be OK */
}

int get_ip_and_port(char const *address, char ip[IP_MAX_SIZE],
                    xcom_port *port) {
  return !match_ip_and_port(address, ip, port);
}
