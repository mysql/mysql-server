
// vim:sw=2:ai

#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include <stdlib.h>
#include <memory>
#include <errno.h>
#include <mysql.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.hpp"
#include "auto_ptrcontainer.hpp"
#include "socket.hpp"
#include "hstcpcli.hpp"
#include "string_util.hpp"
#include "mutex.hpp"

namespace dena {

struct auto_mysql : private noncopyable {
  auto_mysql() : db(0) {
    reset();
  }
  ~auto_mysql() {
    if (db) {
      mysql_close(db);
    }
  }
  void reset() {
    if (db) {
      mysql_close(db);
    }
    if ((db = mysql_init(0)) == 0) {
      fatal_exit("failed to initialize mysql client");
    }
  }
  operator MYSQL *() const { return db; }
 private:
  MYSQL *db;
};

struct auto_mysql_res : private noncopyable {
  auto_mysql_res(MYSQL *db) {
    res = mysql_store_result(db);
  }
  ~auto_mysql_res() {
    if (res) {
      mysql_free_result(res);
    }
  }
  operator MYSQL_RES *() const { return res; }
 private:
  MYSQL_RES *res;
};

struct auto_mysql_stmt : private noncopyable {
  auto_mysql_stmt(MYSQL *db) {
    stmt = mysql_stmt_init(db);
  }
  ~auto_mysql_stmt() {
    if (stmt) {
      mysql_stmt_close(stmt);
    }
  }
  operator MYSQL_STMT *() const { return stmt; }
 private:
  MYSQL_STMT *stmt;
};

double
gettimeofday_double()
{
  struct timeval tv = { };
  if (gettimeofday(&tv, 0) != 0) {
    fatal_abort("gettimeofday");
  }
  return static_cast<double>(tv.tv_usec) / 1000000 + tv.tv_sec;
}

struct record_value {
  mutex lock;
  bool deleted;
  bool unknown_state;
  std::string key;
  std::vector<std::string> values;
  record_value() : deleted(true), unknown_state(false) { }
};

struct hs_longrun_shared {
  config conf;
  socket_args arg;
  int verbose;
  long num_threads;
  int usleep;
  volatile mutable int running;
  auto_ptrcontainer< std::vector<record_value *> > records;
  hs_longrun_shared() : verbose(0), num_threads(0), usleep(0), running(1) { }
};

struct thread_base {
  thread_base() : need_join(false), stack_size(256 * 1024) { }
  virtual ~thread_base() {
    join();
  }
  virtual void run() = 0;
  void start() {
    if (!start_nothrow()) {
      fatal_abort("thread::start");
    }
  }
  bool start_nothrow() {
    if (need_join) {
      return need_join; /* true */
    }
    void *const arg = this;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
      fatal_abort("pthread_attr_init");
    }
    if (pthread_attr_setstacksize(&attr, stack_size) != 0) {
      fatal_abort("pthread_attr_setstacksize");
    }
    const int r = pthread_create(&thr, &attr, thread_main, arg);
    if (pthread_attr_destroy(&attr) != 0) {
      fatal_abort("pthread_attr_destroy");
    }
    if (r != 0) {
      return need_join; /* false */
    }
    need_join = true;
    return need_join; /* true */
  }
  void join() {
    if (!need_join) {
      return;
    }
    int e = 0;
    if ((e = pthread_join(thr, 0)) != 0) {
      fatal_abort("pthread_join");
    }
    need_join = false;
  }
 private:
  static void *thread_main(void *arg) {
    thread_base *p = static_cast<thread_base *>(arg);
    p->run();
    return 0;
  }
 private:
  pthread_t thr;
  bool need_join;
  size_t stack_size;
};

struct hs_longrun_stat {
  unsigned long long verify_error_count;
  unsigned long long runtime_error_count;
  unsigned long long unknown_count;
  unsigned long long success_count;
  hs_longrun_stat()
    : verify_error_count(0), runtime_error_count(0),
      unknown_count(0), success_count(0) { }
  void add(const hs_longrun_stat& x) {
    verify_error_count += x.verify_error_count;
    runtime_error_count += x.runtime_error_count;
    unknown_count += x.unknown_count;
    success_count += x.success_count;
  }
};

struct hs_longrun_thread_base : public thread_base {
  struct arg_type {
    int id;
    std::string worker_type;
    char op;
    int lock_flag;
    const hs_longrun_shared& sh;
    arg_type(int id, const std::string& worker_type, char op, int lock_flag,
      const hs_longrun_shared& sh)
      : id(id), worker_type(worker_type), op(op), lock_flag(lock_flag),
	sh(sh) { }
  };
  arg_type arg;
  hs_longrun_stat stat;
  drand48_data randbuf;
  unsigned int seed;
  hs_longrun_thread_base(const arg_type& arg)
    : arg(arg), seed(0) {
    seed = time(0) + arg.id + 1;
    srand48_r(seed, &randbuf);
  }
  virtual ~hs_longrun_thread_base() { }
  virtual void run() = 0;
  size_t rand_record() {
    double v = 0;
    drand48_r(&randbuf, &v);
    const size_t sz = arg.sh.records.size();
    size_t r = size_t(v * sz);
    if (r >= sz) {
      r = 0;
    }
    return r;
  }
  int verify_update(const std::string& k, const std::string& v1,
    const std::string& v2, const std::string& v3, record_value& rec,
    uint32_t num_rows, bool cur_unknown_state);
  int verify_read(const std::string& k, uint32_t num_rows, uint32_t num_flds,
    const std::string rrec[4], record_value& rec);
  int verify_readnolock(const std::string& k, uint32_t num_rows,
    uint32_t num_flds, const std::string rrec[4]);
};

int
hs_longrun_thread_base::verify_update(const std::string& k,
  const std::string& v1, const std::string& v2, const std::string& v3,
  record_value& rec, uint32_t num_rows, bool cur_unknown_state)
{
  const bool op_success = num_rows == 1;
  int ret = 0;
  if (!rec.unknown_state) {
    if (!rec.deleted && !op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_update_failure\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    } else if (rec.deleted && op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_update_success\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    }
  }
  if (op_success) {
    rec.values.resize(4);
    rec.values[0] = k;
    rec.values[1] = v1;
    rec.values[2] = v2;
    rec.values[3] = v3;
    if (ret == 0 && !rec.unknown_state) {
      ++stat.success_count;
    }
  }
  rec.unknown_state = cur_unknown_state;
  if (arg.sh.verbose >= 100 && ret == 0) {
    fprintf(stderr, "%s %s %s %s %s\n", arg.worker_type.c_str(),
      k.c_str(), v1.c_str(), v2.c_str(), v3.c_str());
  }
  return ret;
}

int
hs_longrun_thread_base::verify_read(const std::string& k,
  uint32_t num_rows, uint32_t num_flds, const std::string rrec[4],
  record_value& rec)
{
  const bool op_success = num_rows != 0;
  int ret = 0;
  if (!rec.unknown_state) {
    if (!rec.deleted && !op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_read_failure\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    } else if (rec.deleted && op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_read_success\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    } else if (num_flds != 4) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_read_fldnum %d\n",
	  arg.worker_type.c_str(), arg.id, k.c_str(),
	  static_cast<int>(num_flds));
      }
      ret = 1;
    } else if (rec.deleted) {
      /* nothing to verify */
    } else {
      int diff = 0;
      for (size_t i = 0; i < 4; ++i) {
	if (rec.values[i] == rrec[i]) {
	  /* ok */
	} else {
	  diff = 1;
	}
      }
      if (diff) {
	std::string mess;
	for (size_t i = 0; i < 4; ++i) {
	  const std::string& expected = rec.values[i];
	  const std::string& val = rrec[i];
	  mess += " " + val + "/" + expected;
	}
	if (arg.sh.verbose > 0) {
	  fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	    "unexpected_read_value %s\n",
	    arg.worker_type.c_str(), arg.id, k.c_str(), mess.c_str());
	}
	ret = 1;
      }
    }
  }
  if (arg.sh.verbose >= 100 && ret == 0) {
    fprintf(stderr, "%s %s\n", arg.worker_type.c_str(), k.c_str());
  }
  if (ret == 0 && !rec.unknown_state) {
    ++stat.success_count;
  }
  return ret;
}

int
hs_longrun_thread_base::verify_readnolock(const std::string& k,
  uint32_t num_rows, uint32_t num_flds, const std::string rrec[4])
{
  int ret = 0;
  if (num_rows != 1 || num_flds != 4) {
    ++stat.verify_error_count;
    if (arg.sh.verbose > 0) {
      fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	"unexpected_read_failure\n",
	arg.worker_type.c_str(), arg.id, k.c_str());
    }
    ret = 1;
  }
  if (arg.sh.verbose >= 100 && ret == 0) {
    fprintf(stderr, "%s -> %s %s %s %s %s\n", arg.worker_type.c_str(),
      k.c_str(), rrec[0].c_str(), rrec[1].c_str(), rrec[2].c_str(),
      rrec[3].c_str());
  }
  if (ret == 0) {
    ++stat.success_count;
  }
  return ret;
}

struct hs_longrun_thread_hs : public hs_longrun_thread_base {
  hs_longrun_thread_hs(const arg_type& arg)
    : hs_longrun_thread_base(arg) { }
  void run();
  int check_hs_error(const char *mess, record_value *rec);
  int op_insert(record_value& rec);
  int op_delete(record_value& rec);
  int op_update(record_value& rec);
  int op_read(record_value& rec);
  int op_readnolock(int k);
  hstcpcli_ptr cli;
  socket_args sockargs;
};

struct lock_guard : noncopyable {
  lock_guard(mutex& mtx) : mtx(mtx) {
    mtx.lock();
  }
  ~lock_guard() {
    mtx.unlock();
  }
  mutex& mtx;
};

string_ref
to_string_ref(const std::string& s)
{
  return string_ref(s.data(), s.size());
}

std::string
to_string(const string_ref& s)
{
  return std::string(s.begin(), s.size());
}

void
hs_longrun_thread_hs::run()
{
  config c = arg.sh.conf;
  if (arg.op == 'R' || arg.op == 'N') {
    c["port"] = to_stdstring(arg.sh.conf.get_int("hsport", 9998));
  } else {
    c["port"] = to_stdstring(arg.sh.conf.get_int("hsport_wr", 9999));
  }
  sockargs.set(c);

  while (arg.sh.running) {
    if (cli.get() == 0 || !cli->stable_point()) {
      cli = hstcpcli_i::create(sockargs);
      if (check_hs_error("connect", 0) != 0) {
	cli.reset();
	continue;
      }
      cli->request_buf_open_index(0, "hstestdb", "hstesttbl", "PRIMARY",
	"k,v1,v2,v3", "k,v1,v2,v3");
      cli->request_send();
      if (check_hs_error("openindex_send", 0) != 0) {
	cli.reset();
	continue;
      }
      size_t num_flds = 0;
      cli->response_recv(num_flds);
      if (check_hs_error("openindex_recv", 0) != 0) {
	cli.reset();
	continue;
      }
      cli->response_buf_remove();
    }
    const size_t rec_id = rand_record();
    if (arg.lock_flag) {
      record_value& rec = *arg.sh.records[rec_id];
      lock_guard g(rec.lock);
      int e = 0;
      switch (arg.op) {
      case 'I':
	e = op_insert(rec);
	break;
      case 'D':
	e = op_delete(rec);
	break;
      case 'U':
	e = op_update(rec);
	break;
      case 'R':
	e = op_read(rec);
	break;
      default:
	break;
      }
    } else {
      int e = 0;
      switch (arg.op) {
      case 'N':
	e = op_readnolock(rec_id);
	break;
      default:
	break;
      }
    }
  }
}

int
hs_longrun_thread_hs::op_insert(record_value& rec)
{
  const std::string k = rec.key;
  const std::string v1 = "iv1_" + k + "_" + to_stdstring(arg.id);
  const std::string v2 = "iv2_" + k + "_" + to_stdstring(arg.id);
  const std::string v3 = "iv3_" + k + "_" + to_stdstring(arg.id);
  const string_ref op_ref("+", 1);
  const string_ref op_args[4] = {
    to_string_ref(k),
    to_string_ref(v1),
    to_string_ref(v2),
    to_string_ref(v3)
  };
  cli->request_buf_exec_generic(0, op_ref, op_args, 4, 1, 0,
    string_ref(), 0, 0, 0, 0);
  cli->request_send();
  if (check_hs_error("op_insert_send", &rec) != 0) { return 1; }
  size_t numflds = 0;
  cli->response_recv(numflds);
  if (arg.sh.verbose > 10) {
    const string_ref *row = cli->get_next_row();
    fprintf(stderr, "HS op=+ errrcode=%d errmess=[%s]\n", cli->get_error_code(),
      row ? to_string(row[0]).c_str() : "");
  }
  const bool op_success = cli->get_error_code() == 0;
  int ret = 0;
  if (!rec.unknown_state) {
    if (rec.deleted && !op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_insert_failure\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    } else if (!rec.deleted && op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_insert_success\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    }
  } else {
    ++stat.unknown_count;
  }
  if (op_success) {
    rec.values.resize(4);
    rec.values[0] = k;
    rec.values[1] = v1;
    rec.values[2] = v2;
    rec.values[3] = v3;
    rec.deleted = false;
    if (arg.sh.verbose >= 100 && ret == 0) {
      fprintf(stderr, "HS_INSERT %s %s %s %s\n", k.c_str(), v1.c_str(),
	v2.c_str(), v3.c_str());
    }
    if (ret == 0 && !rec.unknown_state) {
      ++stat.success_count;
    }
    rec.unknown_state = false;
  }
  cli->response_buf_remove();
  return ret;
}

int
hs_longrun_thread_hs::op_delete(record_value& rec)
{
  const std::string k = rec.key;
  const string_ref op_ref("=", 1);
  const string_ref op_args[1] = {
    to_string_ref(k),
  };
  const string_ref modop_ref("D", 1);
  cli->request_buf_exec_generic(0, op_ref, op_args, 1, 1, 0,
    modop_ref, 0, 0, 0, 0);
  cli->request_send();
  if (check_hs_error("op_delete_send", &rec) != 0) { return 1; }
  size_t numflds = 0;
  cli->response_recv(numflds);
  if (check_hs_error("op_delete_recv", &rec) != 0) { return 1; }
  const string_ref *row = cli->get_next_row();
  const bool op_success = (numflds > 0 && row != 0 &&
    to_string(row[0]) == "1");
  int ret = 0;
  if (!rec.unknown_state) {
    if (!rec.deleted && !op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_delete_failure\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    } else if (rec.deleted && op_success) {
      ++stat.verify_error_count;
      if (arg.sh.verbose > 0) {
	fprintf(stderr, "VERIFY_ERROR: %s wid=%d k=%s "
	  "unexpected_delete_success\n",
	  arg.worker_type.c_str(), arg.id, k.c_str());
      }
      ret = 1;
    }
  }
  cli->response_buf_remove();
  if (op_success) {
    rec.deleted = true;
    if (ret == 0 && !rec.unknown_state) {
      ++stat.success_count;
    }
    rec.unknown_state = false;
  }
  if (arg.sh.verbose >= 100 && ret == 0) {
    fprintf(stderr, "HS_DELETE %s\n", k.c_str());
  }
  return ret;
}

int
hs_longrun_thread_hs::op_update(record_value& rec)
{
  const std::string k = rec.key;
  const std::string v1 = "uv1_" + k + "_" + to_stdstring(arg.id);
  const std::string v2 = "uv2_" + k + "_" + to_stdstring(arg.id);
  const std::string v3 = "uv3_" + k + "_" + to_stdstring(arg.id);
  const string_ref op_ref("=", 1);
  const string_ref op_args[1] = {
    to_string_ref(k),
  };
  const string_ref modop_ref("U", 1);
  const string_ref modop_args[4] = {
    to_string_ref(k),
    to_string_ref(v1),
    to_string_ref(v2),
    to_string_ref(v3)
  };
  cli->request_buf_exec_generic(0, op_ref, op_args, 1, 1, 0,
    modop_ref, modop_args, 4, 0, 0);
  cli->request_send();
  if (check_hs_error("op_update_send", &rec) != 0) { return 1; }
  size_t numflds = 0;
  cli->response_recv(numflds);
  if (check_hs_error("op_update_recv", &rec) != 0) { return 1; }
  const string_ref *row = cli->get_next_row();
  uint32_t num_rows = row
    ? atoi_uint32_nocheck(row[0].begin(), row[0].end()) : 0;
  cli->response_buf_remove();
  const bool cur_unknown_state = (num_rows == 1);
  return verify_update(k, v1, v2, v3, rec, num_rows, cur_unknown_state);
}

int
hs_longrun_thread_hs::op_read(record_value& rec)
{
  const std::string k = rec.key;
  const string_ref op_ref("=", 1);
  const string_ref op_args[1] = {
    to_string_ref(k),
  };
  cli->request_buf_exec_generic(0, op_ref, op_args, 1, 1, 0,
    string_ref(), 0, 0, 0, 0);
  cli->request_send();
  if (check_hs_error("op_read_send", 0) != 0) { return 1; }
  size_t num_flds = 0;
  size_t num_rows = 0;
  cli->response_recv(num_flds);
  if (check_hs_error("op_read_recv", 0) != 0) { return 1; }
  const string_ref *row = cli->get_next_row();
  std::string rrec[4];
  if (row != 0 && num_flds == 4) {
    for (int i = 0; i < 4; ++i) {
      rrec[i] = to_string(row[i]);
    }
    ++num_rows;
  }
  row = cli->get_next_row();
  if (row != 0) {
    ++num_rows;
  }
  cli->response_buf_remove();
  return verify_read(k, num_rows, num_flds, rrec, rec);
}

int
hs_longrun_thread_hs::op_readnolock(int key)
{
  const std::string k = to_stdstring(key);
  const string_ref op_ref("=", 1);
  const string_ref op_args[1] = {
    to_string_ref(k),
  };
  cli->request_buf_exec_generic(0, op_ref, op_args, 1, 1, 0,
    string_ref(), 0, 0, 0, 0);
  cli->request_send();
  if (check_hs_error("op_read_send", 0) != 0) { return 1; }
  size_t num_flds = 0;
  size_t num_rows = 0;
  cli->response_recv(num_flds);
  if (check_hs_error("op_read_recv", 0) != 0) { return 1; }
  const string_ref *row = cli->get_next_row();
  std::string rrec[4];
  if (row != 0 && num_flds == 4) {
    for (int i = 0; i < 4; ++i) {
      rrec[i] = to_string(row[i]);
    }
    ++num_rows;
  }
  row = cli->get_next_row();
  if (row != 0) {
    ++num_rows;
  }
  cli->response_buf_remove();
  return verify_readnolock(k, num_rows, num_flds, rrec);
}

int
hs_longrun_thread_hs::check_hs_error(const char *mess, record_value *rec)
{
  const int err = cli->get_error_code();
  if (err == 0) {
    return 0;
  }
  ++stat.runtime_error_count;
  if (arg.sh.verbose > 0) {
    const std::string estr = cli->get_error();
    fprintf(stderr, "RUNTIME_ERROR: op=%c wid=%d %s: %d %s\n",
      arg.op, arg.id, mess, err, estr.c_str());
  }
  if (rec) {
    rec->unknown_state = true;
  }
  return 1;
}

struct hs_longrun_thread_my : public hs_longrun_thread_base {
  hs_longrun_thread_my(const arg_type& arg)
    : hs_longrun_thread_base(arg), connected(false) { }
  void run();
  void show_mysql_error(const char *mess, record_value *rec);
  int op_insert(record_value& rec);
  int op_delete(record_value& rec);
  int op_update(record_value& rec);
  int op_delins(record_value& rec);
  int op_read(record_value& rec);
  auto_mysql db;
  bool connected;
};

void
hs_longrun_thread_my::run()
{
  const std::string mysql_host = arg.sh.conf.get_str("host", "localhost");
  const std::string mysql_user = arg.sh.conf.get_str("mysqluser", "root");
  const std::string mysql_passwd = arg.sh.conf.get_str("mysqlpass", "");
  const std::string mysql_dbname = "hstestdb";

  while (arg.sh.running) {
    if (!connected) {
      if (!mysql_real_connect(db, mysql_host.c_str(), mysql_user.c_str(),
	mysql_passwd.c_str(), mysql_dbname.c_str(), mysql_port, 0, 0)) {
	show_mysql_error("mysql_real_connect", 0);
	continue;
      }
    }
    connected = true;
    const size_t rec_id = rand_record();
    record_value& rec = *arg.sh.records[rec_id];
    lock_guard g(rec.lock);
    int e = 0;
    switch (arg.op) {
    #if 0
    case 'I':
      e = op_insert(rec);
      break;
    case 'D':
      e = op_delete(rec);
      break;
    case 'U':
      e = op_update(rec);
      break;
    #endif
    case 'T':
      e = op_delins(rec);
      break;
    case 'R':
      e = op_read(rec);
      break;
    default:
      break;
    }
  }
}

int
hs_longrun_thread_my::op_delins(record_value& rec)
{
  const std::string k = rec.key;
  const std::string v1 = "div1_" + k + "_" + to_stdstring(arg.id);
  const std::string v2 = "div2_" + k + "_" + to_stdstring(arg.id);
  const std::string v3 = "div3_" + k + "_" + to_stdstring(arg.id);
  int success = 0;
  bool cur_unknown_state = false;
  do {
    char query[1024];
    #if 1
    if (mysql_query(db, "begin") != 0) {
      if (arg.sh.verbose >= 20) {
	fprintf(stderr, "mysql: e=[%s] q=[%s]\n", mysql_error(db), "begin");
      }
      break;
    }
    #endif
    cur_unknown_state = true;
    snprintf(query, 1024,
      "delete from hstesttbl where k = '%s'", k.c_str());
    if (mysql_query(db, query) != 0) {
      if (arg.sh.verbose >= 20) {
	fprintf(stderr, "mysql: e=[%s] q=[%s]\n", mysql_error(db), query);
      }
      break;
    }
    if (mysql_affected_rows(db) != 1) {
      if (arg.sh.verbose >= 20) {
	fprintf(stderr, "mysql: notfound: [%s]\n", query);
      }
      break;
    }
    snprintf(query, 1024,
      "insert into hstesttbl values ('%s', '%s', '%s', '%s')",
      k.c_str(), v1.c_str(), v2.c_str(), v3.c_str());
    if (mysql_query(db, query) != 0) {
      if (arg.sh.verbose >= 20) {
	fprintf(stderr, "mysql: e=[%s] q=[%s]\n", mysql_error(db), query);
      }
      break;
    }
    #if 1
    if (mysql_query(db, "commit") != 0) {
      if (arg.sh.verbose >= 20) {
	fprintf(stderr, "mysql: e=[%s] q=[%s]\n", mysql_error(db), "commit");
      }
      break;
    }
    #endif
    success = true;
    cur_unknown_state = false;
  } while (false);
  return verify_update(k, v1, v2, v3, rec, (success != 0), cur_unknown_state);
}

int
hs_longrun_thread_my::op_read(record_value& rec)
{
  const std::string k = rec.key;
  char query[1024] = { 0 };
  const int len = snprintf(query, 1024,
    "select k,v1,v2,v3 from hstesttbl where k='%s'", k.c_str());
  const int r = mysql_real_query(db, query, len > 0 ? len : 0);
  if (r != 0) {
    show_mysql_error(query, 0);
    return 1;
  }
  MYSQL_ROW row = 0;
  unsigned long *lengths = 0;
  unsigned int num_rows = 0;
  unsigned int num_flds = 0;
  auto_mysql_res res(db);
  std::string rrec[4];
  if (res != 0) {
    num_flds = mysql_num_fields(res);
    row = mysql_fetch_row(res);
    if (row != 0) {
      lengths = mysql_fetch_lengths(res);
      if (num_flds == 4) {
	for (int i = 0; i < 4; ++i) {
	  rrec[i] = std::string(row[i], lengths[i]);
	}
      }
      ++num_rows;
      row = mysql_fetch_row(res);
      if (row != 0) {
	++num_rows;
      }
    }
  }
  return verify_read(k, num_rows, num_flds, rrec, rec);
}

void
hs_longrun_thread_my::show_mysql_error(const char *mess, record_value *rec)
{
  ++stat.runtime_error_count;
  if (arg.sh.verbose > 0) {
    fprintf(stderr, "RUNTIME_ERROR: op=%c wid=%d [%s]: %s\n",
      arg.op, arg.id, mess, mysql_error(db));
  }
  if (rec) {
    rec->unknown_state = true;
  }
  db.reset();
  connected = false;
}

void
mysql_do(MYSQL *db, const char *query)
{
  if (mysql_real_query(db, query, strlen(query)) != 0) {
    fprintf(stderr, "mysql: e=[%s] q=[%s]\n", mysql_error(db), query);
    fatal_exit("mysql_do");
  }
}

void
hs_longrun_init_table(const config& conf, int num_prepare,
  hs_longrun_shared& shared)
{
  const std::string mysql_host = conf.get_str("host", "localhost");
  const std::string mysql_user = conf.get_str("mysqluser", "root");
  const std::string mysql_passwd = conf.get_str("mysqlpass", "");
  const std::string mysql_dbname = "";
  auto_mysql db;
  if (!mysql_real_connect(db, mysql_host.c_str(), mysql_user.c_str(),
    mysql_passwd.c_str(), mysql_dbname.c_str(), mysql_port, 0, 0)) {
    fprintf(stderr, "mysql: error=[%s]\n", mysql_error(db));
    fatal_exit("hs_longrun_init_table");
  }
  mysql_do(db, "drop database if exists hstestdb");
  mysql_do(db, "create database hstestdb");
  mysql_do(db, "use hstestdb");
  mysql_do(db,
    "create table hstesttbl ("
    "k int primary key,"
    "v1 varchar(32) not null,"
    "v2 varchar(32) not null,"
    "v3 varchar(32) not null"
    ") character set utf8 collate utf8_bin engine = innodb");
  for (int i = 0; i < num_prepare; ++i) {
    const std::string i_str = to_stdstring(i);
    const std::string v1 = "pv1_" + i_str;
    const std::string v2 = "pv2_" + i_str;
    const std::string v3 = "pv3_" + i_str;
    char buf[1024];
    snprintf(buf, 1024, "insert into hstesttbl(k, v1, v2, v3) values"
      "(%d, '%s', '%s', '%s')", i, v1.c_str(), v2.c_str(), v3.c_str());
    mysql_do(db, buf);
    record_value *rec = shared.records[i];
    rec->key = i_str;
    rec->values.resize(4);
    rec->values[0] = i_str;
    rec->values[1] = v1;
    rec->values[2] = v2;
    rec->values[3] = v3;
    rec->deleted = false;
  }
}

int
hs_longrun_main(int argc, char **argv)
{
  hs_longrun_shared shared;
  parse_args(argc, argv, shared.conf);
  shared.conf["host"] = shared.conf.get_str("host", "localhost");
  shared.verbose = shared.conf.get_int("verbose", 1);
  const int table_size = shared.conf.get_int("table_size", 10000);
  for (int i = 0; i < table_size; ++i) {
    std::auto_ptr<record_value> rec(new record_value());
    rec->key = to_stdstring(i);
    shared.records.push_back_ptr(rec);
  }
  mysql_library_init(0, 0, 0);
  const int duration = shared.conf.get_int("duration", 10);
  const int num_hsinsert = shared.conf.get_int("num_hsinsert", 10);
  const int num_hsdelete = shared.conf.get_int("num_hsdelete", 10);
  const int num_hsupdate = shared.conf.get_int("num_hsupdate", 10);
  const int num_hsread = shared.conf.get_int("num_hsread", 10);
  const int num_myread = shared.conf.get_int("num_myread", 10);
  const int num_mydelins = shared.conf.get_int("num_mydelins", 10);
  int num_hsreadnolock = shared.conf.get_int("num_hsreadnolock", 10);
  const bool always_filled = (num_hsinsert == 0 && num_hsdelete == 0);
  if (!always_filled) {
    num_hsreadnolock = 0;
  }
  hs_longrun_init_table(shared.conf, always_filled ? table_size : 0,
    shared);
  /* create worker threads */
  static const struct thrtmpl_type {
    const char *type; char op; int num; int hs; int lock;
  } thrtmpl[] = {
    { "hsinsert", 'I', num_hsinsert, 1, 1 },
    { "hsdelete", 'D', num_hsdelete, 1, 1 },
    { "hsupdate", 'U', num_hsupdate, 1, 1 },
    { "hsread", 'R', num_hsread, 1, 1 },
    { "hsreadnolock", 'N', num_hsreadnolock, 1, 0 },
    { "myread", 'R', num_myread, 0, 1 },
    { "mydelins", 'T', num_mydelins, 0, 1 },
  };
  typedef auto_ptrcontainer< std::vector<hs_longrun_thread_base *> > thrs_type;
  thrs_type thrs;
  for (size_t i = 0; i < sizeof(thrtmpl)/sizeof(thrtmpl[0]); ++i) {
    const thrtmpl_type& e = thrtmpl[i];
    for (int j = 0; j < e.num; ++j) {
      int id = thrs.size();
      const hs_longrun_thread_hs::arg_type arg(id, e.type, e.op, e.lock,
	shared);
      std::auto_ptr<hs_longrun_thread_base> thr;
      if (e.hs) {
      	thr.reset(new hs_longrun_thread_hs(arg));
      } else {
	thr.reset(new hs_longrun_thread_my(arg));
      }
      thrs.push_back_ptr(thr);
    }
  }
  shared.num_threads = thrs.size();
  /* start threads */
  fprintf(stderr, "START\n");
  shared.running = 1;
  for (size_t i = 0; i < thrs.size(); ++i) {
    thrs[i]->start();
  }
  /* wait */
  sleep(duration);
  /* stop thread */
  shared.running = 0;
  for (size_t i = 0; i < thrs.size(); ++i) {
    thrs[i]->join();
  }
  fprintf(stderr, "DONE\n");
  /* summary */
  typedef std::map<std::string, hs_longrun_stat> stat_map;
  stat_map sm;
  for (size_t i = 0; i < thrs.size(); ++i) {
    hs_longrun_thread_base *const thr = thrs[i];
    const std::string wt = thr->arg.worker_type;
    hs_longrun_stat& v = sm[wt];
    v.add(thr->stat);
  }
  hs_longrun_stat total;
  for (stat_map::const_iterator i = sm.begin(); i != sm.end(); ++i) {
    if (i->second.verify_error_count != 0) {
      fprintf(stderr, "%s verify_error %llu\n", i->first.c_str(),
	i->second.verify_error_count);
    }
    if (i->second.runtime_error_count) {
      fprintf(stderr, "%s runtime_error %llu\n", i->first.c_str(),
	i->second.runtime_error_count);
    }
    if (i->second.unknown_count) {
      fprintf(stderr, "%s unknown %llu\n", i->first.c_str(),
	i->second.unknown_count);
    }
    fprintf(stderr, "%s success %llu\n", i->first.c_str(),
      i->second.success_count);
    total.add(i->second);
  }
  if (total.verify_error_count != 0) {
    fprintf(stderr, "TOTAL verify_error %llu\n", total.verify_error_count);
  }
  if (total.runtime_error_count != 0) {
    fprintf(stderr, "TOTAL runtime_error %llu\n", total.runtime_error_count);
  }
  if (total.unknown_count != 0) {
    fprintf(stderr, "TOTAL unknown %llu\n", total.unknown_count);
  }
  fprintf(stderr, "TOTAL success %llu\n", total.success_count);
  mysql_library_end();
  return 0;
}

};

int
main(int argc, char **argv)
{
  return dena::hs_longrun_main(argc, argv);
}

