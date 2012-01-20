/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef HOSTNAME_INCLUDED
#define HOSTNAME_INCLUDED

#include "my_global.h"                          /* uint */
#include "hash_filo.h"

#ifndef __WIN__
#include <netdb.h> /* INET6_ADDRSTRLEN */
#endif

struct Host_errors
{
public:
  Host_errors();
  ~Host_errors();

  void reset();
  void aggregate(const Host_errors *errors);

  /** Number of blocking errors. */
  uint m_blocking_errors;
  /** Number of transient errors from getnameinfo(). */
  uint m_nameinfo_transient_errors;
  /** Number of permanent errors from getnameinfo(). */
  uint m_nameinfo_permanent_errors;
  /** Number of errors from is_hostname_valid(). */
  uint m_format_errors;
  /** Number of transient errors from getaddrinfo(). */
  uint m_addrinfo_transient_errors;
  /** Number of permanent errors from getaddrinfo(). */
  uint m_addrinfo_permanent_errors;
  /** Number of errors from Forward-Confirmed reverse DNS checks. */
  uint m_FCrDNS_errors;
  /** Number of errors from host grants. */
  uint m_host_acl_errors;
  /** Number of errors from authentication plugins. */
  uint m_handshake_errors;
  /** Number of errors from authentication. */
  uint m_authentication_errors;
  /** Number of errors from user grants. */
  uint m_user_acl_errors;
  /** Number of errors from the server itself. */
  uint m_local_errors;
  /** Number of unknown errors. */
  uint m_unknown_errors;

  bool has_error() const
  {
    return ((m_nameinfo_transient_errors != 0)
      || (m_nameinfo_permanent_errors != 0)
      || (m_format_errors != 0)
      || (m_addrinfo_transient_errors != 0)
      || (m_addrinfo_permanent_errors != 0)
      || (m_FCrDNS_errors != 0)
      || (m_host_acl_errors != 0)
      || (m_handshake_errors != 0)
      || (m_authentication_errors != 0)
      || (m_user_acl_errors != 0)
      || (m_local_errors != 0)
      || (m_unknown_errors != 0));
  }
};

/** Size of IP address string in the hash cache. */
#define HOST_ENTRY_KEY_SIZE INET6_ADDRSTRLEN

/**
  An entry in the hostname hash table cache.

  Host name cache does two things:
    - caches host names to save DNS look ups;
    - counts errors from IP.

  Host name can be empty (that means DNS look up failed),
  but errors still are counted.
*/
class Host_entry : public hash_filo_element
{
public:
  Host_entry *next()
  { return (Host_entry*) hash_filo_element::next(); }

  /**
    Client IP address. This is the key used with the hash table.

    The client IP address is always expressed in IPv6, even when the
    network IPv6 stack is not present.

    This IP address is never used to connect to a socket.
  */
  char ip_key[HOST_ENTRY_KEY_SIZE];

  /**
    One of the host names for the IP address. May be a zero length string.
  */
  char m_hostname[HOSTNAME_LENGTH + 1];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length;
  /** The hostname is validated and used for authorization. */
  bool m_host_validated;
  ulonglong m_first_seen;
  ulonglong m_last_seen;
  ulonglong m_first_error_seen;
  ulonglong m_last_error_seen;
  /** Error statistics. */
  Host_errors m_errors;

  void set_error_timestamps(ulonglong now)
  {
    if (m_first_error_seen == 0)
      m_first_error_seen= now;
    m_last_error_seen= now;
  }
};

/** The size of the host_cache. */
extern ulong host_cache_size;

bool ip_to_hostname(struct sockaddr_storage *ip_storage,
                    const char *ip_string,
                    char **hostname, uint *connect_errors);
void inc_host_errors(const char *ip_string, const Host_errors *errors);
void reset_host_errors(const char *ip_string);
bool hostname_cache_init();
void hostname_cache_free();
void hostname_cache_refresh(void);
uint hostname_cache_size();
void hostname_cache_resize(uint size);
void hostname_cache_lock();
void hostname_cache_unlock();
Host_entry *hostname_cache_first();

#endif /* HOSTNAME_INCLUDED */
