
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <my_config.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <signal.h>
#include <list>
#if __linux__
#include <sys/epoll.h>
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "hstcpsvr_worker.hpp"
#include "string_buffer.hpp"
#include "auto_ptrcontainer.hpp"
#include "string_util.hpp"
#include "escape.hpp"

#define DBG_FD(x)
#define DBG_TR(x)
#define DBG_EP(x)
#define DBG_MULTI(x)

/* TODO */
#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

namespace dena {

struct dbconnstate {
  string_buffer readbuf;
  string_buffer writebuf;
  std::vector<prep_stmt> prep_stmts;
  size_t resp_begin_pos;
  size_t find_nl_pos;
  void reset() {
    readbuf.clear();
    writebuf.clear();
    prep_stmts.clear();
    resp_begin_pos = 0;
    find_nl_pos = 0;
  }
  dbconnstate() : resp_begin_pos(0), find_nl_pos(0) { }
};

struct hstcpsvr_conn;
typedef auto_ptrcontainer< std::list<hstcpsvr_conn *> > hstcpsvr_conns_type;

struct hstcpsvr_conn : public dbcallback_i {
 public:
  auto_file fd;
  sockaddr_storage addr;
  size_socket addr_len;
  dbconnstate cstate;
  std::string err;
  size_t readsize;
  bool nonblocking;
  bool read_finished;
  bool write_finished;
  time_t nb_last_io;
  hstcpsvr_conns_type::iterator conns_iter;
  bool authorized;
 public:
  bool closed() const;
  bool ok_to_close() const;
  void reset();
  int accept(const hstcpsvr_shared_c& cshared);
  bool write_more(bool *more_r = 0);
  bool read_more(bool *more_r = 0);
 public:
  virtual void dbcb_set_prep_stmt(size_t pst_id, const prep_stmt& v);
  virtual const prep_stmt *dbcb_get_prep_stmt(size_t pst_id) const;
  virtual void dbcb_resp_short(uint32_t code, const char *msg);
  virtual void dbcb_resp_short_num(uint32_t code, uint32_t value);
  virtual void dbcb_resp_short_num64(uint32_t code, uint64_t value);
  virtual void dbcb_resp_begin(size_t num_flds);
  virtual void dbcb_resp_entry(const char *fld, size_t fldlen);
  virtual void dbcb_resp_end();
  virtual void dbcb_resp_cancel();
 public:
  hstcpsvr_conn() : addr_len(sizeof(addr)), readsize(4096),
    nonblocking(false), read_finished(false), write_finished(false),
    nb_last_io(0), authorized(false) { }
};

bool
hstcpsvr_conn::closed() const
{
  return fd.get() < 0;
}

bool
hstcpsvr_conn::ok_to_close() const
{
  return write_finished || (read_finished && cstate.writebuf.size() == 0);
}

void
hstcpsvr_conn::reset()
{
  addr = sockaddr_storage();
  addr_len = sizeof(addr);
  cstate.reset();
  fd.reset();
  read_finished = false;
  write_finished = false;
}

int
hstcpsvr_conn::accept(const hstcpsvr_shared_c& cshared)
{
  reset();
  return socket_accept(cshared.listen_fd.get(), fd, cshared.sockargs, addr,
    addr_len, err);
}

bool
hstcpsvr_conn::write_more(bool *more_r)
{
  if (write_finished || cstate.writebuf.size() == 0) {
    return false;
  }
  const size_t wlen = cstate.writebuf.size();
  ssize_t len = send(fd.get(), cstate.writebuf.begin(), wlen, MSG_NOSIGNAL);
  if (len <= 0) {
    if (len == 0 || !nonblocking || errno != EWOULDBLOCK) {
      cstate.writebuf.clear();
      write_finished = true;
    }
    return false;
  }
  cstate.writebuf.erase_front(len);
    /* FIXME: reallocate memory if too large */
  if (more_r) {
    *more_r = (static_cast<size_t>(len) == wlen);
  }
  return true;
}

bool
hstcpsvr_conn::read_more(bool *more_r)
{
  if (read_finished) {
    return false;
  }
  const size_t block_size = readsize > 4096 ? readsize : 4096;
  char *wp = cstate.readbuf.make_space(block_size);
  const ssize_t len = read(fd.get(), wp, block_size);
  if (len <= 0) {
    if (len == 0 || !nonblocking || errno != EWOULDBLOCK) {
      read_finished = true;
    }
    return false;
  }
  cstate.readbuf.space_wrote(len);
  if (more_r) {
    *more_r = (static_cast<size_t>(len) == block_size);
  }
  return true;
}

void
hstcpsvr_conn::dbcb_set_prep_stmt(size_t pst_id, const prep_stmt& v)
{
  if (cstate.prep_stmts.size() <= pst_id) {
    cstate.prep_stmts.resize(pst_id + 1);
  }
  cstate.prep_stmts[pst_id] = v;
}

const prep_stmt *
hstcpsvr_conn::dbcb_get_prep_stmt(size_t pst_id) const
{
  if (cstate.prep_stmts.size() <= pst_id) {
    return 0;
  }
  return &cstate.prep_stmts[pst_id];
}

void
hstcpsvr_conn::dbcb_resp_short(uint32_t code, const char *msg)
{
  write_ui32(cstate.writebuf, code);
  const size_t msglen = strlen(msg);
  if (msglen != 0) {
    cstate.writebuf.append_literal("\t1\t");
    cstate.writebuf.append(msg, msg + msglen);
  } else {
    cstate.writebuf.append_literal("\t1");
  }
  cstate.writebuf.append_literal("\n");
}

void
hstcpsvr_conn::dbcb_resp_short_num(uint32_t code, uint32_t value)
{
  write_ui32(cstate.writebuf, code);
  cstate.writebuf.append_literal("\t1\t");
  write_ui32(cstate.writebuf, value);
  cstate.writebuf.append_literal("\n");
}

void
hstcpsvr_conn::dbcb_resp_short_num64(uint32_t code, uint64_t value)
{
  write_ui32(cstate.writebuf, code);
  cstate.writebuf.append_literal("\t1\t");
  write_ui64(cstate.writebuf, value);
  cstate.writebuf.append_literal("\n");
}

void
hstcpsvr_conn::dbcb_resp_begin(size_t num_flds)
{
  cstate.resp_begin_pos = cstate.writebuf.size();
  cstate.writebuf.append_literal("0\t");
  write_ui32(cstate.writebuf, num_flds);
}

void
hstcpsvr_conn::dbcb_resp_entry(const char *fld, size_t fldlen)
{
  if (fld != 0) {
    cstate.writebuf.append_literal("\t");
    escape_string(cstate.writebuf, fld, fld + fldlen);
  } else {
    static const char t[] = "\t\0";
    cstate.writebuf.append(t, t + 2);
  }
}

void
hstcpsvr_conn::dbcb_resp_end()
{
  cstate.writebuf.append_literal("\n");
  cstate.resp_begin_pos = 0;
}

void
hstcpsvr_conn::dbcb_resp_cancel()
{
  cstate.writebuf.resize(cstate.resp_begin_pos);
  cstate.resp_begin_pos = 0;
}

struct hstcpsvr_worker : public hstcpsvr_worker_i, private noncopyable {
  hstcpsvr_worker(const hstcpsvr_worker_arg& arg);
  virtual void run();
 private:
  const hstcpsvr_shared_c& cshared;
  volatile hstcpsvr_shared_v& vshared;
  long worker_id;
  dbcontext_ptr dbctx;
  hstcpsvr_conns_type conns; /* conns refs dbctx */
  time_t last_check_time;
  std::vector<pollfd> pfds;
  #ifdef __linux__
  std::vector<epoll_event> events_vec;
  auto_file epoll_fd;
  #endif
  bool accept_enabled;
  int accept_balance;
  std::vector<string_ref> invalues_work;
  std::vector<record_filter> filters_work;
 private:
  int run_one_nb();
  int run_one_ep();
  void execute_lines(hstcpsvr_conn& conn);
  void execute_line(char *start, char *finish, hstcpsvr_conn& conn);
  void do_open_index(char *start, char *finish, hstcpsvr_conn& conn);
  void do_exec_on_index(char *cmd_begin, char *cmd_end, char *start,
    char *finish, hstcpsvr_conn& conn);
  void do_authorization(char *start, char *finish, hstcpsvr_conn& conn);
};

hstcpsvr_worker::hstcpsvr_worker(const hstcpsvr_worker_arg& arg)
  : cshared(*arg.cshared), vshared(*arg.vshared), worker_id(arg.worker_id),
    dbctx(cshared.dbptr->create_context(cshared.for_write_flag)),
    last_check_time(time(0)), accept_enabled(true), accept_balance(0)
{
  #ifdef __linux__
  if (cshared.sockargs.use_epoll) {
    epoll_fd.reset(epoll_create(10));
    if (epoll_fd.get() < 0) {
      fatal_abort("epoll_create");
    }
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.ptr = 0;
    if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, cshared.listen_fd.get(), &ev)
      != 0) {
      fatal_abort("epoll_ctl EPOLL_CTL_ADD");
    }
    events_vec.resize(10240);
  }
  #endif
  accept_balance = cshared.conf.get_int("accept_balance", 0);
}

namespace {

struct thr_init {
  thr_init(const dbcontext_ptr& dc, volatile int& shutdown_flag) : dbctx(dc) {
    dbctx->init_thread(this, shutdown_flag);
  }
  ~thr_init() {
    dbctx->term_thread();
  }
  const dbcontext_ptr& dbctx;
};

}; // namespace

void
hstcpsvr_worker::run()
{
  thr_init initobj(dbctx, vshared.shutdown);

  #ifdef __linux__
  if (cshared.sockargs.use_epoll) {
    while (!vshared.shutdown && dbctx->check_alive()) {
      run_one_ep();
    }
  } else if (cshared.sockargs.nonblocking) {
    while (!vshared.shutdown && dbctx->check_alive()) {
      run_one_nb();
    }
  } else {
    /* UNUSED */
    fatal_abort("run_one");
  }
  #else
  while (!vshared.shutdown && dbctx->check_alive()) {
    run_one_nb();
  }
  #endif
}

int
hstcpsvr_worker::run_one_nb()
{
  size_t nfds = 0;
  /* CLIENT SOCKETS */
  for (hstcpsvr_conns_type::const_iterator i = conns.begin();
    i != conns.end(); ++i) {
    if (pfds.size() <= nfds) {
      pfds.resize(nfds + 1);
    }
    pollfd& pfd = pfds[nfds++];
    pfd.fd = (*i)->fd.get();
    short ev = 0;
    if ((*i)->cstate.writebuf.size() != 0) {
      ev = POLLOUT;
    } else {
      ev = POLLIN;
    }
    pfd.events = pfd.revents = ev;
  }
  /* LISTENER */
  {
    const size_t cpt = cshared.nb_conn_per_thread;
    const short ev = (cpt > nfds) ? POLLIN : 0;
    if (pfds.size() <= nfds) {
      pfds.resize(nfds + 1);
    }
    pollfd& pfd = pfds[nfds++];
    pfd.fd = cshared.listen_fd.get();
    pfd.events = pfd.revents = ev;
  }
  /* POLL */
  const int npollev = poll(&pfds[0], nfds, 1 * 1000);
  dbctx->set_statistics(conns.size(), npollev);
  const time_t now = time(0);
  size_t j = 0;
  const short mask_in = ~POLLOUT;
  const short mask_out = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
  /* READ */
  for (hstcpsvr_conns_type::iterator i = conns.begin(); i != conns.end();
    ++i, ++j) {
    pollfd& pfd = pfds[j];
    if ((pfd.revents & mask_in) == 0) {
      continue;
    }
    hstcpsvr_conn& conn = **i;
    if (conn.read_more()) {
      if (conn.cstate.readbuf.size() > 0) {
	const char ch = conn.cstate.readbuf.begin()[0];
	if (ch == 'Q') {
	  vshared.shutdown = 1;
	} else if (ch == '/') {
	  conn.cstate.readbuf.clear();
	  conn.cstate.find_nl_pos = 0;
	  conn.cstate.writebuf.clear();
	  conn.read_finished = true;
	  conn.write_finished = true;
	}
      }
      conn.nb_last_io = now;
    }
  }
  /* EXECUTE */
  j = 0;
  for (hstcpsvr_conns_type::iterator i = conns.begin(); i != conns.end();
    ++i, ++j) {
    pollfd& pfd = pfds[j];
    if ((pfd.revents & mask_in) == 0 || (*i)->cstate.readbuf.size() == 0) {
      continue;
    }
    execute_lines(**i);
  }
  /* COMMIT */
  dbctx->unlock_tables_if();
  const bool commit_error = dbctx->get_commit_error();
  dbctx->clear_error();
  /* WRITE/CLOSE */
  j = 0;
  for (hstcpsvr_conns_type::iterator i = conns.begin(); i != conns.end();
    ++j) {
    pollfd& pfd = pfds[j];
    hstcpsvr_conn& conn = **i;
    hstcpsvr_conns_type::iterator icur = i;
    ++i;
    if (commit_error) {
      conn.reset();
      continue;
    }
    if ((pfd.revents & (mask_out | mask_in)) != 0) {
      if (conn.write_more()) {
	conn.nb_last_io = now;
      }
    }
    if (cshared.sockargs.timeout != 0 &&
      conn.nb_last_io + cshared.sockargs.timeout < now) {
      conn.reset();
    }
    if (conn.closed() || conn.ok_to_close()) {
      conns.erase_ptr(icur);
    }
  }
  /* ACCEPT */
  {
    pollfd& pfd = pfds[nfds - 1];
    if ((pfd.revents & mask_in) != 0) {
      std::auto_ptr<hstcpsvr_conn> c(new hstcpsvr_conn());
      c->nonblocking = true;
      c->readsize = cshared.readsize;
      c->accept(cshared);
      if (c->fd.get() >= 0) {
	if (fcntl(c->fd.get(), F_SETFL, O_NONBLOCK) != 0) {
	  fatal_abort("F_SETFL O_NONBLOCK");
	}
	c->nb_last_io = now;
	conns.push_back_ptr(c);
      } else {
	/* errno == 11 (EAGAIN) is not a fatal error. */
	DENA_VERBOSE(100, fprintf(stderr,
	  "accept failed: errno=%d (not fatal)\n", errno));
      }
    }
  }
  DENA_VERBOSE(30, fprintf(stderr, "nb: %p nfds=%zu cns=%zu\n", this, nfds,
    conns.size()));
  if (conns.empty()) {
    dbctx->close_tables_if();
  }
  dbctx->set_statistics(conns.size(), 0);
  return 0;
}

#ifdef __linux__
int
hstcpsvr_worker::run_one_ep()
{
  epoll_event *const events = &events_vec[0];
  const size_t num_events = events_vec.size();
  const time_t now = time(0);
  size_t in_count = 0, out_count = 0, accept_count = 0;
  int nfds = epoll_wait(epoll_fd.get(), events, num_events, 1000);
  /* READ/ACCEPT */
  dbctx->set_statistics(conns.size(), nfds);
  for (int i = 0; i < nfds; ++i) {
    epoll_event& ev = events[i];
    if ((ev.events & EPOLLIN) == 0) {
      continue;
    }
    hstcpsvr_conn *const conn = static_cast<hstcpsvr_conn *>(ev.data.ptr);
    if (conn == 0) {
      /* listener */
      ++accept_count;
      DBG_EP(fprintf(stderr, "IN listener\n"));
      std::auto_ptr<hstcpsvr_conn> c(new hstcpsvr_conn());
      c->nonblocking = true;
      c->readsize = cshared.readsize;
      c->accept(cshared);
      if (c->fd.get() >= 0) {
	if (fcntl(c->fd.get(), F_SETFL, O_NONBLOCK) != 0) {
	  fatal_abort("F_SETFL O_NONBLOCK");
	}
	epoll_event cev;
        memset(&cev, 0, sizeof(cev));
	cev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	cev.data.ptr = c.get();
	c->nb_last_io = now;
	const int fd = c->fd.get();
	conns.push_back_ptr(c);
	conns.back()->conns_iter = --conns.end();
	if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, fd, &cev) != 0) {
	  fatal_abort("epoll_ctl EPOLL_CTL_ADD");
	}
      } else {
	DENA_VERBOSE(100, fprintf(stderr,
	  "accept failed: errno=%d (not fatal)\n", errno));
      }
    } else {
      /* client connection */
      ++in_count;
      DBG_EP(fprintf(stderr, "IN client\n"));
      bool more_data = false;
      while (conn->read_more(&more_data)) {
	DBG_EP(fprintf(stderr, "IN client read_more\n"));
	conn->nb_last_io = now;
	if (!more_data) {
	  break;
	}
      }
    }
  }
  /* EXECUTE */
  for (int i = 0; i < nfds; ++i) {
    epoll_event& ev = events[i];
    hstcpsvr_conn *const conn = static_cast<hstcpsvr_conn *>(ev.data.ptr);
    if ((ev.events & EPOLLIN) == 0 || conn == 0 ||
      conn->cstate.readbuf.size() == 0) {
      continue;
    }
    const char ch = conn->cstate.readbuf.begin()[0];
    if (ch == 'Q') {
      vshared.shutdown = 1;
    } else if (ch == '/') {
      conn->cstate.readbuf.clear();
      conn->cstate.find_nl_pos = 0;
      conn->cstate.writebuf.clear();
      conn->read_finished = true;
      conn->write_finished = true;
    } else {
      execute_lines(*conn);
    }
  }
  /* COMMIT */
  dbctx->unlock_tables_if();
  const bool commit_error = dbctx->get_commit_error();
  dbctx->clear_error();
  /* WRITE */
  for (int i = 0; i < nfds; ++i) {
    epoll_event& ev = events[i];
    hstcpsvr_conn *const conn = static_cast<hstcpsvr_conn *>(ev.data.ptr);
    if (commit_error && conn != 0) {
      conn->reset();
      continue;
    }
    if ((ev.events & EPOLLOUT) == 0) {
      continue;
    }
    ++out_count;
    if (conn == 0) {
      /* listener */
      DBG_EP(fprintf(stderr, "OUT listener\n"));
    } else {
      /* client connection */
      DBG_EP(fprintf(stderr, "OUT client\n"));
      bool more_data = false;
      while (conn->write_more(&more_data)) {
	DBG_EP(fprintf(stderr, "OUT client write_more\n"));
	conn->nb_last_io = now;
	if (!more_data) {
	  break;
	}
      }
    }
  }
  /* CLOSE */
  for (int i = 0; i < nfds; ++i) {
    epoll_event& ev = events[i];
    hstcpsvr_conn *const conn = static_cast<hstcpsvr_conn *>(ev.data.ptr);
    if (conn != 0 && conn->ok_to_close()) {
      DBG_EP(fprintf(stderr, "CLOSE close\n"));
      conns.erase_ptr(conn->conns_iter);
    }
  }
  /* TIMEOUT & cleanup */
  if (last_check_time + 10 < now) {
    for (hstcpsvr_conns_type::iterator i = conns.begin();
      i != conns.end(); ) {
      hstcpsvr_conns_type::iterator icur = i;
      ++i;
      if (cshared.sockargs.timeout != 0 &&
	(*icur)->nb_last_io + cshared.sockargs.timeout < now) {
	conns.erase_ptr((*icur)->conns_iter);
      }
    }
    last_check_time = now;
    DENA_VERBOSE(20, fprintf(stderr, "ep: %p nfds=%d cns=%zu\n", this, nfds,
      conns.size()));
  }
  DENA_VERBOSE(30, fprintf(stderr, "%p in=%zu out=%zu ac=%zu, cns=%zu\n",
    this, in_count, out_count, accept_count, conns.size()));
  if (conns.empty()) {
    dbctx->close_tables_if();
  }
  /* STATISTICS */
  const size_t num_conns = conns.size();
  dbctx->set_statistics(num_conns, 0);
  /* ENABLE/DISABLE ACCEPT */
  if (accept_balance != 0) {
    cshared.thread_num_conns[worker_id] = num_conns;
    size_t total_num_conns = 0;
    for (long i = 0; i < cshared.num_threads; ++i) {
      total_num_conns += cshared.thread_num_conns[i];
    }
    bool e_acc = false;
    if (num_conns < 10 ||
      total_num_conns * 2 > num_conns * cshared.num_threads) {
      e_acc = true;
    }
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.ptr = 0;
    if (e_acc == accept_enabled) {
    } else if (e_acc) {
      if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, cshared.listen_fd.get(), &ev)
	!= 0) {
	fatal_abort("epoll_ctl EPOLL_CTL_ADD");
      }
    } else {
      if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_DEL, cshared.listen_fd.get(), &ev)
	!= 0) {
	fatal_abort("epoll_ctl EPOLL_CTL_ADD");
      }
    }
    accept_enabled = e_acc;
  }
  return 0;
}
#endif 

void
hstcpsvr_worker::execute_lines(hstcpsvr_conn& conn)
{
  DBG_MULTI(int cnt = 0);
  dbconnstate& cstate = conn.cstate;
  char *buf_end = cstate.readbuf.end();
  char *line_begin = cstate.readbuf.begin();
  char *find_pos = line_begin + cstate.find_nl_pos;
  while (true) {
    char *const nl = memchr_char(find_pos, '\n', buf_end - find_pos);
    if (nl == 0) {
      break;
    }
    char *const lf = (line_begin != nl && nl[-1] == '\r') ? nl - 1 : nl;
    DBG_MULTI(cnt++);
    execute_line(line_begin, lf, conn);
    find_pos = line_begin = nl + 1;
  }
  cstate.readbuf.erase_front(line_begin - cstate.readbuf.begin());
  cstate.find_nl_pos = cstate.readbuf.size();
  DBG_MULTI(fprintf(stderr, "cnt=%d\n", cnt));
}

void
hstcpsvr_worker::execute_line(char *start, char *finish, hstcpsvr_conn& conn)
{
  /* safe to modify, safe to dereference 'finish' */
  char *const cmd_begin = start;
  read_token(start, finish);
  char *const cmd_end = start;
  skip_one(start, finish);
  if (cmd_begin == cmd_end) {
    return conn.dbcb_resp_short(2, "cmd");
  }
  if (cmd_begin + 1 == cmd_end) {
    if (cmd_begin[0] == 'P') {
      if (cshared.require_auth && !conn.authorized) {
	return conn.dbcb_resp_short(3, "unauth");
      }
      return do_open_index(start, finish, conn);
    }
    if (cmd_begin[0] == 'A') {
      return do_authorization(start, finish, conn);
    }
  }
  if (cmd_begin[0] >= '0' && cmd_begin[0] <= '9') {
    if (cshared.require_auth && !conn.authorized) {
      return conn.dbcb_resp_short(3, "unauth");
    }
    return do_exec_on_index(cmd_begin, cmd_end, start, finish, conn);
  }
  return conn.dbcb_resp_short(2, "cmd");
}

void
hstcpsvr_worker::do_open_index(char *start, char *finish, hstcpsvr_conn& conn)
{
  const size_t pst_id = read_ui32(start, finish);
  skip_one(start, finish);
  /* dbname */
  char *const dbname_begin = start;
  read_token(start, finish);
  char *const dbname_end = start;
  skip_one(start, finish);
  /* tblname */
  char *const tblname_begin = start;
  read_token(start, finish);
  char *const tblname_end = start;
  skip_one(start, finish);
  /* idxname */
  char *const idxname_begin = start;
  read_token(start, finish);
  char *const idxname_end = start;
  skip_one(start, finish);
  /* retfields */
  char *const retflds_begin = start;
  read_token(start, finish);
  char *const retflds_end = start;
  skip_one(start, finish);
  /* filfields */
  char *const filflds_begin = start;
  read_token(start, finish);
  char *const filflds_end = start;
  dbname_end[0] = 0;
  tblname_end[0] = 0;
  idxname_end[0] = 0;
  retflds_end[0] = 0;
  filflds_end[0] = 0;
  cmd_open_args args;
  args.pst_id = pst_id;
  args.dbn = dbname_begin;
  args.tbl = tblname_begin;
  args.idx = idxname_begin;
  args.retflds = retflds_begin;
  args.filflds = filflds_begin;
  return dbctx->cmd_open(conn, args);
}

void
hstcpsvr_worker::do_exec_on_index(char *cmd_begin, char *cmd_end, char *start,
  char *finish, hstcpsvr_conn& conn)
{
  cmd_exec_args args;
  const size_t pst_id = read_ui32(cmd_begin, cmd_end);
  if (pst_id >= conn.cstate.prep_stmts.size()) {
    return conn.dbcb_resp_short(2, "stmtnum");
  }
  args.pst = &conn.cstate.prep_stmts[pst_id];
  char *const op_begin = start;
  read_token(start, finish);
  char *const op_end = start;
  args.op = string_ref(op_begin, op_end);
  skip_one(start, finish);
  const uint32_t fldnum = read_ui32(start, finish);
  string_ref *const flds = DENA_ALLOCA_ALLOCATE(string_ref, fldnum);
  auto_alloca_free<string_ref> flds_autofree(flds);
  args.kvals = flds;
  args.kvalslen = fldnum;
  for (size_t i = 0; i < fldnum; ++i) {
    skip_one(start, finish);
    char *const f_begin = start;
    read_token(start, finish);
    char *const f_end = start;
    if (is_null_expression(f_begin, f_end)) {
      /* null */
      flds[i] = string_ref();
    } else {
      /* non-null */
      char *wp = f_begin;
      unescape_string(wp, f_begin, f_end);
      flds[i] = string_ref(f_begin, wp - f_begin);
    }
  }
  skip_one(start, finish);
  args.limit = read_ui32(start, finish);
  skip_one(start, finish);
  args.skip = read_ui32(start, finish);
  if (start == finish) {
    /* simple query */
    return dbctx->cmd_exec(conn, args);
  }
  /* has more options */
  skip_one(start, finish);
  /* in-clause */
  if (start[0] == '@') {
    read_token(start, finish); /* '@' */
    skip_one(start, finish);
    args.invalues_keypart = read_ui32(start, finish);
    skip_one(start, finish);
    args.invalueslen = read_ui32(start, finish);
    if (args.invalueslen <= 0) {
      return conn.dbcb_resp_short(2, "invalueslen");
    }
    if (invalues_work.size() < args.invalueslen) {
      invalues_work.resize(args.invalueslen);
    }
    args.invalues = &invalues_work[0];
    for (uint32_t i = 0; i < args.invalueslen; ++i) {
      skip_one(start, finish);
      char *const invalue_begin = start;
      read_token(start, finish);
      char *const invalue_end = start;
      char *wp = invalue_begin;
      unescape_string(wp, invalue_begin, invalue_end);
      invalues_work[i] = string_ref(invalue_begin, wp - invalue_begin);
    }
    skip_one(start, finish);
  }
  if (start == finish) {
    /* no more options */
    return dbctx->cmd_exec(conn, args);
  }
  /* filters */
  size_t filters_count = 0;
  while (start != finish && (start[0] == 'W' || start[0] == 'F')) {
    char *const filter_type_begin = start;
    read_token(start, finish);
    char *const filter_type_end = start;
    skip_one(start, finish);
    char *const filter_op_begin = start;
    read_token(start, finish);
    char *const filter_op_end = start;
    skip_one(start, finish);
    const uint32_t ff_offset = read_ui32(start, finish);
    skip_one(start, finish);
    char *const filter_val_begin = start;
    read_token(start, finish);
    char *const filter_val_end = start;
    skip_one(start, finish);
    if (filters_work.size() <= filters_count) {
      filters_work.resize(filters_count + 1);
    }
    record_filter& fi = filters_work[filters_count];
    if (filter_type_end != filter_type_begin + 1) {
      return conn.dbcb_resp_short(2, "filtertype");
    }
    fi.filter_type = (filter_type_begin[0] == 'W')
      ? record_filter_type_break : record_filter_type_skip;
    const uint32_t num_filflds = args.pst->get_filter_fields().size();
    if (ff_offset >= num_filflds) {
      return conn.dbcb_resp_short(2, "filterfld");
    }
    fi.op = string_ref(filter_op_begin, filter_op_end);
    fi.ff_offset = ff_offset;
    if (is_null_expression(filter_val_begin, filter_val_end)) {
      /* null */
      fi.val = string_ref();
    } else {
      /* non-null */
      char *wp = filter_val_begin;
      unescape_string(wp, filter_val_begin, filter_val_end);
      fi.val = string_ref(filter_val_begin, wp - filter_val_begin);
    }
    ++filters_count;
  }
  if (filters_count > 0) {
    if (filters_work.size() <= filters_count) {
      filters_work.resize(filters_count + 1);
    }
    filters_work[filters_count].op = string_ref(); /* sentinel */
    args.filters = &filters_work[0];
  } else {
    args.filters = 0;
  }
  if (start == finish) {
    /* no modops */
    return dbctx->cmd_exec(conn, args);
  }
  /* has modops */
  char *const mod_op_begin = start;
  read_token(start, finish);
  char *const mod_op_end = start;
  args.mod_op = string_ref(mod_op_begin, mod_op_end);
  const size_t num_uvals = args.pst->get_ret_fields().size();
  string_ref *const uflds = DENA_ALLOCA_ALLOCATE(string_ref, num_uvals);
  auto_alloca_free<string_ref> uflds_autofree(uflds);
  for (size_t i = 0; i < num_uvals; ++i) {
    skip_one(start, finish);
    char *const f_begin = start;
    read_token(start, finish);
    char *const f_end = start;
    if (is_null_expression(f_begin, f_end)) {
      /* null */
      uflds[i] = string_ref();
    } else {
      /* non-null */
      char *wp = f_begin;
      unescape_string(wp, f_begin, f_end);
      uflds[i] = string_ref(f_begin, wp - f_begin);
    }
  }
  args.uvals = uflds;
  return dbctx->cmd_exec(conn, args);
}

void
hstcpsvr_worker::do_authorization(char *start, char *finish,
  hstcpsvr_conn& conn)
{
  /* auth type */
  char *const authtype_begin = start;
  read_token(start, finish);
  char *const authtype_end = start;
  const size_t authtype_len = authtype_end - authtype_begin;
  skip_one(start, finish);
  /* key */
  char *const key_begin = start;
  read_token(start, finish);
  char *const key_end = start;
  const size_t key_len = key_end - key_begin;
  authtype_end[0] = 0;
  key_end[0] = 0;
  char *wp = key_begin;
  unescape_string(wp, key_begin, key_end);
  if (authtype_len != 1 || authtype_begin[0] != '1') {
    return conn.dbcb_resp_short(3, "authtype");
  }
  if (cshared.plain_secret.size() == key_len &&
    memcmp(cshared.plain_secret.data(), key_begin, key_len) == 0) {
    conn.authorized = true;
  } else {
    conn.authorized = false;
  }
  if (!conn.authorized) {
    return conn.dbcb_resp_short(3, "unauth");
  } else {
    return conn.dbcb_resp_short(0, "");
  }
}

hstcpsvr_worker_ptr
hstcpsvr_worker_i::create(const hstcpsvr_worker_arg& arg)
{
  return hstcpsvr_worker_ptr(new hstcpsvr_worker(arg));
}

};

