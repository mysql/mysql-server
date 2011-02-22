
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_THREAD_HPP
#define DENA_THREAD_HPP

#include <stdexcept>
#include <pthread.h>

#include "fatal.hpp"

namespace dena {

template <typename T>
struct thread : private noncopyable {
  template <typename Ta> thread(const Ta& arg, size_t stack_sz = 256 * 1024)
    : obj(arg), thr(0), need_join(false), stack_size(stack_sz) { }
  template <typename Ta0, typename Ta1> thread(const Ta0& a0,
    volatile Ta1& a1, size_t stack_sz = 256 * 1024)
    : obj(a0, a1), thr(0), need_join(false), stack_size(stack_sz) { }
  ~thread() {
    join();
  }
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
  T& operator *() { return obj; }
  T *operator ->() { return &obj; }
 private:
  static void *thread_main(void *arg) {
    thread *p = static_cast<thread *>(arg);
    p->obj();
    return 0;
  }
 private:
  T obj;
  pthread_t thr;
  bool need_join;
  size_t stack_size;
};

};

#endif

