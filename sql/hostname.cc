/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  Get hostname for an IP address.

  Hostnames are checked with reverse name lookup and checked that they
  doesn't resemble an IP address.
*/

#include "sql_priv.h"
#include "hostname.h"
#include "my_global.h"
#ifndef __WIN__
#include <netdb.h>        // getservbyname, servent
#endif
#include "hash_filo.h"
#include <m_ctype.h>
#include "log.h"                                // sql_print_warning,
                                                // sql_print_information
#include "violite.h"                            // vio_getnameinfo,
                                                // vio_get_normalized_ip_string
#ifdef	__cplusplus
extern "C" {					// Because of SCO 3.2V4.2
#endif
#if !defined( __WIN__)
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <sys/utsname.h>
#endif // __WIN__
#ifdef	__cplusplus
}
#endif

/*
  HOST_ENTRY_KEY_SIZE -- size of IP address string in the hash cache.
*/

#define HOST_ENTRY_KEY_SIZE INET6_ADDRSTRLEN

/**
  An entry in the hostname hash table cache.

  Host name cache does two things:
    - caches host names to save DNS look ups;
    - counts connect errors from IP.

  Host name can be NULL (that means DNS look up failed), but connect errors
  still are counted.
*/

class Host_entry :public hash_filo_element
{
public:
  /**
    Client IP address. This is the key used with the hash table.

    The client IP address is always expressed in IPv6, even when the
    network IPv6 stack is not present.

    This IP address is never used to connect to a socket.
  */
  char ip_key[HOST_ENTRY_KEY_SIZE];

  /**
    Number of errors during handshake phase from the IP address.
  */
  uint connect_errors;

  /**
    One of the host names for the IP address. May be NULL.
  */
  const char *hostname;
};

static hash_filo *hostname_cache;

void hostname_cache_refresh()
{
  hostname_cache->clear();
}

bool hostname_cache_init()
{
  Host_entry tmp;
  uint key_offset= (uint) ((char*) (&tmp.ip_key) - (char*) &tmp);

  if (!(hostname_cache= new hash_filo(HOST_CACHE_SIZE,
                                      key_offset, HOST_ENTRY_KEY_SIZE,
                                      NULL, (my_hash_free_key) free,
                                      &my_charset_bin)))
    return 1;

  hostname_cache->clear();

  return 0;
}

void hostname_cache_free()
{
  delete hostname_cache;
  hostname_cache= NULL;
}

static void prepare_hostname_cache_key(const char *ip_string,
                                       char *ip_key)
{
  int ip_string_length= strlen(ip_string);
  DBUG_ASSERT(ip_string_length < HOST_ENTRY_KEY_SIZE);

  memset(ip_key, 0, HOST_ENTRY_KEY_SIZE);
  memcpy(ip_key, ip_string, ip_string_length);
}

static inline Host_entry *hostname_cache_search(const char *ip_key)
{
  return (Host_entry *) hostname_cache->search((uchar *) ip_key, 0);
}

static bool add_hostname_impl(const char *ip_key, const char *hostname)
{
  if (hostname_cache_search(ip_key))
    return FALSE;

  size_t hostname_size= hostname ? strlen(hostname) + 1 : 0;

  Host_entry *entry= (Host_entry *) malloc(sizeof (Host_entry) + hostname_size);

  if (!entry)
    return TRUE;

  char *hostname_copy;

  memcpy(&entry->ip_key, ip_key, HOST_ENTRY_KEY_SIZE);

  if (hostname_size)
  {
    hostname_copy= (char *) (entry + 1);
    memcpy(hostname_copy, hostname, hostname_size);

    DBUG_PRINT("info", ("Adding '%s' -> '%s' to the hostname cache...'",
                        (const char *) ip_key,
                        (const char *) hostname_copy));
  }
  else
  {
    hostname_copy= NULL;

    DBUG_PRINT("info", ("Adding '%s' -> NULL to the hostname cache...'",
                        (const char *) ip_key));
  }

  entry->hostname= hostname_copy;
  entry->connect_errors= 0;

  return hostname_cache->add(entry);
}

static bool add_hostname(const char *ip_key, const char *hostname)
{
  if (specialflag & SPECIAL_NO_HOST_CACHE)
    return FALSE;

  mysql_mutex_lock(&hostname_cache->lock);

  bool err_status= add_hostname_impl(ip_key, hostname);

  mysql_mutex_unlock(&hostname_cache->lock);

  return err_status;
}

void inc_host_errors(const char *ip_string)
{
  if (!ip_string)
    return;

  char ip_key[HOST_ENTRY_KEY_SIZE];
  prepare_hostname_cache_key(ip_string, ip_key);

  mysql_mutex_lock(&hostname_cache->lock);

  Host_entry *entry= hostname_cache_search(ip_key);

  if (entry)
    entry->connect_errors++;

  mysql_mutex_unlock(&hostname_cache->lock);
}


void reset_host_errors(const char *ip_string)
{
  if (!ip_string)
    return;

  char ip_key[HOST_ENTRY_KEY_SIZE];
  prepare_hostname_cache_key(ip_string, ip_key);

  mysql_mutex_lock(&hostname_cache->lock);

  Host_entry *entry= hostname_cache_search(ip_key);

  if (entry)
    entry->connect_errors= 0;

  mysql_mutex_unlock(&hostname_cache->lock);
}


static inline bool is_ip_loopback(const struct sockaddr *ip)
{
  switch (ip->sa_family) {
  case AF_INET:
    {
      /* Check for IPv4 127.0.0.1. */
      struct in_addr *ip4= &((struct sockaddr_in *) ip)->sin_addr;
      return ntohl(ip4->s_addr) == INADDR_LOOPBACK;
    }

#ifdef HAVE_IPV6
  case AF_INET6:
    {
      /* Check for IPv6 ::1. */
      struct in6_addr *ip6= &((struct sockaddr_in6 *) ip)->sin6_addr;
      return IN6_IS_ADDR_LOOPBACK(ip6);
    }
#endif /* HAVE_IPV6 */

  default:
    return FALSE;
  }
}

static inline bool is_hostname_valid(const char *hostname)
{
  /*
    A hostname is invalid if it starts with a number followed by a dot
    (IPv4 address).
  */

  if (!my_isdigit(&my_charset_latin1, hostname[0]))
    return TRUE;

  const char *p= hostname + 1;

  while (my_isdigit(&my_charset_latin1, *p))
    ++p;

  return *p != '.';
}

/**
  Resolve IP-address to host name.

  This function does the following things:
    - resolves IP-address;
    - employs Forward Confirmed Reverse DNS technique to validate IP-address;
    - returns host name if IP-address is validated;
    - set value to out-variable connect_errors -- this variable represents the
      number of connection errors from the specified IP-address.

  NOTE: connect_errors are counted (are supported) only for the clients
  where IP-address can be resolved and FCrDNS check is passed.

  @param [in]  ip_storage IP address (sockaddr). Must be set.
  @param [in]  ip_string  IP address (string). Must be set.
  @param [out] hostname
  @param [out] connect_errors

  @return Error status
  @retval FALSE Success
  @retval TRUE Error

  The function does not set/report MySQL server error in case of failure.
  It's caller's responsibility to handle failures of this function
  properly.
*/

bool ip_to_hostname(struct sockaddr_storage *ip_storage,
                    const char *ip_string,
                    char **hostname, uint *connect_errors)
{
  const struct sockaddr *ip= (const sockaddr *) ip_storage;
  int err_code;
  bool err_status;

  DBUG_ENTER("ip_to_hostname");
  DBUG_PRINT("info", ("IP address: '%s'; family: %d.",
                      (const char *) ip_string,
                      (int) ip->sa_family));

  /* Check if we have loopback address (127.0.0.1 or ::1). */

  if (is_ip_loopback(ip))
  {
    DBUG_PRINT("info", ("Loopback address detected."));

    *connect_errors= 0; /* Do not count connect errors from localhost. */
    *hostname= (char *) my_localhost;

    DBUG_RETURN(FALSE);
  }

  /* Prepare host name cache key. */

  char ip_key[HOST_ENTRY_KEY_SIZE];
  prepare_hostname_cache_key(ip_string, ip_key);

  /* Check first if we have host name in the cache. */

  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    mysql_mutex_lock(&hostname_cache->lock);

    Host_entry *entry= hostname_cache_search(ip_key);

    if (entry)
    {
      *connect_errors= entry->connect_errors;
      *hostname= NULL;

      if (entry->hostname)
        *hostname= my_strdup(entry->hostname, MYF(0));

      DBUG_PRINT("info",("IP (%s) has been found in the cache. "
                         "Hostname: '%s'; connect_errors: %d",
                         (const char *) ip_key,
                         (const char *) (*hostname? *hostname : "null"),
                         (int) *connect_errors));

      mysql_mutex_unlock(&hostname_cache->lock);

      DBUG_RETURN(FALSE);
    }

    mysql_mutex_unlock(&hostname_cache->lock);
  }

  /*
    Resolve host name. Return an error if a host name can not be resolved
    (instead of returning the numeric form of the host name).
  */

  char hostname_buffer[NI_MAXHOST];

  DBUG_PRINT("info", ("Resolving '%s'...", (const char *) ip_key));

  err_code= vio_getnameinfo(ip, hostname_buffer, NI_MAXHOST, NULL, 0,
                            NI_NAMEREQD);

  if (err_code)
  {
    // NOTE: gai_strerror() returns a string ending by a dot.

    DBUG_PRINT("error", ("IP address '%s' could not be resolved: %s",
                         (const char *) ip_key,
                         (const char *) gai_strerror(err_code)));

    sql_print_warning("IP address '%s' could not be resolved: %s",
                      (const char *) ip_key,
                      (const char *) gai_strerror(err_code));

    if (vio_is_no_name_error(err_code))
    {
      /*
        The no-name error means that there is no reverse address mapping
        for the IP address. A host name can not be resolved.

        If it is not the no-name error, we should not cache the hostname
        (or rather its absence), because the failure might be transient.
      */

      add_hostname(ip_key, NULL);

      *hostname= NULL;
      *connect_errors= 0; /* New IP added to the cache. */
    }

    DBUG_RETURN(FALSE);
  }

  DBUG_PRINT("info", ("IP '%s' resolved to '%s'.",
                      (const char *) ip_key,
                      (const char *) hostname_buffer));

  /*
    Validate hostname: the server does not accept host names, which
    resemble IP addresses.

    The thing is that theoretically, a host name can be in a form of IPv4
    address (123.example.org, or 1.2 or even 1.2.3.4). We have to deny such
    host names because ACL-systems is not designed to work with them.

    For example, it is possible to specify a host name mask (like
    192.168.1.%) for an ACL rule. Then, if IPv4-like hostnames are allowed,
    there is a security hole: instead of allowing access for
    192.168.1.0/255 network (which was assumed by the user), the access
    will be allowed for host names like 192.168.1.example.org.
  */

  if (!is_hostname_valid(hostname_buffer))
  {
    DBUG_PRINT("error", ("IP address '%s' has been resolved "
                         "to the host name '%s', which resembles "
                         "IPv4-address itself.",
                         (const char *) ip_key,
                         (const char *) hostname_buffer));

    sql_print_warning("IP address '%s' has been resolved "
                      "to the host name '%s', which resembles "
                      "IPv4-address itself.",
                      (const char *) ip_key,
                      (const char *) hostname_buffer);

    err_status= add_hostname(ip_key, NULL);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(err_status);
  }

  /* Get IP-addresses for the resolved host name (FCrDNS technique). */

  struct addrinfo hints;
  struct addrinfo *addr_info_list;

  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;
  hints.ai_family= AF_UNSPEC;

  DBUG_PRINT("info", ("Getting IP addresses for hostname '%s'...",
                      (const char *) hostname_buffer));

  err_code= getaddrinfo(hostname_buffer, NULL, &hints, &addr_info_list);

  if (err_code == EAI_NONAME)
  {
    /*
      Don't cache responses when the DNS server is down, as otherwise
      transient DNS failure may leave any number of clients (those
      that attempted to connect during the outage) unable to connect
      indefinitely.
    */

    err_status= add_hostname(ip_key, NULL);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(err_status);
  }
  else if (err_code)
  {
    DBUG_PRINT("error", ("getaddrinfo() failed with error code %d.", err_code));
    DBUG_RETURN(TRUE);
  }

  /* Check that getaddrinfo() returned the used IP (FCrDNS technique). */

  DBUG_PRINT("info", ("The following IP addresses found for '%s':",
                      (const char *) hostname_buffer));

  for (struct addrinfo *addr_info= addr_info_list;
       addr_info; addr_info= addr_info->ai_next)
  {
    char ip_buffer[HOST_ENTRY_KEY_SIZE];

    {
      err_status=
        vio_get_normalized_ip_string(addr_info->ai_addr, addr_info->ai_addrlen,
                                     ip_buffer, sizeof (ip_buffer));
      DBUG_ASSERT(!err_status);
    }

    DBUG_PRINT("info", ("  - '%s'", (const char *) ip_buffer));

    if (strcmp(ip_key, ip_buffer) == 0)
    {
      /* Copy host name string to be stored in the cache. */

      *hostname= my_strdup(hostname_buffer, MYF(0));

      if (!*hostname)
      {
        DBUG_PRINT("error", ("Out of memory."));

        freeaddrinfo(addr_info_list);
        DBUG_RETURN(TRUE);
      }

      break;
    }
  }

  /* Log resolved IP-addresses if no match was found. */

  if (!*hostname)
  {
    sql_print_information("Hostname '%s' does not resolve to '%s'.",
                          (const char *) hostname_buffer,
                          (const char *) ip_key);
    sql_print_information("Hostname '%s' has the following IP addresses:",
                          (const char *) hostname_buffer);

    for (struct addrinfo *addr_info= addr_info_list;
         addr_info; addr_info= addr_info->ai_next)
    {
      char ip_buffer[HOST_ENTRY_KEY_SIZE];

      err_status=
        vio_get_normalized_ip_string(addr_info->ai_addr, addr_info->ai_addrlen,
                                     ip_buffer, sizeof (ip_buffer));
      DBUG_ASSERT(!err_status);

      sql_print_information(" - %s\n", (const char *) ip_buffer);
    }
  }

  /* Free the result of getaddrinfo(). */

  freeaddrinfo(addr_info_list);

  /* Add an entry for the IP to the cache. */

  if (*hostname)
  {
    err_status= add_hostname(ip_key, *hostname);
    *connect_errors= 0;
  }
  else
  {
    DBUG_PRINT("error",("Couldn't verify hostname with getaddrinfo()."));

    err_status= add_hostname(ip_key, NULL);
    *hostname= NULL;
    *connect_errors= 0;
  }

  DBUG_RETURN(err_status);
}
