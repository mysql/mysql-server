/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include <my_global.h>

#ifdef HAVE_OPENSSL

#include <mysql_com.h>

#include <errno.h>
#include <assert.h>
#include <violite.h>
#include <my_sys.h>
#include <my_net.h>
#include <m_string.h>

#ifndef __WIN__
#define HANDLE void *
#endif

static void
report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags, any_ssl_error = 0;
  DBUG_ENTER("report_errors");

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
  {
    char buf[200];
    any_ssl_error = 1;
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }
  if (!any_ssl_error) {
    DBUG_PRINT("info", ("No OpenSSL errors."));
  }
  DBUG_PRINT("info", ("BTW, errno=%d", socket_errno));
  DBUG_VOID_RETURN;
}


void vio_ssl_delete(Vio * vio)
{
  /* It must be safe to delete null pointers. */
  /* This matches the semantics of C++'s delete operator. */
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
		       vio->sd, buf, size, vio->ssl_));

#ifndef DBUG_OFF
  errno = 0;
#endif /* DBUG_OFF */
  r = SSL_read(vio->ssl_, buf, size);
#ifndef DBUG_OFF
  if ( r<= 0) {
    r=SSL_get_error(vio->ssl_, r);
    DBUG_PRINT("info",("SSL_get_error returned %d",r));
    report_errors();
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_write(Vio * vio, const gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_ssl_write");
  DBUG_PRINT("enter", ("sd=%d, buf=%p, size=%d", vio->sd, buf, size));

#ifndef DBUG_OFF
  errno = 0;
#endif /* DBUG_OFF */
  r = SSL_write(vio->ssl_, buf, size);
#ifndef DBUG_OFF
  if (r<0)
    report_errors();
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_ssl_fastsend(Vio * vio __attribute__((unused)))
{
  int r=0;
  DBUG_ENTER("vio_ssl_fastsend");

#ifdef IPTOS_THROUGHPUT
  {
#ifndef __EMX__
    int tos = IPTOS_THROUGHPUT;
    if (!setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos)))
#endif				/* !__EMX__ */
    {
      int nodelay = 1;
      if (setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY, (void *) &nodelay,
		     sizeof(nodelay))) {
	DBUG_PRINT("warning",
		   ("Couldn't set socket option for fast send"));
	r= -1;
      }
    }
  }
#endif	/* IPTOS_THROUGHPUT */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

int vio_ssl_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  uint opt = 0;
  DBUG_ENTER("vio_ssl_keepalive");
  DBUG_PRINT("enter", ("sd=%d, set_keep_alive=%d", vio->sd, (int)
		       set_keep_alive));
  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    if (set_keep_alive)
      opt = 1;
    r = setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
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


int vio_ssl_close(Vio * vio)
{
  int r;
  DBUG_ENTER("vio_ssl_close");
  r=0;
  if (vio->ssl_)
  {
    r = SSL_shutdown(vio->ssl_);
    SSL_free(vio->ssl_);
    vio->ssl_= 0;
  }
  if (shutdown(vio->sd,2))
    r= -1;
  if (closesocket(vio->sd))
    r= -1;
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


my_bool vio_ssl_peer_addr(Vio * vio, char *buf)
{
  DBUG_ENTER("vio_ssl_peer_addr");
  DBUG_PRINT("enter", ("sd=%d", vio->sd));
  if (vio->localhost)
  {
    strmov(buf,"127.0.0.1");
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
    /* FIXME */
/*    my_inet_ntoa(vio->remote.sin_addr,buf); */
  }
  DBUG_PRINT("exit", ("addr=%s", buf));
  DBUG_RETURN(0);
}


void vio_ssl_in_addr(Vio *vio, struct in_addr *in)
{
  DBUG_ENTER("vio_ssl_in_addr");
  if (vio->localhost)
    bzero((char*) in, sizeof(*in));	/* This should never be executed */
  else
    *in=vio->remote.sin_addr;
  DBUG_VOID_RETURN;
}


/* Return 0 if there is data to be read */

my_bool vio_ssl_poll_read(Vio *vio,uint timeout)
{
#ifndef HAVE_POLL
  return 0;
#else
  struct pollfd fds;
  int res;
  DBUG_ENTER("vio_ssl_poll");
  fds.fd=vio->sd;
  fds.events=POLLIN;
  fds.revents=0;
  if ((res=poll(&fds,1,(int) timeout*1000)) <= 0)
  {
    DBUG_RETURN(res < 0 ? 0 : 1);		/* Don't return 1 on errors */
  }
  DBUG_RETURN(fds.revents & POLLIN ? 0 : 1);
#endif
}

void sslaccept(struct st_VioSSLAcceptorFd* ptr, Vio* vio, long timeout)
{
  X509* client_cert;
  char *str;
  char buf[1024];
  DBUG_ENTER("sslaccept");
  DBUG_PRINT("enter", ("sd=%d ptr=%p", vio->sd,ptr));
  vio_reset(vio,VIO_TYPE_SSL,vio->sd,0,FALSE);
  vio->ssl_=0;
  vio->open_=FALSE; 
  if (!(vio->ssl_ = SSL_new(ptr->ssl_context_)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors();
    DBUG_VOID_RETURN;
  }
  DBUG_PRINT("info", ("ssl_=%p",vio->ssl_));
  SSL_clear(vio->ssl_);
  vio_blocking(vio, FALSE);
  SSL_SESSION_set_timeout(SSL_get_session(vio->ssl_), timeout);
  SSL_set_fd(vio->ssl_,vio->sd);
  SSL_set_accept_state(vio->ssl_);
  SSL_do_handshake(vio->ssl_);
  vio->open_ = TRUE;
#ifndef DBUF_OFF
  DBUG_PRINT("info",("SSL_get_cipher_name() = '%s'"
		     ,SSL_get_cipher_name(vio->ssl_)));
  client_cert = SSL_get_peer_certificate (vio->ssl_);
  if (client_cert != NULL) {
    DBUG_PRINT("info",("Client certificate:"));
    str = X509_NAME_oneline (X509_get_subject_name (client_cert), 0, 0);
    DBUG_PRINT("info",("\t subject: %s", str));
    free (str);

    str = X509_NAME_oneline (X509_get_issuer_name  (client_cert), 0, 0);
    DBUG_PRINT("info",("\t issuer: %s", str));
    free (str);

    X509_free (client_cert);
  } else
    DBUG_PRINT("info",("Client does not have certificate."));
  
  str=SSL_get_shared_ciphers(vio->ssl_, buf, sizeof(buf));
  if(str)
  {
    DBUG_PRINT("info",("SSL_get_shared_ciphers() returned '%s'",str));
  }
  else
  {
    DBUG_PRINT("info",("no shared ciphers!"));
  }

#endif
  DBUG_VOID_RETURN;
}

void sslconnect(struct st_VioSSLConnectorFd* ptr, Vio* vio, long timeout)
{
  char *str;
  X509*    server_cert;
  DBUG_ENTER("sslconnect");
  DBUG_PRINT("enter", ("sd=%d ptr=%p ctx: %p", vio->sd,ptr,ptr->ssl_context_));
  vio_reset(vio,VIO_TYPE_SSL,vio->sd,0,FALSE);

  vio->ssl_=0;
  vio->open_=FALSE; 
  if (!(vio->ssl_ = SSL_new(ptr->ssl_context_)))
  {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors();
    DBUG_VOID_RETURN;
  }
  DBUG_PRINT("info",("ssl_=%p",vio->ssl_));
  SSL_clear(vio->ssl_);
  vio_blocking(vio, FALSE);
  SSL_SESSION_set_timeout(SSL_get_session(vio->ssl_), timeout);
  SSL_set_fd (vio->ssl_, vio->sd);
  SSL_set_connect_state(vio->ssl_);
  SSL_do_handshake(vio->ssl_);
  vio->open_ = TRUE;
#ifndef DBUG_OFF
  DBUG_PRINT("info",("SSL_get_cipher_name() = '%s'"
		     ,SSL_get_cipher_name(vio->ssl_)));
  server_cert = SSL_get_peer_certificate (vio->ssl_);
  if (server_cert != NULL) {
    DBUG_PRINT("info",("Server certificate:"));
    str = X509_NAME_oneline (X509_get_subject_name (server_cert), 0, 0);
    DBUG_PRINT("info",("\t subject: %s", str));
    free (str);

    str = X509_NAME_oneline (X509_get_issuer_name  (server_cert), 0, 0);
    DBUG_PRINT("info",("\t issuer: %s", str));
    free (str);

    /* We could do all sorts of certificate verification stuff here before
     *        deallocating the certificate. */

    X509_free (server_cert);
  } else
    DBUG_PRINT("info",("Server does not have certificate."));
#endif
  DBUG_VOID_RETURN;
}

#endif /* HAVE_OPENSSL */
