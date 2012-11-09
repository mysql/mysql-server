/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>
#include <NdbTCP.h>


/* On some operating systems (e.g. Solaris) INADDR_NONE is not defined */
#ifndef INADDR_NONE
#define INADDR_NONE -1                          /* Error value from inet_addr */
#endif

/* Return codes from getaddrinfo() */
/* EAI_NODATA is obsolete and has been removed from some platforms */
#ifndef EAI_NODATA
#define EAI_NODATA EAI_NONAME
#endif

extern "C"
int
Ndb_getInAddr(struct in_addr * dst, const char *address)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // Only IPv4 address
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* ai_list;
  if (getaddrinfo(address, NULL, &hints, &ai_list) != 0)
  {
    dst->s_addr = INADDR_NONE;
    return -1;
  }

  /* Return sin_addr for the first address returned */
  struct sockaddr_in* sin = (struct sockaddr_in*)ai_list->ai_addr;
  memcpy(dst, &sin->sin_addr, sizeof(struct in_addr));

  freeaddrinfo(ai_list);
  return 0;
}

#ifdef TEST_NDBGETINADDR
#include <NdbTap.hpp>

static void
CHECK(const char* address, int expected_res, bool is_numeric= false)
{
  struct in_addr addr;

  fprintf(stderr, "Testing '%s'\n", address);

  int res= Ndb_getInAddr(&addr, address);

  if (res != expected_res)
  {
    fprintf(stderr, "> unexpected result: %d, expected: %d\n",
            res, expected_res);
    abort();
  }

  if (res != 0)
  {
    fprintf(stderr, "> returned -1, checking INADDR_NONE\n");

    // Should return INADDR_NONE when when lookup fails
    struct in_addr none;
    none.s_addr = INADDR_NONE;
    if (memcmp(&addr, &none, sizeof(none)) != 0)
    {
      fprintf(stderr, "> didn't return INADDR_NONE after failure, "
             "got: '%s', expected; '%s'\n",
             inet_ntoa(addr), inet_ntoa(none));
      abort();
    }
    fprintf(stderr, "> ok\n");
    return;
  }

  fprintf(stderr, "> '%s' -> '%s'\n", address, inet_ntoa(addr));

  if (is_numeric)
  {
    // Check that numeric address always map back to itself
    // ie. compare to value returned by 'inet_aton'
    fprintf(stderr, "> Checking numeric address against inet_addr\n");
    struct in_addr addr2;
    addr2.s_addr = inet_addr(address);
    fprintf(stderr, "> inet_addr(%s) -> '%s'\n", address, inet_ntoa(addr2));

    if (memcmp(&addr, &addr2, sizeof(struct in_addr)) != 0)
    {
      fprintf(stderr, "> numeric address '%s' didn't map to same value as "
              "inet_addr: '%s'", address, inet_ntoa(addr2));
      abort();
    }
    fprintf(stderr, "> ok\n");
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
  int err = getaddrinfo(name, NULL, &hints, &ai_list);

  if (err)
  {
    fprintf(stderr, "> '%s' -> error: %d '%s'\n",
             name, err, gai_strerror(err));

    if (err == EAI_NODATA ||
	err == EAI_NONAME)
    {
      // An OK error 
      fprintf(stderr, ">  skipping tests with this name...\n");
      return false;
    }

    // Another unhandled error
    abort();
  }

  freeaddrinfo(ai_list);

  return true;
}


TAPTEST(NdbGetInAddr)
{
  socket_library_init();

  if (can_resolve_hostname("localhost"))
    CHECK("localhost", 0);
  CHECK("127.0.0.1", 0, true);

  char hostname_buf[256];
  if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0 &&
      can_resolve_hostname(hostname_buf))
  {
    // Check this machines hostname
    CHECK(hostname_buf, 0);

    struct in_addr addr;
    Ndb_getInAddr(&addr, hostname_buf);
    // Convert hostname to dotted decimal string ip and check
    CHECK(inet_ntoa(addr), 0, true);
  }
  CHECK("unknown_?host", -1); // Does not exist
  CHECK("3ffe:1900:4545:3:200:f8ff:fe21:67cf", -1); // No IPv6
  CHECK("fe80:0:0:0:200:f8ff:fe21:67cf", -1);
  CHECK("fe80::200:f8ff:fe21:67cf", -1);
  CHECK("::1", -1); // the loopback, but still No IPv6

  socket_library_end();

  return 1; // OK
}
#endif


static inline
int my_socket_nfds(ndb_socket_t s, int nfds)
{
#ifdef _WIN32
  (void)s;
#else
  if(s.fd > nfds)
    return s.fd;
#endif
  return nfds;
}

#define my_FD_SET(sock,set)   FD_SET(ndb_socket_get_native(sock), set)
#define my_FD_ISSET(sock,set) FD_ISSET(ndb_socket_get_native(sock), set)


int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock)
{
#ifdef HAVE_POLL
  struct pollfd pfd[1];
  int r;

  pfd[0].fd= sock.fd; // FIXME: THIS IS A BUG
  pfd[0].events= POLLHUP | POLLIN | POLLOUT | POLLNVAL;
  pfd[0].revents= 0;
  r= poll(pfd,1,0);
  if(pfd[0].revents & (POLLHUP|POLLERR))
    return 1;

  return 0;
#else /* HAVE_POLL */
  fd_set readfds, writefds, errorfds;
  struct timeval tv= {0,0};
  int s_err;
  SOCKET_SIZE_TYPE s_err_size= sizeof(s_err);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);

  my_FD_SET(sock, &readfds);
  my_FD_SET(sock, &writefds);
  my_FD_SET(sock, &errorfds);

  if(select(my_socket_nfds(sock,0)+1, &readfds, &writefds, &errorfds, &tv)<0)
    return 1;

  if(my_FD_ISSET(sock,&errorfds))
    return 1;

  s_err=0;
  if (my_getsockopt(sock, SOL_SOCKET, SO_ERROR, &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {                                             /* getsockopt could succeed */
    return(1);                                 /* but return an error... */
  }

  return 0;
#endif /* HAVE_POLL */
}
