
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/un.h>

#include "socket.hpp"
#include "string_util.hpp"
#include "fatal.hpp"

namespace dena {

void
ignore_sigpipe()
{
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    fatal_abort("SIGPIPE SIG_IGN");
  }
}

void
socket_args::set(const config& conf)
{
  timeout = conf.get_int("timeout", 600);
  listen_backlog = conf.get_int("listen_backlog", 256);
  std::string node = conf.get_str("host", "");
  std::string port = conf.get_str("port", "");
  if (!node.empty() || !port.empty()) {
    if (family == AF_UNIX || node == "/") {
      set_unix_domain(port.c_str());
    } else {
      const char *nd = node.empty() ? 0 : node.c_str();
      if (resolve(nd, port.c_str()) != 0) {
	fatal_exit("getaddrinfo failed: " + node + ":" + port);
      }
    }
  }
  sndbuf = conf.get_int("sndbuf", 0);
  rcvbuf = conf.get_int("rcvbuf", 0);
}

void
socket_args::set_unix_domain(const char *path)
{
  family = AF_UNIX; 
  addr = sockaddr_storage();
  addrlen = sizeof(sockaddr_un);
  sockaddr_un *const ap = reinterpret_cast<sockaddr_un *>(&addr);
  ap->sun_family = AF_UNIX;
  strncpy(ap->sun_path, path, sizeof(ap->sun_path) - 1);
}

int
socket_args::resolve(const char *node, const char *service)
{
  const int flags = (node == 0) ? AI_PASSIVE : 0;
  auto_addrinfo ai;
  addr = sockaddr_storage();
  addrlen = 0;
  const int r = ai.resolve(node, service, flags, family, socktype, protocol);
  if (r != 0) {
    return r;
  }
  memcpy(&addr, ai.get()->ai_addr, ai.get()->ai_addrlen);
  addrlen = ai.get()->ai_addrlen;
  return 0;
}

int
socket_set_options(auto_file& fd, const socket_args& args, std::string& err_r)
{
  if (args.timeout != 0 && !args.nonblocking) {
    struct timeval tv;
    tv.tv_sec = args.timeout;
    tv.tv_usec = 0;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
      return errno_string("setsockopt SO_RCVTIMEO", errno, err_r);
    }
    tv.tv_sec = args.timeout;
    tv.tv_usec = 0;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
      return errno_string("setsockopt SO_RCVTIMEO", errno, err_r);
    }
  }
  if (args.nonblocking && fcntl(fd.get(), F_SETFL, O_NONBLOCK) != 0) {
    return errno_string("fcntl O_NONBLOCK", errno, err_r);
  }
  if (args.sndbuf != 0) {
    const int v = args.sndbuf;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_SNDBUF, &v, sizeof(v)) != 0) {
      return errno_string("setsockopt SO_SNDBUF", errno, err_r);
    }
  }
  if (args.rcvbuf != 0) {
    const int v = args.rcvbuf;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_RCVBUF, &v, sizeof(v)) != 0) {
      return errno_string("setsockopt SO_RCVBUF", errno, err_r);
    }
  }
  return 0;
}

int
socket_open(auto_file& fd, const socket_args& args, std::string& err_r)
{
  fd.reset(socket(args.family, args.socktype, args.protocol));
  if (fd.get() < 0) {
    return errno_string("socket", errno, err_r);
  }
  return socket_set_options(fd, args, err_r);
}

int
socket_connect(auto_file& fd, const socket_args& args, std::string& err_r)
{
  int r = 0;
  if ((r = socket_open(fd, args, err_r)) != 0) {
    return r;
  }
  if (connect(fd.get(), reinterpret_cast<const sockaddr *>(&args.addr),
    args.addrlen) != 0) {
    if (!args.nonblocking || errno != EINPROGRESS) {
      return errno_string("connect", errno, err_r);
    }
  }
  return 0;
}

int
socket_bind(auto_file& fd, const socket_args& args, std::string& err_r)
{
  fd.reset(socket(args.family, args.socktype, args.protocol));
  if (fd.get() < 0) {
    return errno_string("socket", errno, err_r);
  }
  if (args.reuseaddr) {
    if (args.family == AF_UNIX) {
      const sockaddr_un *const ap =
	reinterpret_cast<const sockaddr_un *>(&args.addr);
      if (unlink(ap->sun_path) != 0 && errno != ENOENT) {
	return errno_string("unlink uds", errno, err_r);
      }
    } else {
      int v = 1;
      if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) != 0) {
	return errno_string("setsockopt SO_REUSEADDR", errno, err_r);
      }
    }
  }
  if (bind(fd.get(), reinterpret_cast<const sockaddr *>(&args.addr),
    args.addrlen) != 0) {
    return errno_string("bind", errno, err_r);
  }
  if (listen(fd.get(), args.listen_backlog) != 0) {
    return errno_string("listen", errno, err_r);
  }
  if (args.nonblocking && fcntl(fd.get(), F_SETFL, O_NONBLOCK) != 0) {
    return errno_string("fcntl O_NONBLOCK", errno, err_r);
  }
  return 0;
}

int
socket_accept(int listen_fd, auto_file& fd, const socket_args& args,
  sockaddr_storage& addr_r, size_socket& addrlen_r, std::string& err_r)
{
  fd.reset(accept(listen_fd, reinterpret_cast<sockaddr *>(&addr_r),
    &addrlen_r));
  if (fd.get() < 0) {
    return errno_string("accept", errno, err_r);
  }
  return socket_set_options(fd, args, err_r);
}

};

