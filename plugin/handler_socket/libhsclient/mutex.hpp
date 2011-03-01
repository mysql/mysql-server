
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_MUTEX_HPP
#define DENA_MUTEX_HPP

#include <pthread.h>
#include <stdlib.h>

#include "fatal.hpp"
#include "util.hpp"

namespace dena {

struct condition;

struct mutex : private noncopyable {
  friend struct condition;
  mutex() {
    if (pthread_mutex_init(&mtx, 0) != 0) {
      fatal_abort("pthread_mutex_init");
    }
  }
  ~mutex() {
    if (pthread_mutex_destroy(&mtx) != 0) {
      fatal_abort("pthread_mutex_destroy");
    }
  }
  void lock() const {
    if (pthread_mutex_lock(&mtx) != 0) {
      fatal_abort("pthread_mutex_lock");
    }
  }
  void unlock() const {
    if (pthread_mutex_unlock(&mtx) != 0) {
      fatal_abort("pthread_mutex_unlock");
    }
  }
 private:
  mutable pthread_mutex_t mtx;
};

};

#endif

