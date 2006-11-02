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


int vio_ssl_read(Vio *vio, gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_ssl_read");
  DBUG_PRINT("enter", ("sd: %d, buf: 0x%lx, size: %d, ssl_: 0x%lx",
		       vio->sd, buf, size, vio->ssl_arg));

  r= SSL_read((SSL*) vio->ssl_arg, buf, size);
#ifndef DBUG_OFF
  if (r < 0)
    report_errors((SSL*) vio->ssl_arg);
#endif
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_write(Vio *vio, const gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_ssl_write");
  DBUG_PRINT("enter", ("sd: %d, buf: 0x%lx, size: %d", vio->sd, buf, size));

  r= SSL_write((SSL*) vio->ssl_arg, buf, size);
#ifndef DBUG_OFF
  if (r < 0)
    report_errors((SSL*) vio->ssl_arg);
#endif
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_close(Vio *vio)
{
  int r= 0;
  SSL *ssl= (SSL*)vio->ssl_arg;
  DBUG_ENTER("vio_ssl_close");

  if (ssl)
  {
    switch ((r= SSL_shutdown(ssl)))
    {
    case 1: /* Shutdown successful */
      break;
    case 0: /* Shutdown not yet finished, call it again */
      if ((r= SSL_shutdown(ssl) >= 0))
        break;
      /* Fallthrough */
    default: /* Shutdown failed */
      DBUG_PRINT("vio_error", ("SSL_shutdown() failed, error: %s",
                               SSL_get_error(ssl, r)));
      break;
    }
    SSL_free(ssl);
    vio->ssl_arg= 0;
  }
  DBUG_RETURN(vio_close(vio));
}


int sslaccept(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  SSL *ssl;
  my_bool unused;
  my_bool net_blocking;
  enum enum_vio_type old_type;
  DBUG_ENTER("sslaccept");
  DBUG_PRINT("enter", ("sd: %d  ptr: %p, timeout: %d",
                       vio->sd, ptr, timeout));

  old_type= vio->type;
  net_blocking= vio_is_blocking(vio);
  vio_blocking(vio, 1, &unused);	/* Must be called before reset */
  vio_reset(vio, VIO_TYPE_SSL, vio->sd, 0, FALSE);

  if (!(ssl= SSL_new(ptr->ssl_context)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors(ssl);
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }
  vio->ssl_arg= (void*)ssl;
  DBUG_PRINT("info", ("ssl_: %p  timeout: %ld", ssl, timeout));
  SSL_clear(ssl);
  SSL_SESSION_set_timeout(SSL_get_session(ssl), timeout);
  SSL_set_fd(ssl, vio->sd);
  if (SSL_accept(ssl) < 1)
  {
    DBUG_PRINT("error", ("SSL_accept failure"));
    report_errors(ssl);
    SSL_free(ssl);
    vio->ssl_arg= 0;
    vio_reset(vio, old_type,vio->sd,0,FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }

#ifndef DBUG_OFF
  {
    char buf[1024];
    X509 *client_cert;
    DBUG_PRINT("info",("cipher_name= '%s'", SSL_get_cipher_name(ssl)));

    if ((client_cert= SSL_get_peer_certificate (ssl)))
    {
      DBUG_PRINT("info",("Client certificate:"));
      X509_NAME_oneline (X509_get_subject_name (client_cert),
                         buf, sizeof(buf));
      DBUG_PRINT("info",("\t subject: %s", buf));

      X509_NAME_oneline (X509_get_issuer_name  (client_cert),
                         buf, sizeof(buf));
      DBUG_PRINT("info",("\t issuer: %s", buf));

      X509_free (client_cert);
    }
    else
      DBUG_PRINT("info",("Client does not have certificate."));

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


int sslconnect(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  SSL *ssl;
  my_bool unused;
  my_bool net_blocking;
  enum enum_vio_type old_type;

  DBUG_ENTER("sslconnect");
  DBUG_PRINT("enter", ("sd: %d,  ptr: %p, ctx: %p",
                       vio->sd, ptr, ptr->ssl_context));

  old_type= vio->type;
  net_blocking= vio_is_blocking(vio);
  vio_blocking(vio, 1, &unused);	/* Must be called before reset */
  vio_reset(vio, VIO_TYPE_SSL, vio->sd, 0, FALSE);
  if (!(ssl= SSL_new(ptr->ssl_context)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors(ssl);
    vio_reset(vio, old_type, vio->sd, 0, FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }
  vio->ssl_arg= (void*)ssl;
  DBUG_PRINT("info", ("ssl: %p, timeout: %ld", ssl, timeout));
  SSL_clear(ssl);
  SSL_SESSION_set_timeout(SSL_get_session(ssl), timeout);
  SSL_set_fd(ssl, vio->sd);
  if (SSL_connect(ssl) < 1)
  {
    DBUG_PRINT("error", ("SSL_connect failure"));
    report_errors(ssl);
    SSL_free(ssl);
    vio->ssl_arg= 0;
    vio_reset(vio, old_type, vio->sd, 0, FALSE);
    vio_blocking(vio, net_blocking, &unused);
    DBUG_RETURN(1);
  }
#ifndef DBUG_OFF
  {
    X509 *server_cert;
    DBUG_PRINT("info",("cipher_name: '%s'" , SSL_get_cipher_name(ssl)));

    if ((server_cert= SSL_get_peer_certificate (ssl)))
    {
      char buf[256];
      DBUG_PRINT("info",("Server certificate:"));
      X509_NAME_oneline(X509_get_subject_name(server_cert), buf, sizeof(buf));
      DBUG_PRINT("info",("\t subject: %s", buf));
      X509_NAME_oneline (X509_get_issuer_name(server_cert), buf, sizeof(buf));
      DBUG_PRINT("info",("\t issuer: %s", buf));
      X509_free (server_cert);
    }
    else
      DBUG_PRINT("info",("Server does not have certificate."));
  }
#endif

  DBUG_RETURN(0);
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
