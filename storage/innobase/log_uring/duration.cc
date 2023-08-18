#include <atomic>
#include "log_uring/duration.h"

struct log_stat {

  log_stat() {
    append_count = 0;
    sync_count = 0;
    total_log_size = 0;
    start = std::chrono::steady_clock::now();
  }

  std::atomic_int64_t total_log_size;
  std::atomic_int64_t append_count;
  std::atomic_int64_t sync_count;
  std::chrono::steady_clock::time_point start;
};

static log_stat _stat;

void log_append_count_inc(uint64_t size) {
  _stat.total_log_size ++;
  _stat.append_count ++;
}

void log_sync_count_inc() {
  _stat.sync_count ++;
}

std::string log_stat_period() {
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = end - _stat.start;
    int64_t append = _stat.append_count.load(std::memory_order_seq_cst);
    int64_t sync = _stat.sync_count.load(std::memory_order_seq_cst);
    uint64_t total_size = _stat.total_log_size.load(std::memory_order_seq_cst);
    auto start = _stat.start;
    _stat.append_count.store(0, std::memory_order_seq_cst);
    _stat.sync_count.store(0, std::memory_order_seq_cst);
    _stat.total_log_size.store(0, std::memory_order_seq_cst);
    _stat.start = end;
    std::chrono::duration<double> duration = end - start;
    std::stringstream ssm;
    if (duration.count() > 0.000001) {
      ssm << 
      "total write: " << total_size << " bytes, "
      "avg log size: " << total_size / append << " bytes, "
      "append/s: " << append / duration.count() << ", " 
      "sync/s: " << sync / duration.count();
    }
    return ssm.str();
}