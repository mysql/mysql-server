
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_HSTCPCLI_HPP
#define DENA_HSTCPCLI_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <memory>

#include "config.hpp"
#include "socket.hpp"
#include "string_ref.hpp"
#include "string_buffer.hpp"

namespace dena {

struct hstcpcli_filter {
  string_ref filter_type;
  string_ref op;
  size_t ff_offset;
  string_ref val;
  hstcpcli_filter() : ff_offset(0) { }
};

struct hstcpcli_i;
typedef std::auto_ptr<hstcpcli_i> hstcpcli_ptr;

struct hstcpcli_i {
  virtual ~hstcpcli_i() { }
  virtual void close() = 0;
  virtual int reconnect() = 0;
  virtual bool stable_point() = 0;
  virtual void request_buf_auth(const char *secret, const char *typ) = 0;
  virtual void request_buf_open_index(size_t pst_id, const char *dbn,
    const char *tbl, const char *idx, const char *retflds,
    const char *filflds = 0) = 0;
  virtual void request_buf_exec_generic(size_t pst_id, const string_ref& op,
    const string_ref *kvs, size_t kvslen, uint32_t limit, uint32_t skip,
    const string_ref& mod_op, const string_ref *mvs, size_t mvslen,
    const hstcpcli_filter *fils = 0, size_t filslen = 0,
    int invalues_keypart = -1, const string_ref *invalues = 0,
    size_t invalueslen = 0) = 0; // FIXME: too long
  virtual int request_send() = 0;
  virtual int response_recv(size_t& num_flds_r) = 0;
  virtual const string_ref *get_next_row() = 0;
  virtual void response_buf_remove() = 0;
  virtual int get_error_code() = 0;
  virtual std::string get_error() = 0;
  static hstcpcli_ptr create(const socket_args& args);
};

};

#endif

