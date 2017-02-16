
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_AUTO_FILE_HPP
#define DENA_AUTO_FILE_HPP

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

#include "util.hpp"

namespace dena {

struct auto_file : private noncopyable {
  auto_file() : fd(-1) { }
  ~auto_file() {
    reset();
  }
  int get() const { return fd; }
  int close() {
    if (fd < 0) {
      return 0;
    }
    const int r = ::close(fd);
    fd = -1;
    return r;
  }
  void reset(int x = -1) {
    if (fd >= 0) {
      this->close();
    }
    fd = x;
  }
 private:
  int fd;
};

struct auto_dir : private noncopyable {
  auto_dir() : dp(0) { }
  ~auto_dir() {
    reset();
  }
  DIR *get() const { return dp; }
  void reset(DIR *d = 0) {
    if (dp != 0) {
      closedir(dp);
    }
    dp = d;
  }
 private:
  DIR *dp;
};

};

#endif

