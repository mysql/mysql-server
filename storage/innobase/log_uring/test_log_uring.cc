
#define USE_IO_URING
#include "log_uring/ptr.hpp"
#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/ptr.hpp"
#include "log_uring/define.h"
#include <stdlib.h>
#include <thread>
#include <iostream>
#include <boost/program_options.hpp>


namespace po = boost::program_options;




class duration_list {
  std::mutex mutex_;
  std::condition_variable cond_;
  std::vector<xlog_op_duration> list_;
  int thread_;
  std::atomic_bool stopped_;
public:
  duration_list() : thread_(0), stopped_(false) {

  }
  void append(xlog_op_duration duration) {
    std::unique_lock l (mutex_);
    list_.push_back(duration);
  }

  void notify(int thread) {
    std::unique_lock l(mutex_);
    thread_ = thread;
    cond_.notify_all();
  }

  void wait() {
    std::unique_lock l(mutex_);
    cond_.wait(l, [this] {
        return thread_ != 0 && (int) list_.size() == thread_;
      });
  }

  bool is_stopped() {
    return stopped_.load();
  }

  void stop() {
    stopped_.store(true);
  }

  void calculate() {

  }
};

class log_thread_handler {
public:
    void operator()() {
        log_uring(NULL);
    }
};

ptr<duration_list> _list = cs_new<duration_list>();

class worker_thread_handler {
public:
  worker_thread_handler(
    ptr<duration_list> list,
    xlog* log, 
    int log_size,
    int num_log_entries_sync,
    int num_thread
    ): 
    log_(log),
    num_log_entries_sync_(num_log_entries_sync),
    num_thread_(num_thread)
  {
    buffer_.resize(log_size, 0);
  }
  
  void operator()() {
    log_->wait_start();
    
    for (int i = 0; (i < num_log_entries_sync_ || num_log_entries_sync_ == 0) && !list_->is_stopped(); i++) {
      
      uint64_t lsn = log_->append(buffer_.data(), buffer_.size());
      if (i % (size_t)num_log_entries_sync_ == (size_t)num_log_entries_sync_ - 1) {
        log_->sync(lsn);
      }
    }
    xlog_op_duration duration = xlog::op_duration();
    list_->append(duration);

    list_->notify(num_thread_);
  }
private:

  ptr<duration_list> list_;
  xlog* log_;
  int num_log_entries_sync_;
  int num_thread_;
  std::vector<uint8_t> buffer_;
};


class calculate_thread_handler {
public:
  calculate_thread_handler(
    ptr<duration_list> list,
    int wait_seconds
    ):list_(list),
    wait_seconds_(wait_seconds)
  {

  }
  
  void operator()() {
    sleep(wait_seconds_);
    list_->stop();
    list_->wait();

    list_->calculate();
  }
private:
  ptr<duration_list> list_;
  int wait_seconds_;
};

ptr<std::thread> create_log_thread() {
  ptr<std::thread> thd(new std::thread(log_thread_handler()));
  return thd;
}


ptr<std::thread> create_worker_thread(
  xlog*log, 
  int log_size, 
  int num_log_entries_sync,
  int num_thread
  ) {
  ptr<std::thread> thd(new std::thread(
    worker_thread_handler(
      _list,
      log, 
      log_size,
      num_log_entries_sync, 
      num_thread
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
        num_log_entries_sync,
        num_worker_threads
        );
    threads.push_back(thd);
  }
  
  // create calculate thread
  {
    ptr<std::thread> calculate_thread(new std::thread(
      calculate_thread_handler(
        _list,
        30 // 30seconds
        )));
    threads.push_back(calculate_thread);
  }

  for (size_t i = 0; i < threads.size(); i++) {
    threads[i]->join();
  }
}