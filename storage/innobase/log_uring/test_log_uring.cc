
#define USE_IO_URING
#include "log_uring/ptr.hpp"
#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include <stdlib.h>
#include <thread>
#include <iostream>
#include <boost/program_options.hpp>


namespace po = boost::program_options;

const int NUM_WORKER_THREADS = 1;
const size_t BUFFER_SIZE = 51200;
const size_t NUM_APPEND_LOGS = 0;

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
    log_->wait_start();
    for (size_t i = 0; i < NUM_APPEND_LOGS || NUM_APPEND_LOGS == 0; i++) {
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

int main(int argc, const char*argv[]) {
  int num_log_files = 32;
  int num_uring_sqes = 32000;
  int num_worker_threads = NUM_WORKER_THREADS;

  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("num_log_files", po::value<int>(), "number of log files")
      ("num_uring_sqes", po::value<int>(), "number of iouring SQEs")
      ("num_worker_threads", po::value<int>(), "number of worker threads issue append log request")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);    

  if (vm.count("help")) {
      std::cout << desc << "\n";
      return 1;
  }

  if (vm.count("num_log_files")) {
      num_log_files = vm["num_log_files"].as<int>();
  } 

  if (vm.count("num_uring_sqes")) {
      num_log_files = vm["num_uring_sqes"].as<int>();
  }

  if (vm.count("num_worker_threads")) {
      num_worker_threads = vm["num_worker_threads"].as<int>();
  }

  log_iouring_create(num_log_files, num_uring_sqes);

  std::vector<ptr<std::thread>> threads;
  ptr<std::thread> t = create_log_thread();
  threads.push_back(t);
  xlog *log = get_xlog();
  for (int i = 0; i < num_worker_threads; i++) {
    ptr<std::thread> thd = create_worker_thread(log);
    threads.push_back(thd);
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i]->join();
  }
}