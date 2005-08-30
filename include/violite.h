/* Copyright (C) 2000 MySQL AB

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

/*
 * Vio Lite.
 * Purpose: include file for Vio that will work with C and C++
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

#ifndef __WIN__
#define HANDLE void *
#endif

Vio*	vio_new(my_socket sd, enum enum_vio_type type, my_bool localhost);
#ifdef __WIN__
Vio*	vio_new_win32pipe(HANDLE hPipe);
#endif
void	vio_delete(Vio* vio);
int	vio_close(Vio* vio);

#ifdef EMBEDDED_LIBRARY
void vio_reset(Vio *vio);
#else
void vio_reset(Vio* vio, enum enum_vio_type type,
	       my_socket sd, HANDLE hPipe, my_bool localhost);
#endif

int	vio_read(Vio *vio, gptr	buf, int size);
int	vio_write(Vio *vio, const gptr buf, int size);
int	vio_blocking(Vio *vio, my_bool onoff, my_bool *old_mode);
my_bool	vio_is_blocking(Vio *vio);
/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible */
int	vio_fastsend(Vio *vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible */
int	vio_keepalive(Vio *vio, my_bool	onoff);
/* Whenever we should retry the last read/write operation. */
my_bool	vio_should_retry(Vio *vio);
/* Check that operation was timed out */
my_bool	vio_was_interrupted(Vio *vio);
/* Short text description of the socket for those, who are curious.. */
const char* vio_description(Vio *vio);
/* Return the type of the connection */
enum enum_vio_type vio_type(Vio* vio);
/* Return last error number */
int vio_errno(Vio*vio);
/* Get socket number */
my_socket vio_fd(Vio*vio);
/* Remote peer's address and name in text form */
my_bool vio_peer_addr(Vio* vio, char *buf, uint16 *port);
/* Remotes in_addr */
void vio_in_addr(Vio *vio, struct in_addr *in);
my_bool vio_poll_read(Vio *vio,uint timeout);
void vio_timeout(Vio *vio,uint timeout);

#ifdef HAVE_OPENSSL
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x0090700f
#define DES_cblock des_cblock
#define DES_key_schedule des_key_schedule
#define DES_set_key_unchecked(k,ks) des_set_key_unchecked((k),*(ks))
#define DES_ede3_cbc_encrypt(i,o,l,k1,k2,k3,iv,e) des_ede3_cbc_encrypt((i),(o),(l),*(k1),*(k2),*(k3),(iv),(e))
#endif

#define HEADER_DES_LOCL_H dummy_something
#include <openssl/ssl.h>
#include <openssl/err.h>

struct st_VioSSLAcceptorFd 
{
  SSL_CTX *ssl_context;
  SSL_METHOD *ssl_method;
  struct st_VioSSLAcceptorFd *session_id_context;
};

/* One copy for client */
struct st_VioSSLConnectorFd
{
  SSL_CTX *ssl_context;
  /* function pointers which are only once for SSL client */ 
  SSL_METHOD *ssl_method;
};

int sslaccept(struct st_VioSSLAcceptorFd*, Vio *, long timeout);
int sslconnect(struct st_VioSSLConnectorFd*, Vio *, long timeout);

struct st_VioSSLConnectorFd
*new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
		       const char *ca_file,  const char *ca_path,
		       const char *cipher);
struct st_VioSSLAcceptorFd
*new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
		      const char *ca_file,const char *ca_path,
		      const char *cipher);
Vio *new_VioSSL(struct st_VioSSLAcceptorFd *fd, Vio *sd, int state);
#endif

#ifdef	__cplusplus
}
#endif

#if defined(HAVE_VIO) && !defined(DONT_MAP_VIO)
#define vio_delete(vio) 			(vio)->viodelete(vio)
#define vio_errno(vio)	 			(vio)->vioerrno(vio)
#define vio_read(vio, buf, size) 		(vio)->read(vio,buf,size)
#define vio_write(vio, buf, size) 		(vio)->write(vio, buf, size)
#define vio_blocking(vio, set_blocking_mode, old_mode)\
 	(vio)->vioblocking(vio, set_blocking_mode, old_mode)
#define vio_is_blocking(vio) 			(vio)->is_blocking(vio)
#define vio_fastsend(vio)			(vio)->fastsend(vio)
#define vio_keepalive(vio, set_keep_alive)	(vio)->viokeepalive(vio, set_keep_alive)
#define vio_should_retry(vio) 			(vio)->should_retry(vio)
#define vio_was_interrupted(vio) 		(vio)->was_interrupted(vio)
#define vio_close(vio)				((vio)->vioclose)(vio)
#define vio_peer_addr(vio, buf, prt)		(vio)->peer_addr(vio, buf, prt)
#define vio_in_addr(vio, in)			(vio)->in_addr(vio, in)
#define vio_timeout(vio, seconds)			(vio)->timeout(vio, seconds)
#endif /* defined(HAVE_VIO) && !defined(DONT_MAP_VIO) */

/* This enumerator is used in parser - should be always visible */
enum SSL_type
{
  SSL_TYPE_NOT_SPECIFIED= -1,
  SSL_TYPE_NONE,
  SSL_TYPE_ANY,
  SSL_TYPE_X509,
  SSL_TYPE_SPECIFIED
};

#ifndef EMBEDDED_LIBRARY
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
  void    (*viodelete)(Vio*);
  int     (*vioerrno)(Vio*);
  int     (*read)(Vio*, gptr, int);
  int     (*write)(Vio*, gptr, int);
  int     (*vioblocking)(Vio*, my_bool, my_bool *);
  my_bool (*is_blocking)(Vio*);
  int     (*viokeepalive)(Vio*, my_bool);
  int     (*fastsend)(Vio*);
  my_bool (*peer_addr)(Vio*, char *, uint16*);
  void    (*in_addr)(Vio*, struct in_addr*);
  my_bool (*should_retry)(Vio*);
  my_bool (*was_interrupted)(Vio*);
  int     (*vioclose)(Vio*);
  void	  (*timeout)(Vio*, unsigned int timeout);
  void	  *ssl_arg;
#endif /* HAVE_VIO */
};
#endif /* EMBEDDED_LIBRARY */
#endif /* vio_violite_h_ */
