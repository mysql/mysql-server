
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_DATABASE_HPP
#define DENA_DATABASE_HPP

#include <string>
#include <memory>
#include <vector>
#include <stdint.h>

#include "string_buffer.hpp"
#include "string_ref.hpp"
#include "config.hpp"

namespace dena {

struct database_i;
typedef std::auto_ptr<volatile database_i> database_ptr;

struct dbcontext_i;
typedef std::auto_ptr<dbcontext_i> dbcontext_ptr;

struct database_i {
  virtual ~database_i() { }
  virtual dbcontext_ptr create_context(bool for_write) volatile = 0;
  virtual void stop() volatile = 0;
  virtual const config& get_conf() const volatile = 0;
  static database_ptr create(const config& conf);
};

struct prep_stmt {
  typedef std::vector<uint32_t> fields_type;
 private:
  dbcontext_i *dbctx; /* must be valid while *this is alive */
  size_t table_id; /* a prep_stmt object holds a refcount of the table */
  size_t idxnum;
  fields_type ret_fields;
  fields_type filter_fields;
 public:
  prep_stmt();
  prep_stmt(dbcontext_i *c, size_t tbl, size_t idx, const fields_type& rf,
    const fields_type& ff);
  ~prep_stmt();
  prep_stmt(const prep_stmt& x);
  prep_stmt& operator =(const prep_stmt& x);
 public:
  size_t get_table_id() const { return table_id; }
  size_t get_idxnum() const { return idxnum; }
  const fields_type& get_ret_fields() const { return ret_fields; }
  const fields_type& get_filter_fields() const { return filter_fields; }
};

struct dbcallback_i {
  virtual ~dbcallback_i () { }
  virtual void dbcb_set_prep_stmt(size_t pst_id, const prep_stmt& v) = 0;
  virtual const prep_stmt *dbcb_get_prep_stmt(size_t pst_id) const = 0;
  virtual void dbcb_resp_short(uint32_t code, const char *msg) = 0;
  virtual void dbcb_resp_short_num(uint32_t code, uint32_t value) = 0;
  virtual void dbcb_resp_short_num64(uint32_t code, uint64_t value) = 0;
  virtual void dbcb_resp_begin(size_t num_flds) = 0;
  virtual void dbcb_resp_entry(const char *fld, size_t fldlen) = 0;
  virtual void dbcb_resp_end() = 0;
  virtual void dbcb_resp_cancel() = 0;
};

enum record_filter_type {
  record_filter_type_skip = 0,
  record_filter_type_break = 1,
};

struct record_filter {
  record_filter_type filter_type;
  string_ref op;
  uint32_t ff_offset; /* offset in filter_fields */
  string_ref val;
  record_filter() : filter_type(record_filter_type_skip), ff_offset(0) { }
};

struct cmd_open_args {
  size_t pst_id;
  const char *dbn;
  const char *tbl;
  const char *idx;
  const char *retflds;
  const char *filflds;
  cmd_open_args() : pst_id(0), dbn(0), tbl(0), idx(0), retflds(0),
    filflds(0) { }
};

struct cmd_exec_args {
  const prep_stmt *pst;
  string_ref op;
  const string_ref *kvals;
  size_t kvalslen;
  uint32_t limit;
  uint32_t skip;
  string_ref mod_op;
  const string_ref *uvals; /* size must be pst->retfieelds.size() */
  const record_filter *filters;
  int invalues_keypart;
  const string_ref *invalues;
  size_t invalueslen;
  cmd_exec_args() : pst(0), kvals(0), kvalslen(0), limit(0), skip(0),
    uvals(0), filters(0), invalues_keypart(-1), invalues(0), invalueslen(0) { }
};

struct dbcontext_i {
  virtual ~dbcontext_i() { }
  virtual void init_thread(const void *stack_bottom,
    volatile int& shutdown_flag) = 0;
  virtual void term_thread() = 0;
  virtual bool check_alive() = 0;
  virtual void lock_tables_if() = 0;
  virtual void unlock_tables_if() = 0;
  virtual bool get_commit_error() = 0;
  virtual void clear_error() = 0;
  virtual void close_tables_if() = 0;
  virtual void table_addref(size_t tbl_id) = 0; /* TODO: hide */
  virtual void table_release(size_t tbl_id) = 0; /* TODO: hide */
  virtual void cmd_open(dbcallback_i& cb, const cmd_open_args& args) = 0;
  virtual void cmd_exec(dbcallback_i& cb, const cmd_exec_args& args) = 0;
  virtual void set_statistics(size_t num_conns, size_t num_active) = 0;
};

};

extern unsigned long long int open_tables_count;
extern unsigned long long int close_tables_count;
extern unsigned long long int lock_tables_count;
extern unsigned long long int unlock_tables_count;
#if 0
extern unsigned long long int index_exec_count;
#endif

#endif

