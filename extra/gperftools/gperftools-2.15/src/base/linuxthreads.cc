// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
/* Copyright (c) 2005-2007, Google Inc.
 * Copyright (c) 2023, gperftools Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ---
 * Author: Markus Gutschke
 *
 * Substantial upgrades by Aliaksey Kandratsenka. All bugs are mine.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "base/linuxthreads.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>

#include "base/basictypes.h"
#include "base/logging.h"

#ifndef CLONE_UNTRACED
#define CLONE_UNTRACED 0x00800000
#endif

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

namespace {

class SetPTracerSetup {
public:
  ~SetPTracerSetup() {
    if (need_cleanup_) {
      prctl(PR_SET_PTRACER, 0, 0, 0, 0);
    }
  }
  void Prepare(int clone_pid) {
    if (prctl(PR_SET_PTRACER, clone_pid, 0, 0, 0) == 0) {
      need_cleanup_ = true;
    }
  }

private:
  bool need_cleanup_ = false;
};

class UniqueFD {
public:
  explicit UniqueFD(int fd) : fd_(fd) {}

  int ReleaseFD() {
    int retval = fd_;
    fd_ = -1;
    return retval;
  }

  ~UniqueFD() {
    if (fd_ < 0) {
      return;
    }
    (void)close(fd_);
  }
private:
  int fd_;
};

template <typename Body>
struct SimpleCleanup {
  const Body body;

  explicit SimpleCleanup(const Body& body) : body(body) {}

  ~SimpleCleanup() {
    body();
  }
};

template <typename Body>
SimpleCleanup<Body> MakeSimpleCleanup(const Body& body) {
  return SimpleCleanup<Body>{body};
};

}  // namespace

/* Synchronous signals that should not be blocked while in the lister thread.
 */
static const int sync_signals[]  = {
  SIGABRT, SIGILL,
  SIGFPE, SIGSEGV, SIGBUS,
#ifdef SIGEMT
  SIGEMT,
#endif
  SIGSYS, SIGTRAP,
  SIGXCPU, SIGXFSZ };

ATTRIBUTE_NOINLINE
static int local_clone (int (*fn)(void *), void *arg) {
#ifdef __PPC64__
  /* To avoid the gap cross page boundaries, increase by the large parge
   * size mostly PowerPC system uses.  */

  // FIXME(alk): I don't really understand why ppc needs this and why
  // 64k pages matter. I.e. some other architectures have 64k pages,
  // so should we do the same there?
  uintptr_t clone_stack_size = 64 << 10;
#else
  uintptr_t clone_stack_size = 4 << 10;
#endif

  bool grows_to_low = (&arg < arg);
  if (grows_to_low) {
    // Negate clone_stack_size if stack grows to lower addresses
    // (common for arch-es that matter).
    clone_stack_size = ~clone_stack_size + 1;
  }

#if defined(__i386__) || defined(__x86_64__) || defined(__riscv) || defined(__arm__) || defined(__aarch64__)
  // Sanity check code above. We know that those arch-es grow stack to
  // lower addresses.
  CHECK(grows_to_low);
#endif

  /* Leave 4kB of gap between the callers stack and the new clone. This
   * should be more than sufficient for the caller to call waitpid() until
   * the cloned thread terminates.
   *
   * It is important that we set the CLONE_UNTRACED flag, because newer
   * versions of "gdb" otherwise attempt to attach to our thread, and will
   * attempt to reap its status codes. This subsequently results in the
   * caller hanging indefinitely in waitpid(), waiting for a change in
   * status that will never happen. By setting the CLONE_UNTRACED flag, we
   * prevent "gdb" from stealing events, but we still expect the thread
   * lister to fail, because it cannot PTRACE_ATTACH to the process that
   * is being debugged. This is OK and the error code will be reported
   * correctly.
   */
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&arg) + clone_stack_size;
  stack_addr &= ~63; // align stack address on 64 bytes (x86 needs 16, but lets be generous)
  return clone(fn, reinterpret_cast<void*>(stack_addr),
               CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_UNTRACED,
               arg, 0, 0, 0);
}


/* Local substitute for the atoi() function, which is not necessarily safe
 * to call once threads are suspended (depending on whether libc looks up
 * locale information,  when executing atoi()).
 */
static int local_atoi(const char *s) {
  int n   = 0;
  int neg = *s == '-';
  if (neg)
    s++;
  while (*s >= '0' && *s <= '9')
    n = 10*n + (*s++ - '0');
  return neg ? -n : n;
}

static int ptrace_detach(pid_t pid) {
  return ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
}

/* Re-runs fn until it doesn't cause EINTR
 */
#define NO_INTR(fn)   do {} while ((fn) < 0 && errno == EINTR)

/* abort() is not safely reentrant, and changes it's behavior each time
 * it is called. This means, if the main application ever called abort()
 * we cannot safely call it again. This would happen if we were called
 * from a SIGABRT signal handler in the main application. So, document
 * that calling SIGABRT from the thread lister makes it not signal safe
 * (and vice-versa).
 * Also, since we share address space with the main application, we
 * cannot call abort() from the callback and expect the main application
 * to behave correctly afterwards. In fact, the only thing we can do, is
 * to terminate the main application with extreme prejudice (aka
 * PTRACE_KILL).
 * We set up our own SIGABRT handler to do this.
 * In order to find the main application from the signal handler, we
 * need to store information about it in global variables. This is
 * safe, because the main application should be suspended at this
 * time. If the callback ever called TCMalloc_ResumeAllProcessThreads(), then
 * we are running a higher risk, though. So, try to avoid calling
 * abort() after calling TCMalloc_ResumeAllProcessThreads.
 */
static volatile int *sig_pids, sig_num_threads;


/* Signal handler to help us recover from dying while we are attached to
 * other threads.
 */
static void SignalHandler(int signum, siginfo_t *si, void *data) {
  RAW_LOG(ERROR, "Got fatal signal %d inside ListerThread", signum);

  if (sig_pids != NULL) {
    if (signum == SIGABRT) {
      prctl(PR_SET_PDEATHSIG, 0);
      while (sig_num_threads-- > 0) {
        /* Not sure if sched_yield is really necessary here, but it does not */
        /* hurt, and it might be necessary for the same reasons that we have */
        /* to do so in ptrace_detach().                                  */
        sched_yield();
        ptrace(PTRACE_KILL, sig_pids[sig_num_threads], 0, 0);
      }
    } else if (sig_num_threads > 0) {
      TCMalloc_ResumeAllProcessThreads(sig_num_threads, (int *)sig_pids);
    }
  }
  sig_pids = NULL;

  syscall(SYS_exit, signum == SIGABRT ? 1 : 2);
}


/* Try to dirty the stack, and hope that the compiler is not smart enough
 * to optimize this function away. Or worse, the compiler could inline the
 * function and permanently allocate the data on the stack.
 */
static void DirtyStack(size_t amount) {
  char buf[amount];
  memset(buf, 0, amount);
  read(-1, buf, amount);
}


/* Data structure for passing arguments to the lister thread.
 */
#define ALT_STACKSIZE (MINSIGSTKSZ + 4096)

struct ListerParams {
  int         result, err;
  pid_t       ppid;
  int         start_pipe_rd;
  int         start_pipe_wr;
  char        *altstack_mem;
  ListAllProcessThreadsCallBack callback;
  void        *parameter;
  va_list     ap;
  int         proc_fd;
};

struct kernel_dirent64 { // see man 2 getdents
  int64_t        d_ino;    /* 64-bit inode number */
  int64_t        d_off;    /* 64-bit offset to next structure */
  unsigned short d_reclen; /* Size of this dirent */
  unsigned char  d_type;   /* File type */
  char           d_name[]; /* Filename (null-terminated) */
};

static const kernel_dirent64 *BumpDirentPtr(const kernel_dirent64 *ptr, uintptr_t by_bytes) {
  return reinterpret_cast<kernel_dirent64*>(reinterpret_cast<uintptr_t>(ptr) + by_bytes);
}

static int ListerThread(struct ListerParams *args) {
  int                found_parent = 0;
  pid_t              clone_pid  = syscall(SYS_gettid);
  int                proc = args->proc_fd, num_threads = 0;
  int                max_threads = 0, sig;
  struct stat        proc_sb;
  stack_t            altstack;

  /* Wait for parent thread to set appropriate permissions to allow
   * ptrace activity. Note we using pipe pair, so which ensures we
   * don't sleep past parent's death.
   */
  (void)close(args->start_pipe_wr);
  {
    char tmp;
    read(args->start_pipe_rd, &tmp, sizeof(tmp));
  }

  // No point in continuing if parent dies before/during ptracing.
  prctl(PR_SET_PDEATHSIG, SIGKILL);

  /* Catch signals on an alternate pre-allocated stack. This way, we can
   * safely execute the signal handler even if we ran out of memory.
   */
  memset(&altstack, 0, sizeof(altstack));
  altstack.ss_sp    = args->altstack_mem;
  altstack.ss_flags = 0;
  altstack.ss_size  = ALT_STACKSIZE;
  sigaltstack(&altstack, nullptr);

  /* Some kernels forget to wake up traced processes, when the
   * tracer dies.  So, intercept synchronous signals and make sure
   * that we wake up our tracees before dying. It is the caller's
   * responsibility to ensure that asynchronous signals do not
   * interfere with this function.
   */
  for (sig = 0; sig < sizeof(sync_signals)/sizeof(*sync_signals); sig++) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = SignalHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags      = SA_ONSTACK|SA_SIGINFO|SA_RESETHAND;
    sigaction(sync_signals[sig], &sa, nullptr);
  }

  /* Read process directories in /proc/...                                   */
  for (;;) {
    if (lseek(proc, 0, SEEK_SET) < 0) {
      goto failure;
    }
    if (fstat(proc, &proc_sb) < 0) {
      goto failure;
    }

    /* Since we are suspending threads, we cannot call any libc
     * functions that might acquire locks. Most notably, we cannot
     * call malloc(). So, we have to allocate memory on the stack,
     * instead. Since we do not know how much memory we need, we
     * make a best guess. And if we guessed incorrectly we retry on
     * a second iteration (by jumping to "detach_threads").
     *
     * Unless the number of threads is increasing very rapidly, we
     * should never need to do so, though, as our guestimate is very
     * conservative.
     */
    if (max_threads < proc_sb.st_nlink + 100) {
      max_threads = proc_sb.st_nlink + 100;
    }

    /* scope */ {
      pid_t pids[max_threads];
      int   added_entries = 0;
      sig_num_threads     = num_threads;
      sig_pids            = pids;
      for (;;) {
        // lets make sure to align buf to store kernel_dirent64-s properly.
        int64_t buf[4096 / sizeof(int64_t)];

        ssize_t nbytes = syscall(SYS_getdents64, proc, buf, sizeof(buf));
        // fprintf(stderr, "nbytes = %zd\n", nbytes);

        if (nbytes < 0) {
          goto failure;
        }

        if (nbytes == 0) {
          if (added_entries) {
            /* Need to keep iterating over "/proc" in multiple
             * passes until we no longer find any more threads. This
             * algorithm eventually completes, when all threads have
             * been suspended.
             */
            added_entries = 0;
            lseek(proc, 0, SEEK_SET);
            continue;
          }
          break;
        }

        const kernel_dirent64 *entry = reinterpret_cast<kernel_dirent64*>(buf);
        const kernel_dirent64 *end = BumpDirentPtr(entry, nbytes);

        for (;entry < end; entry = BumpDirentPtr(entry, entry->d_reclen)) {
          if (entry->d_ino == 0) {
            continue;
          }

          const char *ptr = entry->d_name;
          // fprintf(stderr, "name: %s\n", ptr);
          pid_t pid;

          /* Some kernels hide threads by preceding the pid with a '.'     */
          if (*ptr == '.')
            ptr++;

          /* If the directory is not numeric, it cannot be a
           * process/thread
           */
          if (*ptr < '0' || *ptr > '9')
            continue;
          pid = local_atoi(ptr);
          // fprintf(stderr, "pid = %d (%d)\n", pid, getpid());

          if (!pid || pid == clone_pid) {
            continue;
          }

          /* Attach (and suspend) all threads                              */
          long i, j;

          /* Found one of our threads, make sure it is no duplicate    */
          for (i = 0; i < num_threads; i++) {
            /* Linear search is slow, but should not matter much for
             * the typically small number of threads.
             */
            if (pids[i] == pid) {
              /* Found a duplicate; most likely on second pass         */
              goto next_entry;
            }
          }

          /* Check whether data structure needs growing                */
          if (num_threads >= max_threads) {
            /* Back to square one, this time with more memory          */
            goto detach_threads;
          }

          /* Attaching to thread suspends it                           */
          pids[num_threads++] = pid;
          sig_num_threads     = num_threads;

          if (ptrace(PTRACE_ATTACH, pid, (void *)0,
                     (void *)0) < 0) {
            /* If operation failed, ignore thread. Maybe it
             * just died?  There might also be a race
             * condition with a concurrent core dumper or
             * with a debugger. In that case, we will just
             * make a best effort, rather than failing
             * entirely.
             */
            num_threads--;
            sig_num_threads = num_threads;
            goto next_entry;
          }
          while (waitpid(pid, (int *)0, __WALL) < 0) {
            if (errno != EINTR) {
              ptrace_detach(pid);
              num_threads--;
              sig_num_threads = num_threads;
              goto next_entry;
            }
          }

          if (syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &i, &j) || i++ != j ||
              syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &i, &j) || i   != j) {
            /* Address spaces are distinct. This is probably
             * a forked child process rather than a thread.
             */
            ptrace_detach(pid);
            num_threads--;
            sig_num_threads = num_threads;
            goto next_entry;
          }

          found_parent |= pid == args->ppid;
          added_entries++;

        next_entry:;
        }  // entries iterations loop
      }  // getdents loop

      /* If we never found the parent process, something is very wrong.
       * Most likely, we are running in debugger. Any attempt to operate
       * on the threads would be very incomplete. Let's just report an
       * error to the caller.
       */
      if (!found_parent) {
        TCMalloc_ResumeAllProcessThreads(num_threads, pids);
        return 3;
      }

      /* Now we are ready to call the callback,
       * which takes care of resuming the threads for us.
       */
      args->result = args->callback(args->parameter, num_threads,
                                    pids, args->ap);
      args->err = errno;

      /* Callback should have resumed threads, but better safe than sorry  */
      if (TCMalloc_ResumeAllProcessThreads(num_threads, pids)) {
        /* Callback forgot to resume at least one thread, report error     */
        args->err    = EINVAL;
        args->result = -1;
      }

      return 0;

    detach_threads:
      /* Resume all threads prior to retrying the operation */
      TCMalloc_ResumeAllProcessThreads(num_threads, pids);
      sig_pids = NULL;
      num_threads = 0;
      sig_num_threads = num_threads;
      max_threads += 100;
    }  // pids[max_threads] scope
  } // for (;;)

failure:
  args->result = -1;
  args->err    = errno;
  return 1;
}

/* This function gets the list of all linux threads of the current process
 * passes them to the 'callback' along with the 'parameter' pointer; at the
 * call back call time all the threads are paused via
 * PTRACE_ATTACH.
 * The callback is executed from a separate thread which shares only the
 * address space, the filesystem, and the filehandles with the caller. Most
 * notably, it does not share the same pid and ppid; and if it terminates,
 * the rest of the application is still there. 'callback' is supposed to do
 * or arrange for TCMalloc_ResumeAllProcessThreads. This happens automatically, if
 * the thread raises a synchronous signal (e.g. SIGSEGV); asynchronous
 * signals are blocked. If the 'callback' decides to unblock them, it must
 * ensure that they cannot terminate the application, or that
 * TCMalloc_ResumeAllProcessThreads will get called.
 * It is an error for the 'callback' to make any library calls that could
 * acquire locks. Most notably, this means that most system calls have to
 * avoid going through libc. Also, this means that it is not legal to call
 * exit() or abort().
 * We return -1 on error and the return value of 'callback' on success.
 */
int TCMalloc_ListAllProcessThreads(void *parameter,
                                   ListAllProcessThreadsCallBack callback, ...) {
  char                   altstack_mem[ALT_STACKSIZE];
  struct ListerParams    args;
  pid_t                  clone_pid;
  int                    dumpable = 1;
  int                    need_sigprocmask = 0;
  sigset_t               sig_blocked, sig_old;
  int                    status, rc;

  SetPTracerSetup        ptracer_setup;

  auto cleanup = MakeSimpleCleanup([&] () {
    int old_errno = errno;

    if (need_sigprocmask) {
      sigprocmask(SIG_SETMASK, &sig_old, nullptr);
    }

    if (!dumpable) {
      prctl(PR_SET_DUMPABLE, dumpable);
    }

    errno = old_errno;
  });

  va_start(args.ap, callback);

  /* If we are short on virtual memory, initializing the alternate stack
   * might trigger a SIGSEGV. Let's do this early, before it could get us
   * into more trouble (i.e. before signal handlers try to use the alternate
   * stack, and before we attach to other threads).
   */
  memset(altstack_mem, 0, sizeof(altstack_mem));

  /* Some of our cleanup functions could conceivable use more stack space.
   * Try to touch the stack right now. This could be defeated by the compiler
   * being too smart for it's own good, so try really hard.
   */
  DirtyStack(32768);

  /* Make this process "dumpable". This is necessary in order to ptrace()
   * after having called setuid().
   */
  dumpable = prctl(PR_GET_DUMPABLE, 0);
  if (!dumpable) {
    prctl(PR_SET_DUMPABLE, 1);
  }

  /* Fill in argument block for dumper thread                                */
  args.result       = -1;
  args.err          = 0;
  args.ppid         = getpid();
  args.altstack_mem = altstack_mem;
  args.parameter    = parameter;
  args.callback     = callback;

  NO_INTR(args.proc_fd = open("/proc/self/task/", O_RDONLY|O_DIRECTORY|O_CLOEXEC));
  UniqueFD proc_closer{args.proc_fd};

  if (args.proc_fd < 0) {
    return -1;
  }

  int pipefds[2];
  if (pipe2(pipefds, O_CLOEXEC)) {
    return -1;
  }

  UniqueFD pipe_rd_closer{pipefds[0]};
  UniqueFD pipe_wr_closer{pipefds[1]};

  args.start_pipe_rd = pipefds[0];
  args.start_pipe_wr = pipefds[1];

  /* Before cloning the thread lister, block all asynchronous signals, as we */
  /* are not prepared to handle them.                                        */
  sigfillset(&sig_blocked);
  for (int sig = 0; sig < sizeof(sync_signals)/sizeof(*sync_signals); sig++) {
    sigdelset(&sig_blocked, sync_signals[sig]);
  }
  if (sigprocmask(SIG_BLOCK, &sig_blocked, &sig_old)) {
    return -1;
  }
  need_sigprocmask = 1;

  // make sure all functions used by parent from local_clone to after
  // waitpid have plt entries fully initialized. We cannot afford
  // dynamic linker running relocations and messing with errno (see
  // comment just below)
  (void)prctl(PR_GET_PDEATHSIG, 0);
  (void)close(-1);
  (void)waitpid(INT_MIN, nullptr, 0);

  /* After cloning, both the parent and the child share the same
   * instance of errno. We deal with this by being very
   * careful. Specifically, child immediately calls into sem_wait
   * which never fails (cannot even EINTR), so doesn't touch errno.
   *
   * Parent sets up PR_SET_PTRACER prctl (if it fails, which usually
   * doesn't happen, we ignore that failure). Then parent does close
   * on write side of start pipe. After that child runs complex code,
   * including arbitrary callback. So parent avoids screwing with
   * errno by immediately calling waitpid with async signals disabled.
   *
   * I.e. errno is parent's up until close below. Then errno belongs
   * to child up until it exits.
   */
  clone_pid = local_clone((int (*)(void *))ListerThread, &args);
  if (clone_pid < 0) {
    return -1;
  }

  /* Most Linux kernels in the wild have Yama LSM enabled, so
   * requires us to explicitly give permission for child to ptrace
   * us. See man 2 ptrace for details. This then requires us to
   * synchronize with the child (see close on start pipe
   * below). I.e. so that child doesn't start ptracing before we've
   * completed this prctl call.
   */
  ptracer_setup.Prepare(clone_pid);

  /* Closing write side of pipe works like releasing the lock. It
   * allows the ListerThread to run past read() call on read side of
   * pipe and ptrace us.
   */
  close(pipe_wr_closer.ReleaseFD());

  /* So here child runs (see ListerThread), it finds and ptraces all
   * threads, runs whatever callback is setup and then
   * detaches/resumes everything. In any case we wait for child's
   * completion to gather status and synchronize everything. */

  rc = waitpid(clone_pid, &status, __WALL);

  if (rc < 0) {
    if (errno == EINTR) {
      RAW_LOG(FATAL, "BUG: EINTR from waitpid shouldn't be possible!");
    }
    // Any error waiting for child is sign of some bug, so abort
    // asap. Continuing is unsafe anyways with child potentially writing to our
    // stack.
    RAW_LOG(FATAL, "BUG: waitpid inside TCMalloc_ListAllProcessThreads cannot fail, but it did. Raw errno: %d\n", errno);
  } else if (WIFEXITED(status)) {
    errno = args.err;
    switch (WEXITSTATUS(status)) {
    case 0: break;             /* Normal process termination           */
    case 2: args.err = EFAULT; /* Some fault (e.g. SIGSEGV) detected   */
      args.result = -1;
      break;
    case 3: args.err = EPERM;  /* Process is already being traced      */
      args.result = -1;
      break;
    default:args.err = ECHILD; /* Child died unexpectedly              */
      args.result = -1;
      break;
    }
  } else if (!WIFEXITED(status)) {
    args.err    = EFAULT;        /* Terminated due to an unhandled signal*/
    args.result = -1;
  }

  errno = args.err;
  return args.result;
}

/* This function resumes the list of all linux threads that
 * TCMalloc_ListAllProcessThreads pauses before giving to its callback.
 * The function returns non-zero if at least one thread was
 * suspended and has now been resumed.
 */
int TCMalloc_ResumeAllProcessThreads(int num_threads, pid_t *thread_pids) {
  int detached_at_least_one = 0;
  while (num_threads-- > 0) {
    detached_at_least_one |= (ptrace_detach(thread_pids[num_threads]) >= 0);
  }
  return detached_at_least_one;
}
