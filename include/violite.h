/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
 * Vio Lite.
 * Purpose: include file for Vio that will work with C and C++
 */

#ifndef vio_violite_h_
#define	vio_violite_h_

#include <my_thread.h> /* my_thread_handle */
#include <mysql/psi/psi.h>
#include <pfs_socket_provider.h>
#include <mysql/psi/mysql_socket.h>

/* Simple vio interface in C;  The functions are implemented in violite.c */

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef HAVE_PSI_INTERFACE
void init_vio_psi_keys();
#endif

#ifdef __cplusplus
typedef struct st_vio Vio;
#endif /* __cplusplus */

enum enum_vio_type
{
  /**
    Type of the connection is unknown.
  */
  NO_VIO_TYPE= 0,
  /**
    Used in case of TCP/IP connections.
  */
  VIO_TYPE_TCPIP= 1,
  /**
    Used for Unix Domain socket connections. Unix only.
  */
  VIO_TYPE_SOCKET= 2,
  /**
    Used for named pipe connections. Windows only.
  */
  VIO_TYPE_NAMEDPIPE= 3,
  /**
    Used in case of SSL connections.
  */
  VIO_TYPE_SSL= 4,
  /**
    Used for shared memory connections. Windows only.
  */
  VIO_TYPE_SHARED_MEMORY= 5,
  /**
    Used internally by the prepared statements
  */
  VIO_TYPE_LOCAL= 6,
  /**
    Implicitly used by plugins that doesn't support any other VIO_TYPE.
  */
  VIO_TYPE_PLUGIN= 7,

  FIRST_VIO_TYPE= VIO_TYPE_TCPIP,
  /*
    If a new type is added, please update LAST_VIO_TYPE. In addition, please
    change get_vio_type_name() in vio/vio.c to return correct name for it.
  */
  LAST_VIO_TYPE= VIO_TYPE_PLUGIN
};

/**
  Convert a vio type to a printable string.
  @param vio_type the type
  @param[out] str the string
  @param[out] len the string length
*/
void get_vio_type_name(enum enum_vio_type vio_type, const char ** str, int * len);

/**
  VIO I/O events.
*/
enum enum_vio_io_event
{
  VIO_IO_EVENT_READ,
  VIO_IO_EVENT_WRITE,
  VIO_IO_EVENT_CONNECT
};

#define VIO_LOCALHOST 1                         /* a localhost connection */
#define VIO_BUFFERED_READ 2                     /* use buffered read */
#define VIO_READ_BUFFER_SIZE 16384              /* size of read buffer */
#define VIO_DESCRIPTION_SIZE 30                 /* size of description */

Vio* vio_new(my_socket sd, enum enum_vio_type type, uint flags);
Vio*  mysql_socket_vio_new(MYSQL_SOCKET mysql_socket, enum enum_vio_type type, uint flags);
#ifdef _WIN32
Vio* vio_new_win32pipe(HANDLE hPipe);
Vio* vio_new_win32shared_memory(HANDLE handle_file_map,
                                HANDLE handle_map,
                                HANDLE event_server_wrote,
                                HANDLE event_server_read,
                                HANDLE event_client_wrote,
                                HANDLE event_client_read,
                                HANDLE event_conn_closed);
#else
#define HANDLE void *
#endif /* _WIN32 */

void    vio_delete(Vio* vio);
int vio_shutdown(Vio* vio);
my_bool vio_reset(Vio* vio, enum enum_vio_type type,
                  my_socket sd, void *ssl, uint flags);
size_t  vio_read(Vio *vio, uchar *	buf, size_t size);
size_t  vio_read_buff(Vio *vio, uchar * buf, size_t size);
size_t  vio_write(Vio *vio, const uchar * buf, size_t size);
/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible */
int vio_fastsend(Vio *vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible */
int vio_keepalive(Vio *vio, my_bool	onoff);
/* Whenever we should retry the last read/write operation. */
my_bool vio_should_retry(Vio *vio);
/* Check that operation was timed out */
my_bool vio_was_timeout(Vio *vio);
/* Short text description of the socket for those, who are curious.. */
const char* vio_description(Vio *vio);
/* Return the type of the connection */
enum enum_vio_type vio_type(Vio* vio);
/* Return last error number */
int	vio_errno(Vio*vio);
/* Get socket number */
my_socket vio_fd(Vio*vio);
/* Remote peer's address and name in text form */
my_bool vio_peer_addr(Vio *vio, char *buf, uint16 *port, size_t buflen);
/* Wait for an I/O event notification. */
int vio_io_wait(Vio *vio, enum enum_vio_io_event event, int timeout);
my_bool vio_is_connected(Vio *vio);
#ifndef DBUG_OFF
ssize_t vio_pending(Vio *vio);
#endif
/* Set timeout for a network operation. */
int vio_timeout(Vio *vio, uint which, int timeout_sec);
/* Connect to a peer. */
my_bool vio_socket_connect(Vio *vio, struct sockaddr *addr, socklen_t len,
                           int timeout);

my_bool vio_get_normalized_ip_string(const struct sockaddr *addr, size_t addr_length,
                                     char *ip_string, size_t ip_string_size);

my_bool vio_is_no_name_error(int err_code);

int vio_getnameinfo(const struct sockaddr *sa,
                    char *hostname, size_t hostname_size,
                    char *port, size_t port_size,
                    int flags);

#ifdef HAVE_OPENSSL
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x0090700f
#define DES_cblock des_cblock
#define DES_key_schedule des_key_schedule
#define DES_set_key_unchecked(k,ks) des_set_key_unchecked((k),*(ks))
#define DES_ede3_cbc_encrypt(i,o,l,k1,k2,k3,iv,e) des_ede3_cbc_encrypt((i),(o),(l),*(k1),*(k2),*(k3),(iv),(e))
#endif
/* apple deprecated openssl in MacOSX Lion */
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define HEADER_DES_LOCL_H dummy_something
#define YASSL_MYSQL_COMPATIBLE
#ifndef YASSL_PREFIX
#define YASSL_PREFIX
#endif
/* Set yaSSL to use same type as MySQL do for socket handles */
typedef my_socket YASSL_SOCKET_T;
#define YASSL_SOCKET_T_DEFINED
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifndef EMBEDDED_LIBRARY
enum enum_ssl_init_error
{
  SSL_INITERR_NOERROR= 0, SSL_INITERR_CERT, SSL_INITERR_KEY, 
  SSL_INITERR_NOMATCH, SSL_INITERR_BAD_PATHS, SSL_INITERR_CIPHERS, 
  SSL_INITERR_MEMFAIL, SSL_INITERR_NO_USABLE_CTX, SSL_INITERR_DHFAIL,
  SSL_TLS_VERSION_INVALID, SSL_INITERR_LASTERR
};
const char* sslGetErrString(enum enum_ssl_init_error err);

struct st_VioSSLFd
{
  SSL_CTX *ssl_context;
};

int sslaccept(struct st_VioSSLFd*, Vio *, long timeout, unsigned long *errptr);
int sslconnect(struct st_VioSSLFd*, Vio *, long timeout, unsigned long *errptr);

struct st_VioSSLFd
*new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
                       const char *ca_file,  const char *ca_path,
                       const char *cipher, enum enum_ssl_init_error *error,
                       const char *crl_file, const char *crl_path, const long ssl_ctx_flags);

long process_tls_version(const char *tls_version);

struct st_VioSSLFd
*new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
                      const char *ca_file,const char *ca_path,
                      const char *cipher, enum enum_ssl_init_error *error,
                      const char *crl_file, const char *crl_path, const long ssl_ctx_flags);
void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd);

void vio_ssl_end();

#endif /* ! EMBEDDED_LIBRARY */
#endif /* HAVE_OPENSSL */

void ssl_start(void);
void vio_end(void);

#ifdef  __cplusplus
}
#endif

#if !defined(DONT_MAP_VIO)
#define vio_delete(vio)                         (vio)->viodelete(vio)
#define vio_errno(vio)                          (vio)->vioerrno(vio)
#define vio_read(vio, buf, size)                ((vio)->read)(vio,buf,size)
#define vio_write(vio, buf, size)               ((vio)->write)(vio, buf, size)
#define vio_fastsend(vio)                       (vio)->fastsend(vio)
#define vio_keepalive(vio, set_keep_alive)  (vio)->viokeepalive(vio, set_keep_alive)
#define vio_should_retry(vio)                   (vio)->should_retry(vio)
#define vio_was_timeout(vio)                    (vio)->was_timeout(vio)
#define vio_shutdown(vio)                       ((vio)->vioshutdown)(vio)
#define vio_peer_addr(vio, buf, prt, buflen)    (vio)->peer_addr(vio, buf, prt, buflen)
#define vio_io_wait(vio, event, timeout)        (vio)->io_wait(vio, event, timeout)
#define vio_is_connected(vio)                   (vio)->is_connected(vio)
#endif /* !defined(DONT_MAP_VIO) */

/* This enumerator is used in parser - should be always visible */
enum SSL_type
{
  SSL_TYPE_NOT_SPECIFIED= -1,
  SSL_TYPE_NONE,
  SSL_TYPE_ANY,
  SSL_TYPE_X509,
  SSL_TYPE_SPECIFIED
};


/* HFTODO - hide this if we don't want client in embedded server */
/* This structure is for every connection on both sides */
struct st_vio
{
  MYSQL_SOCKET  mysql_socket;           /* Instrumented socket */
  my_bool       localhost;              /* Are we from localhost? */
  struct sockaddr_storage   local;      /* Local internet address */
  struct sockaddr_storage   remote;     /* Remote internet address */
  size_t addrLen;                       /* Length of remote address */
  enum enum_vio_type    type;           /* Type of connection */
  my_bool               inactive; /* Connection inactive (has been shutdown) */
  char                  desc[VIO_DESCRIPTION_SIZE]; /* Description string. This
                                                      member MUST NOT be
                                                      used directly, but only
                                                      via function
                                                      "vio_description" */
  char                  *read_buffer;   /* buffer for vio_read_buff */
  char                  *read_pos;      /* start of unfetched data in the
                                           read buffer */
  char                  *read_end;      /* end of unfetched data */
  int                   read_timeout;   /* Timeout value (ms) for read ops. */
  int                   write_timeout;  /* Timeout value (ms) for write ops. */
  
  /* 
     VIO vtable interface to be implemented by VIO's like SSL, Socket,
     Named Pipe, etc.
  */
  
  /* 
     viodelete is responsible for cleaning up the VIO object by freeing 
     internal buffers, closing descriptors, handles. 
  */
  void    (*viodelete)(Vio*);
  int     (*vioerrno)(Vio*);
  size_t  (*read)(Vio*, uchar *, size_t);
  size_t  (*write)(Vio*, const uchar *, size_t);
  int     (*timeout)(Vio*, uint, my_bool);
  int     (*viokeepalive)(Vio*, my_bool);
  int     (*fastsend)(Vio*);
  my_bool (*peer_addr)(Vio*, char *, uint16*, size_t);
  void    (*in_addr)(Vio*, struct sockaddr_storage*);
  my_bool (*should_retry)(Vio*);
  my_bool (*was_timeout)(Vio*);
  /* 
     vioshutdown is resposnible to shutdown/close the channel, so that no 
     further communications can take place, however any related buffers,
     descriptors, handles can remain valid after a shutdown.
  */
  int     (*vioshutdown)(Vio*);
  my_bool (*is_connected)(Vio*);
  my_bool (*has_data) (Vio*);
  int (*io_wait)(Vio*, enum enum_vio_io_event, int);
  my_bool (*connect)(Vio*, struct sockaddr *, socklen_t, int);
#ifdef _WIN32
  OVERLAPPED overlapped;
  HANDLE hPipe;
#endif
#ifdef HAVE_OPENSSL
  void    *ssl_arg;
#endif
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  HANDLE  handle_file_map;
  char    *handle_map;
  HANDLE  event_server_wrote;
  HANDLE  event_server_read;
  HANDLE  event_client_wrote;
  HANDLE  event_client_read;
  HANDLE  event_conn_closed;
  size_t  shared_memory_remain;
  char    *shared_memory_pos;
#endif /* _WIN32 && !EMBEDDED_LIBRARY */
};

#ifdef HAVE_OPENSSL
#define SSL_handle SSL*
#else
#define SSL_handle void*
#endif

#endif /* vio_violite_h_ */
