#pragma once

#include <stdlib.h>
#include <chrono>
#include <sstream>
#include <string>

class op_duration {
  std::chrono::duration<double> duration_;
  size_t count_;
public:
  op_duration():duration_(0), count_(0) {

  }

  void reset() {
    duration_ = std::chrono::duration<double>(0);
    count_ = 0;
  }

  size_t count() const {
    return count_;
  }
  
  void add(std::chrono::duration<double> duration) {
    duration_ += duration;
    count_ += 1;
  }

  void add(const op_duration &op) {
    count_ += op.count_;
    duration_ += op.duration_;
  }

  std::chrono::duration<double> avg_duration() const {
    return duration_ / count_;
  }
};


class xlog_op_duration {
private:
  op_duration append_;
  op_duration sync_;
  uint64_t total_size_;
public:
  xlog_op_duration():total_size_(0) {

  }

  void reset() {
    append_.reset();
    sync_.reset();
  }

  void sync_add(std::chrono::duration<double> duration) {
    sync_.add(duration);
  }

  void append_add(std::chrono::duration<double> duration, uint32_t size) {
    append_.add(duration);
    total_size_ += size;
  }

  void add(const xlog_op_duration &duration) {
      sync_.add(duration.sync_);
      append_.add(duration.append_);
      total_size_ += duration.total_size_;
  }
  
  std::string avg_time_str(int wait_seconds) const {
    std::string ret;
    std::stringstream ssm;
    ssm << 
      "append: " << append_.avg_duration().count() * 1000000 << "us"
      << ", invoke: "  << append_.count() << " times"
      << ", sync: " << sync_.avg_duration().count() * 1000000 << "us"
      << ", invoke: " << sync_.count() << " times"
      << ", write bytes/s: " << (double)(total_size_) / (double)(wait_seconds)
      << ", append/seconds: " <<  ((double)append_.count()) / ((double)wait_seconds)
      << ", sync/seconds: " <<  ((double)sync_.count()) / ((double)wait_seconds)
      ;
    return ssm.str();
  }
};


void log_append_count_inc(uint64_t size);
void log_sync_count_inc();
std::string log_stat_period();
