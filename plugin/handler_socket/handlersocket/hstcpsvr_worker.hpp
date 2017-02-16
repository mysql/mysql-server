
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_HSTCPSVR_WORKER_HPP
#define DENA_HSTCPSVR_WORKER_HPP

#include "hstcpsvr.hpp"

namespace dena {

struct hstcpsvr_worker_i;
typedef std::auto_ptr<hstcpsvr_worker_i> hstcpsvr_worker_ptr;

struct hstcpsvr_worker_arg {
  const hstcpsvr_shared_c *cshared;
  volatile hstcpsvr_shared_v *vshared;
  long worker_id;
  hstcpsvr_worker_arg() : cshared(0), vshared(0), worker_id(0) { }
};

struct hstcpsvr_worker_i {
  virtual ~hstcpsvr_worker_i() { }
  virtual void run() = 0;
  static hstcpsvr_worker_ptr create(const hstcpsvr_worker_arg& arg);
};

};

#endif

