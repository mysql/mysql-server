/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* We implement virtual IO by trapping needed vio_* calls and mapping
 * them to different function pointers by type
 */




/*
 * Purpose: include file for st_vio that will work with C and C++
 */

#ifndef vio_violite_h_
#define	vio_violite_h_

#include "my_net.h"			/* needed because of struct in_addr */


/* Simple vio interface in C;  The functions are implemented in violite.c */

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

enum enum_vio_type { VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET,
	                     VIO_TYPE_NAMEDPIPE, VIO_TYPE_SSL};
#ifndef st_vio_defined
#define st_vio_defined
struct st_vio;					/* Only C */
typedef struct st_vio st_vio;
#endif

st_vio*		vio_new(my_socket	sd,
			enum enum_vio_type type,
			my_bool		localhost);
#ifdef __WIN__
st_vio*		vio_new_win32pipe(HANDLE hPipe);
#endif
void		vio_delete(st_vio* vio);

#ifdef EMBEDDED_LIBRARY
void vio_reset(st_vio *vio);
#endif

/*
 * vio_read and vio_write should have the same semantics
 * as read(2) and write(2).
 */
int		vio_read(		st_vio*		vio,
					gptr		buf,	int	size);
int		vio_write(		st_vio*		vio,
					const gptr	buf,
					int		size);
/*
 * Whenever the socket is set to blocking mode or not.
 */
int		vio_blocking(		st_vio*		vio,
					my_bool    	onoff);
my_bool		vio_is_blocking(	st_vio*		vio);
/*
 * setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible.
 */
  int		vio_fastsend(		st_vio*		vio);
/*
 * setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible.
 */
int		vio_keepalive(		st_vio*		vio,
					my_bool		onoff);
/*
 * Whenever we should retry the last read/write operation.
 */
my_bool		vio_should_retry(	st_vio*		vio);
/*
 * When the workday is over...
 */
int		vio_close(st_vio* vio);
/*
 * Short text description of the socket for those, who are curious..
 */
const char*	vio_description(	st_vio*		vio);

/* Return the type of the connection */
 enum enum_vio_type vio_type(st_vio* vio);

/* Return last error number */
int vio_errno(st_vio *vio);

/* Get socket number */
my_socket vio_fd(st_vio *vio);

/*
 * Remote peer's address and name in text form.
 */
my_bool vio_peer_addr(st_vio * vio, char *buf);

/* Remotes in_addr */

void vio_in_addr(st_vio *vio, struct in_addr *in);

  /* Return 1 if there is data to be read */
my_bool vio_poll_read(st_vio *vio,uint timeout);

#ifdef	__cplusplus
}
#endif
#endif /* vio_violite_h_ */
#ifdef HAVE_VIO
#ifndef DONT_MAP_VIO
#define vio_delete(vio) 			(vio)->viodelete(vio)
#define vio_errno(vio)	 			(vio)->vioerrno(vio)
#define vio_read(vio, buf, size) 		(vio)->read(vio,buf,size)
#define vio_write(vio, buf, size) 		(vio)->write(vio, buf, size)
#define vio_blocking(vio, set_blocking_mode) 	(vio)->vioblocking(vio, set_blocking_mode)
#define vio_is_blocking(vio) 			(vio)->is_blocking(vio)
#define vio_fastsend(vio)			(vio)->fastsend(vio)
#define vio_keepalive(vio, set_keep_alive)	(vio)->viokeepalive(vio, set_keep_alive)
#define vio_should_retry(vio) 			(vio)->should_retry(vio)
#define vio_close(vio)				((vio)->vioclose)(vio)
#define vio_peer_addr(vio, buf)			(vio)->peer_addr(vio, buf)
#define vio_in_addr(vio, in)			(vio)->in_addr(vio, in)
#define vio_poll_read(vio,timeout)		(vio)->poll_read(vio,timeout)
#endif /* !DONT_MAP_VIO */
#endif /* HAVE_VIO */


#ifdef HAVE_OPENSSL
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/asn1.h>
#include "my_net.h"			/* needed because of struct in_addr */


#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef st_vio_defined
#define st_vio_defined
struct st_vio;					/* Only C */
typedef struct st_vio st_vio;
#endif

void vio_ssl_delete(st_vio* vio);

#ifdef EMBEDDED_LIBRARY
void vio_reset(st_vio *vio);
#endif

int		vio_ssl_read(st_vio* vio,gptr buf,	int size);
int		vio_ssl_write(st_vio* vio,const gptr buf,int size);
int		vio_ssl_blocking(st_vio* vio,my_bool onoff);
my_bool		vio_ssl_is_blocking(st_vio* vio);

/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible. */
  int vio_ssl_fastsend(st_vio* vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible. */
int vio_ssl_keepalive(st_vio* vio, my_bool onoff);
/* Whenever we should retry the last read/write operation. */
my_bool vio_ssl_should_retry(st_vio* vio);
/* When the workday is over... */
int vio_ssl_close(st_vio* vio);
/* Return last error number */
int vio_ssl_errno(st_vio *vio);
my_bool vio_ssl_peer_addr(st_vio * vio, char *buf);
void vio_ssl_in_addr(st_vio *vio, struct in_addr *in);

/* Return 1 if there is data to be read */
my_bool vio_ssl_poll_read(st_vio *vio,uint timeout);

#ifdef HAVE_OPENSSL

/* Single copy for server */
struct st_VioSSLAcceptorFd 
{
  SSL_CTX* ssl_context_;
  SSL_METHOD* ssl_method_;
  struct st_VioSSLAcceptorFd* session_id_context_;
  enum {
    state_connect       = 1,
    state_accept        = 2
  };
  BIO* bio_;
  char *ssl_cip_;
  char desc_[100];
  st_vio* sd_;

  /* function pointers which are only once for SSL server */ 
//  struct st_vio *(*sslaccept)(struct st_VioSSLAcceptorFd*,st_vio*);
};

/* One copy for client */
struct st_VioSSLConnectorFd
{
  BIO* bio_;
  gptr ssl_;
  SSL_CTX* ssl_context_;
  SSL_METHOD* ssl_method_;
  /* function pointers which are only once for SSL client */ 
};
struct st_vio *sslaccept(struct st_VioSSLAcceptorFd*, struct st_vio*);
struct st_vio *sslconnect(struct st_VioSSLConnectorFd*, struct st_vio*);

#else /* HAVE_OPENSSL */
/* This dummy is required to maintain proper size of st_mysql in mysql.h */
struct st_VioSSLConnectorFd {};
#endif /* HAVE_OPENSSL */
struct st_VioSSLConnectorFd *new_VioSSLConnectorFd(
		const char* key_file,const char* cert_file,const char* ca_file,const char* ca_path);
struct st_VioSSLAcceptorFd *new_VioSSLAcceptorFd(
		const char* key_file,const char* cert_file,const char* ca_file,const char* ca_path);
struct st_vio* new_VioSSL(struct st_VioSSLAcceptorFd* fd, struct st_vio * sd,int state);
static int
init_bio_(struct st_VioSSLAcceptorFd* fd, struct st_vio* sd, int state,  int bio_flags);
static void
report_errors();
	
#ifdef	__cplusplus
}
#endif
#endif /* HAVE_OPENSSL */

#ifndef __WIN__
#define HANDLE void *
#endif

/* This structure is for every connection on both sides */
struct st_vio
{
  my_socket		sd;		/* my_socket - real or imaginary */
  HANDLE hPipe;
  my_bool		localhost;	/* Are we from localhost? */
  int			fcntl_mode;	/* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_in	local;		/* Local internet address */
  struct sockaddr_in	remote;		/* Remote internet address */
  enum enum_vio_type	type;		/* Type of connection */
  char			desc[30];	/* String description */
#ifdef HAVE_VIO
  /* function pointers. They are similar for socket/SSL/whatever */
  void (*viodelete)(st_vio *);
  int(*vioerrno)(st_vio*);
  int(*read)(st_vio*, gptr, int);
  int(*write)(st_vio*, gptr, int);
  int(*vioblocking)(st_vio*, my_bool);
  my_bool(*is_blocking)(st_vio*);
  int(*viokeepalive)(st_vio *, my_bool);
  int(*fastsend)(st_vio *);
  my_bool(*peer_addr)(st_vio*, gptr);
  void(*in_addr)(st_vio*, struct in_addr*);
  my_bool(*should_retry)(st_vio *);
  int(*vioclose)(st_vio *);
  my_bool(*poll_read)(st_vio *,uint);

#ifdef HAVE_OPENSSL
  BIO* bio_;
  SSL* ssl_;
  my_bool open_;
#endif /* HAVE_OPENSSL */
#endif /* HAVE_VIO */
};


