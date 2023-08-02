#pragma once

#include <chrono>
#include <sstream>

class op_duration {
  std::chrono::duration<double> duration_;
  int count_;
public:
  int count() const {
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
  
public:
  xlog_op_duration() {

  }

  void sync_add(std::chrono::duration<double> duration) {
    sync_.add(duration);
  }

  void append_add(std::chrono::duration<double> duration) {
    append_.add(duration);
  }

  void add(const xlog_op_duration &duration) {
      sync_.add(duration.sync_);
      append_.add(duration.append_);
  }
  
  std::string avg_time_str() const {
    std::string ret;
    std::stringstream ssm;
    ssm << 
      "append: " << append_.avg_duration().count()
      << ", "
      "sync" << sync_.avg_duration().count();
    return ssm.str();
  }
};
