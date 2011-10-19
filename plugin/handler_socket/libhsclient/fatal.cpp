
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "fatal.hpp"

namespace dena {

const int opt_syslog = LOG_ERR | LOG_PID | LOG_CONS;

void
fatal_exit(const std::string& message)
{
  fprintf(stderr, "FATAL_EXIT: %s\n", message.c_str());
  syslog(opt_syslog, "FATAL_EXIT: %s", message.c_str());
  _exit(1);
}

void
fatal_abort(const std::string& message)
{
  fprintf(stderr, "FATAL_COREDUMP: %s\n", message.c_str());
  syslog(opt_syslog, "FATAL_COREDUMP: %s", message.c_str());
  abort();
}

};

