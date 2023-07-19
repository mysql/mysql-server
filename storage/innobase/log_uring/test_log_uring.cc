
#define USE_IO_URING
#include <stdlib.h>
#include <thread>
#include "log_uring/ptr.hpp"
#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"

const int NUM_WORKER_THREADS = 100;
const size_t BUFFER_SIZE = 512;
const size_t NUM_APPEND_LOGS = 1000000;
class log_thread_handler {
public:
    void operator()() {
        log_iouring(NULL);
    }
};

class worker_thread_handler {
public:
  worker_thread_handler(xlog* log): log_(log) {

  }
  
  void operator()() {
    for (size_t i = 0; i < 1000000; i++) {
      uint64_t lsn = log_->append(buffer_, sizeof(buffer_));
      if (i % 10 == 9) {
        log_->sync(lsn);
      }
    }
  }
private:
  xlog* log_;
  uint8_t buffer_[BUFFER_SIZE];
};

ptr<std::thread> create_log_thread() {
  ptr<std::thread> thd(new std::thread(log_thread_handler()));
  return thd;
}


ptr<std::thread> create_worker_thread(xlog*log) {
  ptr<std::thread> thd(new std::thread(worker_thread_handler(log)));
  return thd;
}

int main() {
  std::vector<ptr<std::thread>> threads;
  ptr<std::thread> t = create_log_thread();
  threads.push_back(t);
  xlog *log = get_xlog();
  for (int i = 0; i < NUM_WORKER_THREADS; i++) {
    ptr<std::thread> thd = create_worker_thread(log);
    threads.push_back(thd);
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i]->join();
  }
}