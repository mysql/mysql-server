
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_AUTO_ADDRINFO_HPP
#define DENA_AUTO_ADDRINFO_HPP

#include <my_config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "util.hpp"

typedef SOCKET_SIZE_TYPE size_socket;

namespace dena {

struct auto_addrinfo : private noncopyable {
  auto_addrinfo() : addr(0) { }
  ~auto_addrinfo() {
    reset();
  }
  void reset(addrinfo *a = 0) {
    if (addr != 0) {
      freeaddrinfo(addr);
    }
    addr = a;
  }
  const addrinfo *get() const { return addr; }
  int resolve(const char *node, const char *service, int flags = 0,
    int family = AF_UNSPEC, int socktype = SOCK_STREAM, int protocol = 0) {
    addrinfo hints;
    reset();
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = flags;
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    return getaddrinfo(node, service, &hints, &addr);
  }
 private:
  addrinfo *addr;
};

};

#endif

