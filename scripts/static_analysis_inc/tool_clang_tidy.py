# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

import os
import sys
from multiprocessing.pool import ThreadPool
from static_analysis_inc import helpers

# Performance tweaking
# How many source code files to pass to a single clang-tidy process.
# Large batch with large number of instances can end up using
# a lot of CPU and RAM.
TIDY_BATCH = 2

TIDY_BIN = ''
BUILD_PATH = ''


def run_tidy(files):
    """Execute clang-tidy on a batch of source code files, update progress"""
    arg_files = ' '.join(files)
    # Advantages using clang-tidy directly vs using run-clang-tidy wrapper:
    # - progress tracking
    # - job control (how many parallel processes to use)
    # - timeout control (kill stuck subprocesses)
    # - better control of what files we process (filtering)
    # - speedup: process more than one file with a single clang-tidy process (batching)
    cmd = f"{TIDY_BIN} -extra-arg='-ferror-limit=0' -p {BUILD_PATH} {arg_files}"
    # do not use --quiet mode to detect "no checks enabled"
    out, err, rc = helpers.shell_execute(cmd)
    if not err or not 'Error: no checks enabled' in err:
        print(out);
    # try to avoid printing not so useful info like:
    # 352679 warnings generated.
    if rc != 0 and err and not out and not 'Error: no checks enabled' in err:
        print(err, file=sys.stderr)
    helpers.progress_update(len(files))
    helpers.progress_report()


def scan_tree(clang_tidy_binary, repo_root, jobs_count, build_path, scan_root):
    helpers.verbose_log(f"Scan source root folder: \'{repo_root}\'")

    global BUILD_PATH
    BUILD_PATH = build_path

    global TIDY_BIN
    TIDY_BIN = clang_tidy_binary

    git_branch = helpers.git_branch_name()
    helpers.verbose_log(f"Detected git branch: \'{git_branch}\'")

    target_files = helpers.list_sources(build_path, scan_root)
    helpers.progress_start(len(target_files))
    helpers.progress_report()

    # run static analysis with a thread pool
    with ThreadPool(jobs_count) as pool:
        try:
            pool.map(run_tidy, helpers.batched(target_files, TIDY_BATCH))
        except KeyboardInterrupt:
            print("\nScript aborted.", file=sys.stderr)
            helpers.progress_done()
            sys.exit(1)

    helpers.progress_done()


def scan_commit(clang_tidy_diff_script, commit, jobs_count, build_path):
    helpers.verbose_log(f"Scan commit: \'{commit}\'")

    cmd = f"git diff-tree -p {commit} -U0  -- '*.cc' '*.cpp' '*.c++' '*.cxx' '*.c' '*.cl' '*.h' '*.hpp' '*.hh' '*.i' '*.ic' ':!*/extra/*' | python3 {clang_tidy_diff_script} -timeout 600 -path {build_path} -j={jobs_count} -p1 -extra-arg='-ferror-limit=0'"
    out, err, rc = helpers.shell_execute(cmd)
    if rc != 0 and err:
        print(err, file=sys.stderr)
    print(out)
