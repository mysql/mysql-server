/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

/* yaSSL already uses BSD sockets */
#ifndef HAVE_YASSL

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

#endif /* HAVE_YASSL */
#endif /* __NETWARE__ */


static void
report_errors(SSL* ssl)
{
  unsigned long	l;
  const char *file;
  const char *data;
  int line, flags;
#ifndef DBUG_OFF
  char buf[512];
#endif

  DBUG_ENTER("report_errors");

  while ((l= ERR_get_error_line_data(&file,&line,&data,&flags)))
  {
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }

  if (ssl)
    DBUG_PRINT("error", ("error: %s",
                         ERR_error_string(SSL_get_error(ssl, l), buf)));

  DBUG_PRINT("info", ("socket_errno: %d", socket_errno));
  DBUG_VOID_RETURN;
}


size_t vio_ssl_read(Vio *vio, uchar* buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_ssl_read");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u  ssl: 0x%lx",
		       vio->sd, (long) buf, (uint) size, (long) vio->ssl_arg));

  r= SSL_read((SSL*) vio->ssl_arg, buf, size);
#ifndef DBUG_OFF
  if (r == (size_t) -1)
    report_errors((SSL*) vio->ssl_arg);
#endif
  DBUG_PRINT("exit", ("%u", (uint) r));
  DBUG_RETURN(r);
}


size_t vio_ssl_write(Vio *vio, const uchar* buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_ssl_write");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd,
                       (long) buf, (uint) size));

  r= SSL_write((SSL*) vio->ssl_arg, buf, size);
#ifndef DBUG_OFF
  if (r == (size_t) -1)
    report_errors((SSL*) vio->ssl_arg);
#endif
  DBUG_PRINT("exit", ("%u", (uint) r));
  DBUG_RETURN(r);
}


int vio_ssl_close(Vio *vio)
{
  int r= 0;
  SSL *ssl= (SSL*)vio->ssl_arg;
  DBUG_ENTER("vio_ssl_close");

  if (ssl)
  {
    /*
    THE SSL standard says that SSL sockets must send and receive a close_notify
    alert on socket shutdown to avoid truncation attacks. However, this can
    cause problems since we often hold a lock during shutdown and this IO can
    take an unbounded amount of time to complete. Since our packets are self
    describing with length, we aren't vunerable to these attacks. Therefore,
    we just shutdown by closing the socket (quiet shutdown).
    */
    SSL_set_quiet_shutdown(ssl, 1); 
    
    switch ((r= SSL_shutdown(ssl))) {
    case 1:
      /* Shutdown successful */
      break;
    case 0:
      /*
        Shutdown not yet finished - since the socket is going to
        be closed there is no need to call SSL_shutdown() a second
        time to wait for the other side to respond
      */
      break;
    default: /* Shutdown failed */
      DBUG_PRINT("vio_error", ("SSL_shutdown() failed, error: %d",
                               SSL_get_error(ssl, r)));
      break;
    }
  }
  DBUG_RETURN(vio_close(vio));
}


void vio_ssl_delete(Vio *vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointer */

  if (vio->type == VIO_TYPE_SSL)
    vio_ssl_close(vio); /* Still open, close connection first */

  if (vio->ssl_arg)
  {
    SSL_free((SSL*) vio->ssl_arg);
    vio->ssl_arg= 0;
  }

  vio_delete(vio);
}


static int ssl_do(struct st_VioSSLFd *ptr, Vio *vio, long timeout,
                  int (*connect_accept_func)(SSL*))
{
  SSL *ssl;
  my_bool unused;
  my_bool was_blocking;

  DBUG_ENTER("ssl_do");
  DBUG_PRINT("enter", ("ptr: 0x%lx, sd: %d  ctx: 0x%lx",
                       (long) ptr, vio->sd, (long) ptr->ssl_context));

  /* Set socket to blocking if not already set */
  vio_blocking(vio, 1, &was_blocking);

  if (!(ssl= SSL_new(ptr->ssl_context)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors(ssl);
    vio_blocking(vio, was_blocking, &unused);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("ssl: 0x%lx timeout: %ld", (long) ssl, timeout));
  SSL_clear(ssl);
  SSL_SESSION_set_timeout(SSL_get_session(ssl), timeout);
  SSL_set_fd(ssl, vio->sd);

  if (connect_accept_func(ssl) < 1)
  {
    DBUG_PRINT("error", ("SSL_connect/accept failure"));
    report_errors(ssl);
    SSL_free(ssl);
    vio_blocking(vio, was_blocking, &unused);
    DBUG_RETURN(1);
  }

  /*
    Connection succeeded. Install new function handlers,
    change type, set sd to the fd used when connecting
    and set pointer to the SSL structure
  */
  vio_reset(vio, VIO_TYPE_SSL, SSL_get_fd(ssl), 0, 0);
  vio->ssl_arg= (void*)ssl;

#ifndef DBUG_OFF
  {
    /* Print some info about the peer */
    X509 *cert;
    char buf[512];

    DBUG_PRINT("info",("SSL connection succeeded"));
    DBUG_PRINT("info",("Using cipher: '%s'" , SSL_get_cipher_name(ssl)));

    if ((cert= SSL_get_peer_certificate (ssl)))
    {
      DBUG_PRINT("info",("Peer certificate:"));
      X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
      DBUG_PRINT("info",("\t subject: '%s'", buf));
      X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
      DBUG_PRINT("info",("\t issuer: '%s'", buf));
      X509_free(cert);
    }
    else
      DBUG_PRINT("info",("Peer does not have certificate."));

    if (SSL_get_shared_ciphers(ssl, buf, sizeof(buf)))
    {
      DBUG_PRINT("info",("shared_ciphers: '%s'", buf));
    }
    else
      DBUG_PRINT("info",("no shared ciphers!"));
  }
#endif

  DBUG_RETURN(0);
}


int sslaccept(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  DBUG_ENTER("sslaccept");
  DBUG_RETURN(ssl_do(ptr, vio, timeout, SSL_accept));
}


int sslconnect(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  DBUG_ENTER("sslconnect");
  DBUG_RETURN(ssl_do(ptr, vio, timeout, SSL_connect));
}


int vio_ssl_blocking(Vio *vio __attribute__((unused)),
		     my_bool set_blocking_mode,
		     my_bool *old_mode)
{
  /* Mode is always blocking */
  *old_mode= 1;
  /* Return error if we try to change to non_blocking mode */
  return (set_blocking_mode ? 0 : 1);
}



#endif /* HAVE_OPENSSL */
