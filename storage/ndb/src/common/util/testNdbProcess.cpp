/*
   Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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
#include "portlib/ndb_socket_poller.h"

#include "util/BaseString.hpp"

#include "unittest/mytap/tap.h"

int run_child_1(void);
int run_child_2(void);
int run_parent(const char * argv0);

int main(int argc, const char * argv[]) {
  if(argc > 1) {
    if(strcmp(argv[1], "child1") == 0)
      return run_child_1();
    if(strcmp(argv[1], "child2") == 0)
      return run_child_2();
    BAIL_OUT("Unrecognized option: %s", argv[1]);
  }
  return run_parent(argv[0]);
}

int pollReadable(NdbProcess::pipe_handle_t fd, int timeout)
{
#ifdef _WIN32
  switch(WaitForMultipleObjects(1, &fd, FALSE, timeout))
  {
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

void read_response(NdbProcess::Pipes & pipes, FILE * rfp) {
  char response[32];
  response[0] = 'F';

  /* Poll; 20 ms */
  int r = pollReadable(pipes.parentRead(), 20);
  ok(r == 1, "poll readable");

  /* Read response */
  if(r) {
    puts("Reading response.");
    r = fread(response, 8, 1, rfp);
    ok(r > 0, "got response %d", r);
    response[8] = '\0';
    ok(strcmp(response, "goodbye.") == 0, "got expected response");
  }
}

NdbProcess * create_peer(const char * argv0, const char * argv1,
                         NdbProcess::Pipes & pipes) {
  BaseString cmd;
  NdbProcess::Args args;

  cmd.assign(argv0);
  args.add(argv1);

  ok(pipes.connected(), "pipes connected");
  NdbProcess * p = NdbProcess::create("TestPeer", cmd, nullptr, args, &pipes);
  ok((p != nullptr), "created process");
  return p;
}

int test(const char * argv0, const char * argv1, bool expectResponse) {
  NdbProcess::Pipes pipes;
  NdbProcess * proc = create_peer(argv0, argv1, pipes);
  int r1 = -100;

  FILE *wfp = pipes.open(pipes.parentWrite(), "w");
  FILE *rfp = pipes.open(pipes.parentRead(), "r");

  ok((wfp != nullptr), "Parent write pipe");
  ok((rfp != nullptr), "Parent read pipe");

  fprintf(wfp, "hello.");
  fclose(wfp);

  if(expectResponse)
    read_response(pipes, rfp);

  bool r = proc->wait(r1, 100);

  /* Kill it */
  if(!r) ok(proc->stop(), "proc->stop()");

  delete proc;
  return (r == expectResponse);
}

int run_parent(const char * argv0) {
  NdbTick_Init();
  bool r;

  r = test(argv0, "child1", true);
  ok(r, "test1: response arrives and wait() succeeds");

  bool t2 = test(argv0, "child2", false);
  ok(t2, "test2: response does not arrive and wait() fails");

  return exit_status();
}

/* Child process reads and writes to pipes as expected, then exits */
int run_child_1() {
  /* Read "hello" */
  char buff[32];
  int r = fread(buff, 32, 1, stdin);

  /* Write "goodbye." */
  if(r >= 0)
    fprintf(stdout, "goodbye.");

  return 0;
}

/* Child process sleeps for 10 seconds, then exits */
int run_child_2() {
  NdbSleep_SecSleep(10);
  return 0;
}
