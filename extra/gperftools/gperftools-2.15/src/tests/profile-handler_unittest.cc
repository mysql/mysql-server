// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2009 Google Inc. All Rights Reserved.
// Author: Nabeel Mian (nabeelmian@google.com)
//         Chris Demetriou (cgd@google.com)
//
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
//
//
// This file contains the unit tests for profile-handler.h interface.

#include "config_for_unittests.h"

#include "profile-handler.h"

#include <atomic>

#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>

#include "base/logging.h"
#include "base/simple_mutex.h"

// Some helpful macros for the test class
#define TEST_F(cls, fn)    void cls :: fn()

// Do we expect the profiler to be enabled?
DEFINE_bool(test_profiler_enabled, true,
            "expect profiler to be enabled during tests");


// In order to cover fix for github issue at:
// https://github.com/gperftools/gperftools/issues/412 we override
// operators new/delete to simulate condition where another thread is
// having malloc lock and making sure that profiler handler can
// unregister callbacks without deadlocking. Thus this
// "infrastructure" below.
namespace {
std::atomic<intptr_t> allocate_count;
std::atomic<intptr_t> free_count;
// We also "frob" this lock down in BusyThread.
Mutex allocate_lock;

void* do_allocate(size_t sz) {
  MutexLock h(&allocate_lock);
  allocate_count++;
  return malloc(sz);
}
void do_free(void* p) {
  MutexLock h(&allocate_lock);
  free_count++;
  free(p);
}
}  // namespace

void* operator new(size_t sz) { return do_allocate(sz); }
void* operator new[](size_t sz) { return do_allocate(sz); }
void operator delete(void* p) { do_free(p); }
void operator delete[](void* p) { do_free(p); }

void operator delete(void* p, size_t sz) { do_free(p); }
void operator delete[](void* p, size_t sz) { do_free(p); }

void* operator new(size_t sz, const std::nothrow_t& nt) { return do_allocate(sz); }
void* operator new[](size_t sz, const std::nothrow_t& nt) { return do_allocate(sz); };

void operator delete(void* p, const std::nothrow_t& nt) { do_free(p); }
void operator delete[](void* p, const std::nothrow_t& nt) { do_free(p); }

namespace {

// TODO(csilvers): error-checking on the pthreads routines
class Thread {
 public:
  Thread() : joinable_(false) { }
  virtual ~Thread() { }
  void SetJoinable(bool value) { joinable_ = value; }
  void Start() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&thread_, &attr, &DoRun, this);
    pthread_attr_destroy(&attr);
  }
  void Join()  {
    assert(joinable_);
    pthread_join(thread_, NULL);
  }
  virtual void Run() = 0;
 private:
  static void* DoRun(void* cls) {
    Thread* self = static_cast<Thread*>(cls);
    if (!self->joinable_) {
      CHECK_EQ(0, pthread_detach(pthread_self()));
    }

    ProfileHandlerRegisterThread();
    self->Run();
    return NULL;
  }
  pthread_t thread_;
  bool joinable_;
};

// Sleep interval in nano secs. ITIMER_PROF goes off only afer the specified CPU
// time is consumed. Under heavy load this process may no get scheduled in a
// timely fashion. Therefore, give enough time (20x of ProfileHandle timer
// interval 10ms (100Hz)) for this process to accumulate enought CPU time to get
// a profile tick.
int kSleepInterval = 200000000;

// Sleep interval in nano secs. To ensure that if the timer has expired it is
// reset.
int kTimerResetInterval = 5000000;

static bool linux_per_thread_timers_mode_ = false;
static int timer_type_ = ITIMER_PROF;

// Delays processing by the specified number of nano seconds. 'delay_ns'
// must be less than the number of nano seconds in a second (1000000000).
void Delay(int delay_ns) {
  static const int kNumNSecInSecond = 1000000000;
  EXPECT_LT(delay_ns, kNumNSecInSecond);
  struct timespec delay = { 0, delay_ns };
  nanosleep(&delay, 0);
}

// Checks whether the profile timer is enabled for the current thread.
bool IsTimerEnabled() {
  itimerval current_timer;
  EXPECT_EQ(0, getitimer(timer_type_, &current_timer));
  return (current_timer.it_interval.tv_sec != 0 ||
          current_timer.it_interval.tv_usec != 0);
}

// Dummy worker thread to accumulate cpu time.
class BusyThread : public Thread {
 public:
  BusyThread() : stop_work_(false) {
  }

  // Setter/Getters
  bool stop_work() {
    MutexLock lock(&mu_);
    return stop_work_;
  }
  void set_stop_work(bool stop_work) {
    MutexLock lock(&mu_);
    stop_work_ = stop_work;
  }

 private:
  // Protects stop_work_ below.
  Mutex mu_;
  // Whether to stop work?
  bool stop_work_;

  // Do work until asked to stop. We also stump on allocate_lock to
  // verify that perf handler re/unre-gistration doesn't deadlock with
  // malloc locks.
  void Run() {
    for (;;) {
      MutexLock h(&allocate_lock);
      for (int i = 1000; i > 0; i--) {
        if (stop_work()) {
          return;
        }
        (void)*(const_cast<volatile bool*>(&stop_work_));
      }
    }
  }
};

class NullThread : public Thread {
 private:
  void Run() {
  }
};

// Signal handler which tracks the profile timer ticks.
static void TickCounter(int sig, siginfo_t* sig_info, void *vuc,
                        void* tick_counter) {
  int* counter = static_cast<int*>(tick_counter);
  ++(*counter);
}

// This class tests the profile-handler.h interface.
class ProfileHandlerTest {
 protected:

  // Determines the timer type.
  static void SetUpTestCase() {
    timer_type_ = (getenv("CPUPROFILE_REALTIME") ? ITIMER_REAL : ITIMER_PROF);

#if HAVE_LINUX_SIGEV_THREAD_ID
    linux_per_thread_timers_mode_ = (getenv("CPUPROFILE_PER_THREAD_TIMERS") != NULL);
    const char *signal_number = getenv("CPUPROFILE_TIMER_SIGNAL");
    if (signal_number) {
      //signal_number_ = strtol(signal_number, NULL, 0);
      linux_per_thread_timers_mode_ = true;
      Delay(kTimerResetInterval);
    }
#endif
  }

  // Sets up the profile timers and SIGPROF/SIGALRM handler in a known state.
  // It does the following:
  // 1. Unregisters all the callbacks, stops the timer and clears out
  //    timer_sharing state in the ProfileHandler. This clears out any state
  //    left behind by the previous test or during module initialization when
  //    the test program was started.
  // 3. Starts a busy worker thread to accumulate CPU usage.
  virtual void SetUp() {
    // Reset the state of ProfileHandler between each test. This unregisters
    // all callbacks and stops the timer.
    ProfileHandlerReset();
    EXPECT_EQ(0, GetCallbackCount());
    VerifyDisabled();
    // Start worker to accumulate cpu usage.
    StartWorker();
  }

  virtual void TearDown() {
    ProfileHandlerReset();
    // Stops the worker thread.
    StopWorker();
  }

  // Starts a busy worker thread to accumulate cpu time. There should be only
  // one busy worker running. This is required for the case where there are
  // separate timers for each thread.
  void StartWorker() {
    busy_worker_ = new BusyThread();
    busy_worker_->SetJoinable(true);
    busy_worker_->Start();
    // Wait for worker to start up and register with the ProfileHandler.
    // TODO(nabeelmian) This may not work under very heavy load.
    Delay(kSleepInterval);
  }

  // Stops the worker thread.
  void StopWorker() {
    busy_worker_->set_stop_work(true);
    busy_worker_->Join();
    delete busy_worker_;
  }

  // Gets the number of callbacks registered with the ProfileHandler.
  uint32 GetCallbackCount() {
    ProfileHandlerState state;
    ProfileHandlerGetState(&state);
    return state.callback_count;
  }

  // Gets the current ProfileHandler interrupt count.
  uint64 GetInterruptCount() {
    ProfileHandlerState state;
    ProfileHandlerGetState(&state);
    return state.interrupts;
  }

  // Verifies that a callback is correctly registered and receiving
  // profile ticks.
  void VerifyRegistration(const int& tick_counter) {
    // Check the callback count.
    EXPECT_GT(GetCallbackCount(), 0);
    // Check that the profile timer is enabled.
    EXPECT_EQ(FLAGS_test_profiler_enabled, linux_per_thread_timers_mode_ || IsTimerEnabled());
    uint64 interrupts_before = GetInterruptCount();
    // Sleep for a bit and check that tick counter is making progress.
    int old_tick_count = tick_counter;
    Delay(kSleepInterval);
    int new_tick_count = tick_counter;
    uint64 interrupts_after = GetInterruptCount();
    if (FLAGS_test_profiler_enabled) {
      EXPECT_GT(new_tick_count, old_tick_count);
      EXPECT_GT(interrupts_after, interrupts_before);
    } else {
      EXPECT_EQ(new_tick_count, old_tick_count);
      EXPECT_EQ(interrupts_after, interrupts_before);
    }
  }

  // Verifies that a callback is not receiving profile ticks.
  void VerifyUnregistration(const int& tick_counter) {
    // Sleep for a bit and check that tick counter is not making progress.
    int old_tick_count = tick_counter;
    Delay(kSleepInterval);
    int new_tick_count = tick_counter;
    EXPECT_EQ(old_tick_count, new_tick_count);
    // If no callbacks, timer should be disabled.
    if (GetCallbackCount() == 0) {
      EXPECT_FALSE(IsTimerEnabled());
    }
  }

  // Verifies that the timer is disabled. Expects the worker to be running.
  void VerifyDisabled() {
    // Check that the callback count is 0.
    EXPECT_EQ(0, GetCallbackCount());
    // Check that the timer is disabled.
    EXPECT_FALSE(IsTimerEnabled());
    // Verify that the ProfileHandler is not accumulating profile ticks.
    uint64 interrupts_before = GetInterruptCount();
    Delay(kSleepInterval);
    uint64 interrupts_after = GetInterruptCount();
    EXPECT_EQ(interrupts_before, interrupts_after);
  }

  // Registers a callback and waits for kTimerResetInterval for timers to get
  // reset.
  ProfileHandlerToken* RegisterCallback(void* callback_arg) {
    ProfileHandlerToken* token = ProfileHandlerRegisterCallback(
        TickCounter, callback_arg);
    Delay(kTimerResetInterval);
    return token;
  }

  // Unregisters a callback and waits for kTimerResetInterval for timers to get
  // reset.
  void UnregisterCallback(ProfileHandlerToken* token) {
    allocate_count.store(0);
    free_count.store(0);
    ProfileHandlerUnregisterCallback(token);
    Delay(kTimerResetInterval);
    CHECK(free_count.load() > 0);
  }

  // Busy worker thread to accumulate cpu usage.
  BusyThread* busy_worker_;

 private:
  // The tests to run
  void RegisterUnregisterCallback();
  void MultipleCallbacks();
  void Reset();
  void RegisterCallbackBeforeThread();

 public:
#define RUN(test)  do {                         \
    printf("Running %s\n", #test);              \
    ProfileHandlerTest pht;                     \
    pht.SetUp();                                \
    pht.test();                                 \
    pht.TearDown();                             \
} while (0)

  static int RUN_ALL_TESTS() {
    SetUpTestCase();
    RUN(RegisterUnregisterCallback);
    RUN(MultipleCallbacks);
    RUN(Reset);
    RUN(RegisterCallbackBeforeThread);
    printf("Done\n");
    return 0;
  }
};

// Verifies ProfileHandlerRegisterCallback and
// ProfileHandlerUnregisterCallback.
TEST_F(ProfileHandlerTest, RegisterUnregisterCallback) {
  int tick_count = 0;
  ProfileHandlerToken* token = RegisterCallback(&tick_count);
  VerifyRegistration(tick_count);
  UnregisterCallback(token);
  VerifyUnregistration(tick_count);
}

// Verifies that multiple callbacks can be registered.
TEST_F(ProfileHandlerTest, MultipleCallbacks) {
  // Register first callback.
  int first_tick_count = 0;
  ProfileHandlerToken* token1 = RegisterCallback(&first_tick_count);
  // Check that callback was registered correctly.
  VerifyRegistration(first_tick_count);
  EXPECT_EQ(1, GetCallbackCount());

  // Register second callback.
  int second_tick_count = 0;
  ProfileHandlerToken* token2 = RegisterCallback(&second_tick_count);
  // Check that callback was registered correctly.
  VerifyRegistration(second_tick_count);
  EXPECT_EQ(2, GetCallbackCount());

  // Unregister first callback.
  UnregisterCallback(token1);
  VerifyUnregistration(first_tick_count);
  EXPECT_EQ(1, GetCallbackCount());
  // Verify that second callback is still registered.
  VerifyRegistration(second_tick_count);

  // Unregister second callback.
  UnregisterCallback(token2);
  VerifyUnregistration(second_tick_count);
  EXPECT_EQ(0, GetCallbackCount());

  // Verify that the timers is correctly disabled.
  if (!linux_per_thread_timers_mode_) VerifyDisabled();
}

// Verifies ProfileHandlerReset
TEST_F(ProfileHandlerTest, Reset) {
  // Verify that the profile timer interrupt is disabled.
  if (!linux_per_thread_timers_mode_) VerifyDisabled();
  int first_tick_count = 0;
  RegisterCallback(&first_tick_count);
  VerifyRegistration(first_tick_count);
  EXPECT_EQ(1, GetCallbackCount());

  // Register second callback.
  int second_tick_count = 0;
  RegisterCallback(&second_tick_count);
  VerifyRegistration(second_tick_count);
  EXPECT_EQ(2, GetCallbackCount());

  // Reset the profile handler and verify that callback were correctly
  // unregistered and the timer is disabled.
  ProfileHandlerReset();
  VerifyUnregistration(first_tick_count);
  VerifyUnregistration(second_tick_count);
  if (!linux_per_thread_timers_mode_) VerifyDisabled();
}

// Verifies that ProfileHandler correctly handles a case where a callback was
// registered before the second thread started.
TEST_F(ProfileHandlerTest, RegisterCallbackBeforeThread) {
  // Stop the worker.
  StopWorker();
  // Unregister all existing callbacks and stop the timer.
  ProfileHandlerReset();
  EXPECT_EQ(0, GetCallbackCount());
  VerifyDisabled();

  // Start the worker.
  StartWorker();
  // Register a callback and check that profile ticks are being delivered and
  // the timer is enabled.
  int tick_count = 0;
  RegisterCallback(&tick_count);
  EXPECT_EQ(1, GetCallbackCount());
  VerifyRegistration(tick_count);
  EXPECT_EQ(FLAGS_test_profiler_enabled, linux_per_thread_timers_mode_ || IsTimerEnabled());
}

}  // namespace

int main(int argc, char** argv) {
  return ProfileHandlerTest::RUN_ALL_TESTS();
}
