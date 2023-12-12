// Author: Ming Zhang
// Copyright (c) 2022

#pragma once

#include <cxxabi.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <thread>

#include "common.h"
#include "rlib/logging.hpp"

using namespace rdmaio;

#define ASSERT(condition)     \
  if (unlikely(!(condition))) \
  ::rdmaio::MessageLogger((char*)__FILE__, __LINE__, ::rdmaio::FATAL + 1).stream() << "Assertion! "

#define TLOG(n, tid)       \
  if (n >= RDMA_LOG_LEVEL) \
  LogicalThreadLogger((char*)__FILE__, __LINE__, n, tid).stream()

// Use the logical thread ID
class LogicalThreadLogger {
 public:
  LogicalThreadLogger(const char* file, int line, int level, t_id_t tid) : level_(level), tid_(tid) {
    if (level_ < RDMA_LOG_LEVEL)
      return;
    stream_ << "[" << StripBasename(std::string(file)) << ":" << line << "] ";
  }

  ~LogicalThreadLogger() {
    if (level_ >= RDMA_LOG_LEVEL) {
      std::ofstream fout;
      std::string log_file_name = "./" + std::to_string(tid_) + "_log.txt";
      fout.open(log_file_name, std::ios::app);
      fout << stream_.str() << std::endl;
      fout.close();
      if (level_ >= ::rdmaio::FATAL)
        abort();
    }
  }

  // Return the stream associated with the logger object.
  std::stringstream& stream() { return stream_; }

 private:
  std::stringstream stream_;
  int level_;
  t_id_t tid_;

  static std::string StripBasename(const std::string& full_path) {
    const char kSeparator = '/';
    size_t pos = full_path.rfind(kSeparator);
    if (pos != std::string::npos) {
      return full_path.substr(pos + 1, std::string::npos);
    } else {
      return full_path;
    }
  }
};

// Use the physical thread ID
class PhysicalThreadLogger {
 public:
  PhysicalThreadLogger(const char* file, int line, int level, std::thread::id tid) : level_(level), tid_(tid) {
    if (level_ < RDMA_LOG_LEVEL)
      return;
    stream_ << "[" << StripBasename(std::string(file)) << ":" << line << "] ";
  }

  ~PhysicalThreadLogger() {
    if (level_ >= RDMA_LOG_LEVEL) {
      std::ofstream fout;

      std::ostringstream oss;
      oss << tid_;
      std::string stid = oss.str();
      std::string log_file_name = "./" + stid + "_log.txt";
      fout.open(log_file_name, std::ios::app);
      fout << stream_.str() << std::endl;
      fout.close();
      if (level_ >= ::rdmaio::FATAL)
        abort();
    }
  }

  // Return the stream associated with the logger object.
  std::stringstream& stream() { return stream_; }

 private:
  std::stringstream stream_;
  int level_;
  std::thread::id tid_;

  static std::string StripBasename(const std::string& full_path) {
    const char kSeparator = '/';
    size_t pos = full_path.rfind(kSeparator);
    if (pos != std::string::npos) {
      return full_path.substr(pos + 1, std::string::npos);
    } else {
      return full_path;
    }
  }
};

// https://panthema.net/2008/0901-stacktrace-demangled/
static void PrintStackTrace(FILE* out = stderr, unsigned int max_frames = 63) {
  fprintf(out, "stack trace:\n");

  // storage array for stack trace address data
  void* addrlist[max_frames + 1];

  // retrieve current stack addresses
  int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

  if (addrlen == 0) {
    fprintf(out, "  <empty, possibly corrupt>\n");
    return;
  }

  // resolve addresses into strings containing "filename(function+address)",
  // this array must be free()-ed
  char** symbollist = backtrace_symbols(addrlist, addrlen);

  // allocate string which will be filled with the demangled function name
  size_t funcnamesize = 256;
  char* funcname = (char*)malloc(funcnamesize);

  // iterate over the returned symbol lines. skip the first, it is the
  // address of this function.
  for (int i = 1; i < addrlen; i++) {
    char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

    // find parentheses and +address offset surrounding the mangled name:
    // ./module(function+0x15c) [0x8048a6d]
    for (char* p = symbollist[i]; *p; ++p) {
      if (*p == '(')
        begin_name = p;
      else if (*p == '+')
        begin_offset = p;
      else if (*p == ')' && begin_offset) {
        end_offset = p;
        break;
      }
    }

    if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
      *begin_name++ = '\0';
      *begin_offset++ = '\0';
      *end_offset = '\0';

      // mangled name is now in [begin_name, begin_offset) and caller
      // offset in [begin_offset, end_offset). now apply
      // __cxa_demangle():

      int status;
      char* ret = abi::__cxa_demangle(begin_name,
                                      funcname, &funcnamesize, &status);
      if (status == 0) {
        funcname = ret;  // use possibly realloc()-ed string
        fprintf(out, "  %s : %s+%s\n",
                symbollist[i], funcname, begin_offset);
      } else {
        // demangling failed. Output function name as a C function with
        // no arguments.
        fprintf(out, "  %s : %s()+%s\n",
                symbollist[i], begin_name, begin_offset);
      }
    } else {
      // couldn't parse the line? print the whole line.
      fprintf(out, "  %s\n", symbollist[i]);
    }
  }

  free(funcname);
  free(symbollist);
}
