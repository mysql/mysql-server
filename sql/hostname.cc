/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/**
  @file

  @brief
  Get hostname for an IP address.

  Hostnames are checked with reverse name lookup and checked that they
  doesn't resemble an IP address.
*/

#include "my_global.h"
#include "sql_priv.h"
#include "hostname.h"
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

Host_errors::Host_errors()
: m_nameinfo_transient_errors(0),
  m_nameinfo_permanent_errors(0),
  m_format_errors(0),
  m_addrinfo_transient_errors(0),
  m_addrinfo_permanent_errors(0),
  m_FCrDNS_errors(0),
  m_host_acl_errors(0),
  m_handshake_errors(0),
  m_authentication_errors(0),
  m_user_acl_errors(0),
  m_local_errors(0),
  m_unknown_errors(0)
{}
    
Host_errors::~Host_errors()
{}

void Host_errors::reset()
{
  m_nameinfo_transient_errors= 0;
  m_nameinfo_permanent_errors= 0;
  m_format_errors= 0;
  m_addrinfo_transient_errors= 0;
  m_addrinfo_permanent_errors= 0;
  m_FCrDNS_errors= 0;
  m_host_acl_errors= 0;
  m_handshake_errors= 0;
  m_authentication_errors= 0;
  m_user_acl_errors= 0;
  m_local_errors= 0;
  m_unknown_errors= 0;
}

uint Host_errors::get_blocking_errors()
{
  uint blocking= 0;
  blocking+= m_host_acl_errors;
  blocking+= m_authentication_errors;
  blocking+= m_user_acl_errors;
  return blocking;
}

void Host_errors::aggregate(const Host_errors *errors)
{
  m_nameinfo_transient_errors+= errors->m_nameinfo_transient_errors;
  m_nameinfo_permanent_errors+= errors->m_nameinfo_permanent_errors;
  m_format_errors+= errors->m_format_errors;
  m_addrinfo_transient_errors+= errors->m_addrinfo_transient_errors;
  m_addrinfo_permanent_errors+= errors->m_addrinfo_permanent_errors;
  m_FCrDNS_errors+= errors->m_FCrDNS_errors;
  m_host_acl_errors+= errors->m_host_acl_errors;
  m_handshake_errors+= errors->m_handshake_errors;
  m_authentication_errors+= errors->m_authentication_errors;
  m_user_acl_errors+= errors->m_user_acl_errors;
  m_local_errors+= errors->m_local_errors;
  m_unknown_errors+= errors->m_unknown_errors;
}

static hash_filo *hostname_cache;
ulong host_cache_size;

void hostname_cache_refresh()
{
  hostname_cache->clear();
}

uint hostname_cache_size()
{
  return hostname_cache->size();
}

void hostname_cache_resize(uint size)
{
  hostname_cache->resize(size);
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

void hostname_cache_lock()
{
  mysql_mutex_lock(&hostname_cache->lock);
}

void hostname_cache_unlock()
{
  mysql_mutex_unlock(&hostname_cache->lock);
}

static void prepare_hostname_cache_key(const char *ip_string,
                                       char *ip_key)
{
  int ip_string_length= strlen(ip_string);
  DBUG_ASSERT(ip_string_length < HOST_ENTRY_KEY_SIZE);

  memset(ip_key, 0, HOST_ENTRY_KEY_SIZE);
  memcpy(ip_key, ip_string, ip_string_length);
}

Host_entry *hostname_cache_first()
{ return (Host_entry *) hostname_cache->first(); }

static inline Host_entry *hostname_cache_search(const char *ip_key)
{
  return (Host_entry *) hostname_cache->search((uchar *) ip_key, 0);
}

static bool add_hostname_impl(const char *ip_key, const char *hostname,
                              bool validated, Host_errors *errors)
{
  Host_entry *entry;
  bool need_add= false;

  entry= hostname_cache_search(ip_key);

  if (likely(entry == NULL))
  {
    entry= (Host_entry *) malloc(sizeof (Host_entry));
    if (entry == NULL)
      return true;

    need_add= true;
    memcpy(&entry->ip_key, ip_key, HOST_ENTRY_KEY_SIZE);
    entry->m_errors.reset();
    entry->m_hostname_length= 0;
    entry->m_host_validated= false;
  }

  if (validated)
  {
    if (hostname != NULL)
    {
      uint len= strlen(hostname);
      if (len > sizeof(entry->m_hostname) - 1)
        len= sizeof(entry->m_hostname) - 1;
      memcpy(entry->m_hostname, hostname, len);
      entry->m_hostname[len]= '\0';
      entry->m_hostname_length= len;

      DBUG_PRINT("info",
                 ("Adding/Updating '%s' -> '%s' (validated) to the hostname cache...'",
                 (const char *) ip_key,
                 (const char *) entry->m_hostname));
    }
    else
    {
      entry->m_hostname_length= 0;
      DBUG_PRINT("info",
                 ("Adding/Updating '%s' -> NULL (validated) to the hostname cache...'",
                 (const char *) ip_key));
    }
    entry->m_host_validated= true;
  }
  else
  {
    entry->m_hostname_length= 0;
    /* There are currently no use cases that invalidate an entry. */
    DBUG_ASSERT(! entry->m_host_validated);
    entry->m_host_validated= false;
    DBUG_PRINT("info",
               ("Adding/Updating '%s' -> NULL (not validated) to the hostname cache...'",
               (const char *) ip_key));
  }

  entry->m_errors.aggregate(errors);

  if (need_add)
    hostname_cache->add(entry);

  return false;
}

static bool add_hostname(const char *ip_key, const char *hostname,
                         bool validated, Host_errors *errors)
{
  if (specialflag & SPECIAL_NO_HOST_CACHE)
    return FALSE;

  mysql_mutex_lock(&hostname_cache->lock);

  bool err_status= add_hostname_impl(ip_key, hostname, validated, errors);

  mysql_mutex_unlock(&hostname_cache->lock);

  return err_status;
}

void inc_host_errors(const char *ip_string, const Host_errors *errors)
{
  if (!ip_string)
    return;

  char ip_key[HOST_ENTRY_KEY_SIZE];
  prepare_hostname_cache_key(ip_string, ip_key);

  mysql_mutex_lock(&hostname_cache->lock);

  Host_entry *entry= hostname_cache_search(ip_key);

  if (entry)
    entry->m_errors.aggregate(errors);

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
    entry->m_errors.reset();

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
  Host_errors errors;

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
      /*
        If there is an IP -> HOSTNAME association in the cache,
        but for a hostname that was not validated,
        do not return that hostname: perform the network validation again.
      */
      if (entry->m_host_validated)
      {
        *connect_errors= entry->m_errors.get_blocking_errors();
        *hostname= NULL;

        if (entry->m_hostname_length)
          *hostname= my_strdup(entry->m_hostname, MYF(0));

        DBUG_PRINT("info",("IP (%s) has been found in the cache. "
                           "Hostname: '%s'; connect_errors: %d",
                           (const char *) ip_key,
                           (const char *) (*hostname? *hostname : "null"),
                           (int) *connect_errors));

        mysql_mutex_unlock(&hostname_cache->lock);

        DBUG_RETURN(FALSE);
      }
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

  DBUG_EXECUTE_IF("getnameinfo_error_noname",
                  {
                    strcpy(hostname_buffer, "<garbage>");
                    err_code= EAI_NONAME;
                  }
                  );

  DBUG_EXECUTE_IF("getnameinfo_error_again",
                  {
                    strcpy(hostname_buffer, "<garbage>");
                    err_code= EAI_AGAIN;
                  }
                  );

  DBUG_EXECUTE_IF("getnameinfo_fake_ipv4",
                  {
                    strcpy(hostname_buffer, "santa.claus.ipv4.example.com");
                    err_code= 0;
                  }
                  );

  DBUG_EXECUTE_IF("getnameinfo_fake_ipv6",
                  {
                    strcpy(hostname_buffer, "santa.claus.ipv6.example.com");
                    err_code= 0;
                  }
                  );

  DBUG_EXECUTE_IF("getnameinfo_format_ipv4",
                  {
                    strcpy(hostname_buffer, "12.12.12.12");
                    err_code= 0;
                  }
                  );

  DBUG_EXECUTE_IF("getnameinfo_format_ipv6",
                  {
                    strcpy(hostname_buffer, "12:DEAD:BEEF:0");
                    err_code= 0;
                  }
                  );

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

      add_hostname(ip_key, NULL, false, &errors);

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

    errors.m_format_errors= 1;
    err_status= add_hostname(ip_key, hostname_buffer, false, &errors);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(err_status);
  }

  /* Get IP-addresses for the resolved host name (FCrDNS technique). */

  struct addrinfo hints;
  struct addrinfo *addr_info_list;
  /*
    Makes fault injection with DBUG_EXECUTE_IF easier.
    Invoking free_addr_info(NULL) crashes on some platforms.
  */
  bool free_addr_info_list= false;

  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;
  hints.ai_family= AF_UNSPEC;

  DBUG_PRINT("info", ("Getting IP addresses for hostname '%s'...",
                      (const char *) hostname_buffer));

  err_code= getaddrinfo(hostname_buffer, NULL, &hints, &addr_info_list);
  if (err_code == 0)
    free_addr_info_list= true;

  /*
  ===========================================================================
                             DBUG CODE (begin)
  ===========================================================================
  */
  DBUG_EXECUTE_IF("getaddrinfo_error_noname",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    addr_info_list= NULL;
                    err_code= EAI_NONAME;
                    free_addr_info_list= false;
                  }
                  );

  DBUG_EXECUTE_IF("getaddrinfo_error_again",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    addr_info_list= NULL;
                    err_code= EAI_AGAIN;
                    free_addr_info_list= false;
                  }
                  );

  DBUG_EXECUTE_IF("getaddrinfo_fake_bad_ipv4",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    struct sockaddr_in *debug_addr;
                    /*
                      Not thread safe, which is ok.
                      Only one connection at a time is tested with
                      fault injection.
                    */
                    static struct sockaddr_in debug_sock_addr[2];
                    static struct addrinfo debug_addr_info[2];
                    /* Simulating ipv4 192.0.2.126 */
                    debug_addr= & debug_sock_addr[0];
                    debug_addr->sin_family= AF_INET;
                    inet_pton(AF_INET, "192.0.2.126", & debug_addr->sin_addr);

                    /* Simulating ipv4 192.0.2.127 */
                    debug_addr= & debug_sock_addr[1];
                    debug_addr->sin_family= AF_INET;
                    inet_pton(AF_INET, "192.0.2.127", & debug_addr->sin_addr);

                    debug_addr_info[0].ai_addr= (struct sockaddr*) & debug_sock_addr[0];
                    debug_addr_info[0].ai_addrlen= sizeof (struct sockaddr_in);
                    debug_addr_info[0].ai_next= & debug_addr_info[1];

                    debug_addr_info[1].ai_addr= (struct sockaddr*) & debug_sock_addr[1];
                    debug_addr_info[1].ai_addrlen= sizeof (struct sockaddr_in);
                    debug_addr_info[1].ai_next= NULL;

                    addr_info_list= & debug_addr_info[0];
                    err_code= 0;
                    free_addr_info_list= false;
                  }
                  );

  DBUG_EXECUTE_IF("getaddrinfo_fake_good_ipv4",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    struct sockaddr_in *debug_addr;
                    static struct sockaddr_in debug_sock_addr[2];
                    static struct addrinfo debug_addr_info[2];
                    /* Simulating ipv4 192.0.2.5 */
                    debug_addr= & debug_sock_addr[0];
                    debug_addr->sin_family= AF_INET;
                    inet_pton(AF_INET, "192.0.2.5", & debug_addr->sin_addr);

                    /* Simulating ipv4 192.0.2.4 */
                    debug_addr= & debug_sock_addr[1];
                    debug_addr->sin_family= AF_INET;
                    inet_pton(AF_INET, "192.0.2.4", & debug_addr->sin_addr);

                    debug_addr_info[0].ai_addr= (struct sockaddr*) & debug_sock_addr[0];
                    debug_addr_info[0].ai_addrlen= sizeof (struct sockaddr_in);
                    debug_addr_info[0].ai_next= & debug_addr_info[1];

                    debug_addr_info[1].ai_addr= (struct sockaddr*) & debug_sock_addr[1];
                    debug_addr_info[1].ai_addrlen= sizeof (struct sockaddr_in);
                    debug_addr_info[1].ai_next= NULL;

                    addr_info_list= & debug_addr_info[0];
                    err_code= 0;
                    free_addr_info_list= false;
                  }
                  );

  DBUG_EXECUTE_IF("getaddrinfo_fake_bad_ipv6",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    struct sockaddr_in6 *debug_addr;
                    /*
                      Not thread safe, which is ok.
                      Only one connection at a time is tested with
                      fault injection.
                    */
                    static struct sockaddr_in6 debug_sock_addr[2];
                    static struct addrinfo debug_addr_info[2];
                    /* Simulating ipv6 2001:DB8::6:7E */
                    debug_addr= & debug_sock_addr[0];
                    debug_addr->sin6_family= AF_INET6;
                    inet_pton(AF_INET6, "2001:DB8::6:7E", & debug_addr->sin6_addr);

                    /* Simulating ipv6 2001:DB8::6:7F */
                    debug_addr= & debug_sock_addr[1];
                    debug_addr->sin6_family= AF_INET6;
                    inet_pton(AF_INET6, "2001:DB8::6:7F", & debug_addr->sin6_addr);

                    debug_addr_info[0].ai_addr= (struct sockaddr*) & debug_sock_addr[0];
                    debug_addr_info[0].ai_addrlen= sizeof (struct sockaddr_in6);
                    debug_addr_info[0].ai_next= & debug_addr_info[1];

                    debug_addr_info[1].ai_addr= (struct sockaddr*) & debug_sock_addr[1];
                    debug_addr_info[1].ai_addrlen= sizeof (struct sockaddr_in6);
                    debug_addr_info[1].ai_next= NULL;

                    addr_info_list= & debug_addr_info[0];
                    err_code= 0;
                    free_addr_info_list= false;
                  }
                  );

  DBUG_EXECUTE_IF("getaddrinfo_fake_good_ipv6",
                  {
                    if (free_addr_info_list)
                      freeaddrinfo(addr_info_list);

                    struct sockaddr_in6 *debug_addr;
                    /*
                      Not thread safe, which is ok.
                      Only one connection at a time is tested with
                      fault injection.
                    */
                    static struct sockaddr_in6 debug_sock_addr[2];
                    static struct addrinfo debug_addr_info[2];
                    /* Simulating ipv6 2001:DB8::6:7 */
                    debug_addr= & debug_sock_addr[0];
                    debug_addr->sin6_family= AF_INET6;
                    inet_pton(AF_INET6, "2001:DB8::6:7", & debug_addr->sin6_addr);

                    /* Simulating ipv6 2001:DB8::6:6 */
                    debug_addr= & debug_sock_addr[1];
                    debug_addr->sin6_family= AF_INET6;
                    inet_pton(AF_INET6, "2001:DB8::6:6", & debug_addr->sin6_addr);

                    debug_addr_info[0].ai_addr= (struct sockaddr*) & debug_sock_addr[0];
                    debug_addr_info[0].ai_addrlen= sizeof (struct sockaddr_in6);
                    debug_addr_info[0].ai_next= & debug_addr_info[1];

                    debug_addr_info[1].ai_addr= (struct sockaddr*) & debug_sock_addr[1];
                    debug_addr_info[1].ai_addrlen= sizeof (struct sockaddr_in6);
                    debug_addr_info[1].ai_next= NULL;

                    addr_info_list= & debug_addr_info[0];
                    err_code= 0;
                    free_addr_info_list= false;
                  }
                  );

  /*
  ===========================================================================
                             DBUG CODE (end)
  ===========================================================================
  */

  if (err_code == EAI_NONAME)
  {
    errors.m_addrinfo_permanent_errors= 1;
    err_status= add_hostname(ip_key, NULL, true, &errors);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(FALSE);
  }

  if (err_code)
  {
    // NOTE: gai_strerror() returns a string ending by a dot.

    DBUG_PRINT("error", ("Host name '%s' could not be resolved: %s",
                         hostname_buffer,
                         (const char *) gai_strerror(err_code)));

    sql_print_warning("Host name '%s' could not be resolved: %s",
                      hostname_buffer,
                      (const char *) gai_strerror(err_code));

    /*
      Don't cache responses when the DNS server is down, as otherwise
      transient DNS failure may leave any number of clients (those
      that attempted to connect during the outage) unable to connect
      indefinitely.
    */

    errors.m_addrinfo_transient_errors= 1;
    err_status= add_hostname(ip_key, NULL, false, &errors);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(FALSE);
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

    if (strcasecmp(ip_key, ip_buffer) == 0)
    {
      /* Copy host name string to be stored in the cache. */

      *hostname= my_strdup(hostname_buffer, MYF(0));

      if (!*hostname)
      {
        DBUG_PRINT("error", ("Out of memory."));

        if (free_addr_info_list)
          freeaddrinfo(addr_info_list);
        DBUG_RETURN(TRUE);
      }

      break;
    }
  }

  /* Log resolved IP-addresses if no match was found. */

  if (!*hostname)
  {
    sql_print_warning("Hostname '%s' does not resolve to '%s'.",
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

      sql_print_information(" - %s", (const char *) ip_buffer);
    }
  }

  /* Free the result of getaddrinfo(). */

  if (free_addr_info_list)
    freeaddrinfo(addr_info_list);

  /* Add an entry for the IP to the cache. */

  if (*hostname)
  {
    err_status= add_hostname(ip_key, *hostname, true, &errors);
    *connect_errors= 0;
  }
  else
  {
    DBUG_PRINT("error",("Couldn't verify hostname with getaddrinfo()."));

    errors.m_FCrDNS_errors= 1;
    err_status= add_hostname(ip_key, NULL, false, &errors);
    *hostname= NULL;
    *connect_errors= 0;
  }

  DBUG_RETURN(err_status);
}
