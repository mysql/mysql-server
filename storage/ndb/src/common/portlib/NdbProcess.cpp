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

#ifdef TEST_NDBPROCESS

#include "portlib/NdbProcess.hpp"
#include <stdlib.h>
#include <string.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include "unittest/mytap/tap.h"

using std::fprintf;
using std::strcmp;
using std::strlen;
using std::filesystem::canonical;
using std::filesystem::path;

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

bool check_call_output(const NdbProcess::Args &args, FILE *rfile) {
  size_t max_length = 0;
  for (unsigned i = 0; i < args.args().size(); i++)
    if (max_length < args.args()[i].length())
      max_length = args.args()[i].length();
  // Allow longer arguments to detect some failure
  max_length = max_length + 1 + 2;

  auto arg_buf = std::make_unique<char[]>(max_length);

  int argi = 0;
  int argc = args.args().size();
  for (; argi < argc; argi++) {
    int len = -1;
    char buf[19 + 1 + 1];
    if (fgets(buf, sizeof(buf), rfile) == nullptr) {
      fprintf(stderr, "ERROR: %s: %u: expected length got EOF\n", __func__,
              __LINE__);
      return false;
    }
    long llen = -1;
    char *endp = nullptr;
    llen = strtol(buf, &endp, 10);
    if (endp == buf) {
      fprintf(stderr, "ERROR: %s: %u: expected length got nothing\n", __func__,
              __LINE__);
      return false;
    }
    if (llen < INT_MIN || llen > INT_MAX) {
      fprintf(stderr, "ERROR: %s: %u: invalid length %ld\n", __func__, __LINE__,
              llen);
      return false;
    }
    if (*endp != '\n') {
      if (*endp != '\r' || endp[1] != '\n') {
        fprintf(stderr,
                "ERROR: %s: %u: expected newline after length got %d %d\n",
                __func__, __LINE__, endp[0], endp[1]);
        return false;
      }
      endp++;
    }
    if (endp + 1 != buf + strlen(buf)) {
      fprintf(stderr,
              "ERROR: %s: %u: expected newline after length got more %d\n",
              __func__, __LINE__, endp[1]);
      return false;
    }
    len = int(llen);
    if (len < 0) {
      fprintf(stderr, "ERROR: %s: %u: Bad argument length %d.\n", __func__,
              __LINE__, len);
      return false;
    }
    if (size_t(len + 1) > max_length) {
      fprintf(stderr, "ERROR: %s: %u: Bad argument length %d.\n", __func__,
              __LINE__, len);
      return false;
    }
    size_t n = fread(arg_buf.get(), 1, len, rfile);
    if (n != size_t(len)) {
      fprintf(stderr, "ERROR: %s: %u: Got partial argument (%zu of %d) %*s.\n",
              __func__, __LINE__, n, len, int(n), arg_buf.get());
      return false;
    }
    arg_buf[n] = 0;
    int ch = fgetc(rfile);
    if (ch != '\n') {
      if (ch == '\r') {
        // Also allow CR+NL as newline.
        ch = fgetc(rfile);
      }
      if (ch != '\n') {
        fprintf(stderr, "ERROR: %s: %u: Expected <newline> got char(%d).\n",
                __func__, __LINE__, ch);
        return false;
      }
    }
    if (strcmp(arg_buf.get(), args.args()[argi].c_str()) != 0) {
      fprintf(stderr, "ERROR: %s: %u: GOT: %s: EXPECTED: %s.\n", __func__,
              __LINE__, arg_buf.get(), args.args()[argi].c_str());
      return false;
    }
  }

  if (argi != argc) {
    fprintf(stderr, "ERROR: %s: %u: to few arg: got %d expected %d\n", __func__,
            __LINE__, argi, argc);
    return false;
  }
  if (fgets(arg_buf.get(), max_length, rfile) != nullptr) {
    fprintf(stderr, "ERROR: %s: %u: to many arg\n", __func__, __LINE__);
    return false;
  }
  return true;
}

bool test_call_arg_passing(const char *host, const path &prog,
                           NdbProcess::Args cmdargs,
                           const NdbProcess::Args &args) {
  cmdargs.add(args);

  NdbProcess::Pipes pipes;
  if (!pipes.connected()) {
    perror("pipes");
    fprintf(stderr, "ERROR: %s: %u: !pipes.connected()\n", __func__, __LINE__);
    return false;
  }

  std::unique_ptr<NdbProcess> proc;
  if (!host)
    proc = NdbProcess::create(prog.string().c_str(), prog.string().c_str(),
                              nullptr, cmdargs, &pipes);
  else
    proc = NdbProcess::create_via_ssh(prog.string().c_str(), host,
                                      prog.string().c_str(), nullptr, cmdargs,
                                      &pipes);
  if (!proc) {
    fprintf(stderr, "ERROR: %s: %u: !proc.create()\n", __func__, __LINE__);
    return false;
  }

  bool ok;
  FILE *rfile = pipes.open(pipes.parentRead(), "r");
  if (!rfile) {
    fprintf(stderr, "ERROR: %s: %u: !parent open read pipe()\n", __func__,
            __LINE__);
    ok = false;
  } else {
    ok = check_call_output(args, rfile);
  }
  fclose(rfile);

  int ret = 1;
  if (!proc->wait(ret, 30'000)) {
    fprintf(stderr, "Program stopped wait failed ret=%d\n", ret);
    proc->stop();
    proc->wait(ret, 30'000);
  }
  ok &= (ret == 0);

  return ok;
}

int print_arguments(int argc, const char *argv[]) {
  for (int argi = 0; argi < argc; argi++) {
    const char *arg = argv[argi];
    int len = strlen(arg);
    printf("%d\n%s\n", len, arg);
  }
  return 0;
}

int main(int argc, const char *argv[]) {
  const path prog = argv[0];
  const path full_prog = canonical(prog);
  int argi = 1;
  path ssh_prog;

  std::string cmd;
  NdbProcess::Args cmdargs;
  bool dry_run = false;
  if (argc <= 1) {
  } else if (strcmp(argv[1], "--print-arguments") == 0) {
    return print_arguments(argc - 2, argv + 2);
  }
  const char *host = nullptr;
  for (; argi < argc; argi++) {
    const char *arg = argv[argi];
    if (strcmp(arg, "--") == 0) {
      argi++;
      break;
    }
    if (strcmp(arg, "--dry-run") == 0) {
      dry_run = true;
    } else if (strncmp(arg, "--exec=", 7) == 0) {
      cmd = &arg[7];
    } else if (strncmp(arg, "--ssh=", 6) == 0) {
      host = &arg[6];
    } else if (strncmp(arg, "--ssh", 5) == 0) {
      host = "localhost";
    } else if (strncmp(arg, "--", 2) == 0) {
      fprintf(stderr, "ERROR: Unknown option '%s'.\n", arg);
      return 2;
    } else
      break;
  }
  if (cmd.empty()) {
    if (host == nullptr || strcmp(host, "localhost") == 0)
      cmd = full_prog.string();
    else
      cmd = prog.filename().string();
  }
  cmdargs.add("--print-arguments");

  if (argi < argc) {
    for (; argi < argc; argi++) {
      NdbProcess::Args testargs;
      testargs.add(argv[argi]);
      if (!dry_run) {
        bool pass = test_call_arg_passing(host, cmd, cmdargs, testargs);
        ok(pass, "arg = \"%s\"\n", argv[argi]);
      } else {
        cmdargs.add(testargs);
        fprintf(stderr, "info: %s: %u: CMD: %s\n", __func__, __LINE__,
                cmd.c_str());
        for (size_t i = 0; i < cmdargs.args().size(); i++)
          fprintf(stderr, "info: %s: %u: ARG#%zu: %s\n", __func__, __LINE__, i,
                  cmdargs.args()[i].c_str());
      }
    }
  } else {
    for (int ch = 1; ch < 256; ch++) {
      if (ch > '0' && ch < '9') continue;
      if (ch > 'a' && ch < 'z') continue;
      if (ch > 'A' && ch < 'Z') continue;
      bool expect = true;
#ifdef _WIN32
      if (host)
        expect = !(strchr("\r\n\032/", ch));  // For ssh win-cmd-c
      else
        expect = (ch != '\032');
#else
      if (host) expect = (ch != '\\');  // For ssh. Wrongly guessed as Windows
#endif
      char str[2] = {char(ch), 0};
      {
        NdbProcess::Args args;
        args.add(str);
        bool pass = test_call_arg_passing(host, cmd, cmdargs, args);
        ok(pass == expect, "arg = %c (ASCII %d) (%s)\n", (ch < 32 ? ' ' : ch),
           ch, (expect ? "supported" : "not supported"));
      }
      char str2[3] = {char(ch), 'q', 0};
      {
        NdbProcess::Args args;
        args.add(str2);
        bool pass = test_call_arg_passing(host, cmd, cmdargs, args);
        ok(pass == expect, "arg = %c%c (ASCII %d) (%s)\n", (ch < 32 ? ' ' : ch),
           str2[1], ch, (expect ? "supported" : "not supported"));
      }
    }
  }
  return exit_status();
}

#endif
