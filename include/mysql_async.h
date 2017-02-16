/* Copyright (C) 2012 MariaDB Services and Kristian Nielsen

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

/* Common definitions for MariaDB non-blocking client library. */

#ifndef MYSQL_ASYNC_H
#define MYSQL_ASYNC_H

extern int my_connect_async(struct mysql_async_context *b, my_socket fd,
                            const struct sockaddr *name, uint namelen,
                            uint timeout);
extern ssize_t my_recv_async(struct mysql_async_context *b, int fd,
                             unsigned char *buf, size_t size, uint timeout);
extern ssize_t my_send_async(struct mysql_async_context *b, int fd,
                             const unsigned char *buf, size_t size,
                             uint timeout);
extern my_bool my_poll_read_async(struct mysql_async_context *b,
                                  uint timeout);
#ifdef HAVE_OPENSSL
extern int my_ssl_read_async(struct mysql_async_context *b, SSL *ssl,
                             void *buf, int size);
extern int my_ssl_write_async(struct mysql_async_context *b, SSL *ssl,
                              const void *buf, int size);
#endif

#endif  /* MYSQL_ASYNC_H */
