#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>  //hostent
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <map>

#include "logging.hpp"


namespace rdmaio {

constexpr struct timeval default_timeout = {0, 8000};
constexpr struct timeval no_timeout = {0, 0};  // it means forever

inline __attribute__((always_inline))  // inline to avoid multiple-definiations
int64_t
diff_time(const struct timeval& end, const struct timeval& start) {
  int64_t diff = (end.tv_sec > start.tv_sec) ? (end.tv_sec - start.tv_sec) * 1000 : 0;
  if (end.tv_usec > start.tv_usec) {
    diff += (end.tv_usec - start.tv_usec);
  } else {
    diff -= (start.tv_usec - end.tv_usec);
  }
  return diff;
}

class PreConnector {  // helper class used to exchange QP information using TCP/IP
 public:
  static int get_listen_socket([[maybe_unused]] const std::string& addr, int port) {
    struct sockaddr_in serv_addr;
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    RDMA_ASSERT(sockfd >= 0) << "ERROR opening listen socket: " << strerror(errno);

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // port
    serv_addr.sin_port = htons(port);
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    // [pre_connector.hpp:51] Assertion! ERROR on binding: Address already in use
    RDMA_ASSERT(bind(sockfd, (struct sockaddr*) &serv_addr,
                     sizeof(serv_addr)) == 0)
      << "ERROR on binding: " << strerror(errno);
    return sockfd;
  }

  static int get_send_socket(const std::string& addr, int port, struct timeval timeout = default_timeout) {
    int sockfd;
    struct sockaddr_in serv_addr;

    RDMA_ASSERT((sockfd = socket(AF_INET, SOCK_STREAM, 0)) >= 0) << "Error open socket for send!";
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    auto ip = host_to_ip(addr);
    if (ip == "") {
      close(sockfd);
      return -1;
    }

    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
      if (errno == EINPROGRESS) {
        goto PROGRESS;
      }
      close(sockfd);
      return -1;
    }
    PROGRESS:
    // check return status
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    if (select(sockfd + 1, NULL, &fdset, NULL, &timeout) == 1) {
      int so_error;
      socklen_t len = sizeof so_error;

      getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

      if (so_error == 0) {
        // success
      } else {
        close(sockfd);
        return -1;
      }
    }

    return sockfd;
  }

  // timeout in microsend
  static bool wait_recv(int socket, uint32_t timeout = 2000) {
    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);

      struct timeval s_timeout = {0, timeout};
      int ready = select(socket + 1, &rfds, NULL, NULL, &s_timeout);
      RDMA_ASSERT(ready != -1);

      if (ready == 0) {  // no file descriptor found
        continue;
      }

      if (ready < 0) {  // error case
        RDMA_ASSERT(false) << "select error " << strerror(errno);
      }

      if (FD_ISSET(socket, &rfds)) {
        break;  // ready
      }
    }
    return true;
  }

  static void wait_close(int socket) {
    shutdown(socket, SHUT_WR);
    char buf[2];

    struct timeval timeout = {1, 0};
    auto ret = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &timeout, sizeof(timeout));
    RDMA_ASSERT(ret == 0);

    recv(socket, buf, 2, 0);
    close(socket);
  }

  static int send_to(int fd, char* usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char* bufp = usrbuf;

    while (nleft > 0) {
      if ((nwritten = write(fd, bufp, nleft)) <= 0) {
        if (errno == EINTR) /* Interrupted by sig handler return */
          nwritten = 0;     /* and call write() again */
        else
          return -1; /* errno set by write() */
      }
      nleft -= nwritten;
      bufp += nwritten;
    }
    return n;
  }

  typedef std::map<std::string, std::string> ipmap_t;
  static ipmap_t& local_ip_cache() {
    static __thread ipmap_t cache;
    return cache;
  }

  static std::string host_to_ip(const std::string& host) {
    ipmap_t cache = local_ip_cache();
    if (cache.find(host) != cache.end())
      return cache[host];

    std::string res = "";

    struct addrinfo hints, * infoptr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // AF_INET means IPv4 only addresses

    int result = getaddrinfo(host.c_str(), NULL, &hints, &infoptr);
    if (result) {
      fprintf(stderr, "getaddrinfo: %s at %s\n", gai_strerror(result), host.c_str());
      return "";
    }
    char ip[64];
    memset(ip, 0, sizeof(ip));

    for (struct addrinfo* p = infoptr; p != NULL; p = p->ai_next) {
      getnameinfo(p->ai_addr, p->ai_addrlen, ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);
    }

    res = std::string(ip);
    if (res != "")
      cache.insert(std::make_pair(host, res));
    return res;
  }
};

};  // namespace rdmaio
