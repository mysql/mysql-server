#include "log_uring/duration.h"
#include <atomic>
#include <chrono>
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time
#include <string>  // string


class log_stat {
public:
  log_stat():
  total_log_size(0),
  append_count(0),
  sync_count(0),
  start(std::chrono::steady_clock::now()),
  calculate(false),
  zero_count(0)
  {

  }

  std::atomic_int64_t total_log_size;
  std::atomic_int64_t append_count;
  std::atomic_int64_t sync_count;
  std::chrono::steady_clock::time_point start;
  bool calculate;
  int zero_count;
};

static log_stat _stat;

void log_append_count_inc(uint64_t size) {
  _stat.total_log_size += size;
  _stat.append_count ++;
}

void log_sync_count_inc() {
  _stat.sync_count ++;
}




std::string return_current_time_and_date()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}

std::string log_stat_period() {
    std::stringstream ssm;
    auto end = std::chrono::steady_clock::now();
    int64_t append = _stat.append_count.load(std::memory_order_seq_cst);
    int64_t sync = _stat.sync_count.load(std::memory_order_seq_cst);
    uint64_t total_size = _stat.total_log_size.load(std::memory_order_seq_cst);
    auto start = _stat.start;
    _stat.append_count.store(0, std::memory_order_seq_cst);
    _stat.sync_count.store(0, std::memory_order_seq_cst);
    _stat.total_log_size.store(0, std::memory_order_seq_cst);
    _stat.start = end;
    std::chrono::duration<double> duration = end - start;
    if (append != 0) {
        if (!_stat.calculate) {
            _stat.zero_count = 0;
            _stat.calculate = true;
            ssm << return_current_time_and_date() << " I/O statistic, begin calculate" << std::endl;
        }
    } else {
        _stat.zero_count ++;
        if (_stat.zero_count > 3 && _stat.calculate) {
            ssm << return_current_time_and_date() << " I/O statistic, end calculate" << std::endl;
            _stat.calculate = false;
        }
    }
    if (_stat.calculate) {
        double avg_size = 0;
        if (append != 0) {
            avg_size = (double)total_size / (double)append;
        }
        if (duration.count() > 0.000001) {
          ssm <<
          "total write: " << total_size << " bytes, "
          "append count: " << append << ", "
          "avg log size: " << avg_size << " bytes, "
          "append/s: " << append / duration.count() << ", "
          "sync/s: " << sync / duration.count();
        }
    }
    return ssm.str();
}