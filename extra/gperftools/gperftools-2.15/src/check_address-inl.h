// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2023, gperftools Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This is internal implementation details of
// stacktrace_generic_fp-inl.h module. We only split this into
// separate header to enable unit test coverage.

// This is only used on OS-es with mmap support.
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#if HAVE_SYS_SYSCALL_H && !__APPLE__
#include <sys/syscall.h>
#endif

namespace {

#if defined(__linux__) && !defined(FORCE_PIPES)
#define CHECK_ADDRESS_USES_SIGPROCMASK

// Linux kernel ABI for sigprocmask requires us to pass exact sizeof
// for kernel's sigset_t. Which is 64-bit for most arches, with only
// notable exception of mips.
#if defined(__mips__)
static constexpr int kKernelSigSetSize = 16;
#else
static constexpr int kKernelSigSetSize = 8;
#endif

// For Linux we have two strategies. One is calling sigprocmask with
// bogus HOW argument and 'new' sigset arg our address. Kernel ends up
// reading new sigset before interpreting how. So then we either get
// EFAULT when addr is unreadable, or we get EINVAL for readable addr,
// but bogus HOW argument.
//
// We 'steal' this idea from abseil. But nothing guarantees this exact
// behavior of Linux. So to be future-compatible (some our binaries
// will run tens of years from the time they're compiled), we also
// have second more robust method.
bool CheckAccessSingleSyscall(uintptr_t addr, int pagesize) {
  addr &= ~uintptr_t{15};

  if (addr == 0) {
    return false;
  }

  int rv = syscall(SYS_rt_sigprocmask, ~0, addr, uintptr_t{0}, kKernelSigSetSize);
  RAW_CHECK(rv < 0, "sigprocmask(~0, addr, ...)");

  return (errno != EFAULT);
}

// This is second strategy. Idea is more or less same as before, but
// we use SIG_BLOCK for HOW argument. Then if this succeeds (with side
// effect of blocking random set of signals), we simply restore
// previous signal mask.
bool CheckAccessTwoSyscalls(uintptr_t addr, int pagesize) {
  addr &= ~uintptr_t{15};

  if (addr == 0) {
    return false;
  }

  uintptr_t old[(kKernelSigSetSize + sizeof(uintptr_t) - 1) / sizeof(uintptr_t)];
  int rv = syscall(SYS_rt_sigprocmask, SIG_BLOCK, addr, old, kKernelSigSetSize);
  if (rv  == 0) {
    syscall(SYS_rt_sigprocmask, SIG_SETMASK, old, nullptr, kKernelSigSetSize);
    return true;
  }
  return false;
}

bool CheckAddressFirstCall(uintptr_t addr, int pagesize);

bool (* volatile CheckAddress)(uintptr_t addr, int pagesize) = CheckAddressFirstCall;

// And we choose between strategies by checking at runtime if
// single-syscall approach actually works and switch to a proper
// version.
bool CheckAddressFirstCall(uintptr_t addr, int pagesize) {
  void* unreadable = mmap(0, pagesize, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  RAW_CHECK(unreadable != MAP_FAILED, "mmap of unreadable");

  if (!CheckAccessSingleSyscall(reinterpret_cast<uintptr_t>(unreadable), pagesize)) {
    CheckAddress = CheckAccessSingleSyscall;
  } else {
    CheckAddress = CheckAccessTwoSyscalls;
  }

  // Sanity check that our unreadable address is unreadable and that
  // our readable address (our own fn pointer variable) is readable.
  RAW_CHECK(CheckAddress(reinterpret_cast<uintptr_t>(CheckAddress),
                         pagesize),
            "sanity check for readable addr");
  RAW_CHECK(!CheckAddress(reinterpret_cast<uintptr_t>(unreadable),
                          pagesize),
            "sanity check for unreadable addr");

  (void)munmap(unreadable, pagesize);

  return CheckAddress(addr, pagesize);
};

#else

#if HAVE_SYS_SYSCALL_H && !__APPLE__
static int raw_read(int fd, void* buf, size_t count) {
  return syscall(SYS_read, fd, buf, count);
}
static int raw_write(int fd, void* buf, size_t count) {
  return syscall(SYS_write, fd, buf, count);
}
#else
#define raw_read read
#define raw_write write
#endif

bool CheckAddress(uintptr_t addr, int pagesize) {
  static tcmalloc::TrivialOnce once;
  static int fds[2];

  once.RunOnce([] () {
    RAW_CHECK(pipe(fds) == 0, "pipe(fds)");

    auto add_flag = [] (int fd, int get, int set, int the_flag) {
      int flags = fcntl(fd, get, 0);
      RAW_CHECK(flags >= 0, "fcntl get");
      flags |= the_flag;
      RAW_CHECK(fcntl(fd, set, flags) == 0, "fcntl set");
    };

    for (int i = 0; i < 2; i++) {
      add_flag(fds[i], F_GETFD, F_SETFD, FD_CLOEXEC);
      add_flag(fds[i], F_GETFL, F_SETFL, O_NONBLOCK);
    }
  });

  do {
    int rv = raw_write(fds[1], reinterpret_cast<void*>(addr), 1);
    RAW_CHECK(rv != 0, "raw_write(...) == 0");
    if (rv > 0) {
      return true;
    }
    if (errno == EFAULT) {
      return false;
    }

    RAW_CHECK(errno == EAGAIN, "write errno must be EAGAIN");

    char drainbuf[256];
    do {
      rv = raw_read(fds[0], drainbuf, sizeof(drainbuf));
      if (rv < 0 && errno != EINTR) {
        RAW_CHECK(errno == EAGAIN, "read errno must be EAGAIN");
        break;
      }
      // read succeeded or we got EINTR
    } while (true);
  } while (true);

  return false;
}

#endif

}  // namespace
