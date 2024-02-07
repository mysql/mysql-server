/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "portlib/NdbProcess.hpp"
#include "portlib/NdbSleep.h"
#include "portlib/NdbTick.h"
#include "portlib/ndb_socket.h"

#include "util/BaseString.hpp"

#include "unittest/mytap/tap.h"

/* The unresponsive child process sleeps for this long and then exits */
static constexpr int SleeperProcessTimeMsec = 5000;

int run_parent(void);

const char *argv0 = nullptr;

/* Child process reads and writes to pipes as expected, then exits */
int run_child_responsive() {
  /* Read "hello." */
  char buff[32];
  int r = fread(buff, 6, 1, stdin);

  /* Write "goodbye." */
  if (r == 1 && strncmp("hello.", buff, 6) == 0)
    fprintf(stdout, "goodbye.");
  else
    perror("fread()");

  return 0;
}

/* Child process sleeps for some time, then exits with code 93 */
int run_child_sleeper() {
  NdbSleep_MilliSleep(SleeperProcessTimeMsec);
  return 93;
}

/* main()
   Depending on arguments, run a child process, or set argv0 and run parent
*/
int main(int argc, const char *argv[]) {
  if (argc > 1) {
    if (strcmp(argv[1], "responsive") == 0) return run_child_responsive();
    if (strcmp(argv[1], "sleeper") == 0) return run_child_sleeper();
    BAIL_OUT("Unrecognized option: %s", argv[1]);
  }
  argv0 = argv[0];
  return run_parent();
}

int pollReadable(NdbProcess::pipe_handle_t fd, int timeout) {
#ifdef _WIN32
  switch (WaitForMultipleObjects(1, &fd, FALSE, timeout)) {
    case WAIT_OBJECT_0:
      return 1;
    case WAIT_TIMEOUT:
      return 0;
    default:
      return -1;
  }
#else
  ndb_socket_t s = ndb_socket_create_from_native(fd);
  return ndb_poll(s, true, false, timeout);
#endif
}

bool read_response(NdbProcess::Pipes &pipes, FILE *rfp) {
  char response[32];
  response[0] = 'F';
  bool match = false;

  /* Wait 250ms for socket to become readable, then read response */
  if (pollReadable(pipes.parentRead(), 250) == 1) {
    if (fread(response, 8, 1, rfp) == 1) {
      response[8] = '\0';
      match = !strcmp(response, "goodbye.");
    }
  }
  return match;
}

std::unique_ptr<NdbProcess> create_peer(const char *argv1,
                                        NdbProcess::Pipes &pipes) {
  BaseString cmd;
  NdbProcess::Args args;

  cmd.assign(argv0);
  args.add(argv1);

  assert(pipes.connected());
  std::unique_ptr<NdbProcess> p =
      NdbProcess::create("TestPeer", cmd, nullptr, args, &pipes);
  ok((p != nullptr), "created process");
  return p;
}

class Test {
  const char *child_argv1;
  int waitTime1{0}, waitTime2{0}, expectExitCode{0};
  bool expectResponse{false};

 public:
  Test(const char *a) : child_argv1(a) {}
  Test &setResponse(bool b) {
    expectResponse = b;
    return *this;
  }
  Test &setWait1(int t) {
    waitTime1 = t;
    return *this;
  }
  Test &setWait2(int t) {
    waitTime2 = t;
    return *this;
  }
  Test &setExitCode(int i) {
    expectExitCode = i;
    return *this;
  }
  void run();
};

const char *trueFalse(bool r) { return r ? "true" : "false"; }

void Test::run() {
  NdbProcess::Pipes pipes;
  ok(pipes.connected(), "created pipes");
  std::unique_ptr<NdbProcess> proc = create_peer(child_argv1, pipes);
  bool stopped = false;
  int actualExitCode = -100;

  FILE *wfp = pipes.open(pipes.parentWrite(), "w");
  FILE *rfp = pipes.open(pipes.parentRead(), "r");

  fprintf(wfp, "hello.");
  fclose(wfp);

  bool r = read_response(pipes, rfp);
  ok(r == expectResponse, "read_response => %s", trueFalse(expectResponse));
  fclose(rfp);

  bool expectWait1, expectWait2;
  if (expectResponse) {
    expectWait1 = true;
    expectWait2 = false;
    assert(waitTime2 == 0);
  } else {
    expectWait1 = (waitTime1 >= SleeperProcessTimeMsec);
    expectWait2 = ((waitTime1 + waitTime2) >= SleeperProcessTimeMsec);
  }

  /* Timing is unreliable, so the first wait might succeed */
  if (waitTime1) {
    stopped = proc->wait(actualExitCode, waitTime1);
    ok(stopped || !expectWait1, "wait1() (%s)", trueFalse(stopped));
  }

  if (waitTime2 && !stopped) {
    stopped = proc->wait(actualExitCode, waitTime2);
    ok(stopped == expectWait2, "wait2() => %s", trueFalse(expectWait2));
  }

  /* Kill it if necessary */
  if (stopped) {
    ok(actualExitCode == expectExitCode, "exit code %d == %d", actualExitCode,
       expectExitCode);
  } else {
    r = proc->stop();
    ok(r, "force kill process");
    ok(proc->wait(actualExitCode, 500), "wait() after kill");
  }
}

int run_parent() {
  NdbTick_Init();

  puts("Test 1: response arrives and wait() succeeds");
  Test("responsive").setResponse(true).setWait1(500).run();
  puts("");

  puts("Test 2: no response; wait() may fail; stop() succeeds");
  Test("sleeper").setResponse(false).setWait1(1000).setExitCode(93).run();
  puts("");

  puts("Test 3: no response; first wait() may fail; second wait() succeeds");
  Test("sleeper")
      .setResponse(false)
      .setWait1(1500)
      .setWait2(4000)
      .setExitCode(93)
      .run();
  puts("");

  return exit_status();
}
