
#define USE_IO_URING
#include "log_uring/ptr.hpp"
#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/define.h"
#include <stdlib.h>
#include <thread>
#include <iostream>
#include <boost/program_options.hpp>


namespace po = boost::program_options;



class log_thread_handler {
public:
    void operator()() {
        log_uring(NULL);
    }
};

class worker_thread_handler {
public:
  worker_thread_handler(
    xlog* log, 
    int log_size,
    int num_log_entries_sync
    ): 
    log_(log),
    num_log_entries_sync_(num_log_entries_sync)
  {
    buffer_.resize(log_size, 0);
  }
  
  void operator()() {
    log_->wait_start();
    
    for (size_t i = 0; i < NUM_APPEND_LOGS || NUM_APPEND_LOGS == 0; i++) {
      
      uint64_t lsn = log_->append(buffer_.data(), buffer_.size());
      if (i % (size_t)num_log_entries_sync_ == (size_t)num_log_entries_sync_ - 1) {
        log_->sync(lsn);
      }
    }
  }
private:
  xlog* log_;
  int num_log_entries_sync_;
  std::vector<uint8_t> buffer_;
};

ptr<std::thread> create_log_thread() {
  ptr<std::thread> thd(new std::thread(log_thread_handler()));
  return thd;
}


ptr<std::thread> create_worker_thread(xlog*log, int log_size, int num_log_entries_sync) {
  ptr<std::thread> thd(new std::thread(
    worker_thread_handler(
      log, 
      log_size,
      num_log_entries_sync
      )));
  return thd;
}

int main(int argc, const char*argv[]) {
  int num_log_files = NUM_LOG_FILES;
  int num_uring_sqes = NUM_URING_SQES;
  int num_worker_threads = NUM_WORKER_THREADS;
  int log_size = LOG_SIZE;
  bool use_iouring = USE_URING;
  int num_log_entries_sync = NUM_LOG_ENTRIES_SYNC;
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("num_log_files,l", po::value<int>(), "number of log files")
      ("num_uring_sqes,s", po::value<int>(), "number of iouring SQEs")
      ("num_worker_threads,t", po::value<int>(), "number of worker threads issue log request")
      ("log_size,g", po::value<int>(), "average log size in bytes")
      ("use_iouring,u", po::value<bool>(), "use io_uring")
      ("num_log_entries_sync,e", po::value<int>(), "number of log entries before invoke sync")
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

  if (vm.count("use_iouring")) {
      use_iouring = vm["use_iouring"].as<bool>();
  }

  if (vm.count("log_size")) {
      log_size = vm["log_size"].as<int>();
  }

  if (vm.count("num_log_entries_sync")) {
    num_log_entries_sync = vm["num_log_entries_sync"].as<int>();
  }

  log_uring_create(num_log_files, num_uring_sqes, use_iouring);

  std::vector<ptr<std::thread>> threads;
  ptr<std::thread> t = create_log_thread();
  threads.push_back(t);
  xlog *log = get_xlog();
  for (int i = 0; i < num_worker_threads; i++) {
    ptr<std::thread> thd = create_worker_thread(
        log, 
        log_size, 
        num_log_entries_sync
        );
    threads.push_back(thd);
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i]->join();
  }
}