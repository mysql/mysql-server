
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdlib.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "hstcpsvr.hpp"
#include "hstcpsvr_worker.hpp"
#include "thread.hpp"
#include "fatal.hpp"
#include "auto_ptrcontainer.hpp"

#define DBG(x)

namespace dena {

struct worker_throbj {
  worker_throbj(const hstcpsvr_worker_arg& arg)
    : worker(hstcpsvr_worker_i::create(arg)) { }
  void operator ()() {
    worker->run();
  }
  hstcpsvr_worker_ptr worker;
};

struct hstcpsvr : public hstcpsvr_i, private noncopyable {
  hstcpsvr(const config& c);
  ~hstcpsvr();
  virtual std::string start_listen();
 private:
  hstcpsvr_shared_c cshared;
  volatile hstcpsvr_shared_v vshared;
  typedef thread<worker_throbj> worker_thread_type;
  typedef auto_ptrcontainer< std::vector<worker_thread_type *> > threads_type;
  threads_type threads;
  std::vector<unsigned int> thread_num_conns_vec;
 private:
  void stop_workers();
};

namespace {

void
check_nfile(size_t nfile)
{
  struct rlimit rl;
  const int r = getrlimit(RLIMIT_NOFILE, &rl);
  if (r != 0) {
    fatal_abort("check_nfile: getrlimit failed");
  }
  if (rl.rlim_cur < static_cast<rlim_t>(nfile + 1000)) {
    fprintf(stderr,
      "[Warning] handlersocket: open_files_limit is too small.\n");
  }
}

};

hstcpsvr::hstcpsvr(const config& c)
  : cshared(), vshared()
{
  vshared.shutdown = 0;
  cshared.conf = c; /* copy */
  if (cshared.conf["port"] == "") {
    cshared.conf["port"] = "9999";
  }
  cshared.num_threads = cshared.conf.get_int("num_threads", 32);
  cshared.sockargs.nonblocking = cshared.conf.get_int("nonblocking", 1);
  cshared.sockargs.use_epoll = cshared.conf.get_int("use_epoll", 1);
  if (cshared.sockargs.use_epoll) {
    cshared.sockargs.nonblocking = 1;
  }
  cshared.readsize = cshared.conf.get_int("readsize", 1);
  cshared.nb_conn_per_thread = cshared.conf.get_int("conn_per_thread", 1024);
  cshared.for_write_flag = cshared.conf.get_int("for_write", 0);
  cshared.plain_secret = cshared.conf.get_str("plain_secret", "");
  cshared.require_auth = !cshared.plain_secret.empty();
  cshared.sockargs.set(cshared.conf);
  cshared.dbptr = database_i::create(c);
  check_nfile(cshared.num_threads * cshared.nb_conn_per_thread);
  thread_num_conns_vec.resize(cshared.num_threads);
  cshared.thread_num_conns = thread_num_conns_vec.empty()
    ? 0 : &thread_num_conns_vec[0];
}

hstcpsvr::~hstcpsvr()
{
  stop_workers();
}

std::string
hstcpsvr::start_listen()
{
  std::string err;
  if (threads.size() != 0) {
    return "start_listen: already running";
  }
  if (socket_bind(cshared.listen_fd, cshared.sockargs, err) != 0) {
    return "bind: " + err;
  }
  DENA_VERBOSE(20, fprintf(stderr, "bind done\n"));
  const size_t stack_size = std::max(
    cshared.conf.get_int("stack_size", 1 * 1024LL * 1024), 8 * 1024LL * 1024);
  for (long i = 0; i < cshared.num_threads; ++i) {
    hstcpsvr_worker_arg arg;
    arg.cshared = &cshared;
    arg.vshared = &vshared;
    arg.worker_id = i;
    std::auto_ptr< thread<worker_throbj> > thr(
      new thread<worker_throbj>(arg, stack_size));
    threads.push_back_ptr(thr);
  }
  DENA_VERBOSE(20, fprintf(stderr, "threads created\n"));
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->start();
  }
  DENA_VERBOSE(20, fprintf(stderr, "threads started\n"));
  return std::string();
}

void
hstcpsvr::stop_workers()
{
  vshared.shutdown = 1;
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  threads.clear();
}

hstcpsvr_ptr
hstcpsvr_i::create(const config& conf)
{
  return hstcpsvr_ptr(new hstcpsvr(conf));
}

};

