
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "database.hpp"
#include "string_util.hpp"
#include "escape.hpp"
#include "mysql_incl.hpp"

#define DBG_KEY(x)
#define DBG_SHUT(x)
#define DBG_LOCK(x)
#define DBG_THR(x)
#define DBG_CMP(x)
#define DBG_FLD(x)
#define DBG_FILTER(x)
#define DBG_REFCNT(x)
#define DBG_KEYLEN(x)
#define DBG_DELETED

/* status variables */
unsigned long long int open_tables_count;
unsigned long long int close_tables_count;
unsigned long long int lock_tables_count;
unsigned long long int unlock_tables_count;
unsigned long long int index_exec_count;

namespace dena {

prep_stmt::prep_stmt()
  : dbctx(0), table_id(static_cast<size_t>(-1)),
    idxnum(static_cast<size_t>(-1))
{
}
prep_stmt::prep_stmt(dbcontext_i *c, size_t tbl, size_t idx,
  const fields_type& rf, const fields_type& ff)
  : dbctx(c), table_id(tbl), idxnum(idx), ret_fields(rf), filter_fields(ff)
{
  if (dbctx) {
    dbctx->table_addref(table_id);
  }
}
prep_stmt::~prep_stmt()
{
  if (dbctx) {
    dbctx->table_release(table_id);
  }
}

prep_stmt::prep_stmt(const prep_stmt& x)
  : dbctx(x.dbctx), table_id(x.table_id), idxnum(x.idxnum),
  ret_fields(x.ret_fields), filter_fields(x.filter_fields)
{
  if (dbctx) {
    dbctx->table_addref(table_id);
  }
}

prep_stmt&
prep_stmt::operator =(const prep_stmt& x)
{
  if (this != &x) {
    if (dbctx) {
      dbctx->table_release(table_id);
    }
    dbctx = x.dbctx;
    table_id = x.table_id;
    idxnum = x.idxnum;
    ret_fields = x.ret_fields;
    filter_fields = x.filter_fields;
    if (dbctx) {
      dbctx->table_addref(table_id);
    }
  }
  return *this;
}

struct database : public database_i, private noncopyable {
  database(const config& c);
  virtual ~database();
  virtual dbcontext_ptr create_context(bool for_write) volatile;
  virtual void stop() volatile;
  virtual const config& get_conf() const volatile;
 public:
  int child_running;
 private:
  config conf;
};

struct tablevec_entry {
  TABLE *table;
  size_t refcount;
  bool modified;
  tablevec_entry() : table(0), refcount(0), modified(false) { }
};

struct expr_user_lock : private noncopyable {
  expr_user_lock(THD *thd, int timeout)
    : lck_key("handlersocket_wr", 16, &my_charset_latin1),
      lck_timeout(timeout),
      lck_func_get_lock(&lck_key, &lck_timeout),
      lck_func_release_lock(&lck_key)
  {
    lck_key.fix_fields(thd, 0);
    lck_timeout.fix_fields(thd, 0);
    lck_func_get_lock.fix_fields(thd, 0);
    lck_func_release_lock.fix_fields(thd, 0);
  }
  long long get_lock() {
    return lck_func_get_lock.val_int();
  }
  long long release_lock() {
    return lck_func_release_lock.val_int();
  }
 private:
  Item_string lck_key;
  Item_int lck_timeout;
  Item_func_get_lock lck_func_get_lock;
  Item_func_release_lock lck_func_release_lock;
};

struct dbcontext : public dbcontext_i, private noncopyable {
  dbcontext(volatile database *d, bool for_write);
  virtual ~dbcontext();
  virtual void init_thread(const void *stack_botton,
    volatile int& shutdown_flag);
  virtual void term_thread();
  virtual bool check_alive();
  virtual void lock_tables_if();
  virtual void unlock_tables_if();
  virtual bool get_commit_error();
  virtual void clear_error();
  virtual void close_tables_if();
  virtual void table_addref(size_t tbl_id);
  virtual void table_release(size_t tbl_id);
  virtual void cmd_open(dbcallback_i& cb, const cmd_open_args& args);
  virtual void cmd_exec(dbcallback_i& cb, const cmd_exec_args& args);
  virtual void set_statistics(size_t num_conns, size_t num_active);
 private:
  int set_thread_message(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  bool parse_fields(TABLE *const table, const char *str,
    prep_stmt::fields_type& flds);
  void cmd_insert_internal(dbcallback_i& cb, const prep_stmt& pst,
    const string_ref *fvals, size_t fvalslen);
  void cmd_sql_internal(dbcallback_i& cb, const prep_stmt& pst,
    const string_ref *fvals, size_t fvalslen);
  void cmd_find_internal(dbcallback_i& cb, const prep_stmt& pst,
    ha_rkey_function find_flag, const cmd_exec_args& args);
  size_t calc_filter_buf_size(TABLE *table, const prep_stmt& pst,
    const record_filter *filters);
  bool fill_filter_buf(TABLE *table, const prep_stmt& pst,
    const record_filter *filters, uchar *filter_buf, size_t len);
  int check_filter(dbcallback_i& cb, TABLE *table, const prep_stmt& pst,
    const record_filter *filters, const uchar *filter_buf);
  void resp_record(dbcallback_i& cb, TABLE *const table, const prep_stmt& pst);
  void dump_record(dbcallback_i& cb, TABLE *const table, const prep_stmt& pst);
  int modify_record(dbcallback_i& cb, TABLE *const table,
    const prep_stmt& pst, const cmd_exec_args& args, char mod_op,
    size_t& modified_count);
 private:
  typedef std::vector<tablevec_entry> table_vec_type;
  typedef std::pair<std::string, std::string> table_name_type;
  typedef std::map<table_name_type, size_t> table_map_type;
 private:
  volatile database *const dbref;
  bool for_write_flag;
  THD *thd;
  MYSQL_LOCK *lock;
  bool lock_failed;
  std::auto_ptr<expr_user_lock> user_lock;
  int user_level_lock_timeout;
  bool user_level_lock_locked;
  bool commit_error;
  std::vector<char> info_message_buf;
  table_vec_type table_vec;
  table_map_type table_map;
};

database::database(const config& c)
  : child_running(1), conf(c)
{
}

database::~database()
{
}

dbcontext_ptr
database::create_context(bool for_write) volatile
{
  return dbcontext_ptr(new dbcontext(this, for_write));
}

void
database::stop() volatile
{
  child_running = false;
}

const config&
database::get_conf() const volatile
{
  return const_cast<const config&>(conf);
}

database_ptr
database_i::create(const config& conf)
{
  return database_ptr(new database(conf));
}

dbcontext::dbcontext(volatile database *d, bool for_write)
  : dbref(d), for_write_flag(for_write), thd(0), lock(0), lock_failed(false),
    user_level_lock_timeout(0), user_level_lock_locked(false),
    commit_error(false)
{
  info_message_buf.resize(8192);
  user_level_lock_timeout = d->get_conf().get_int("wrlock_timeout", 12);
}

dbcontext::~dbcontext()
{
}

namespace {

int
wait_server_to_start(THD *thd, volatile int& shutdown_flag)
{
  int r = 0;
  DBG_SHUT(fprintf(stderr, "HNDSOCK wsts\n"));
  pthread_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started) {
    timespec abstime;
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&COND_server_started, &LOCK_server_started,
      &abstime);
    pthread_mutex_unlock(&LOCK_server_started);
    pthread_mutex_lock(&thd->mysys_var->mutex);
    killed_state st = thd->killed;
    pthread_mutex_unlock(&thd->mysys_var->mutex);
    DBG_SHUT(fprintf(stderr, "HNDSOCK wsts kst %d\n", (int)st));
    pthread_mutex_lock(&LOCK_server_started);
    if (st != NOT_KILLED) {
      DBG_SHUT(fprintf(stderr, "HNDSOCK wsts kst %d break\n", (int)st));
      r = -1;
      break;
    }
    if (shutdown_flag) {
      DBG_SHUT(fprintf(stderr, "HNDSOCK wsts kst shut break\n"));
      r = -1;
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_server_started);
  DBG_SHUT(fprintf(stderr, "HNDSOCK wsts done\n"));
  return r;
}

}; // namespace

#define DENA_THR_OFFSETOF(fld) ((char *)(&thd->fld) - (char *)thd)

void
dbcontext::init_thread(const void *stack_bottom, volatile int& shutdown_flag)
{
  DBG_THR(fprintf(stderr, "HNDSOCK init thread\n"));
  {
    my_thread_init();
    thd = new THD;
    thd->thread_stack = (char *)stack_bottom;
    DBG_THR(fprintf(stderr,
      "thread_stack = %p sizeof(THD)=%zu sizeof(mtx)=%zu "
      "O: %zu %zu %zu %zu %zu %zu %zu\n",
      thd->thread_stack, sizeof(THD), sizeof(LOCK_thread_count),
      DENA_THR_OFFSETOF(mdl_context),
      DENA_THR_OFFSETOF(net),
      DENA_THR_OFFSETOF(LOCK_thd_data),
      DENA_THR_OFFSETOF(mysys_var),
      DENA_THR_OFFSETOF(stmt_arena),
      DENA_THR_OFFSETOF(limit_found_rows),
      DENA_THR_OFFSETOF(locked_tables_list)));
    thd->store_globals();
    thd->system_thread = static_cast<enum_thread_type>(1<<30UL);
    memset(&thd->net, 0, sizeof(thd->net));
    if (for_write_flag) {
      #if MYSQL_VERSION_ID >= 50505
      thd->variables.option_bits |= OPTION_BIN_LOG;
      #else
      thd->options |= OPTION_BIN_LOG;
      #endif
      safeFree(thd->db);
      thd->db = 0;
      thd->db = my_strdup("handlersocket", MYF(0));
    }
    my_pthread_setspecific_ptr(THR_THD, thd);
    DBG_THR(fprintf(stderr, "HNDSOCK x0 %p\n", thd));
  }
  {
    pthread_mutex_lock(&LOCK_thread_count);
    thd->thread_id = thread_id++;
    threads.append(thd);
    ++thread_count;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  DBG_THR(fprintf(stderr, "HNDSOCK init thread wsts\n"));
  wait_server_to_start(thd, shutdown_flag);
  DBG_THR(fprintf(stderr, "HNDSOCK init thread done\n"));

  thd_proc_info(thd, &info_message_buf[0]);
  set_thread_message("hs:listening");
  DBG_THR(fprintf(stderr, "HNDSOCK x1 %p\n", thd));

  lex_start(thd);

  user_lock.reset(new expr_user_lock(thd, user_level_lock_timeout));
}

int
dbcontext::set_thread_message(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(&info_message_buf[0], info_message_buf.size(),
    fmt, ap);
  va_end(ap);
  return n;
}

void
dbcontext::term_thread()
{
  DBG_THR(fprintf(stderr, "HNDSOCK thread end %p\n", thd));
  unlock_tables_if();
  my_pthread_setspecific_ptr(THR_THD, 0);
  {
    pthread_mutex_lock(&LOCK_thread_count);
    delete thd;
    thd = 0;
    --thread_count;
    pthread_mutex_unlock(&LOCK_thread_count);
    my_thread_end();
  }
}

bool
dbcontext::check_alive()
{
  pthread_mutex_lock(&thd->mysys_var->mutex);
  killed_state st = thd->killed;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  DBG_SHUT(fprintf(stderr, "chk HNDSOCK kst %p %p %d %zu\n", thd, &thd->killed,
    (int)st, sizeof(*thd)));
  if (st != NOT_KILLED) {
    DBG_SHUT(fprintf(stderr, "chk HNDSOCK kst %d break\n", (int)st));
    return false;
  }
  return true;
}

void
dbcontext::lock_tables_if()
{
  if (lock_failed) {
    return;
  }
  if (for_write_flag && !user_level_lock_locked) {
    if (user_lock->get_lock()) {
      user_level_lock_locked = true;
    } else {
      lock_failed = true;
      return;
    }
  }
  if (lock == 0) {
    const size_t num_max = table_vec.size();
    TABLE **const tables = DENA_ALLOCA_ALLOCATE(TABLE *, num_max + 1);
    size_t num_open = 0;
    for (size_t i = 0; i < num_max; ++i) {
      if (table_vec[i].refcount > 0) {
	tables[num_open++] = table_vec[i].table;
      }
      table_vec[i].modified = false;
    }
    #if MYSQL_VERSION_ID >= 50505
    lock = thd->lock = mysql_lock_tables(thd, &tables[0], num_open, 0);
    #else
    bool need_reopen= false;
    lock = thd->lock = mysql_lock_tables(thd, &tables[0], num_open,
      MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN, &need_reopen);
    #endif
    statistic_increment(lock_tables_count, &LOCK_status);
    thd_proc_info(thd, &info_message_buf[0]);
    DENA_VERBOSE(100, fprintf(stderr, "HNDSOCK lock tables %p %p %zu %zu\n",
      thd, lock, num_max, num_open));
    if (lock == 0) {
      lock_failed = true;
      DENA_VERBOSE(10, fprintf(stderr, "HNDSOCK failed to lock tables %p\n",
	thd));
    }
    if (for_write_flag) {
      #if MYSQL_VERSION_ID >= 50505
      thd->set_current_stmt_binlog_format_row();
      #else
      thd->current_stmt_binlog_row_based = 1;
      #endif
    }
    DENA_ALLOCA_FREE(tables);
  }
  DBG_LOCK(fprintf(stderr, "HNDSOCK tblnum=%d\n", (int)tblnum));
}

void
dbcontext::unlock_tables_if()
{
  if (lock != 0) {
    DENA_VERBOSE(100, fprintf(stderr, "HNDSOCK unlock tables %p %p\n",
      thd, thd->lock));
    if (for_write_flag) {
      for (size_t i = 0; i < table_vec.size(); ++i) {
	if (table_vec[i].modified) {
	  query_cache_invalidate3(thd, table_vec[i].table, 1);
	  table_vec[i].table->file->ha_release_auto_increment();
	}
      }
    }
    {
      bool suc = true;
      #if MYSQL_VERSION_ID >= 50505
      suc = (trans_commit_stmt(thd) == 0);
      #else
      suc = (ha_autocommit_or_rollback(thd, 0) == 0);
      #endif
      if (!suc) {
	commit_error = true;
	DENA_VERBOSE(10, fprintf(stderr,
	  "HNDSOCK unlock tables: commit failed\n"));
      }
    }
    mysql_unlock_tables(thd, lock);
    lock = thd->lock = 0;
    statistic_increment(unlock_tables_count, &LOCK_status);
  }
  if (user_level_lock_locked) {
    if (user_lock->release_lock()) {
      user_level_lock_locked = false;
    }
  }
}

bool
dbcontext::get_commit_error()
{
  return commit_error;
}

void
dbcontext::clear_error()
{
  lock_failed = false;
  commit_error = false;
}

void
dbcontext::close_tables_if()
{
  unlock_tables_if();
  DENA_VERBOSE(100, fprintf(stderr, "HNDSOCK close tables\n"));
  close_thread_tables(thd);
  #if MYSQL_VERSION_ID >= 50505
  thd->mdl_context.release_transactional_locks();
  #endif
  if (!table_vec.empty()) {
    statistic_increment(close_tables_count, &LOCK_status);
    table_vec.clear();
    table_map.clear();
  }
}

void
dbcontext::table_addref(size_t tbl_id)
{
  table_vec[tbl_id].refcount += 1;
  DBG_REFCNT(fprintf(stderr, "%p %zu %zu addref\n", this, tbl_id,
    table_vec[tbl_id].refcount));
}

void
dbcontext::table_release(size_t tbl_id)
{
  table_vec[tbl_id].refcount -= 1;
  DBG_REFCNT(fprintf(stderr, "%p %zu %zu release\n", this, tbl_id,
    table_vec[tbl_id].refcount));
}

void
dbcontext::resp_record(dbcallback_i& cb, TABLE *const table,
  const prep_stmt& pst)
{
  char rwpstr_buf[64];
  String rwpstr(rwpstr_buf, sizeof(rwpstr_buf), &my_charset_bin);
  const prep_stmt::fields_type& rf = pst.get_ret_fields();
  const size_t n = rf.size();
  for (size_t i = 0; i < n; ++i) {
    uint32_t fn = rf[i];
    Field *const fld = table->field[fn];
    DBG_FLD(fprintf(stderr, "fld=%p %zu\n", fld, fn));
    if (fld->is_null()) {
      /* null */
      cb.dbcb_resp_entry(0, 0);
    } else {
      fld->val_str(&rwpstr, &rwpstr);
      const size_t len = rwpstr.length();
      if (len != 0) {
	/* non-empty */
	cb.dbcb_resp_entry(rwpstr.ptr(), rwpstr.length());
      } else {
	/* empty */
	static const char empty_str[] = "";
	cb.dbcb_resp_entry(empty_str, 0);
      }
    }
  }
}

void
dbcontext::dump_record(dbcallback_i& cb, TABLE *const table,
  const prep_stmt& pst)
{
  char rwpstr_buf[64];
  String rwpstr(rwpstr_buf, sizeof(rwpstr_buf), &my_charset_bin);
  const prep_stmt::fields_type& rf = pst.get_ret_fields();
  const size_t n = rf.size();
  for (size_t i = 0; i < n; ++i) {
    uint32_t fn = rf[i];
    Field *const fld = table->field[fn];
    if (fld->is_null()) {
      /* null */
      fprintf(stderr, "NULL");
    } else {
      fld->val_str(&rwpstr, &rwpstr);
      const std::string s(rwpstr.ptr(), rwpstr.length());
      fprintf(stderr, "[%s]", s.c_str());
    }
  }
  fprintf(stderr, "\n");
}

int
dbcontext::modify_record(dbcallback_i& cb, TABLE *const table,
  const prep_stmt& pst, const cmd_exec_args& args, char mod_op,
  size_t& modified_count)
{
  if (mod_op == 'U') {
    /* update */
    handler *const hnd = table->file;
    uchar *const buf = table->record[0];
    store_record(table, record[1]);
    const prep_stmt::fields_type& rf = pst.get_ret_fields();
    const size_t n = rf.size();
    for (size_t i = 0; i < n; ++i) {
      const string_ref& nv = args.uvals[i];
      uint32_t fn = rf[i];
      Field *const fld = table->field[fn];
      if (nv.begin() == 0) {
	fld->set_null();
      } else {
	fld->set_notnull();
	fld->store(nv.begin(), nv.size(), &my_charset_bin);
      }
    }
    table_vec[pst.get_table_id()].modified = true;
    const int r = hnd->ha_update_row(table->record[1], buf);
    if (r != 0 && r != HA_ERR_RECORD_IS_THE_SAME) {
      return r;
    }
    ++modified_count; /* TODO: HA_ERR_RECORD_IS_THE_SAME? */
  } else if (mod_op == 'D') {
    /* delete */
    handler *const hnd = table->file;
    table_vec[pst.get_table_id()].modified = true;
    const int r = hnd->ha_delete_row(table->record[0]);
    if (r != 0) {
      return r;
    }
    ++modified_count;
  } else if (mod_op == '+' || mod_op == '-') {
    /* increment/decrement */
    handler *const hnd = table->file;
    uchar *const buf = table->record[0];
    store_record(table, record[1]);
    const prep_stmt::fields_type& rf = pst.get_ret_fields();
    const size_t n = rf.size();
    size_t i = 0;
    for (i = 0; i < n; ++i) {
      const string_ref& nv = args.uvals[i];
      uint32_t fn = rf[i];
      Field *const fld = table->field[fn];
      if (fld->is_null() || nv.begin() == 0) {
	continue;
      }
      const long long pval = fld->val_int();
      const long long llv = atoll_nocheck(nv.begin(), nv.end());
      /* TODO: llv == 0? */
      long long nval = 0;
      if (mod_op == '+') {
	/* increment */
	nval = pval + llv;
      } else {
	/* decrement */
	nval = pval - llv;
	if ((pval < 0 && nval > 0) || (pval > 0 && nval < 0)) {
	  break; /* don't modify */
	}
      }
      fld->store(nval, false);
    }
    if (i == n) {
      /* modify */
      table_vec[pst.get_table_id()].modified = true;
      const int r = hnd->ha_update_row(table->record[1], buf);
      if (r != 0 && r != HA_ERR_RECORD_IS_THE_SAME) {
	return r;
      }
      ++modified_count;
    }
  }
  return 0;
}

void
dbcontext::cmd_insert_internal(dbcallback_i& cb, const prep_stmt& pst,
  const string_ref *fvals, size_t fvalslen)
{
  if (!for_write_flag) {
    return cb.dbcb_resp_short(2, "readonly");
  }
  lock_tables_if();
  if (lock == 0) {
    return cb.dbcb_resp_short(1, "lock_tables");
  }
  if (pst.get_table_id() >= table_vec.size()) {
    return cb.dbcb_resp_short(2, "tblnum");
  }
  TABLE *const table = table_vec[pst.get_table_id()].table;
  handler *const hnd = table->file;
  uchar *const buf = table->record[0];
  empty_record(table);
  memset(buf, 0, table->s->null_bytes); /* clear null flags */
  const prep_stmt::fields_type& rf = pst.get_ret_fields();
  const size_t n = rf.size();
  for (size_t i = 0; i < n; ++i) {
    uint32_t fn = rf[i];
    Field *const fld = table->field[fn];
    if (fvals[i].begin() == 0) {
      fld->set_null();
    } else {
      fld->store(fvals[i].begin(), fvals[i].size(), &my_charset_bin);
    }
  }
  table->next_number_field = table->found_next_number_field;
    /* FIXME: test */
  const int r = hnd->ha_write_row(buf);
  const ulonglong insert_id = table->file->insert_id_for_cur_row;
  table->next_number_field = 0;
  table_vec[pst.get_table_id()].modified = true;
  if (r == 0 && table->found_next_number_field != 0) {
    return cb.dbcb_resp_short_num64(0, insert_id);
  }
  if (r != 0) {
    return cb.dbcb_resp_short_num(1, r);
  }
  return cb.dbcb_resp_short(0, "");
}

void
dbcontext::cmd_sql_internal(dbcallback_i& cb, const prep_stmt& pst,
  const string_ref *fvals, size_t fvalslen)
{
  if (fvalslen < 1) {
    return cb.dbcb_resp_short(2, "syntax");
  }
  return cb.dbcb_resp_short(2, "notimpl");
}

static size_t
prepare_keybuf(const cmd_exec_args& args, uchar *key_buf, TABLE *table,
  KEY& kinfo, size_t invalues_index)
{
  size_t kplen_sum = 0;
  DBG_KEY(fprintf(stderr, "SLOW\n"));
  for (size_t i = 0; i < args.kvalslen; ++i) {
    const KEY_PART_INFO & kpt = kinfo.key_part[i];
    string_ref kval = args.kvals[i];
    if (args.invalues_keypart >= 0 &&
      static_cast<size_t>(args.invalues_keypart) == i) {
      kval = args.invalues[invalues_index];
    }
    if (kval.begin() == 0) {
      kpt.field->set_null();
    } else {
      kpt.field->set_notnull();
    }
    kpt.field->store(kval.begin(), kval.size(), &my_charset_bin);
    kplen_sum += kpt.store_length;
    DBG_KEYLEN(fprintf(stderr, "l=%u sl=%zu\n", kpt.length,
      kpt.store_length));
  }
  key_copy(key_buf, table->record[0], &kinfo, kplen_sum);
  DBG_KEYLEN(fprintf(stderr, "sum=%zu flen=%u\n", kplen_sum,
    kinfo.key_length));
  return kplen_sum;
}

void
dbcontext::cmd_find_internal(dbcallback_i& cb, const prep_stmt& pst,
  ha_rkey_function find_flag, const cmd_exec_args& args)
{
  const bool debug_out = (verbose_level >= 100);
  bool need_resp_record = true;
  char mod_op = 0;
  const string_ref& mod_op_str = args.mod_op;
  if (mod_op_str.size() != 0) {
    if (!for_write_flag) {
      return cb.dbcb_resp_short(2, "readonly");
    }
    mod_op = mod_op_str.begin()[0];
    need_resp_record = mod_op_str.size() > 1 && mod_op_str.begin()[1] == '?';
    switch (mod_op) {
    case 'U': /* update */
    case 'D': /* delete */
    case '+': /* increment */
    case '-': /* decrement */
      break;
    default:
      if (debug_out) {
	fprintf(stderr, "unknown modop: %c\n", mod_op);
      }
      return cb.dbcb_resp_short(2, "modop");
    }
  }
  lock_tables_if();
  if (lock == 0) {
    return cb.dbcb_resp_short(1, "lock_tables");
  }
  if (pst.get_table_id() >= table_vec.size()) {
    return cb.dbcb_resp_short(2, "tblnum");
  }
  TABLE *const table = table_vec[pst.get_table_id()].table;
  /* keys */
  if (pst.get_idxnum() >= table->s->keys) {
    return cb.dbcb_resp_short(2, "idxnum");
  }
  KEY& kinfo = table->key_info[pst.get_idxnum()];
  if (args.kvalslen > kinfo.key_parts) {
    return cb.dbcb_resp_short(2, "kpnum");
  }
  uchar *const key_buf = DENA_ALLOCA_ALLOCATE(uchar, kinfo.key_length);
  size_t invalues_idx = 0;
  size_t kplen_sum = prepare_keybuf(args, key_buf, table, kinfo, invalues_idx);
  /* filters */
  uchar *filter_buf = 0;
  if (args.filters != 0) {
    const size_t filter_buf_len = calc_filter_buf_size(table, pst,
      args.filters);
    filter_buf = DENA_ALLOCA_ALLOCATE(uchar, filter_buf_len);
    if (!fill_filter_buf(table, pst, args.filters, filter_buf,
      filter_buf_len)) {
      return cb.dbcb_resp_short(2, "filterblob");
    }
  }
  /* handler */
  table->read_set = &table->s->all_set;
  handler *const hnd = table->file;
  if (!for_write_flag) {
    hnd->init_table_handle_for_HANDLER();
  }
  hnd->ha_index_or_rnd_end();
  hnd->ha_index_init(pst.get_idxnum(), 1);
  if (need_resp_record) {
    cb.dbcb_resp_begin(pst.get_ret_fields().size());
  }
  const uint32_t limit = args.limit ? args.limit : 1;
  uint32_t skip = args.skip;
  size_t modified_count = 0;
  int r = 0;
  bool is_first = true;
  for (uint32_t cnt = 0; cnt < limit + skip;) {
    if (is_first) {
      is_first = false;
      const key_part_map kpm = (1U << args.kvalslen) - 1;
      r = hnd->ha_index_read_map(table->record[0], key_buf, kpm, find_flag);
    } else if (args.invalues_keypart >= 0) {
      if (++invalues_idx >= args.invalueslen) {
	break;
      }
      kplen_sum = prepare_keybuf(args, key_buf, table, kinfo, invalues_idx);
      const key_part_map kpm = (1U << args.kvalslen) - 1;
      r = hnd->ha_index_read_map(table->record[0], key_buf, kpm, find_flag);
    } else {
      switch (find_flag) {
      case HA_READ_BEFORE_KEY:
      case HA_READ_KEY_OR_PREV:
	r = hnd->ha_index_prev(table->record[0]);
	break;
      case HA_READ_AFTER_KEY:
      case HA_READ_KEY_OR_NEXT:
	r = hnd->ha_index_next(table->record[0]);
	break;
      case HA_READ_KEY_EXACT:
	r = hnd->ha_index_next_same(table->record[0], key_buf, kplen_sum);
	break;
      default:
	r = HA_ERR_END_OF_FILE; /* to finish the loop */
	break;
      }
    }
    if (debug_out) {
      fprintf(stderr, "r=%d\n", r);
      if (r == 0 || r == HA_ERR_RECORD_DELETED) { 
	dump_record(cb, table, pst);
      }
    }
    int filter_res = 0;
    if (r != 0) {
      /* no-count */
    } else if (args.filters != 0 && (filter_res = check_filter(cb, table, 
      pst, args.filters, filter_buf)) != 0) {
      if (filter_res < 0) {
	break;
      }
    } else if (skip > 0) {
      --skip;
    } else {
      /* hit */
      if (need_resp_record) {
	resp_record(cb, table, pst);
      }
      if (mod_op != 0) {
	r = modify_record(cb, table, pst, args, mod_op, modified_count);
      }
      ++cnt;
    }
    if (args.invalues_keypart >= 0 && r == HA_ERR_KEY_NOT_FOUND) {
      continue;
    }
    if (r != 0 && r != HA_ERR_RECORD_DELETED) {
      break;
    }
  }
  hnd->ha_index_or_rnd_end();
  if (r != 0 && r != HA_ERR_RECORD_DELETED && r != HA_ERR_KEY_NOT_FOUND &&
    r != HA_ERR_END_OF_FILE) {
    /* failed */
    if (need_resp_record) {
      /* revert dbcb_resp_begin() and dbcb_resp_entry() */
      cb.dbcb_resp_cancel();
    }
    cb.dbcb_resp_short_num(1, r);
  } else {
    /* succeeded */
    if (need_resp_record) {
      cb.dbcb_resp_end();
    } else {
      cb.dbcb_resp_short_num(0, modified_count);
    }
  }
  DENA_ALLOCA_FREE(filter_buf);
  DENA_ALLOCA_FREE(key_buf);
}

size_t
dbcontext::calc_filter_buf_size(TABLE *table, const prep_stmt& pst,
  const record_filter *filters)
{
  size_t filter_buf_len = 0;
  for (const record_filter *f = filters; f->op.begin() != 0; ++f) {
    if (f->val.begin() == 0) {
      continue;
    }
    const uint32_t fn = pst.get_filter_fields()[f->ff_offset];
    filter_buf_len += table->field[fn]->pack_length();
  }
  ++filter_buf_len;
    /* Field_medium::cmp() calls uint3korr(), which may read 4 bytes.
       Allocate 1 more byte for safety. */
  return filter_buf_len;
}

bool
dbcontext::fill_filter_buf(TABLE *table, const prep_stmt& pst,
  const record_filter *filters, uchar *filter_buf, size_t len)
{
  memset(filter_buf, 0, len);
  size_t pos = 0;
  for (const record_filter *f = filters; f->op.begin() != 0; ++f) {
    if (f->val.begin() == 0) {
      continue;
    }
    const uint32_t fn = pst.get_filter_fields()[f->ff_offset];
    Field *const fld = table->field[fn];
    if ((fld->flags & BLOB_FLAG) != 0) {
      return false;
    }
    fld->store(f->val.begin(), f->val.size(), &my_charset_bin);
    const size_t packlen = fld->pack_length();
    memcpy(filter_buf + pos, fld->ptr, packlen);
    pos += packlen;
  }
  return true;
}

int
dbcontext::check_filter(dbcallback_i& cb, TABLE *table, const prep_stmt& pst,
  const record_filter *filters, const uchar *filter_buf)
{
  DBG_FILTER(fprintf(stderr, "check_filter\n"));
  size_t pos = 0;
  for (const record_filter *f = filters; f->op.begin() != 0; ++f) {
    const string_ref& op = f->op;
    const string_ref& val = f->val;
    const uint32_t fn = pst.get_filter_fields()[f->ff_offset];
    Field *const fld = table->field[fn];
    const size_t packlen = fld->pack_length();
    const uchar *const bval = filter_buf + pos;
    int cv = 0;
    if (fld->is_null()) {
      cv = (val.begin() == 0) ? 0 : -1;
    } else {
      cv = (val.begin() == 0) ? 1 : fld->cmp(bval);
    }
    DBG_FILTER(fprintf(stderr, "check_filter cv=%d\n", cv));
    bool cond = true;
    if (op.size() == 1) {
      switch (op.begin()[0]) {
      case '>':
	DBG_FILTER(fprintf(stderr, "check_filter op: >\n"));
	cond = (cv > 0);
	break;
      case '<':
	DBG_FILTER(fprintf(stderr, "check_filter op: <\n"));
	cond = (cv < 0);
	break;
      case '=':
	DBG_FILTER(fprintf(stderr, "check_filter op: =\n"));
	cond = (cv == 0);
	break;
      default:
	DBG_FILTER(fprintf(stderr, "check_filter op: unknown\n"));
	cond = false; /* FIXME: error */
	break;
      }
    } else if (op.size() == 2 && op.begin()[1] == '=') {
      switch (op.begin()[0]) {
      case '>':
	DBG_FILTER(fprintf(stderr, "check_filter op: >=\n"));
	cond = (cv >= 0);
	break;
      case '<':
	DBG_FILTER(fprintf(stderr, "check_filter op: <=\n"));
	cond = (cv <= 0);
	break;
      case '!':
	DBG_FILTER(fprintf(stderr, "check_filter op: !=\n"));
	cond = (cv != 0);
	break;
      default:
	DBG_FILTER(fprintf(stderr, "check_filter op: unknown\n"));
	cond = false; /* FIXME: error */
	break;
      }
    }
    DBG_FILTER(fprintf(stderr, "check_filter cond: %d\n", (int)cond));
    if (!cond) {
      return (f->filter_type == record_filter_type_skip) ? 1 : -1;
    }
    if (val.begin() != 0) {
      pos += packlen;
    }
  }
  return 0;
}

void
dbcontext::cmd_open(dbcallback_i& cb, const cmd_open_args& arg)
{
  unlock_tables_if();
  const table_name_type k = std::make_pair(std::string(arg.dbn),
    std::string(arg.tbl));
  const table_map_type::const_iterator iter = table_map.find(k);
  uint32_t tblnum = 0;
  if (iter != table_map.end()) {
    tblnum = iter->second;
    DBG_CMP(fprintf(stderr, "HNDSOCK k=%s tblnum=%d\n", k.c_str(),
      (int)tblnum));
  } else {
    TABLE_LIST tables;
    TABLE *table = 0;
    bool refresh = true;
    const thr_lock_type lock_type = for_write_flag ? TL_WRITE : TL_READ;
    #if MYSQL_VERSION_ID >= 50505
    tables.init_one_table(arg.dbn, strlen(arg.dbn), arg.tbl, strlen(arg.tbl),
      arg.tbl, lock_type);
    tables.mdl_request.init(MDL_key::TABLE, arg.dbn, arg.tbl,
      for_write_flag ? MDL_SHARED_WRITE : MDL_SHARED_READ, MDL_TRANSACTION);
    Open_table_context ot_act(thd, 0);
    if (!open_table(thd, &tables, thd->mem_root, &ot_act)) {
      table = tables.table;
    }
    #else
    tables.init_one_table(arg.dbn, arg.tbl, lock_type);
    table = open_table(thd, &tables, thd->mem_root, &refresh,
      OPEN_VIEW_NO_PARSE);
    #endif
    if (table == 0) {
      DENA_VERBOSE(20, fprintf(stderr,
	"HNDSOCK failed to open %p [%s] [%s] [%d]\n",
	thd, arg.dbn, arg.tbl, static_cast<int>(refresh)));
      return cb.dbcb_resp_short(1, "open_table");
    }
    statistic_increment(open_tables_count, &LOCK_status);
    table->reginfo.lock_type = lock_type;
    table->use_all_columns();
    tblnum = table_vec.size();
    tablevec_entry e;
    e.table = table;
    table_vec.push_back(e);
    table_map[k] = tblnum;
  }
  size_t idxnum = static_cast<size_t>(-1);
  if (arg.idx[0] >= '0' && arg.idx[0] <= '9') {
    /* numeric */
    TABLE *const table = table_vec[tblnum].table;
    idxnum = atoi(arg.idx);
    if (idxnum >= table->s->keys) {
      return cb.dbcb_resp_short(2, "idxnum");
    }
  } else {
    const char *const idx_name_to_open =
      arg.idx[0]  == '\0' ? "PRIMARY" : arg.idx;
    TABLE *const table = table_vec[tblnum].table;
    for (uint i = 0; i < table->s->keys; ++i) {
      KEY& kinfo = table->key_info[i];
      if (strcmp(kinfo.name, idx_name_to_open) == 0) {
	idxnum = i;
	break;
      }
    }
  }
  if (idxnum == size_t(-1)) {
    return cb.dbcb_resp_short(2, "idxnum");
  }
  prep_stmt::fields_type rf;
  prep_stmt::fields_type ff;
  if (!parse_fields(table_vec[tblnum].table, arg.retflds, rf)) {
    return cb.dbcb_resp_short(2, "fld");
  }
  if (!parse_fields(table_vec[tblnum].table, arg.filflds, ff)) {
    return cb.dbcb_resp_short(2, "fld");
  }
  prep_stmt p(this, tblnum, idxnum, rf, ff);
  cb.dbcb_set_prep_stmt(arg.pst_id, p);
  return cb.dbcb_resp_short(0, "");
}

bool
dbcontext::parse_fields(TABLE *const table, const char *str,
  prep_stmt::fields_type& flds)
{
  string_ref flds_sr(str, strlen(str));
  std::vector<string_ref> fldnms;
  if (flds_sr.size() != 0) {
    split(',', flds_sr, fldnms);
  }
  for (size_t i = 0; i < fldnms.size(); ++i) {
    Field **fld = 0;
    size_t j = 0;
    for (fld = table->field; *fld; ++fld, ++j) {
      DBG_FLD(fprintf(stderr, "f %s\n", (*fld)->field_name));
      string_ref fn((*fld)->field_name, strlen((*fld)->field_name));
      if (fn == fldnms[i]) {
	break;
      }
    }
    if (*fld == 0) {
      DBG_FLD(fprintf(stderr, "UNKNOWN FLD %s [%s]\n", retflds,
	std::string(fldnms[i].begin(), fldnms[i].size()).c_str()));
      return false;
    }
    DBG_FLD(fprintf(stderr, "FLD %s %zu\n", (*fld)->field_name, j));
    flds.push_back(j);
  }
  return true;
}

enum db_write_op {
  db_write_op_none = 0,
  db_write_op_insert = 1,
  db_write_op_sql = 2,
};

void
dbcontext::cmd_exec(dbcallback_i& cb, const cmd_exec_args& args)
{
  const prep_stmt& p = *args.pst;
  if (p.get_table_id() == static_cast<size_t>(-1)) {
    return cb.dbcb_resp_short(2, "stmtnum");
  }
  ha_rkey_function find_flag = HA_READ_KEY_EXACT;
  db_write_op wrop = db_write_op_none;
  if (args.op.size() == 1) {
    switch (args.op.begin()[0]) {
    case '=':
      find_flag = HA_READ_KEY_EXACT;
      break;
    case '>':
      find_flag = HA_READ_AFTER_KEY;
      break;
    case '<':
      find_flag = HA_READ_BEFORE_KEY;
      break;
    case '+':
      wrop = db_write_op_insert;
      break;
    case 'S':
      wrop = db_write_op_sql;
      break;
    default:
      return cb.dbcb_resp_short(2, "op");
    }
  } else if (args.op.size() == 2 && args.op.begin()[1] == '=') {
    switch (args.op.begin()[0]) {
    case '>':
      find_flag = HA_READ_KEY_OR_NEXT;
      break;
    case '<':
      find_flag = HA_READ_KEY_OR_PREV;
      break;
    default:
      return cb.dbcb_resp_short(2, "op");
    }
  } else {
    return cb.dbcb_resp_short(2, "op");
  }
  if (args.kvalslen <= 0) {
    return cb.dbcb_resp_short(2, "klen");
  }
  switch (wrop) {
  case db_write_op_none:
    return cmd_find_internal(cb, p, find_flag, args);
  case db_write_op_insert:
    return cmd_insert_internal(cb, p, args.kvals, args.kvalslen);
  case db_write_op_sql:
    return cmd_sql_internal(cb, p, args.kvals, args.kvalslen);
  }
}

void
dbcontext::set_statistics(size_t num_conns, size_t num_active)
{
  if (for_write_flag) {
    set_thread_message("handlersocket: mode=wr, %zu conns, %zu active",
      num_conns, num_active);
  } else {
    set_thread_message("handlersocket: mode=rd, %zu conns, %zu active",
      num_conns, num_active);
  }
  /*
    Don't set message buf if it's already in use. This saves slow call to
    thd_proc_info() (if profiling is enabled)
  */
  if (thd->proc_info != &info_message_buf[0])
    thd_proc_info(thd, &info_message_buf[0]);
}

};

