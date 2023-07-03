/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_global.h"
#include "portlib/NdbTCP.h"

#include <string.h>

/* By default, prefer IPv4 addresses, for smooth upgrade from an IPv4-only
   environment.
*/
static int lookup_prefer_ip_version = 4;

void NdbTCP_set_preferred_IP_version(int version) {
  assert(version == 4 || version == 6);
  lookup_prefer_ip_version = version;
}

/* Return codes from getaddrinfo() */
/* EAI_NODATA is obsolete and has been removed from some platforms */
#ifndef EAI_NODATA
#define EAI_NODATA EAI_NONAME
#endif

static void Ndb_make_ipv6_from_ipv4(struct sockaddr_in6* dst,
                                    const struct sockaddr_in* src);

void Ndb_make_ipv6_from_ipv4(struct sockaddr_in6* dst,
                             const struct sockaddr_in* src)
{
  /*
   * IPv4 mapped to IPv6 is ::ffff:a.b.c.d or expanded as full hex
   * 0000:0000:0000:0000:0000:ffff:AABB:CCDD
   */
  dst->sin6_family = AF_INET6;
  memset(&dst->sin6_addr.s6_addr[0], 0, 10);
  memset(&dst->sin6_addr.s6_addr[10], 0xff, 2);
  memcpy(&dst->sin6_addr.s6_addr[12], &src->sin_addr.s_addr, 4);
}

static struct addrinfo * get_preferred_address(struct addrinfo * ai_list)
{
  struct addrinfo* first_ip4_addr = nullptr;
  struct addrinfo* first_unscoped_ip6_addr = nullptr;

  for(struct addrinfo *ai = ai_list; ai != nullptr; ai = ai->ai_next)
  {
    if((ai->ai_family == AF_INET) && (first_ip4_addr == nullptr))
    {
      first_ip4_addr = ai;
    }
    if((ai->ai_family == AF_INET6) && (first_unscoped_ip6_addr == nullptr))
    {
      struct sockaddr_in6* addr = (struct sockaddr_in6*)ai->ai_addr;
      if (addr->sin6_scope_id == 0)
      {
        first_unscoped_ip6_addr = ai;
      }
    }
  }

  if(lookup_prefer_ip_version == 4)
  {
    if(first_ip4_addr) return first_ip4_addr;
    if(first_unscoped_ip6_addr) return first_unscoped_ip6_addr;
  }
  else             // prefer IPv6
  {
    if(first_unscoped_ip6_addr) return first_unscoped_ip6_addr;
    if(first_ip4_addr) return first_ip4_addr;
  }
  return ai_list;  // fallback to first address in original list
}

static int get_in6_addr(struct in6_addr* dst, const struct addrinfo* src)
{
  if (src == nullptr)
  {
    return -1;
  }

  struct sockaddr_in6* addr6_ptr;
  sockaddr_in6 addr6;

  if (src->ai_family == AF_INET)
  {
    struct sockaddr_in* addr4_ptr = (struct sockaddr_in*)src->ai_addr;
    Ndb_make_ipv6_from_ipv4(&addr6, addr4_ptr);
    addr6_ptr = &addr6;
  }
  else if (src->ai_family == AF_INET6)
  {
    addr6_ptr = (struct sockaddr_in6*)src->ai_addr;
    if(addr6_ptr->sin6_scope_id != 0)
    {
      return -1;  // require unscoped address
    }
  }
  else
  {
    return -1;
  }
  memcpy(dst, &addr6_ptr->sin6_addr, sizeof(struct in6_addr));
  return 0;
}

int
Ndb_getInAddr6(struct in6_addr * dst, const char *address)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* ai_list;

  if (getaddrinfo(address, nullptr, &hints, &ai_list) != 0)
  {
    return -1;
  }

  struct addrinfo* ai_pref = get_preferred_address(ai_list);

  int ret = get_in6_addr(dst, ai_pref);

  freeaddrinfo(ai_list);

  return ret;
}

char*
Ndb_inet_ntop(int af,
              const void *src,
              char *dst,
              size_t dst_size)
{
  // Function assume there is at least some space in "dst" since there
  // are no way to return failure without writing into "dst". Check
  // that no one seem to call function with too small "dst_size"
  assert(dst);
  assert(dst_size > 0);

  int ret;
  switch (af)
  {
    case AF_INET:
    {
      sockaddr_in sa;
      memset(&sa, 0, sizeof(sa));
      memcpy(&sa.sin_addr, src, sizeof(sa.sin_addr));
      sa.sin_family = AF_INET;
      ret = getnameinfo(reinterpret_cast<sockaddr*>(&sa),
                        sizeof(sockaddr_in),
                        dst,
                        (socklen_t)dst_size,
                        nullptr,
                        0,
                        NI_NUMERICHOST);
      if (ret != 0)
      {
        break;
      }
      return dst;
    }
    case AF_INET6:
    {
      sockaddr_in6 sa;
      memset(&sa, 0, sizeof(sa));
      memcpy(&sa.sin6_addr, src, sizeof(sa.sin6_addr));
      sa.sin6_family = AF_INET6;
      ret = getnameinfo(reinterpret_cast<sockaddr*>(&sa),
                        sizeof(sockaddr_in6),
                        dst,
                        (socklen_t)dst_size,
                        nullptr,
                        0,
                        NI_NUMERICHOST);
      const char* mapped_prefix = "::ffff:";
      size_t mapped_prefix_len = strlen(mapped_prefix);
      if ((dst != nullptr) &&
          (strncmp(mapped_prefix, dst, mapped_prefix_len) == 0))
      {
        memmove(dst, dst + mapped_prefix_len,
                strlen(dst) - mapped_prefix_len + 1);
      }

      if (ret != 0)
      {
        break;
      }
      return dst;
    }
    default:
    {
      break;
    }
  }

  // Copy the string "null" into dst buffer
  // and zero terminate for safety
  strncpy(dst, "null", dst_size);
  dst[dst_size-1] = 0;

  return dst;
}

/**
 * This function takes a string splits it into the address/hostname part
 * and port/service part.
 * If string contains space, it is expected that the part preceding space is
 * host address or name and the succeeding part is service port.
 * It does not do deep verification that passed string makes sense.
 * It is quite optimistic only checking for []: (ipv6-address) and
 * single : (ipv4-address or hostname).
 * Else, assumes valid address/hostname without port/service.
 *
 * @param arg The input string
 * @param host Buffer into which the address/hostname will be written.
 * @param hostlen Size of host in bytes. Address/hostname will be trimmed
 * if longer than hostlen
 * @param serv Buffer into which the port/service will be writeen
 * @param servlen Size of serv in bytes
 * @return 0 for success and -1 for invalid address.
 */
int
Ndb_split_string_address_port(const char *arg, char *host, size_t hostlen,
                              char *serv, size_t servlen)
{
  const char* sep = strchr(arg, ' ');
  if (sep != nullptr)
  {
    size_t hlen = sep - arg;
    char unchecked_host[NDB_ADDR_STRLEN];
    if (hlen >= sizeof(unchecked_host))
    {
      return -1;
    }
    memcpy(unchecked_host, arg, hlen);
    unchecked_host[hlen] = 0;
    while (*sep == ' ') sep++;
    serv[servlen - 1] = 0;
    strncpy(serv, sep, servlen);
    if (serv[servlen - 1] != 0)
    {
      return -1;
    }
    char dummy[1];
    /*
     * Parse host part on its own. Will handle bracketed IPv6 address
     * ([1::2:3]), and also fail if host part contains its own port
     * ("1.2.3.4:5 4444").
     */
    return Ndb_split_string_address_port(unchecked_host, host, hostlen, dummy, 1);
  }

  const char *port_colon = nullptr;

  if (*arg == '[')
  {
    // checking for [IPv6_address]:port
    const char *check_closing_bracket = strchr(arg, ']');

    if (check_closing_bracket == nullptr)
      return -1;

    port_colon = check_closing_bracket + 1;

    if ((*port_colon == ':') || (*port_colon == '\0'))
    {
      size_t copy_bytes = port_colon - arg - 2;
      if ((copy_bytes >= hostlen) ||
          (*port_colon != '\0' && strlen(port_colon + 1) >= servlen))
        return -1; // fail on truncate

      // Check if host has at least one colon
      const char* first_colon = strchr(arg + 1, ':');
      if (first_colon == nullptr || first_colon >= port_colon)
        return -1;

      strncpy(host, arg + 1, copy_bytes);
      host[copy_bytes] = '\0';
      if (*port_colon == ':')
      {
        serv[servlen - 1] = '\0';
        strncpy(serv, port_colon + 1, servlen);
        if (serv[servlen - 1] != '\0')
          return -1;
      }
      else
      {
        serv[0] = '\0';
      }
      return 0;
    }
    return -1;
  }
  else if ((port_colon = strchr(arg, ':')) &&
            (strchr(port_colon + 1, ':') == nullptr))
  {
    // checking for IPv4_address:port or hostname:port
    size_t copy_bytes = port_colon - arg;
    if ((copy_bytes >= hostlen) || (strlen(port_colon + 1) >= servlen))
      return -1; // fail on truncate
    strncpy(host, arg, copy_bytes);
    host[port_colon - arg] = '\0';
    serv[servlen - 1] = '\0';
    strncpy(serv, port_colon + 1, servlen);
    if (serv[servlen - 1] != '\0')
      return -1;
    return 0;
  }
  // more than one colon or no colon - assume no port !
  if (strlen(arg) >= hostlen)
    return -1; // fail on truncate
  host[hostlen - 1] = '\0';
  strncpy(host, arg, hostlen);
  if (host[hostlen - 1] != '\0')
    return -1;
  serv[0] = '\0';
  return 0;
}

char*
Ndb_combine_address_port(char *buf,
                         size_t bufsize,
                         const char *host,
                         Uint16 port)
{
   if (host == nullptr)
   {
    snprintf(buf, bufsize, "*:%d", port);
   }
   else if (strchr(host, ':') == nullptr)
   {
     snprintf(buf, bufsize, "%s:%d", host, port);
   }
   else
   {
     snprintf(buf, bufsize, "[%s]:%d", host, port);
   }
   return buf;
}

#ifdef TEST_NDBGETINADDR
#include <NdbTap.hpp>

static void
CHECK(const char* name, int chk_result, const char* chk_address = nullptr)
{
  struct in6_addr addr;
  char *addr_str1;
  char buf1[NDB_ADDR_STRLEN];

  fprintf(stderr, "Testing '%s' with length: %zu\n", name, strlen(name));

  int res= Ndb_getInAddr6(&addr, name);

  if (res != chk_result)
  {
    fprintf(stderr, "> unexpected result: %d, expected: %d\n", res, chk_result);
    abort();
  }

  addr_str1 = Ndb_inet_ntop(AF_INET6, static_cast<void*>(&addr),
                            buf1, sizeof(buf1));
  fprintf(stderr, "> '%s' -> '%s'\n", name, addr_str1);

  if(chk_address && strcmp(addr_str1, chk_address))
  {
    fprintf(stderr, "> mismatch from expected '%s'\n", chk_address);
    abort();
  }
}

static void
CHECK_SPLIT(const char str[], int chk_result, const char* host = nullptr,
            const char* serv = nullptr)
{
  char host_buf[NDB_DNS_HOST_NAME_LENGTH + 1];
  char serv_buf[NDB_IANA_SERVICE_NAME_LENGTH + 1];
  int res = Ndb_split_string_address_port(str, host_buf, sizeof(host_buf),
                                          serv_buf, sizeof(serv_buf));
  if (res != chk_result)
  {
    fprintf(stderr, "> unexpected result: str '%s' %d, expected: %d\n", str,
            res, chk_result);
    abort();
  }
  if (res != 0)
    return;
  if (host != nullptr && strcmp(host_buf, host) != 0)
  {
    fprintf(stderr, "> unexpected result: str '%s' host '%s', expected '%s'\n",
            str, host_buf, host);
    abort();
  }
  if (serv != nullptr && strcmp(serv_buf, serv) != 0)
  {
    fprintf(stderr,
            "> unexpected result: str '%s' service '%s', expected '%s'\n",
            str, serv_buf, serv);
    abort();
  }
}

/*
  socket_library_init
   - Normally done by ndb_init(), but to avoid
     having to link with "everything", implement it locally
*/

static void
socket_library_init(void)
{
#ifdef _WIN32
  WORD requested_version = MAKEWORD( 2, 0 );
  WSADATA wsa_data;
  if (WSAStartup( requested_version, &wsa_data ))
  {
    fprintf(stderr, "failed to init Winsock\n");
    abort();
  }

  // Confirm that the requested version of the library was loaded
  if (wsa_data.wVersion != requested_version)
  {
    (void)WSACleanup();
    fprintf(stderr, "Wrong version of Winsock loaded\n");
    abort();
  }
#endif
}


static void
socket_library_end()
{
#ifdef _WIN32
  (void)WSACleanup();
#endif
}

static bool
can_resolve_hostname(const char* name)
{
  fprintf(stderr, "Checking if '%s' can be used for testing\n", name);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // Only IPv4 address
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* ai_list;
  int err = getaddrinfo(name, nullptr, &hints, &ai_list);
  freeaddrinfo(ai_list);

  if (err)
  {
    fprintf(stderr, "> '%s' -> error: %d '%s'\n", name, err, gai_strerror(err));

    if (err == EAI_NODATA || err == EAI_NONAME)
    {
      // An OK error 
      fprintf(stderr, ">  skipping tests with this name...\n");
      return false;
    }

    // Another unhandled error
    abort();
  }

  return true;
}


TAPTEST(NdbGetInAddr)
{
  socket_library_init();

  if (can_resolve_hostname("localhost"))
  {
    NdbTCP_set_preferred_IP_version(4);
    CHECK("localhost", 0, "127.0.0.1");
    NdbTCP_set_preferred_IP_version(6);
    CHECK("localhost", 0, "::1");
    NdbTCP_set_preferred_IP_version(4);
  }
  CHECK("127.0.0.1", 0);

  char hostname_buf[NDB_DNS_HOST_NAME_LENGTH + 1];
  char addr_buf[NDB_ADDR_STRLEN];
  if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0 &&
      can_resolve_hostname(hostname_buf))
  {
    // Check this machines hostname
    CHECK(hostname_buf, 0);

    struct in6_addr addr;
    Ndb_getInAddr6(&addr, hostname_buf);
    // Convert hostname to dotted decimal string ip and check
    CHECK(Ndb_inet_ntop(AF_INET6,
                        static_cast<void*>(&addr),
                        addr_buf,
                        sizeof(addr_buf)),
                        0);
  }
  CHECK("unknown_?host", -1); // Does not exist
  CHECK("3ffe:1900:4545:3:200:f8ff:fe21:67cf", 0);
  CHECK("fe80:0:0:0:200:f8ff:fe21:67cf", 0);
  CHECK("fe80::200:f8ff:fe21:67cf", 0);
  CHECK("::1", 0);

  // 255 byte hostname which does not exist
  char long_hostname[NDB_DNS_HOST_NAME_LENGTH + 1];
  memset(long_hostname, 'y', sizeof(long_hostname)-1);
  long_hostname[sizeof(long_hostname)-1] = 0;
  assert(strlen(long_hostname) == 255);
  CHECK(long_hostname, -1);

  {
    // Check with AF_UNSPEC to trigger Ndb_inet_ntop()
    // to return the "null" error string
    fprintf(stderr, "Testing Ndb_inet_ntop(AF_UNSPEC, ...)\n");

    struct in_addr addr;
    const char* addr_str = Ndb_inet_ntop(AF_UNSPEC,
                                         static_cast<void*>(&addr),
                                         addr_buf,
                                         sizeof(addr_buf));
    fprintf(stderr, "> AF_UNSPEC -> '%s'\n", addr_str);
  }

  CHECK_SPLIT("1.2.3.4", 0, "1.2.3.4", "");
  CHECK_SPLIT("001.009.081.0255", 0, "001.009.081.0255", "");
  CHECK_SPLIT("1.2.3.4:5", 0, "1.2.3.4", "5");
  CHECK_SPLIT("1::5:4", 0, "1::5:4", "");
  CHECK_SPLIT("[1::5]:4", 0, "1::5", "4");
  CHECK_SPLIT("my_host:4", 0, "my_host", "4");
  CHECK_SPLIT("localhost:13001", 0, "localhost", "13001");
  CHECK_SPLIT("[fed0:10::182]", 0, "fed0:10::182", "");
  CHECK_SPLIT("fed0:10::182", 0, "fed0:10::182", "");
  CHECK_SPLIT("[fed0:10:0:ff:11:22:33:182]:1186", 0,
              "fed0:10:0:ff:11:22:33:182", "1186");
  CHECK_SPLIT("::", 0, "::", "");
  CHECK_SPLIT("::1", 0, "::1", "");
  CHECK_SPLIT("2001:db8::1", 0, "2001:db8::1", "");
  CHECK_SPLIT("192.0.2.0:1", 0, "192.0.2.0", "1");
  /*
   * When using space separated host and port, host part should not contain
   * port.
   */
  CHECK_SPLIT("192.0.2.0:1 4444", -1);

  char long_host[NDB_DNS_HOST_NAME_LENGTH + 3 + 1];
  for (int i = 0; i < NDB_DNS_HOST_NAME_LENGTH + 3; i++)
    long_host[i] = ((i + 1) % 27) ? 'a' + (i % 27) : '.';
  long_host[NDB_DNS_HOST_NAME_LENGTH + 3] = 0;
  CHECK_SPLIT(long_host, -1);
  long_host[1] = ':';
  CHECK_SPLIT(long_host, -1);
  long_host[1] = 'b';
  long_host[NDB_DNS_HOST_NAME_LENGTH] = ':';
  CHECK_SPLIT(long_host, 0, nullptr, &long_host[NDB_DNS_HOST_NAME_LENGTH + 1]);

  /*
   * Ndb_split_string_address_port will allow the below for now since it only
   * does not do a full validation of host.
   */
  CHECK_SPLIT("192.0.2.0::1", 0, "192.0.2.0::1", "");
  CHECK_SPLIT("fed0:10:0:ff:11:22:33:182:1186", 0,
              "fed0:10:0:ff:11:22:33:182:1186", "");

  CHECK_SPLIT("localhost 13001", 0, "localhost", "13001");
  CHECK_SPLIT("fed0:10:0:ff:11:22:33:182 1186", 0, "fed0:10:0:ff:11:22:33:182",
              "1186");
  CHECK_SPLIT("super:1186 1234", -1);
  CHECK_SPLIT("[2001:db8::1] 20", 0, "2001:db8::1", "20");

  socket_library_end();

  return 1; // OK
}

#endif /* TEST_NDBGETINADDR */
