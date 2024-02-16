/**
 * The logging utilities used in libRDMA.
 */

#pragma once

#include <iostream>
#include <sstream>


namespace rdmaio {

/**
 * \def FATAL
 *   Used for fatal and probably irrecoverable conditions
 * \def ERROR
 *   Used for errors which are recoverable within the scope of the function
 * \def WARNING
 *   Logs interesting conditions which are probably not fatal
 * \def EMPH
 *   Outputs as INFO, but in WARNING colors. Useful for
 *   outputting information you want to emphasize.
 * \def INFO
 *   Used for providing general useful information
 * \def DEBUG
 *   Debugging purposes only
 * \def EVERYTHING
 *   Log everything
 */

enum rdma_loglevel {
  RDMA_LOG_NONE = 7,
  RDMA_LOG_FATAL = 6,
  RDMA_LOG_ERROR = 5,
  RDMA_LOG_WARNING = 4,
  RDMA_LOG_EMPH = 3,
  RDMA_LOG_INFO = 2,
  RDMA_LOG_DBG = 1,
  RDMA_LOG_EVERYTHING = 0
};

#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef RDMA_LOG_LEVEL
#define RDMA_LOG_LEVEL ::rdmaio::RDMA_LOG_DBG
#endif

// logging macro definiations
// default log
#define RDMA_LOG(n)        \
  if (n >= RDMA_LOG_LEVEL) \
  ::rdmaio::MessageLogger((const char *)__FILE__, __LINE__, n).stream()

// #define RDMA_LOG(n)        
//   if (n != ::rdmaio::INFO && n >= RDMA_LOG_LEVEL) 
//   ::rdmaio::MessageLogger((char *)__FILE__, __LINE__, n).stream()

// #define RDMA_LOG(n)        
//   if (false) 
//   ::rdmaio::MessageLogger((char *)__FILE__, __LINE__, n).stream()


// log with tag
#define RDMA_TLOG(n, t)                                           \
  if (n >= RDMA_LOG_LEVEL)                                        \
  ::rdmaio::MessageLogger((const char *)__FILE__, __LINE__, n).stream() \
      << "[" << (t) << "]"

#define RDMA_LOG_IF(n, condition)         \
  if (n >= RDMA_LOG_LEVEL && (condition)) \
  ::rdmaio::MessageLogger((const char *)__FILE__, __LINE__, n).stream()

#define RDMA_ASSERT(condition) \
  if (unlikely(!(condition)))  \
  ::rdmaio::MessageLogger((const char *)__FILE__, __LINE__, ::rdmaio::RDMA_LOG_FATAL + 1).stream() << "Assertion! "

#define RDMA_VERIFY(n, condition) RDMA_LOG_IF(n, (!(condition)))

class MessageLogger {
 public:
  MessageLogger(const char* file, int line, int level) : level_(level) {
    if (level_ < RDMA_LOG_LEVEL)
      return;
    stream_ << "[" << StripBasename(std::string(file)) << ":" << line << "] ";
  }

  ~MessageLogger() {
    if (level_ >= RDMA_LOG_LEVEL) {
      stream_ << "\n";
      std::cout << "\033[" << RDMA_DEBUG_LEVEL_COLOR[std::min(level_, 6)] << "m"
                << stream_.str() << EndcolorFlag();
      if (level_ >= ::rdmaio::RDMA_LOG_FATAL)
        abort();
    }
  }

  // Return the stream associated with the logger object.
  std::stringstream& stream() { return stream_; }

 private:
  std::stringstream stream_;
  int level_;

  // control flags for color
#define R_BLACK 39
#define R_RED 31
#define R_GREEN 32
#define R_YELLOW 33
#define R_BLUE 34
#define R_MAGENTA 35
#define R_CYAN 36
#define R_WHITE 37

  const int RDMA_DEBUG_LEVEL_COLOR[7] = {R_BLACK, R_BLACK, R_YELLOW, R_GREEN, R_MAGENTA, R_RED, R_RED};

  static std::string StripBasename(const std::string& full_path) {
    const char kSeparator = '/';
    size_t pos = full_path.rfind(kSeparator);
    if (pos != std::string::npos) {
      return full_path.substr(pos + 1, std::string::npos);
    } else {
      return full_path;
    }
  }

  static std::string EndcolorFlag() {
    char flag[7];
    snprintf(flag, 7, "%c[0m", 0x1B);
    return std::string(flag);
  }
};

};  // namespace rdmaio
