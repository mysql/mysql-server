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
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include "vio_priv.h"

#ifdef HAVE_OPENSSL

#ifdef __NETWARE__
/*
  The default OpenSSL implementation on NetWare uses WinSock.
  This code allows us to use the BSD sockets.
*/

static int SSL_set_fd_bsd(SSL *s, int fd)
{
  int result= -1;
  BIO_METHOD *BIO_s_bsdsocket();
  BIO *bio;

  if ((bio= BIO_new(BIO_s_bsdsocket())))
  {
    result= BIO_set_fd(bio, fd, BIO_NOCLOSE);
    SSL_set_bio(s, bio, bio);
  }
  return result;
}

#define SSL_set_fd(A, B)  SSL_set_fd_bsd((A), (B))

#endif /* __NETWARE__ */


static void
report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags;
  DBUG_ENTER("report_errors");

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)))
  {
    char buf[512];
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }
  DBUG_PRINT("info", ("errno: %d", socket_errno));
  DBUG_VOID_RETURN;
}

/*
  Delete a vio object

  SYNPOSIS
    vio_ssl_delete()
    vio			Vio object.  May be 0.
*/


void vio_ssl_delete(Vio * vio)
{
  if (vio)
  {
    if (vio->type != VIO_CLOSED)
      vio_close(vio);
    my_free((gptr) vio,MYF(0));
  }
}


int vio_ssl_errno(Vio *vio __attribute__((unused)))
{
  return socket_errno;	/* On Win32 this mapped to WSAGetLastError() */
}


int vio_ssl_read(Vio * vio, gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_ssl_read");
  DBUG_PRINT("enter", ("sd=%d, buf=%p, size=%d, ssl_=%p",
		       vio->sd, buf, size, vio->ssl_arg));

  if ((r= SSL_read((SSL*) vio->ssl_arg, buf, size)) < 0)
  {
    int err= SSL_get_error((SSL*) vio->ssl_arg, r);
    DBUG_PRINT("error",("SSL_read(): %d  SSL_get_error(): %d", r, err));
    report_errors();
  }
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_write(Vio * vio, const gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_ssl_write");
  DBUG_PRINT("enter", ("sd=%d, buf=%p, size=%d", vio->sd, buf, size));

  if ((r= SSL_write((SSL*) vio->ssl_arg, buf, size)) < 0)
    report_errors();
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_fastsend(Vio * vio __attribute__((unused)))
{
  int r=0;
  DBUG_ENTER("vio_ssl_fastsend");

#if defined(IPTOS_THROUGHPUT) && !defined(__EMX__)
  {
    int tos= IPTOS_THROUGHPUT;
    r= setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos));
  }
#endif                                    /* IPTOS_THROUGHPUT && !__EMX__ */
  if (!r)
  {
#ifdef __WIN__
    BOOL nodelay= 1;
    r= setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY, (const char*) &nodelay,
                  sizeof(nodelay));
#else
    int nodelay= 1;
    r= setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay,
                  sizeof(nodelay));
#endif                                          /* __WIN__ */
  }
  if (r)
  {
    DBUG_PRINT("warning", ("Couldn't set socket option for fast send"));
    r= -1;
  }
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  DBUG_ENTER("vio_ssl_keepalive");
  DBUG_PRINT("enter", ("sd=%d, set_keep_alive=%d", vio->sd, (int)
		       set_keep_alive));
  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    uint opt = (set_keep_alive) ? 1 : 0;
    r= setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
		  sizeof(opt));
  }
  DBUG_RETURN(r);
}


my_bool
vio_ssl_should_retry(Vio * vio __attribute__((unused)))
{
  int en = socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK);
}


my_bool
vio_ssl_was_interrupted(Vio *vio __attribute__((unused)))
{
  int en= socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK || en == SOCKET_ETIMEDOUT);
}


int vio_ssl_close(Vio * vio)
{
  int r;
  DBUG_ENTER("vio_ssl_close");
  r=0;
  if ((SSL*) vio->ssl_arg)
  {
    r = SSL_shutdown((SSL*) vio->ssl_arg);
    SSL_free((SSL*) vio->ssl_arg);
    vio->ssl_arg= 0;
  }
  if (vio->sd >= 0)
  {
    if (shutdown(vio->sd, 2))
      r= -1;
    if (closesocket(vio->sd))
      r= -1;
  }
  if (r)
  {
    DBUG_PRINT("error", ("close() failed, error: %d",socket_errno));
    report_errors();
    /* FIXME: error handling (not critical for MySQL) */
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  DBUG_RETURN(r);
}


const char *vio_ssl_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type vio_ssl_type(Vio* vio)
{
  return vio->type;
}

my_socket vio_ssl_fd(Vio* vio)
{
  return vio->sd;
}


my_bool vio_ssl_peer_addr(Vio * vio, char *buf, uint16 *port)
{
  DBUG_ENTER("vio_ssl_peer_addr");
  DBUG_PRINT("enter", ("sd=%d", vio->sd));
  if (vio->localhost)
  {
    strmov(buf,"127.0.0.1");
    *port=0;
  }
  else
  {
    size_socket addrLen = sizeof(struct sockaddr);
    if (getpeername(vio->sd, (struct sockaddr *) (& (vio->remote)),
		    &addrLen) != 0)
    {
      DBUG_PRINT("exit", ("getpeername, error: %d", socket_errno));
      DBUG_RETURN(1);
    }
#ifdef TO_BE_FIXED
    my_inet_ntoa(vio->remote.sin_addr,buf);
    *port= 0;
#else
    strmov(buf, "unknown");
    *port= 0;
#endif
  }
  DBUG_PRINT("exit", ("addr=%s", buf));
  DBUG_RETURN(0);
}


void vio_ssl_in_addr(Vio *vio, struct in_addr *in)
{
  DBUG_ENTER("vio_ssl_in_addr");
  if (vio->localhost)
    bzero((char*) in, sizeof(*in));
  else
    *in=vio->remote.sin_addr;
  DBUG_VOID_RETURN;
}


/*
  TODO: Add documentation
*/

int sslaccept(struct st_VioSSLAcceptorFd* ptr, Vio* vio, long timeout)
{
  char *str;
  char buf[1024];
  X509* client_cert;
  my_bool unused;
  my_bool net_blocking;
  enum enum_vio_type old_type;  
  DBUG_ENTER("sslaccept");
  DBUG_PRINT("enter", ("sd=%d ptr=%p", vio->sd,ptr));

  old_type= vio->type;
  net_blocking = vio_is_blocking(vio);
  vio_blocking(vio, 1, &unused);	/* Must be called before reset */
  vio_reset(vio,VIO_TYPE_SSL,vio->sd,0,FALSE);
  vio->ssl_arg= 0;
  if (!(vio->ssl_arg= (void*) SSL_new(ptr->ssl_context)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors();
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("ssl_=%p  timeout=%ld",(SSL*) vio->ssl_arg, timeout));
  SSL_clear((SSL*) vio->ssl_arg);
  SSL_SESSION_set_timeout(SSL_get_session((SSL*) vio->ssl_arg), timeout);
  SSL_set_fd((SSL*) vio->ssl_arg,vio->sd);
  SSL_set_accept_state((SSL*) vio->ssl_arg);
  if (SSL_do_handshake((SSL*) vio->ssl_arg) < 1)
  {
    DBUG_PRINT("error", ("SSL_do_handshake failure"));
    report_errors();
    SSL_free((SSL*) vio->ssl_arg);
    vio->ssl_arg= 0;
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }
#ifndef DBUG_OFF
  DBUG_PRINT("info",("SSL_get_cipher_name() = '%s'"
		     ,SSL_get_cipher_name((SSL*) vio->ssl_arg)));
  client_cert = SSL_get_peer_certificate ((SSL*) vio->ssl_arg);
  if (client_cert != NULL)
  {
    DBUG_PRINT("info",("Client certificate:"));
    str = X509_NAME_oneline (X509_get_subject_name (client_cert), 0, 0);
    DBUG_PRINT("info",("\t subject: %s", str));
    free (str);

    str = X509_NAME_oneline (X509_get_issuer_name  (client_cert), 0, 0);
    DBUG_PRINT("info",("\t issuer: %s", str));
    free (str);

    X509_free (client_cert);
  }
  else
    DBUG_PRINT("info",("Client does not have certificate."));

  str=SSL_get_shared_ciphers((SSL*) vio->ssl_arg, buf, sizeof(buf));
  if (str)
  {
    DBUG_PRINT("info",("SSL_get_shared_ciphers() returned '%s'",str));
  }
  else
  {
    DBUG_PRINT("info",("no shared ciphers!"));
  }

#endif
  DBUG_RETURN(0);
}


int sslconnect(struct st_VioSSLConnectorFd* ptr, Vio* vio, long timeout)
{
  char *str;
  X509*    server_cert;
  my_bool unused;
  my_bool net_blocking;
  enum enum_vio_type old_type;  
  DBUG_ENTER("sslconnect");
  DBUG_PRINT("enter", ("sd=%d ptr=%p ctx: %p", vio->sd,ptr,ptr->ssl_context));

  old_type= vio->type;
  net_blocking = vio_is_blocking(vio);
  vio_blocking(vio, 1, &unused);	/* Must be called before reset */
  vio_reset(vio,VIO_TYPE_SSL,vio->sd,0,FALSE);
  vio->ssl_arg= 0;
  if (!(vio->ssl_arg = SSL_new(ptr->ssl_context)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors();
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);    
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("ssl_=%p  timeout=%ld",(SSL*) vio->ssl_arg, timeout));
  SSL_clear((SSL*) vio->ssl_arg);
  SSL_SESSION_set_timeout(SSL_get_session((SSL*) vio->ssl_arg), timeout);
  SSL_set_fd ((SSL*) vio->ssl_arg, vio->sd);
  SSL_set_connect_state((SSL*) vio->ssl_arg);
  if (SSL_do_handshake((SSL*) vio->ssl_arg) < 1)
  {
    DBUG_PRINT("error", ("SSL_do_handshake failure"));
    report_errors();
    SSL_free((SSL*) vio->ssl_arg);
    vio->ssl_arg= 0;
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }  
#ifndef DBUG_OFF
  DBUG_PRINT("info",("SSL_get_cipher_name() = '%s'"
		     ,SSL_get_cipher_name((SSL*) vio->ssl_arg)));
  server_cert = SSL_get_peer_certificate ((SSL*) vio->ssl_arg);
  if (server_cert != NULL)
  {
    DBUG_PRINT("info",("Server certificate:"));
    str = X509_NAME_oneline (X509_get_subject_name (server_cert), 0, 0);
    DBUG_PRINT("info",("\t subject: %s", str));
    free(str);

    str = X509_NAME_oneline (X509_get_issuer_name  (server_cert), 0, 0);
    DBUG_PRINT("info",("\t issuer: %s", str));
    free(str);

    /*
      We could do all sorts of certificate verification stuff here before
      deallocating the certificate.
    */
    X509_free (server_cert);
  }
  else
    DBUG_PRINT("info",("Server does not have certificate."));
#endif
  DBUG_RETURN(0);
}


int vio_ssl_blocking(Vio * vio __attribute__((unused)),
		     my_bool set_blocking_mode,
		     my_bool *old_mode)
{
  /* Return error if we try to change to non_blocking mode */
  *old_mode=1;					/* Mode is always blocking */
  return set_blocking_mode ? 0 : 1;
}


void vio_ssl_timeout(Vio *vio __attribute__((unused)),
		     uint which __attribute__((unused)),
                     uint timeout __attribute__((unused)))
{
#ifdef __WIN__
  ulong wait_timeout= (ulong) timeout * 1000;
  (void) setsockopt(vio->sd, SOL_SOCKET,
	which ? SO_SNDTIMEO : SO_RCVTIMEO, (char*) &wait_timeout,
        sizeof(wait_timeout));
#endif /* __WIN__ */
}
#endif /* HAVE_OPENSSL */
