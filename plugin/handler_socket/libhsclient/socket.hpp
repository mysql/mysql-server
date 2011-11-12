
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_SOCKET_HPP
#define DENA_SOCKET_HPP

#include <string>

#include "auto_addrinfo.hpp"
#include "auto_file.hpp"
#include "config.hpp"

namespace dena {

struct socket_args {
  sockaddr_storage addr;
  size_socket addrlen;
  int family;
  int socktype;
  int protocol;
  int timeout;
  int listen_backlog;
  bool reuseaddr;
  bool nonblocking;
  bool use_epoll;
  int sndbuf;
  int rcvbuf;
  socket_args() : addr(), addrlen(0), family(AF_INET), socktype(SOCK_STREAM),
    protocol(0), timeout(600), listen_backlog(256),
    reuseaddr(true), nonblocking(false), use_epoll(false),
    sndbuf(0), rcvbuf(0) { }
  void set(const config& conf);
  void set_unix_domain(const char *path);
  int resolve(const char *node, const char *service);
};

void ignore_sigpipe();
int socket_bind(auto_file& fd, const socket_args& args, std::string& err_r);
int socket_connect(auto_file& fd, const socket_args& args, std::string& err_r);
int socket_accept(int listen_fd, auto_file& fd, const socket_args& args,
  sockaddr_storage& addr_r, size_socket& addrlen_r, std::string& err_r);

};

#endif

