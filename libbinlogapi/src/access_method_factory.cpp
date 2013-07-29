/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#include "access_method_factory.h"
#include "tcp_driver.h"
#include "file_driver.h"

using mysql::system::Binary_log_driver;
using mysql::system::Binlog_tcp_driver;
using mysql::system::Binlog_file_driver;

/**
   Parse the body of a MySQL URI.

   The format is <code>user[:password]@host[:port]</code>
*/
static Binary_log_driver *parse_mysql_url(const char *body, size_t len)
{
  /* Find the beginning of the user name */
  if (strncmp(body, "//", 2) != 0)
    return 0;

  /* Find the user name, which is mandatory */
  const char *user = body + 2;
  const char *user_end= strpbrk(user, ":@");
  if (user_end == 0 || user_end == user)
    return 0;
  assert(user_end - user >= 1);          // There has to be a username

  /* Find the password, which can be empty */
  assert(*user_end == ':' || *user_end == '@');
  const char *const pass = user_end + 1;        // Skip the ':' (or '@')
  const char *pass_end = pass;
  if (*user_end == ':')
  {
    pass_end = strchr(pass, '@');
    if (pass_end == 0)
      return 0;       // There should be a password, but '@' was not found
  }
  assert(pass_end - pass >= 0);               // Password can be empty

  /* Find the host name, which is mandatory */
  // Skip the '@', if there is one
  const char *host = *pass_end == '@' ? pass_end + 1 : pass_end;
  const char *host_end = strchr(host, ':');
  if (host == host_end)
    return 0;                                 // No hostname was found
  /* If no ':' was found there is no port, so the host end at the end
   * of the string */
  if (host_end == 0)
    host_end = body + len;
  assert(host_end - host >= 1);              // There has to be a host

  /* Find the port number */
  uint portno = 3306;
  if (*host_end == ':')
    portno = strtoul(host_end + 1, NULL, 10);

  /* Host name is now the string [host, port-1) if port != NULL and
     [host, EOS) otherwise.
  */
  /* Port number is stored in portno, either the default, or a parsed one */
  return new Binlog_tcp_driver(std::string(user, user_end - user),
                               std::string(pass, pass_end - pass),
                               std::string(host, host_end - host),
                               portno);
}


static Binary_log_driver *parse_file_url(const char *body, size_t length)
{
  /* Find the beginning of the file name */
  if (strncmp(body, "//", 2) != 0)
    return 0;

  /*
    Since we don't support host information yet, there should be a
    slash after the initial "//".
   */
  if (body[2] != '/')
    return 0;

  return new Binlog_file_driver(body + 2);
}

/**
   URI parser information.
 */
struct Parser {
  const char* protocol;
  Binary_log_driver *(*parser)(const char *body, size_t length);
};

/**
   Array of schema names and matching parsers.
*/
static Parser url_parser[] = {
  { "mysql", parse_mysql_url },
  { "file",  parse_file_url },
};

Binary_log_driver *
mysql::system::create_transport(const char *url)
{
  const char *pfx = strchr(url, ':');
  if (pfx == 0)
    return NULL;
  for (unsigned int i = 0 ; i < sizeof(url_parser)/sizeof(*url_parser) ; ++i)
  {
    const char *proto = url_parser[i].protocol;
    if (strncmp(proto, url, strlen(proto)) == 0)
      return (*url_parser[i].parser)(pfx+1, strlen(pfx+1));
  }
  return NULL;
}
