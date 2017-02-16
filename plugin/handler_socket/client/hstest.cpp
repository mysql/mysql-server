
// vim:sw=2:ai

#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <vector>
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
#include "thread.hpp"
#include "hstcpcli.hpp"

#if __GNUC__ >= 4
long atomic_exchange_and_add(volatile long *valp, long c)
{
  return __sync_fetch_and_add(valp, c);
}
#else
#include <bits/atomicity.h>
using namespace __gnu_cxx;
long atomic_exchange_and_add(volatile long *valp, long c)
{
  return __exchange_and_add((volatile _Atomic_word *)valp, c);
}
#endif

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
      fatal_abort("failed to initialize mysql client");
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

namespace {

double
gettimeofday_double()
{
  struct timeval tv;
  if (gettimeofday(&tv, 0) != 0) {
    fatal_abort("gettimeofday");
  }
  return static_cast<double>(tv.tv_usec) / 1000000 + tv.tv_sec;
}

// unused
void
wait_close(int fd)
{
  char buf[1024];
  while (true) {
    int r = read(fd, buf, sizeof(buf));
    if (r <= 0) {
      break;
    }
  }
}

// unused
void
gentle_close(int fd)
{
  int r = shutdown(fd, SHUT_WR);
  if (r != 0) {
    return;
  }
  wait_close(fd);
}

};

struct hstest_shared {
  config conf;
  socket_args arg;
  int verbose;
  size_t loop;
  size_t pipe;
  char op;
  long num_threads;
  mutable volatile long count;
  mutable volatile long conn_count;
  long wait_conn;
  volatile char *keygen;
  long keygen_size;
  mutable volatile int enable_timing;
  int usleep;
  int dump;
  hstest_shared() : verbose(0), loop(0), pipe(0), op('G'), num_threads(0),
    count(0), conn_count(0), wait_conn(0), keygen(0), keygen_size(0),
    enable_timing(0), usleep(0), dump(0) { }
  void increment_count(unsigned int c = 1) const volatile {
    atomic_exchange_and_add(&count, c);
  }
  void increment_conn(unsigned int c) const volatile {
    atomic_exchange_and_add(&conn_count, c);
    while (wait_conn != 0 && conn_count < wait_conn) {
      sleep(1);
    }
    // fprintf(stderr, "wait_conn=%ld done\n", wait_conn);
  }
};

struct hstest_thread {
  struct arg_type {
    size_t id;
    const hstest_shared& sh;
    bool watch_flag;
    arg_type(size_t i, const hstest_shared& s, bool w)
      : id(i), sh(s), watch_flag(w) { }
  };
  hstest_thread(const arg_type& a) : arg(a), io_success_count(0),
    op_success_count(0), response_min(99999), response_max(0),
    response_sum(0), response_avg(0) { }
  void operator ()();
  void test_1();
  void test_2_3(int test_num);
  void test_4_5(int test_num);
  void test_6(int test_num);
  void test_7(int test_num);
  void test_8(int test_num);
  void test_9(int test_num);
  void test_10(int test_num);
  void test_11(int test_num);
  void test_12(int test_num);
  void test_21(int test_num);
  void test_22(int test_num);
  void test_watch();
  void sleep_if();
  void set_timing(double time_spent);
  arg_type arg;
  auto_file fd;
  size_t io_success_count;
  size_t op_success_count;
  double response_min, response_max, response_sum, response_avg;
};

void
hstest_thread::test_1()
{
  char buf[1024];
  unsigned int seed = arg.id;
  seed ^= arg.sh.conf.get_int("seed_xor", 0);
  std::string err;
  if (socket_connect(fd, arg.sh.arg, err) != 0) {
    fprintf(stderr, "connect: %d %s\n", errno, strerror(errno));
    return;
  }
  const char op = arg.sh.op;
  const int tablesize = arg.sh.conf.get_int("tablesize", 0);
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, v = 0, len = 0;
      if (op == 'G') {
	k = rand_r(&seed);
	v = rand_r(&seed); /* unused */
	if (tablesize != 0) {
	  k &= tablesize;
	}
	len = snprintf(buf, sizeof(buf), "%c\tk%d\n", op, k);
      } else {
	k = rand_r(&seed);
	v = rand_r(&seed);
	if (tablesize != 0) {
	  k &= tablesize;
	}
	len = snprintf(buf, sizeof(buf), "%c\tk%d\tv%d\n", op, k, v);
      }
      const int wlen = write(fd.get(), buf, len);
      if (wlen != len) {
	return;
      }
    }
    size_t read_cnt = 0;
    size_t read_pos = 0;
    while (read_cnt < arg.sh.pipe) {
      const int rlen = read(fd.get(), buf + read_pos, sizeof(buf) - read_pos);
      if (rlen <= 0) {
	return;
      }
      read_pos += rlen;
      while (true) {
	const char *const p = static_cast<const char *>(memchr(buf, '\n',
	  read_pos));
	if (p == 0) {
	  break;
	}
	++read_cnt;
	++io_success_count;
	arg.sh.increment_count();
	if (p != buf && buf[0] == '=') {
	  ++op_success_count;
	}
	const size_t rest_size = buf + read_pos - (p + 1);
	if (rest_size != 0) {
	  memmove(buf, p + 1, rest_size);
	}
	read_pos = rest_size;
      }
    }
  }
}

void
hstest_thread::test_2_3(int test_num)
{
#if 0
  char buf_k[128], buf_v[128];
  unsigned int seed = arg.id;
  op_base_t op = static_cast<op_base_t>(arg.sh.op);
  micli_ptr hnd;
  if (test_num == 2) {
    hnd = micli_i::create_remote(arg.sh.conf);
  } else if (test_num == 3) {
    // hnd = micli_i::create_inproc(arg.sh.localdb);
  }
  if (hnd.get() == 0) {
    return;
  }
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, v = 0, klen = 0, vlen = 0;
      k = rand_r(&seed);
      klen = snprintf(buf_k, sizeof(buf_k), "k%d", k);
      v = rand_r(&seed); /* unused */
      vlen = snprintf(buf_v, sizeof(buf_v), "v%d", v);
      string_ref arr[2];
      arr[0] = string_ref(buf_k, klen);
      arr[1] = string_ref(buf_v, vlen);
      pstrarr_ptr rec(arr, 2);
      if (hnd->execute(op, 0, 0, rec.get_const())) {
	++io_success_count;
	arg.sh.increment_count();
	const dataset& res = hnd->get_result_ref();
	if (res.size() == 1) {
	  ++op_success_count;
	}
      }
    }
  }
#endif
}

void
hstest_thread::test_4_5(int test_num)
{
#if 0
  char buf_k[128], buf_v[8192];
  memset(buf_v, ' ', sizeof(buf_v));
  unsigned int seed = arg.id;
  op_base_t op = static_cast<op_base_t>(arg.sh.op);
  micli_ptr hnd;
  if (test_num == 4) {
    hnd = micli_i::create_remote(arg.sh.conf);
  } else if (test_num == 5) {
    hnd = micli_i::create_inproc(arg.sh.localdb);
  }
  if (hnd.get() == 0) {
    return;
  }
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, klen = 0, vlen = 0;
      k = i & 0x0000ffffUL;
      if (k == 0) {
	fprintf(stderr, "k=0\n");
      }
      klen = snprintf(buf_k, sizeof(buf_k), "k%d", k);
      vlen = rand_r(&seed) % 8192;
      string_ref arr[2];
      arr[0] = string_ref(buf_k, klen);
      arr[1] = string_ref(buf_v, vlen);
      pstrarr_ptr rec(arr, 2);
      if (hnd->execute(op, 0, 0, rec.get_const())) {
	++io_success_count;
	const dataset& res = hnd->get_result_ref();
	if (res.size() == 1) {
	  ++op_success_count;
	}
      }
    }
  }
#endif
}

void
hstest_thread::test_6(int test_num)
{
  int count = arg.sh.conf.get_int("count", 1);
  auto_file fds[count];
  for (int i = 0; i < count; ++i) {
    const double t1 = gettimeofday_double();
    std::string err;
    if (socket_connect(fds[i], arg.sh.arg, err) != 0) {
      fprintf(stderr, "id=%zu i=%d err=%s\n", arg.id, i, err.c_str());
    }
    const double t2 = gettimeofday_double();
    if (t2 - t1 > 1) {
      fprintf(stderr, "id=%zu i=%d time %f\n", arg.id, i, t2 - t1);
    }
  }
}

void
hstest_thread::test_7(int num)
{
  /*
    set foo 0 0 10
    0123456789
    STORED
    get foo
    VALUE foo 0 10
    0123456789
    END
    get var
    END
   */
  char buf[1024];
  const int keep_connection = arg.sh.conf.get_int("keep_connection", 1);
  unsigned int seed = arg.id;
  seed ^= arg.sh.conf.get_int("seed_xor", 0);
  const int tablesize = arg.sh.conf.get_int("tablesize", 0);
  const char op = arg.sh.op;
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    const double tm1 = gettimeofday_double();
    std::string err;
    if (fd.get() < 0 && socket_connect(fd, arg.sh.arg, err) != 0) {
      fprintf(stderr, "connect: %d %s\n", errno, strerror(errno));
      return;
    }
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, v = 0, len = 0;
      if (op == 'G') {
	k = rand_r(&seed);
	v = rand_r(&seed); /* unused */
	if (tablesize != 0) {
	  k &= tablesize;
	}
	len = snprintf(buf, sizeof(buf), "get k%d\r\n", k);
      } else {
	k = rand_r(&seed);
	v = rand_r(&seed);
	if (tablesize != 0) {
	  k &= tablesize;
	}
	char vbuf[1024];
	int vlen = snprintf(vbuf, sizeof(vbuf),
	  "v%d"
	  // "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
	  // "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
	  // "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
	  // "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
	  // "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
	  , v);
	len = snprintf(buf, sizeof(buf), "set k%d 0 0 %d\r\n%s\r\n",
	  k, vlen, vbuf);
      }
      const int wlen = write(fd.get(), buf, len);
      if (wlen != len) {
	return;
      }
    }
    size_t read_cnt = 0;
    size_t read_pos = 0;
    bool read_response_done = false;
    bool expect_value = false;
    while (!read_response_done) {
      const int rlen = read(fd.get(), buf + read_pos, sizeof(buf) - read_pos);
      if (rlen <= 0) {
	return;
      }
      read_pos += rlen;
      while (true) {
	const char *const p = static_cast<const char *>(memchr(buf, '\n',
	  read_pos));
	if (p == 0) {
	  break;
	}
	++read_cnt;
	if (expect_value) {
	  expect_value = false;
	} else if (p >= buf + 6 && memcmp(buf, "VALUE ", 6) == 0) {
	  expect_value = true;
	  ++op_success_count;
	} else {
	  if (p == buf + 7 && memcmp(buf, "STORED\r", 7) == 0) {
	    ++op_success_count;
	  }
	  read_response_done = true;
	}
	const size_t rest_size = buf + read_pos - (p + 1);
	if (rest_size != 0) {
	  memmove(buf, p + 1, rest_size);
	}
	read_pos = rest_size;
      }
      ++io_success_count;
    }
    arg.sh.increment_count();
    if (!keep_connection) {
      fd.close();
    }
    const double tm2 = gettimeofday_double();
    set_timing(tm2 - tm1);
    sleep_if();
  }
}

struct rec {
  std::string key;
  std::string value;
};

void
hstest_thread::test_8(int test_num)
{
#if 0
  char buf_k[128], buf_v[128];
  unsigned int seed = arg.id;
  // op_base_t op = static_cast<op_base_t>(arg.sh.op);
  using namespace boost::multi_index;
  typedef member<rec, std::string, &rec::key> rec_get_key;
  typedef ordered_unique<rec_get_key> oui;
  typedef multi_index_container< rec, indexed_by<oui> > mic;
  #if 0
  typedef std::map<std::string, std::string> m_type;
  m_type m;
  #endif
  mic m;
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, v = 0, klen = 0, vlen = 0;
      k = rand_r(&seed);
      klen = snprintf(buf_k, sizeof(buf_k), "k%d", k);
      v = rand_r(&seed); /* unused */
      vlen = snprintf(buf_v, sizeof(buf_v), "v%d", v);
      const std::string ks(buf_k, klen);
      const std::string vs(buf_v, vlen);
      rec r;
      r.key = ks;
      r.value = vs;
      m.insert(r);
      // m.insert(std::make_pair(ks, vs));
      ++io_success_count;
      ++op_success_count;
      arg.sh.increment_count();
    }
  }
#endif
}

struct mysqltest_thread_initobj : private noncopyable {
  mysqltest_thread_initobj() {
    mysql_thread_init();
  }
  ~mysqltest_thread_initobj() {
    mysql_thread_end();
  }
};

void
hstest_thread::test_9(int test_num)
{
  /* create table hstest
   * ( k varchar(255) not null, v varchar(255) not null, primary key(k))
   * engine = innodb; */
  auto_mysql db;
  // mysqltest_thread_initobj initobj;
  std::string err;
  const char op = arg.sh.op;
  const std::string suffix = arg.sh.conf.get_str("value_suffix", "upd");
  unsigned long long err_cnt = 0;
  unsigned long long query_cnt = 0;
  #if 0
  my_bool reconnect = 0;
  if (mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
    err = "mysql_options() failed";
    ++err_cnt;
    return;
  }
  #endif
  unsigned int seed = time(0) + arg.id + 1;
  seed ^= arg.sh.conf.get_int("seed_xor", 0);
  drand48_data randbuf;
  srand48_r(seed, &randbuf);
  const std::string mysql_host = arg.sh.conf.get_str("host", "localhost");
  const int mysql_port = arg.sh.conf.get_int("mysqlport", 3306);
  const int num = arg.sh.loop;
  const std::string mysql_user = arg.sh.conf.get_str("mysqluser", "root");
  const std::string mysql_passwd = arg.sh.conf.get_str("mysqlpass", "");
  const std::string mysql_dbname = arg.sh.conf.get_str("dbname", "hstest");
  const int keep_connection = arg.sh.conf.get_int("keep_connection", 1);
  const int verbose = arg.sh.conf.get_int("verbose", 1);
  const int tablesize = arg.sh.conf.get_int("tablesize", 10000);
  const int moreflds = arg.sh.conf.get_int("moreflds", 0);
  const std::string moreflds_prefix = arg.sh.conf.get_str(
    "moreflds_prefix", "column0123456789_");
  const int use_handler = arg.sh.conf.get_int("handler", 0);
  const int sched_flag = arg.sh.conf.get_int("sched", 0);
  const int use_in = arg.sh.conf.get_int("in", 0);
  const int ssps = use_in ? 0 : arg.sh.conf.get_int("ssps", 0);
  std::string flds = "v";
  for (int i = 0; i < moreflds; ++i) {
    char buf[1024];
    snprintf(buf, sizeof(buf), ",%s%d", moreflds_prefix.c_str(), i);
    flds += std::string(buf);
  }
  int connected = 0;
  std::auto_ptr<auto_mysql_stmt> stmt;
  string_buffer wbuf;
  for (int i = 0; i < num; ++i) {
    const double tm1 = gettimeofday_double();
    const int flags = 0;
    if (connected == 0) {
      if (!mysql_real_connect(db, mysql_host.c_str(),
	mysql_user.c_str(), mysql_user.empty() ? 0 : mysql_passwd.c_str(),
	mysql_dbname.c_str(), mysql_port, 0, flags)) {
	err = "failed to connect: " + std::string(mysql_error(db));
	if (verbose >= 1) {
	  fprintf(stderr, "e=[%s]\n", err.c_str());
	}
	++err_cnt;
	return;
      }
      arg.sh.increment_conn(1);
    }
    int r = 0;
    if (connected == 0 && use_handler) {
      const char *const q = "handler hstest_table1 open";
      r = mysql_real_query(db, q, strlen(q));
      if (r != 0) {
	err = 1;
      }
    }
    if (connected == 0 && ssps) {
      stmt.reset(new auto_mysql_stmt(db));
      const char *const q = "select v from hstest_table1 where k = ?";
      r = mysql_stmt_prepare(*stmt, q, strlen(q));
      if (r != 0) {
	fprintf(stderr, "ssps err\n");
	++err_cnt;
	return;
      }
    }
    connected = 1;
    std::string result_str;
    unsigned int err = 0;
    unsigned int num_flds = 0, num_affected_rows = 0;
    int got_data = 0;
    char buf_query[16384];
    int buf_query_len = 0;
    int k = 0, v = 0;
    {
      double kf = 0, vf = 0;
      drand48_r(&randbuf, &kf);
      drand48_r(&randbuf, &vf);
      k = int(kf * tablesize);
      v = int(vf * tablesize);
      #if 0
      k = rand_r(&seed);
      v = rand_r(&seed);
      if (tablesize != 0) {
	k %= tablesize;
      }
      #endif
      if (op == 'G') {
	if (use_handler) {
	  buf_query_len = snprintf(buf_query, sizeof(buf_query),
	    "handler hstest_table1 read `primary` = ( '%d' )", k);
	    // TODO: moreflds
	} else if (ssps) {
	    //
	} else if (use_in) {
	  wbuf.clear();
	  char *p = wbuf.make_space(1024);
	  int len = snprintf(p, 1024, "select %s from hstest_table1 where k in ('%d'", flds.c_str(), k);
	  wbuf.space_wrote(len);
	  for (int j = 1; j < use_in; ++j) {
	    /* generate more key */
	    drand48_r(&randbuf, &kf);
	    k = int(kf * tablesize);
	    p = wbuf.make_space(1024);
	    int len = snprintf(p, 1024, ", '%d'", k);
	    wbuf.space_wrote(len);
	  }
	  wbuf.append_literal(")");
	} else {
	  buf_query_len = snprintf(buf_query, sizeof(buf_query),
	    "select %s from hstest_table1 where k = '%d'", flds.c_str(), k);
	}
      } else if (op == 'U') {
	buf_query_len = snprintf(buf_query, sizeof(buf_query),
	  "update hstest_table1 set v = '%d_%d%s' where k = '%d'",
	    v, k, suffix.c_str(), k);
      } else if (op == 'R') {
	buf_query_len = snprintf(buf_query, sizeof(buf_query),
	  "replace into hstest_table1 values ('%d', 'v%d')", k, v);
	  // TODO: moreflds
      }
    }
    if (r == 0) {
      if (ssps) {
	MYSQL_BIND bind[1] = { };
	bind[0].buffer_type = MYSQL_TYPE_LONG;
	bind[0].buffer = (char *)&k;
	bind[0].is_null = 0;
	bind[0].length = 0;
	if (mysql_stmt_bind_param(*stmt, bind)) {
	  fprintf(stderr, "err: %s\n", mysql_stmt_error(*stmt));
	  ++err_cnt;
	  return;
	}
	r = mysql_stmt_execute(*stmt);
	// fprintf(stderr, "stmt exec\n");
      } else if (use_in) {
	r = mysql_real_query(db, wbuf.begin(), wbuf.size());
      } else {
	r = mysql_real_query(db, buf_query, buf_query_len);
	// fprintf(stderr, "real query\n");
      }
      ++query_cnt;
    }
    if (r != 0) {
      err = 1;
    } else if (ssps) {
      if (verbose >= 0) {
	char resbuf[1024];
	unsigned long res_len = 0;
	MYSQL_BIND bind[1] = { };
	bind[0].buffer_type = MYSQL_TYPE_STRING;
	bind[0].buffer = resbuf;
	bind[0].buffer_length = sizeof(resbuf);
	bind[0].length = &res_len;
	if (mysql_stmt_bind_result(*stmt, bind)) {
	  fprintf(stderr, "err: %s\n", mysql_stmt_error(*stmt));
	  ++err_cnt;
	  return;
	}
	if (mysql_stmt_fetch(*stmt)) {
	  fprintf(stderr, "err: %s\n", mysql_stmt_error(*stmt));
	  ++err_cnt;
	  return;
	}
	if (!result_str.empty()) {
	  result_str += " ";
	}
	result_str += std::string(resbuf, res_len);
	// fprintf(stderr, "SSPS RES: %s\n", result_str.c_str());
	got_data = 1;
      } else {
	got_data = 1;
      }
    } else {
      auto_mysql_res res(db);
      if (res != 0) {
	if (verbose >= 0) {
	  num_flds = mysql_num_fields(res);
	  MYSQL_ROW row = 0;
	  while ((row = mysql_fetch_row(res)) != 0) {
	    got_data += 1;
	    unsigned long *const lengths = mysql_fetch_lengths(res);
	    if (verbose >= 2) {
	      for (unsigned int i = 0; i < num_flds; ++i) {
		if (!result_str.empty()) {
		  result_str += " ";
		}
		result_str += std::string(row[i], lengths[i]);
	      }
	    }
	  }
	} else {
	  MYSQL_ROW row = 0;
	  while ((row = mysql_fetch_row(res)) != 0) {
	    got_data += 1;
	  }
	}
      } else {
	if (mysql_field_count(db) == 0) {
	  num_affected_rows = mysql_affected_rows(db);
	} else {
	  err = 1;
	}
      }
    }
    if (verbose >= 2 || (verbose >= 1 && err != 0)) {
      if (err) {
	++err_cnt;
	const char *const errstr = mysql_error(db);
	fprintf(stderr, "e=[%s] a=%u q=[%s]\n", errstr,
	  num_affected_rows, buf_query);
      } else {
	fprintf(stderr, "a=%u q=[%s] r=[%s]\n", num_affected_rows, buf_query,
	  result_str.c_str());
      }
    }
    if (err == 0) {
      ++io_success_count;
      if (num_affected_rows > 0 || got_data > 0) {
	op_success_count += got_data;
      } else {
	if (verbose >= 1) {
	  fprintf(stderr, "k=%d numaff=%u gotdata=%d\n",
	    k, num_affected_rows, got_data);
	}
      }
      arg.sh.increment_count();
    }
    if (!keep_connection) {
      if (stmt.get() != 0) {
	stmt.reset();
      }
      db.reset();
      connected = 0;
    }
    const double tm2 = gettimeofday_double();
    set_timing(tm2 - tm1);
    sleep_if();
    if (sched_flag) {
      sched_yield();
    }
  }
  if (verbose >= 1) {
    fprintf(stderr, "thread finished (error_count=%llu)\n", err_cnt);
  }
}

void
hstest_thread::test_10(int test_num)
{
  const int keep_connection = arg.sh.conf.get_int("keep_connection", 1);
  unsigned int seed = time(0) + arg.id + 1;
  seed ^= arg.sh.conf.get_int("seed_xor", 0);
  drand48_data randbuf;
  srand48_r(seed, &randbuf);
  std::string err;
  int keepconn_count = 0;
  const char op = arg.sh.op;
  const int verbose = arg.sh.conf.get_int("verbose", 1);
  const std::string suffix = arg.sh.conf.get_str("value_suffix", "upd");
  const int tablesize = arg.sh.conf.get_int("tablesize", 10000);
  const int firstkey = arg.sh.conf.get_int("firstkey", 0);
  const int sched_flag = arg.sh.conf.get_int("sched", 0);
  const int moreflds = arg.sh.conf.get_int("moreflds", 0);
  const std::string dbname = arg.sh.conf.get_str("dbname", "hstest");
  const std::string table = arg.sh.conf.get_str("table", "hstest_table1");
  const std::string index = arg.sh.conf.get_str("index", "PRIMARY");
  const std::string field = arg.sh.conf.get_str("field", "v");
  const int use_in = arg.sh.conf.get_int("in", 0);
  const std::string moreflds_prefix = arg.sh.conf.get_str(
    "moreflds_prefix", "column0123456789_");
  const int dump = arg.sh.dump;
  const int nodup = arg.sh.conf.get_int("nodup", 0);
  std::string moreflds_str;
  for (int i = 0; i < moreflds; ++i) {
    char sbuf[1024];
    snprintf(sbuf, sizeof(sbuf), ",%s%d", moreflds_prefix.c_str(), i);
    moreflds_str += std::string(sbuf);
  }
  string_buffer wbuf;
  char rbuf[16384];
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    int len = 0, rlen = 0, wlen = 0;
    #if 0
    const double tm1 = gettimeofday_double();
    #endif
    if (fd.get() < 0) {
      if (socket_connect(fd, arg.sh.arg, err) != 0) {
	fprintf(stderr, "connect: %d %s\n", errno, strerror(errno));
	return;
      }
      char *wp = wbuf.make_space(1024);
      len = snprintf(wp, 1024,
	"P\t1\t%s\t%s\tPRIMARY\t%s%s\n", dbname.c_str(), table.c_str(),
	  field.c_str(), moreflds_str.c_str());
	/* pst_num, db, table, index, retflds */
      wbuf.space_wrote(len);
      wlen = write(fd.get(), wbuf.begin(), len);
      if (len != wlen) {
	fprintf(stderr, "write: %d %d\n", len, wlen);
	return;
      }
      wbuf.clear();
      rlen = read(fd.get(), rbuf, sizeof(rbuf));
      if (rlen <= 0 || rbuf[rlen - 1] != '\n') {
	fprintf(stderr, "read: rlen=%d errno=%d\n", rlen, errno);
	return;
      }
      if (rbuf[0] != '0') {
	fprintf(stderr, "failed to open table\n");
	return;
      }
      arg.sh.increment_conn(1);
    }
    const double tm1 = gettimeofday_double();
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      int k = 0, v = 0;
      {
	while (true) {
	  double kf = 0, vf = 0;
	  drand48_r(&randbuf, &kf);
	  drand48_r(&randbuf, &vf);
	  k = int(kf * tablesize) + firstkey;
	  v = int(vf * tablesize) + firstkey;
	  if (k - firstkey < arg.sh.keygen_size) {
	    volatile char *const ptr = arg.sh.keygen + (k - firstkey);
	    // int oldv = __sync_fetch_and_or(ptr, 1);
	    int oldv = *ptr;
	    *ptr += 1;
	    if (nodup && oldv != 0) {
	      if (dump) {
		fprintf(stderr, "retry\n");
	      }
	      continue;
	    }
	  } else {
	    if (nodup) {
	      if (dump) {
		fprintf(stderr, "retry2\n");
	      }
	      continue;
	    }
	  }
	  size_t len = 0;
	  if (op == 'G') {
	    if (use_in) {
	      char *wp = wbuf.make_space(1024);
	      len = snprintf(wp, 1024, "1\t=\t1\t\t%d\t0\t@\t0\t%d\t%d",
		use_in, use_in, k);
	      wbuf.space_wrote(len);
	      for (int j = 1; j < use_in; ++j) {
		drand48_r(&randbuf, &kf);
		k = int(kf * tablesize) + firstkey;
		char *wp = wbuf.make_space(1024);
		len = snprintf(wp, 1024, "\t%d", k);
		wbuf.space_wrote(len);
	      }
	      wbuf.append_literal("\n");
	    } else {
	      char *wp = wbuf.make_space(1024);
	      len = snprintf(wp, 1024, "1\t=\t1\t%d\n", k);
	      wbuf.space_wrote(len);
	    }
	  } else if (op == 'U') {
	    char *wp = wbuf.make_space(1024);
	    len = snprintf(wp, 1024,
	      "1\t=\t1\t%d\t1\t0\tU\t%d_%d%s\n", k, v, k, suffix.c_str());
	    wbuf.space_wrote(len);
	  }
	  break;
	}
      }
    }
    wlen = write(fd.get(), wbuf.begin(), wbuf.size());
    if ((size_t) wlen != wbuf.size()) {
      fprintf(stderr, "write: %d %d\n", (int)wbuf.size(), wlen);
      return;
    }
    wbuf.clear();
    size_t read_cnt = 0;
    size_t read_pos = 0;
    while (read_cnt < arg.sh.pipe) {
      rlen = read(fd.get(), rbuf + read_pos, sizeof(rbuf) - read_pos);
      if (rlen <= 0) {
	fprintf(stderr, "read: %d\n", rlen);
	return;
      }
      read_pos += rlen;
      while (true) {
	const char *const nl = static_cast<const char *>(memchr(rbuf, '\n',
	  read_pos));
	if (nl == 0) {
	  break;
	}
	++read_cnt;
	++io_success_count;
	const char *t1 = static_cast<const char *>(memchr(rbuf, '\t',
	  nl - rbuf));
	if (t1 == 0) {
	  fprintf(stderr, "error \n");
	  break;
	}
	++t1;
	const char *t2 = static_cast<const char *>(memchr(t1, '\t',
	  nl - t1));
	if (t2 == 0) {
	  if (verbose > 1) {
	    fprintf(stderr, "key: notfound \n");
	  }
	  break;
	}
	++t2;
	if (t1 == rbuf + 2 && rbuf[0] == '0') {
	  if (op == 'G') {
	    ++op_success_count;
	    arg.sh.increment_count();
	  } else if (op == 'U') {
	    const char *t3 = t2;
	    while (t3 != nl && t3[0] >= 0x10) {
	      ++t3;
	    }
	    if (t3 != t2 + 1 || t2[0] != '1') {
	      const std::string mess(t2, t3);
	      fprintf(stderr, "mod: %s\n", mess.c_str());
	    } else {
	      ++op_success_count;
	      arg.sh.increment_count();
	      if (arg.sh.dump && arg.sh.pipe == 1) {
		fwrite(wbuf.begin(), wbuf.size(), 1, stderr);
	      }
	    }
	  }
	} else {
	  const char *t3 = t2;
	  while (t3 != nl && t3[0] >= 0x10) {
	    ++t3;
	  }
	  const std::string mess(t2, t3);
	  fprintf(stderr, "err: %s\n", mess.c_str());
	}
	const size_t rest_size = rbuf + read_pos - (nl + 1);
	if (rest_size != 0) {
	  memmove(rbuf, nl + 1, rest_size);
	}
	read_pos = rest_size;
      }
    }
    if (!keep_connection) {
      fd.reset();
      arg.sh.increment_conn(-1);
    } else if (keep_connection > 1 && ++keepconn_count > keep_connection) {
      keepconn_count = 0;
      fd.reset();
      arg.sh.increment_conn(-1);
    }
    const double tm2 = gettimeofday_double();
    set_timing(tm2 - tm1);
    sleep_if();
    if (sched_flag) {
      sched_yield();
    }
  }
  if (dump) {
    fprintf(stderr, "done\n");
  }
}

void
hstest_thread::sleep_if()
{
  if (arg.sh.usleep) {
    struct timespec ts = {
      arg.sh.usleep / 1000000,
      (arg.sh.usleep % 1000000) * 1000
    };
    nanosleep(&ts, 0);
  }
}

void
hstest_thread::set_timing(double time_spent)
{
  response_min = std::min(response_min, time_spent);
  response_max = std::max(response_max, time_spent);
  response_sum += time_spent;
  if (op_success_count != 0) {
    response_avg = response_sum / op_success_count;
  }
}

void
hstest_thread::test_11(int test_num)
{
  const int keep_connection = arg.sh.conf.get_int("keep_connection", 1);
  const int tablesize = arg.sh.conf.get_int("tablesize", 0);
  unsigned int seed = arg.id;
  seed ^= arg.sh.conf.get_int("seed_xor", 0);
  std::string err;
  hstcpcli_ptr cli;
  for (size_t i = 0; i < arg.sh.loop; ++i) {
    if (cli.get() == 0) {
      cli = hstcpcli_i::create(arg.sh.arg);
      cli->request_buf_open_index(0, "hstest", "hstest_table1", "", "v");
	/* pst_num, db, table, index, retflds */
      if (cli->request_send() != 0) {
	fprintf(stderr, "reuqest_send: %s\n", cli->get_error().c_str());
	return;
      }
      size_t num_flds = 0;
      if (cli->response_recv(num_flds) != 0) {
	fprintf(stderr, "reuqest_recv: %s\n", cli->get_error().c_str());
	return;
      }
      cli->response_buf_remove();
    }
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      char buf[256];
      int k = 0, v = 0, len = 0;
      {
	k = rand_r(&seed);
	v = rand_r(&seed); /* unused */
	if (tablesize != 0) {
	  k &= tablesize;
	}
	len = snprintf(buf, sizeof(buf), "%d", k);
      }
      const string_ref key(buf, len);
      const string_ref op("=", 1);
      cli->request_buf_exec_generic(0, op, &key, 1, 1, 0, string_ref(), 0, 0);
    }
    if (cli->request_send() != 0) {
      fprintf(stderr, "reuqest_send: %s\n", cli->get_error().c_str());
      return;
    }
    size_t read_cnt = 0;
    for (size_t j = 0; j < arg.sh.pipe; ++j) {
      size_t num_flds = 0;
      if (cli->response_recv(num_flds) != 0) {
	fprintf(stderr, "reuqest_recv: %s\n", cli->get_error().c_str());
	return;
      }
      {
	++read_cnt;
	++io_success_count;
	arg.sh.increment_count();
	{
	  ++op_success_count;
	}
      }
      cli->response_buf_remove();
    }
    if (!keep_connection) {
      cli.reset();
    }
  }
}

void
hstest_thread::test_watch()
{
  const int timelimit = arg.sh.conf.get_int("timelimit", 0);
  const int timelimit_offset = timelimit / 2;
  int loop = 0;
  double t1 = 0, t2 = 0;
  size_t cnt_t1 = 0, cnt_t2 = 0;
  size_t prev_cnt = 0;
  double now_f = 0;
  while (true) {
    sleep(1);
    const size_t cnt = arg.sh.count;
    const size_t df = cnt - prev_cnt;
    prev_cnt = cnt;
    const double now_prev = now_f;
    now_f = gettimeofday_double();
    if (now_prev != 0) {
      const double rps = static_cast<double>(df) / (now_f - now_prev);
      fprintf(stderr, "now: %zu cntdiff: %zu tdiff: %f rps: %f\n",
        static_cast<size_t>(now_f), df, now_f - now_prev, rps);
    }
    if (timelimit != 0) {
      if (arg.sh.wait_conn == 0 || arg.sh.conn_count >= arg.sh.wait_conn) {
	++loop;
      }
      if (loop == timelimit_offset) {
        t1 = gettimeofday_double();
        cnt_t1 = cnt;
	arg.sh.enable_timing = 1;
	fprintf(stderr, "start timing\n");
      } else if (loop == timelimit_offset + timelimit) {
        t2 = gettimeofday_double();
        cnt_t2 = cnt;
        const size_t cnt_diff = cnt_t2 - cnt_t1;
        const double tdiff = t2 - t1;
        const double qps = cnt_diff / (tdiff != 0 ? tdiff : 1);
        fprintf(stderr, "(%f: %zu, %f: %zu), %10.5f qps\n",
          t1, cnt_t1, t2, cnt_t2, qps);
	size_t keycnt = 0;
	for (int i = 0; i < arg.sh.keygen_size; ++i) {
	  if (arg.sh.keygen[i]) {
	    ++keycnt;
	  }
	}
	fprintf(stderr, "keygen=%zu\n", keycnt);
	break;
      }
    }
  }
#if 0
  int loop = 0;
  double t1 = 0, t2 = 0;
  size_t cnt_t1 = 0, cnt_t2 = 0;
  size_t prev_cnt = 0;
  while (true) {
    sleep(1);
    const size_t cnt = arg.sh.count;
    const size_t df = cnt - prev_cnt;
    prev_cnt = cnt;
    const size_t now = time(0);
    fprintf(stderr, "%zu %zu\n", now, df);
    if (timelimit != 0) {
      ++loop;
      if (loop == timelimit_offset) {
	t1 = gettimeofday_double();
	cnt_t1 = cnt;
      } else if (loop == timelimit_offset + timelimit) {
	t2 = gettimeofday_double();
	cnt_t2 = cnt;
	const size_t cnt_diff = cnt_t2 - cnt_t1;
	const double tdiff = t2 - t1;
	const double qps = cnt_diff / (tdiff != 0 ? tdiff : 1);
	fprintf(stderr, "(%f: %zu, %f: %zu), %10.5f qps\n",
	  t1, cnt_t1, t2, cnt_t2, qps);
	size_t keycnt = 0;
	for (int i = 0; i < arg.sh.keygen_size; ++i) {
	  if (arg.sh.keygen[i]) {
	    ++keycnt;
	  }
	}
	fprintf(stderr, "keygen=%zu\n", keycnt);
	_exit(0);
      }
    }
  }
#endif
}

void
hstest_thread::test_12(int test_num)
{
  /* NOTE: num_threads should be 1 */
  /* create table hstest
   * ( k varchar(255) not null, v varchar(255) not null, primary key(k))
   * engine = innodb; */
  mysqltest_thread_initobj initobj;
  auto_mysql db;
  std::string err;
  unsigned long long err_cnt = 0;
  unsigned long long query_cnt = 0;
  #if 0
  my_bool reconnect = 0;
  if (mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
    err = "mysql_options() failed";
    ++err_cnt;
    return;
  }
  #endif
  const std::string mysql_host = arg.sh.conf.get_str("host", "localhost");
  const int mysql_port = arg.sh.conf.get_int("mysqlport", 3306);
  const unsigned int num = arg.sh.loop;
  const size_t pipe = arg.sh.pipe;
  const std::string mysql_user = arg.sh.conf.get_str("mysqluser", "root");
  const std::string mysql_passwd = arg.sh.conf.get_str("mysqlpass", "");
  const std::string mysql_dbname = arg.sh.conf.get_str("db", "hstest");
  const int keep_connection = arg.sh.conf.get_int("keep_connection", 1);
  const int verbose = arg.sh.conf.get_int("verbose", 1);
  const int use_handler = arg.sh.conf.get_int("handler", 0);
  int connected = 0;
  unsigned int k = 0;
  string_buffer buf;
  for (unsigned int i = 0; i < num; ++i) {
    const int flags = 0;
    if (connected == 0 && !mysql_real_connect(db, mysql_host.c_str(),
      mysql_user.c_str(), mysql_user.empty() ? 0 : mysql_passwd.c_str(),
      mysql_dbname.c_str(), mysql_port, 0, flags)) {
      err = "failed to connect: " + std::string(mysql_error(db));
      if (verbose >= 1) {
	fprintf(stderr, "e=[%s]\n", err.c_str());
      }
      ++err_cnt;
      return;
    }
    int r = 0;
    if (connected == 0 && use_handler) {
      const char *const q = "handler hstest open";
      r = mysql_real_query(db, q, strlen(q));
      if (r != 0) {
	err = 1;
      }
    }
    connected = 1;
    std::string result_str;
    unsigned int err = 0;
    unsigned int num_flds = 0, num_affected_rows = 0;
    int got_data = 0;
    buf.clear();
    buf.append_literal("insert into hstest values ");
    for (size_t j = 0; j < pipe; ++j) {
      const unsigned int v = ~k;
      if (j != 0) {
	buf.append_literal(",");
      }
      char *wp = buf.make_space(64);
      int buf_query_len = snprintf(wp, 64, "('k%u', 'v%u')", k, v);
      buf.space_wrote(buf_query_len);
      ++k;
    }
    if (r == 0) {
      r = mysql_real_query(db, buf.begin(), buf.size());
      ++query_cnt;
    }
    if (r != 0) {
      err = 1;
    } else {
      auto_mysql_res res(db);
      if (res != 0) {
	if (verbose >= 0) {
	  num_flds = mysql_num_fields(res);
	  MYSQL_ROW row = 0;
	  while ((row = mysql_fetch_row(res)) != 0) {
	    got_data = 1;
	    unsigned long *const lengths = mysql_fetch_lengths(res);
	    if (verbose >= 2) {
	      for (unsigned int i = 0; i < num_flds; ++i) {
		if (!result_str.empty()) {
		  result_str += " ";
		}
		result_str += std::string(row[i], lengths[i]);
	      }
	    }
	  }
	}
      } else {
	if (mysql_field_count(db) == 0) {
	  num_affected_rows = mysql_affected_rows(db);
	} else {
	  err = 1;
	}
      }
    }
    if (verbose >= 2 || (verbose >= 1 && err != 0)) {
      if (err) {
	++err_cnt;
	const char *const errstr = mysql_error(db);
	fprintf(stderr, "e=[%s] a=%u q=[%s]\n", errstr,
	  num_affected_rows, std::string(buf.begin(), buf.size()).c_str());
      } else {
	fprintf(stderr, "a=%u q=[%s] r=[%s]\n", num_affected_rows,
	  std::string(buf.begin(), buf.size()).c_str(),
	  result_str.c_str());
      }
    }
    if (err == 0) {
      ++io_success_count;
      if (num_affected_rows > 0 || got_data > 0) {
	++op_success_count;
      }
      arg.sh.increment_count(pipe);
    }
    if (!keep_connection) {
      db.reset();
      connected = 0;
    }
  }
  if (verbose >= 1) {
    fprintf(stderr, "thread finished (error_count=%llu)\n", err_cnt);
  }
}

void
hstest_thread::test_21(int num)
{
  /* fsync test */
  unsigned int id = arg.id;
  std::string err;
  #if 0
  if (socket_connect(fd, arg.sh.arg, err) != 0) {
    fprintf(stderr, "connect: %d %s\n", errno, strerror(errno));
    return;
  }
  #endif
  auto_file logfd;
  char fname[1024];
  snprintf(fname, sizeof(fname), "synctest_%u", id);
  int open_flags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND;
  logfd.reset(open(fname, open_flags, 0644));
  if (logfd.get() < 0) {
    fprintf(stderr, "open: %s: %d %s\n", fname, errno, strerror(errno));
    return;
  }
  char buf[1024];
  unsigned long long count = 0;
  while (true) {
    snprintf(buf, sizeof(buf), "%u %llu\n", id, count);
    const size_t len = strlen(buf);
    if (write(logfd.get(), buf, len) != (ssize_t)len) {
      fprintf(stderr, "write: %s: %d %s\n", fname, errno, strerror(errno));
      return;
    }
    #if 0
    if (write(fd.get(), buf, len) != (ssize_t)len) {
      fprintf(stderr, "write(sock): %d %s\n", errno, strerror(errno));
      return;
    }
    #endif
    if (fdatasync(logfd.get()) != 0) {
      fprintf(stderr, "fsync: %s: %d %s\n", fname, errno, strerror(errno));
      return;
    }
    ++count;
    ++op_success_count;
    arg.sh.increment_count();
  }
}

void
hstest_thread::test_22(int num)
{
  /* dd if=/dev/zero of=dummy.dat bs=1024M count=100 */
  unsigned int id = arg.id;
  std::string err;
  auto_file filefd;
  char fname[1024];
  snprintf(fname, sizeof(fname), "dummy.dat");
  int open_flags = O_RDONLY | O_DIRECT;
  filefd.reset(open(fname, open_flags, 0644));
  if (filefd.get() < 0) {
    fprintf(stderr, "open: %s: %d %s\n", fname, errno, strerror(errno));
    return;
  }
  char buf_x[4096 * 2];
  char *const buf = (char *)(size_t(buf_x + 4096) / 4096 * 4096);
  unsigned long long count = 0;
  drand48_data randbuf;
  unsigned long long seed = time(0);
  seed *= 10;
  seed += id;
  srand48_r(seed, &randbuf);
  for (unsigned int i = 0; i < arg.sh.loop; ++i) {
    double kf = 0;
    drand48_r(&randbuf, &kf);
    kf *= (209715200 / 1);
    // fprintf(stderr, "v=%f\n", kf);
    off_t v = static_cast<off_t>(kf);
    v %= (209715200 / 1);
    v *= (512 * 1);
    const double tm1 = gettimeofday_double();
    const ssize_t r = pread(filefd.get(), buf, (512 * 1), v);
    const double tm2 = gettimeofday_double();
    if (r < 0) {
      fprintf(stderr, "pread: %s: %d %s\n", fname, errno, strerror(errno));
      return;
    }
    ++count;
    ++op_success_count;
    arg.sh.increment_count();
    set_timing(tm2 - tm1);
  }
}

void
hstest_thread::operator ()()
{
  if (arg.watch_flag) {
    return test_watch();
  }
  int test_num = arg.sh.conf.get_int("test", 1);
  if (test_num == 1) {
    test_1();
  } else if (test_num == 2 || test_num == 3) {
    test_2_3(test_num);
  } else if (test_num == 4 || test_num == 5) {
    test_4_5(test_num);
  } else if (test_num == 6) {
    test_6(test_num);
  } else if (test_num == 7) {
    test_7(test_num);
  } else if (test_num == 8) {
    test_8(test_num);
  } else if (test_num == 9) {
    test_9(test_num);
  } else if (test_num == 10) {
    test_10(test_num);
  } else if (test_num == 11) {
    test_11(test_num);
  } else if (test_num == 12) {
    test_12(test_num);
  } else if (test_num == 21) {
    test_21(test_num);
  } else if (test_num == 22) {
    test_22(test_num);
  }
  const int halt = arg.sh.conf.get_int("halt", 0);
  if (halt) {
    fprintf(stderr, "thread halted\n");
    while (true) {
      sleep(100000);
    }
  }
  fprintf(stderr, "thread finished\n");
}

int
hstest_main(int argc, char **argv)
{
  ignore_sigpipe();
  hstest_shared shared;
  parse_args(argc, argv, shared.conf);
  shared.conf["port"] = shared.conf["hsport"];
  shared.arg.set(shared.conf);
  shared.loop = shared.conf.get_int("num", 1000);
  shared.pipe = shared.conf.get_int("pipe", 1);
  shared.verbose = shared.conf.get_int("verbose", 1);
  const int tablesize = shared.conf.get_int("tablesize", 0);
  std::vector<char> keygen(tablesize);
  shared.keygen = &keygen[0];
  shared.keygen_size = tablesize;
  shared.usleep = shared.conf.get_int("usleep", 0);
  shared.dump = shared.conf.get_int("dump", 0);
  shared.num_threads = shared.conf.get_int("num_threads", 10);
  shared.wait_conn = shared.conf.get_int("wait_conn", 0);
  const std::string op = shared.conf.get_str("op", "G");
  if (op.size() > 0) {
    shared.op = op[0];
  }
  #if 0
  const int localdb_flag = shared.conf.get_int("local", 0);
  if (localdb_flag) {
    shared.localdb = database_i::create(shared.conf);
  }
  #endif
  const int num_thrs = shared.num_threads;
  typedef thread<hstest_thread> thread_type;
  typedef std::auto_ptr<thread_type> thread_ptr;
  typedef auto_ptrcontainer< std::vector<thread_type *> > thrs_type;
  thrs_type thrs;
  for (int i = 0; i < num_thrs; ++i) {
    const hstest_thread::arg_type arg(i, shared, false);
    thread_ptr thr(new thread<hstest_thread>(arg));
    thrs.push_back_ptr(thr);
  }
  for (size_t i = 0; i < thrs.size(); ++i) {
    thrs[i]->start();
  }
  thread_ptr watch_thread;
  const int timelimit = shared.conf.get_int("timelimit", 0);
  {
    const hstest_thread::arg_type arg(0, shared, true);
    watch_thread = thread_ptr(new thread<hstest_thread>(arg));
    watch_thread->start();
  }
  size_t iocnt = 0, opcnt = 0;
  double respmin = 999999, respmax = 0;
  double respsum = 0;
  if (timelimit != 0) {
    watch_thread->join();
  }
  for (size_t i = 0; i < thrs.size(); ++i) {
    if (timelimit == 0) {
      thrs[i]->join();
    }
    iocnt += (*thrs[i])->io_success_count;
    opcnt += (*thrs[i])->op_success_count;
    respmin = std::min(respmin, (*thrs[i])->response_min);
    respmax = std::max(respmax, (*thrs[i])->response_max);
    respsum += (*thrs[i])->response_sum;
  }
  fprintf(stderr, "io_success_count=%zu op_success_count=%zu\n", iocnt, opcnt);
  fprintf(stderr, "respmin=%f respmax=%f respsum=%f respavg=%f\n",
    respmin, respmax, respsum, respsum / opcnt);
  size_t keycnt = 0;
  for (size_t i = 0; i < keygen.size(); ++i) {
    if (keygen[i]) {
      ++keycnt;
    }
  }
  fprintf(stderr, "keycnt=%zu\n", keycnt);
  _exit(0);
  return 0;
}

};

int
main(int argc, char **argv)
{
  return dena::hstest_main(argc, argv);
}

