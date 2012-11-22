/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

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

#include "feedback.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#define addrinfo ADDRINFOA
#endif

namespace feedback {

static const uint FOR_READING= 0;
static const uint FOR_WRITING= 1;

/**
  implementation of the Url class that sends the data via HTTP POST request.

  Both http:// and https:// protocols are supported.
*/
class Url_http: public Url {
  protected:
  const LEX_STRING host, port, path;
  bool ssl;

  Url_http(LEX_STRING &url_arg, LEX_STRING &host_arg,
          LEX_STRING &port_arg, LEX_STRING &path_arg, bool ssl_arg) :
    Url(url_arg), host(host_arg), port(port_arg), path(path_arg), ssl(ssl_arg)
    {}
  ~Url_http()
  {
    my_free(host.str);
    my_free(port.str);
    my_free(path.str);
  }

  public:
  int send(const char* data, size_t data_length);

  friend Url* http_create(const char *url, size_t url_length);
};

/**
  create a Url_http object out of the url, if possible.

  @note
  Arbitrary limitations here.

  The url must be http[s]://hostname[:port]/path
  No username:password@ or ?script=parameters are supported.

  But it's ok. This is not a generic purpose www browser - it only needs to be
  good enough to POST the data to mariadb.org.
*/
Url* http_create(const char *url, size_t url_length)
{
  const char *s;
  LEX_STRING full_url= {const_cast<char*>(url), url_length};
  LEX_STRING host, port, path;
  bool ssl= false;

  if (is_prefix(url, "http://"))
    s= url + 7;
#ifdef HAVE_OPENSSL
  else if (is_prefix(url, "https://"))
  {
    ssl= true;
    s= url + 8;
  }
#endif
  else
    return NULL;

  for (url= s; *s && *s != ':' && *s != '/'; s++) /* no-op */;
  host.str= const_cast<char*>(url);
  host.length= s-url;

  if (*s == ':')
  {
    for (url= ++s; *s && *s >= '0' && *s <= '9'; s++) /* no-op */;
    port.str= const_cast<char*>(url);
    port.length= s-url;
  }
  else
  {
    if (ssl)
    {
      port.str= const_cast<char*>("443");
      port.length=3;
    }
    else
    {
      port.str= const_cast<char*>("80");
      port.length=2;
    }
  }

  if (*s == 0)
  {
    path.str= const_cast<char*>("/");
    path.length= 1;
  }
  else
  {
    path.str= const_cast<char*>(s);
    path.length= strlen(s);
  }
  if (!host.length || !port.length || path.str[0] != '/')
    return NULL;

  host.str= my_strndup(host.str, host.length, MYF(MY_WME));
  port.str= my_strndup(port.str, port.length, MYF(MY_WME));
  path.str= my_strndup(path.str, path.length, MYF(MY_WME));

  if (!host.str || !port.str || !path.str)
  {
    my_free(host.str);
    my_free(port.str);
    my_free(path.str);
    return NULL;
  }

  return new Url_http(full_url, host, port, path, ssl);
}

/* do the vio_write and check that all data were sent ok */
#define write_check(VIO, DATA, LEN)             \
  (vio_write((VIO), (uchar*)(DATA), (LEN)) != (LEN))

int Url_http::send(const char* data, size_t data_length)
{
  my_socket fd= INVALID_SOCKET;
  char buf[1024];
  uint len= 0;

  addrinfo *addrs, *addr, filter= {0, AF_UNSPEC, SOCK_STREAM, 6, 0, 0, 0, 0};
  int res= getaddrinfo(host.str, port.str, &filter, &addrs);

  if (res)
  {
    sql_print_error("feedback plugin: getaddrinfo() failed for url '%s': %s",
                    full_url.str, gai_strerror(res));
    return 1;
  }

  for (addr= addrs; addr != NULL; addr= addr->ai_next)
  {
    fd= socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd == INVALID_SOCKET)
      continue;

    if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0)
      break;

    closesocket(fd);
  }

  freeaddrinfo(addrs);

  if (fd == INVALID_SOCKET)
  {
    sql_print_error("feedback plugin: could not connect for url '%s'",
                    full_url.str);
    return 1;
  }

  Vio *vio= vio_new(fd, VIO_TYPE_TCPIP, 0);
  if (!vio)
  {
    sql_print_error("feedback plugin: vio_new failed for url '%s'",
                    full_url.str);
    closesocket(fd);
    return 1;
  }

#ifdef HAVE_OPENSSL
  struct st_VioSSLFd *UNINIT_VAR(ssl_fd);
  if (ssl)
  {
    enum enum_ssl_init_error ssl_init_error= SSL_INITERR_NOERROR;
    ulong ssl_error= 0;
    if (!(ssl_fd= new_VioSSLConnectorFd(0, 0, 0, 0, 0, &ssl_init_error)) ||
        sslconnect(ssl_fd, vio, send_timeout, &ssl_error))
    {
      const char *err;
      if (ssl_init_error != SSL_INITERR_NOERROR)
        err= sslGetErrString(ssl_init_error);
      else
      {
        ERR_error_string_n(ssl_error, buf, sizeof(buf));
        buf[sizeof(buf)-1]= 0;
        err= buf;
      }

      sql_print_error("feedback plugin: ssl failed for url '%s' %s",
                      full_url.str, err);
      if (ssl_fd)
        free_vio_ssl_acceptor_fd(ssl_fd);
      closesocket(fd);
      vio_delete(vio);
      return 1;
    }
  }
#endif

  static const LEX_STRING boundary= 
  { C_STRING_WITH_LEN("----------------------------ba4f3696b39f") };
  static const LEX_STRING header=
  { C_STRING_WITH_LEN("\r\n"
      "Content-Disposition: form-data; name=\"data\"; filename=\"-\"\r\n"
      "Content-Type: application/octet-stream\r\n\r\n")
  };

  len= my_snprintf(buf, sizeof(buf),
                   "POST %s HTTP/1.0\r\n"
                   "User-Agent: MariaDB User Feedback Plugin\r\n"
                   "Host: %s:%s\r\n"
                   "Accept: */*\r\n"
                   "Content-Length: %u\r\n"
                   "Content-Type: multipart/form-data; boundary=%s\r\n"
                   "\r\n",
                   path.str, host.str, port.str,
                   (uint)(2*boundary.length + header.length + data_length + 4),
                   boundary.str + 2);

  vio_timeout(vio, FOR_READING, send_timeout);
  vio_timeout(vio, FOR_WRITING, send_timeout);
  res = write_check(vio, buf, len)
     || write_check(vio, boundary.str, boundary.length)
     || write_check(vio, header.str, header.length)
     || write_check(vio, data, data_length)
     || write_check(vio, boundary.str, boundary.length)
     || write_check(vio, "--\r\n", 4);

  if (res)
    sql_print_error("feedback plugin: failed to send report to '%s'",
                    full_url.str);
  else
  {
    sql_print_information("feedback plugin: report to '%s' was sent",
                          full_url.str);

    /*
      if the data were send successfully, read the reply.
      Extract the first string between <h1>...</h1> tags
      and put it as a server reply into the error log.
    */
    len= 0;
    for (;;)
    {
      size_t i= sizeof(buf) - len - 1;
      if (i)
        i= vio_read(vio, (uchar*)buf + len, i);
      if ((int)i <= 0)
        break;
      len+= i;
    }
    if (len)
    {
      char *from;

      buf[len]= 0; // safety

      if ((from= strstr(buf, "<h1>")))
      {
        from+= 4;
        char *to= strstr(from, "</h1>");
        if (to)
          *to= 0;
        else
          from= NULL;
      }
      if (from)
        sql_print_information("feedback plugin: server replied '%s'", from);
      else
        sql_print_warning("feedback plugin: failed to parse server reply");
    }
    else
    {
      res= 1;
      sql_print_error("feedback plugin: failed to read server reply");
    }
  }

  vio_delete(vio);

#ifdef HAVE_OPENSSL
  if (ssl)
  {
    SSL_CTX_free(ssl_fd->ssl_context);
    my_free(ssl_fd);
  }
#endif

  return res;
}

} // namespace feedback

