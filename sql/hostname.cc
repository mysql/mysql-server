/*
   Copyright (C) 2000-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


/**
  @file

  @brief
  Get hostname for an IP.

  Hostnames are checked with reverse name lookup and checked that they 
  doesn't resemble an IP address.
*/

#include "mysql_priv.h"
#include "hash_filo.h"
#include <m_ctype.h>
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

#ifdef __WIN__
#define HAVE_STRUCT_IN6_ADDR
#endif /* __WIN__ */

/*
  HOST_ENTRY_KEY_SIZE -- size of IP address string in the hash cache. The
  system constant NI_MAXHOST could be used here. However, it means at least
  1024 bytes per IP, which seems to be quite big.

  Since IP address string is created by our function get_ip_string(), we
  can reduce the space.  get_ip_string() returns a hexadecimal
  respresentation (1111:2222:...:8888) in case of IPv6 and the standard
  representation (111.222.333.444) in case of IPv4, which means 39 bytes at
  most. So, we need 40 bytes for storing IP address string including
  trailing zero.
*/

#define HOST_ENTRY_KEY_SIZE 40

/************************************************************************/

/*
  When this code was written there were issues with winsock in pusbuild,
  this constant is in this place for this reason.
*/

#ifdef EAI_NODATA
    const int MY_NONAME_ERR_CODE= EAI_NODATA;
#else
    const int MY_NONAME_ERR_CODE= EAI_NONAME;
#endif

/************************************************************************/

/**
  Get the string representation for IP address. IPv6 and IPv4 addresses are
  supported. The function is needed because getnameinfo() is known to be
  buggy in some circumstances. Actually, this is a basic replacement for
  getnameinfo() called with the NI_NUMERICHOST flag. Only the hostname part is
  dumped (the port part is omitted).
*/

static void get_ip_string(const struct sockaddr *ip,
                          char *ip_str, int ip_str_size)
{
  switch (ip->sa_family) {
  case AF_INET:
  {
    struct in_addr *ip4= &((struct sockaddr_in *) ip)->sin_addr;
    uint32 ip4_int32= ntohl(ip4->s_addr);
    uint8 *ip4_int8= (uint8 *) &ip4_int32;

    int n= my_snprintf(ip_str, ip_str_size, "%d.%d.%d.%d",
                       ip4_int8[0], ip4_int8[1], ip4_int8[2], ip4_int8[3]);

    DBUG_ASSERT(n < ip_str_size);
    break;
  }

#ifdef HAVE_STRUCT_IN6_ADDR
  case AF_INET6:
  {
    struct in6_addr *ip6= &((struct sockaddr_in6 *) ip)->sin6_addr;
    uint16 *ip6_int16= (uint16 *) ip6->s6_addr;

    int n= my_snprintf(ip_str, ip_str_size,
                       "%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X",
                       ntohs(ip6_int16[0]), ntohs(ip6_int16[1]),
                       ntohs(ip6_int16[2]), ntohs(ip6_int16[3]),
                       ntohs(ip6_int16[4]), ntohs(ip6_int16[5]),
                       ntohs(ip6_int16[6]), ntohs(ip6_int16[7]));

    DBUG_ASSERT(n < ip_str_size);
    break;
  }
#endif /* HAVE_STRUCT_IN6_ADDR */

  default:
    DBUG_ASSERT(FALSE);
  }
}

/**
  Get key value for IP address. Key value is a string representation of
  normalized IP address.

  When IPv4 and IPv6 are both used in the network stack, or in the network
  path between a client and a server, a client can have different apparent
  IP addresses, based on the exact route taken.

  This function normalize the client IP representation, so it's suitable to
  use as a key for searches.

  Transformations are implemented as follows:
  - IPv4 a.b.c.d --> IPv6 mapped IPv4 ::ffff:a.b.c.d
  - IPv6 compat IPv4 ::a.b.c.d --> IPv6 mapped IPv4 ::ffff:a.b.c.d
  - IPv6 --> IPv6

  If IPv6 is not available at compile-time, IPv4 form is used.

  @param [in]   ip      IP address
  @param [out]  ip_key  Key for the given IP value

  @note According to RFC3493 the only specified member of the in6_addr
  structure is s6_addr.

  @note It is possible to call hostname_cache_get_key() with zeroed IP
  address (ip->sa_family == 0). In this case hostname_cache_get_key()
  returns TRUE (error status).

  @return Error status.
  @retval FALSE Success
  @retval TRUE Error
*/

static bool hostname_cache_get_key(const struct sockaddr *ip, char *ip_key)
{
  const struct sockaddr *ip_to_generate_key= ip;

  if (ip->sa_family == 0)
    return TRUE; /* IP address is not set. */

#ifdef HAVE_STRUCT_IN6_ADDR
  /* Prepare normalized IP address. */

  struct sockaddr_storage norm_ip;
  struct in6_addr *norm_ip6= &((sockaddr_in6 *) &norm_ip)->sin6_addr;
  uint32 *norm_ip6_int32= (uint32 *) norm_ip6->s6_addr;

  memset(&norm_ip, 0, sizeof (sockaddr_storage));

  switch (ip->sa_family) {
  case AF_INET:
  {
    struct in_addr *ip4= &((struct sockaddr_in *) ip)->sin_addr;

    norm_ip6_int32[0]= 0;
    norm_ip6_int32[1]= 0;
    norm_ip6_int32[2]= htonl(0xffff);
    norm_ip6_int32[3]= ip4->s_addr; /* in net byte order */

    DBUG_ASSERT(IN6_IS_ADDR_V4MAPPED(norm_ip6));
    break;
  }

  case AF_INET6:
  {
    struct in6_addr *ip6= &((struct sockaddr_in6 *) ip)->sin6_addr;
    uint32 *ip6_int32= (uint32 *) ip6->s6_addr;

    if (IN6_IS_ADDR_V4COMPAT(ip6))
    {
      norm_ip6_int32[0]= 0;
      norm_ip6_int32[1]= 0;
      norm_ip6_int32[2]= htonl(0xffff);
      norm_ip6_int32[3]= ip6_int32[3]; /* in net byte order */
      DBUG_ASSERT(IN6_IS_ADDR_V4MAPPED(norm_ip6));
    }
    else
    {
      /* All in net byte order: just copy 16 bytes. */
      memcpy(norm_ip6_int32, ip6_int32, 4 * sizeof (uint32));
    }

    break;
  }

  default:
    DBUG_ASSERT(FALSE);
    break;
  }

  norm_ip.ss_family= AF_INET6;
  ip_to_generate_key= (sockaddr *) &norm_ip;
#endif /* HAVE_STRUCT_IN6_ADDR */

  /*
    Zero all bytes of the key, because it's not just 0-terminated string.
    All bytes are taken into account during hash search.
  */

  memset(ip_key, 0, HOST_ENTRY_KEY_SIZE);

  /* Get numeric representation of the normalized IP address. */
  get_ip_string(ip_to_generate_key, ip_key, HOST_ENTRY_KEY_SIZE);

  return FALSE;
}


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
  char ip[HOST_ENTRY_KEY_SIZE];

  /**
    Number of errors during handshake phase from the IP address.
  */
  uint connect_errors;

  /**
    One of host names for the IP address. May be NULL.
  */
  const char *hostname;
};

static hash_filo *hostname_cache;
static pthread_mutex_t LOCK_hostname;

void hostname_cache_refresh()
{
  hostname_cache->clear();
}

bool hostname_cache_init()
{
  Host_entry tmp;
  uint key_offset= (uint) ((char*) (&tmp.ip) - (char*) &tmp);

  if (!(hostname_cache= new hash_filo(HOST_CACHE_SIZE,
                                      key_offset, HOST_ENTRY_KEY_SIZE,
                                      NULL, (my_hash_free_key) free,
                                      &my_charset_bin)))
    return 1;

  hostname_cache->clear();
  (void) pthread_mutex_init(&LOCK_hostname,MY_MUTEX_INIT_SLOW);
  return 0;
}

void hostname_cache_free()
{
  delete hostname_cache;
  hostname_cache= NULL;
}


static inline Host_entry *hostname_cache_search(const char *ip)
{
  return (Host_entry *) hostname_cache->search((uchar *) ip, 0);
}

static bool add_hostname_impl(const char *ip, const char *hostname)
{
  if (hostname_cache_search(ip))
    return FALSE;

  uint hostname_length= hostname ? (uint) strlen(hostname) : 0;

  Host_entry *entry= (Host_entry *) malloc(sizeof (Host_entry) +
                                           hostname_length + 1);

  if (!entry)
    return TRUE;

  char *hostname_copy;

  memcpy_fixed(&entry->ip, ip, HOST_ENTRY_KEY_SIZE);

  if (hostname_length)
  {
    hostname_copy= (char *) (entry + 1);
    memcpy(hostname_copy, hostname, hostname_length + 1);
    DBUG_PRINT("info", ("Adding '%s' -> '%s' to the hostname cache...'",
                        (const char *) ip,
                        (const char *) hostname_copy));
  }
  else
  {
    hostname_copy= NULL;

    DBUG_PRINT("info", ("Adding '%s' -> NULL to the hostname cache...'",
                        (const char *) ip));
  }

  entry->hostname= hostname_copy;
  entry->connect_errors= 0;

  return hostname_cache->add(entry);
}


static bool add_hostname(const char *ip, const char *hostname)
{
  if (specialflag & SPECIAL_NO_HOST_CACHE)
    return FALSE;

  pthread_mutex_lock(&hostname_cache->lock);

  bool err_status= add_hostname_impl(ip, hostname);

  pthread_mutex_unlock(&hostname_cache->lock);

  return err_status;
}

void inc_host_errors(struct sockaddr_storage *ip)
{
  char key[HOST_ENTRY_KEY_SIZE];

  if (hostname_cache_get_key((struct sockaddr *) ip, key))
    return;

  VOID(pthread_mutex_lock(&hostname_cache->lock));

  Host_entry *entry= hostname_cache_search(key);

  if (entry)
    entry->connect_errors++;

  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}

void reset_host_errors(struct sockaddr_storage *ip)
{
  char key[HOST_ENTRY_KEY_SIZE];

  if (hostname_cache_get_key((struct sockaddr *) ip, key))
    return;

  VOID(pthread_mutex_lock(&hostname_cache->lock));

  Host_entry *entry= hostname_cache_search(key);

  if (entry)
    entry->connect_errors= 0;

  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}


static inline bool is_ip_loopback(const struct sockaddr *ip)
{
  switch (ip->sa_family){
  case AF_INET:
    {
      /* Check for IPv4 127.0.0.1. */
      struct in_addr *ip4= &((struct sockaddr_in *) ip)->sin_addr;
      return ip4->s_addr == INADDR_LOOPBACK;
    }

#ifdef HAVE_STRUCT_IN6_ADDR
  case AF_INET6:
    {
      /*
        Check if we have loopback here:
          - IPv6 loopback             (::1)
          - IPv4-compatible 127.0.0.1 (0:0:0:0:0:0000:7f00:0001)
          - IPv4-mapped 127.0.0.1     (0:0:0:0:0:ffff:7f00:0001)
      */
      struct in6_addr *ip6= &((struct sockaddr_in6 *) ip)->sin6_addr;
      if (IN6_IS_ADDR_V4COMPAT(ip6) || IN6_IS_ADDR_V4MAPPED(ip6))
      {
        uint32 *ip6_int32= (uint32 *) ip6->s6_addr;
        return ntohl(ip6_int32[3]) == INADDR_LOOPBACK;
      }
      else
      {
        return IN6_IS_ADDR_LOOPBACK(ip6);
      }
    }
#endif /* HAVE_STRUCT_IN6_ADDR */

  default:
    DBUG_ASSERT(FALSE);
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

  @param [in] IP address. Must be set.
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
                    char **hostname, uint *connect_errors)
{
  const struct sockaddr *ip= (const sockaddr *) ip_storage;
  char ip_string[HOST_ENTRY_KEY_SIZE];
  int err_code;
  
  DBUG_ENTER("ip_to_hostname");

  /* IP address must be set properly. */

  DBUG_ASSERT(ip_storage->ss_family == AF_INET ||
              ip_storage->ss_family == AF_INET6);

  /* Check if we have loopback address (127.0.0.1 or ::1). */

  if (is_ip_loopback(ip))
  {
    DBUG_PRINT("info", ("Loopback address detected."));

    *connect_errors= 0; /* Do not count connect errors from localhost. */
    *hostname= (char *) my_localhost;

    DBUG_RETURN(FALSE);
  }

  /* Get hostname cache key for the IP address. */

  {
    bool err_status= hostname_cache_get_key(ip, ip_string);
    DBUG_ASSERT(!err_status);
  }

  DBUG_PRINT("info", ("IP address: '%s'.", (const char *) ip_string));

  /* Check first if we have host name in the cache. */
  
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));

    Host_entry *entry= hostname_cache_search(ip_string);

    if (entry)
    {
      *connect_errors= entry->connect_errors;
      *hostname= NULL;

      if (entry->hostname)
        *hostname= my_strdup(entry->hostname, MYF(0));

      DBUG_PRINT("info",("IP (%s) has been found in the cache. "
                         "Hostname: '%s'; connect_errors: %d",
                         (const char *) ip_string,
                         (const char *) (*hostname? *hostname : "null"),
                         (int) *connect_errors));

      VOID(pthread_mutex_unlock(&hostname_cache->lock));

      DBUG_RETURN(FALSE);
    }

    VOID(pthread_mutex_unlock(&hostname_cache->lock));
  }

  /* Resolve host name.  Return an error if a host name can not be resolved
    (instead of returning the numeric form of the host name).
  */

  char hostname_buffer[NI_MAXHOST];

  DBUG_PRINT("info", ("Resolving '%s'...", (const char *) ip_string));

  err_code= getnameinfo(ip, sizeof (sockaddr_storage),
                        hostname_buffer, NI_MAXHOST, NULL, 0, NI_NAMEREQD);

  if (err_code == EAI_NONAME)
  {
    /*
      There is no reverse address mapping for the IP address. A host name
      can not be resolved.
    */

    DBUG_PRINT("error", ("IP address '%s' could not be resolved: "
                         "no reverse address mapping.",
                         (const char *) ip_string));

    sql_print_warning("IP address '%s' could not be resolved: "
                      "no reverse address mapping.",
                      (const char *) ip_string);

    bool err_status= add_hostname(ip_string, NULL);

    *hostname= NULL;
    *connect_errors= 0; /* New IP added to the cache. */

    DBUG_RETURN(err_status);
  }
  else if (err_code)
  {
    DBUG_PRINT("error", ("IP address '%s' could not be resolved: "
                         "getnameinfo() returned %d.",
                         (const char *) ip_string,
                         (int) err_code));

    sql_print_warning("IP address '%s' could not be resolved: "
                      "getnameinfo() returned error (code: %d).",
                      (const char *) ip_string,
                      (int) err_code);

    DBUG_RETURN(TRUE);
  }

  DBUG_PRINT("info", ("IP '%s' resolved to '%s'.",
                      (const char *) ip_string,
                      (const char *) hostname_buffer));

  /*
    Validate hostname: the server does not accept host names, which
    resemble IP addresses.

    The thing is that theoretically, a host name can be in a form of IPv4
    address (123.example.org, or 1.2 or even 1.2.3.4). We have to deny such
    host names because ACL-systems is not designed to work with them.

    For exmaple, it is possible to specify a host name mask (like
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
                         (const char *) ip_string,
                         (const char *) hostname_buffer));

    sql_print_warning("IP address '%s' has been resolved "
                      "to the host name '%s', which resembles "
                      "IPv4-address itself.",
                      (const char *) ip_string,
                      (const char *) hostname_buffer);

    bool err_status= add_hostname(ip_string, NULL);

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

  if (err_code == MY_NONAME_ERR_CODE)
  {
    /*
      Don't cache responses when the DNS server is down, as otherwise
      transient DNS failure may leave any number of clients (those
      that attempted to connect during the outage) unable to connect
      indefinitely.
    */
    bool err_status= add_hostname(ip_string, NULL);

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
    struct sockaddr *resolved_ip= addr_info->ai_addr;
    char resolved_ip_key[HOST_ENTRY_KEY_SIZE];

    {
      bool err_status= hostname_cache_get_key(resolved_ip, resolved_ip_key);
      DBUG_ASSERT(!err_status);
    }

    DBUG_PRINT("info", ("  - '%s'", (const char *) resolved_ip_key));

    if (strcmp(ip_string, resolved_ip_key) == 0)
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
                          (const char *) ip_string);
    sql_print_information("Hostname '%s' has the following IP addresses:",
                          (const char *) hostname_buffer);

    for (struct addrinfo *addr_info= addr_info_list;
         addr_info; addr_info= addr_info->ai_next)

    {
      struct sockaddr *resolved_ip= addr_info->ai_addr;
      char resolved_ip_key[HOST_ENTRY_KEY_SIZE];

      hostname_cache_get_key(resolved_ip, resolved_ip_key);

      sql_print_information(" - %s\n", (const char *) resolved_ip_key);
    }
  }
  
  /* Free the result of getaddrinfo(). */

  freeaddrinfo(addr_info_list);

  /* Add an entry for the IP to the cache. */

  bool err_status;

  if (*hostname)
  {
    err_status= add_hostname(ip_string, *hostname);
    *connect_errors= 0;
  }
  else
  {
    DBUG_PRINT("error",("Couldn't verify hostname with getaddrinfo()."));

    err_status= add_hostname(ip_string, NULL);
    *hostname= NULL;
    *connect_errors= 0;
  }
  
  DBUG_RETURN(err_status);
}
