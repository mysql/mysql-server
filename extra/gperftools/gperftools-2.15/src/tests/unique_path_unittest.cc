/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
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
 */

// ---
// Author: Artem Y. Polyakov
// heavily massaged by alkondratenko@gmail.com, all bugs are mine

#include "config.h"

#include "base/sysinfo.h"

#include "base/logging.h"
#include "base/commandlineflags.h"

#include <stdlib.h>   // for environment primitives
#include <unistd.h>   // for getpid()
#include <limits.h>   // for PATH_MAX
#include <string>

struct WithEnv {
  const std::string var;
  const std::string val;
  WithEnv(const std::string& var, const std::string& val) : var{var}, val{val} {
    Reset();
  }
  ~WithEnv() {
    unsetenv(var.c_str());
  }
  // GetUniquePathFromEnv "updates" environment variable, so this
  // re-sets environment to original value.
  void Reset() {
    setenv(var.c_str(), val.c_str(), 1);
  }
};

std::string AppendPID(const std::string &str) {
  return str + "_" + std::to_string(getpid());
}

#define TEST_VAR "GPROF_TEST_PATH"
#define TEST_VAL "/var/log/some_file_name"

std::string GetTestPath() {
  char path[PATH_MAX];
  CHECK(GetUniquePathFromEnv(TEST_VAR, path));
  return path;
}

void testDefault() {
  WithEnv withTestVar(TEST_VAR, TEST_VAL);

  EXPECT_EQ(TEST_VAL, GetTestPath());

  // Now that we ran GetUniquePathFromEnv once, we can test "child"
  // case, which appends pid
  EXPECT_EQ(AppendPID(TEST_VAL), GetTestPath());

  withTestVar.Reset();
  WithEnv withForced(TEST_VAR "_USE_PID", "1");

  // Test parent case - must include PID (will set the child flag)
  EXPECT_EQ(AppendPID(TEST_VAL), GetTestPath());

  // Test child case
  EXPECT_EQ(AppendPID(TEST_VAL), GetTestPath());
}

// PMIx is one type of MPI environments that we detect specially. We
// expect .rank-${PMIX_RANK} to be appended when we detect this
// environment.
void testPMIx() {
  WithEnv rank("PMIX_RANK", "5");
  WithEnv withTestVar(TEST_VAR, TEST_VAL);

  const auto expectedParent = TEST_VAL ".rank-5";
  const auto expectedChild = AppendPID(expectedParent);

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedParent, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedChild, GetTestPath());

  withTestVar.Reset();
  WithEnv withForced(TEST_VAR "_USE_PID", "1");

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedChild, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedChild, GetTestPath());
}

// Slurm is another type of MPI environments that we detect
// specially. When only SLURM_JOB_ID is detected we force-append pid,
// when SLURM_PROCID is given, we append ".slurmid-${SLURM_PROCID}
void testSlurm() {
  WithEnv withJobID("SLURM_JOB_ID", "1");
  WithEnv withTestVar(TEST_VAR, TEST_VAL);

  auto pathWithPID = AppendPID(TEST_VAL);

  // Test non-forced case (no process ID found)
  EXPECT_EQ(pathWithPID, GetTestPath());

  // Now that we ran GetUniquePathFromEnv once, we can test "child"
  // case, which appends pid
  EXPECT_EQ(pathWithPID, GetTestPath());

  withTestVar.Reset();
  WithEnv withRank("SLURM_PROCID", "5");

  const auto expectedSlurmParent = TEST_VAL ".slurmid-5";
  const auto expectedSlurmChild = AppendPID(expectedSlurmParent);

  // Test parent case - must include "proc id" (will set the child flag)
  EXPECT_EQ(expectedSlurmParent, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedSlurmChild, GetTestPath());

  withTestVar.Reset();
  WithEnv withForced(TEST_VAR "_USE_PID", "1");

  // Now that pid is forced we expect both pid and proc-id appended.
  EXPECT_EQ(expectedSlurmChild, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedSlurmChild, GetTestPath());
}

// Open MPI is another type of MPI environments that we detect. We
// force-append pid if we detect it.
void testOMPI() {
  WithEnv withOMPI("OMPI_HOME", "/some/path");
  WithEnv withTestVar(TEST_VAR, TEST_VAL);

  const auto expectedPath = AppendPID(TEST_VAL);

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedPath, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedPath, GetTestPath());

  withTestVar.Reset();
  WithEnv withForced(TEST_VAR "_USE_PID", "1");

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedPath, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedPath, GetTestPath());
}

// MPICH is another type of MPI environment that we detect. We
// expect .rank-${PMI_RANK} to be appended when we detect this
// environment.
void testMPICH() {
  WithEnv rank("PMI_RANK", "5");
  WithEnv withTestVar(TEST_VAR, TEST_VAL);

  const auto expectedParent = TEST_VAL ".rank-5";
  const auto expectedChild = AppendPID(expectedParent);

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedParent, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedChild, GetTestPath());

  withTestVar.Reset();
  WithEnv withForced(TEST_VAR "_USE_PID", "1");

  // Test parent case (will set the child flag)
  EXPECT_EQ(expectedChild, GetTestPath());

  // Test child case
  EXPECT_EQ(expectedChild, GetTestPath());
}

int main(int argc, char** argv) {
  testDefault();
  testPMIx();
  testSlurm();
  testOMPI();
  testMPICH();

  printf("PASS\n");
  return 0;
}
