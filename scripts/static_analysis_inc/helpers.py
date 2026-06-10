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

import datetime
import itertools
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

PROCESSED_FILES = 0
TOTAL_FILES = 0
START_TIME = 0
VERBOSE = 0
JOB_TIMEOUT_SEC = 20*60


def verbose_log(message, force=False):
    if VERBOSE or force:
        print(message, file=sys.stderr, flush=True)


def get_cpu_count():
    return os.cpu_count()


def is_official_tool(scan_tree, git_branch, version):
    """Official clang-tidy version is v15 for all targets"""
    return (version == 15)


def pick_tidy_binary(tidy_binary, tidy_diff_script, scan_tree, no_warn):
    """Select clang-tidy binary and clang_tidy_diff.py unless already specified by user.
    Selection heuristic assumes using tidy v15 for legacy code (scanning entire repo),
    and more modern one for new code (scanning single commit or working in mysql-trunk branch).
    """
    git_branch = git_branch_name()
    top_dir = git_repository_root_dir()
    # more complex git repo availability test to support detached HEAD mode
    if not git_branch and not top_dir and not scan_tree:
        print("Scanning a commit requires the presence of git repo!",
              file=sys.stderr)
        return None, None

    if tidy_binary is None:
        # Only search what's in the $PATH
        tidy_binary = shutil.which("clang-tidy")
        if not tidy_binary or not os.path.isfile(tidy_binary):
            tidy_binary = None

        verbose_log(f"Detected clang-tidy binary: \'{tidy_binary}\'")

        # Optional warning when detected tidy tool is not
        # of the official verson for a given scan target.
        if tidy_binary and not no_warn:
            version = get_tidy_major_version(tidy_binary)
            if not is_official_tool(scan_tree, git_branch, version):
                verbose_log(
                    "WARNING: Not using official clang-tidy binary version for this scan target.",
                    True)

    elif not os.path.isfile(tidy_binary):
        # Path given, but does not exist
        tidy_binary = None

    if tidy_diff_script is None:
        # Pick latest available version for trunk branch
        if not git_branch or git_branch == "mysql-trunk":
            tidy_diff_script = "/opt/llvm-17.0.1/share/clang/clang-tidy-diff.py"
        # Use v15 for legacy branches or as a fallback
        if not tidy_diff_script or not os.path.isfile(tidy_diff_script):
            tidy_diff_script = "/opt/llvm-15.0.7/share/clang/clang-tidy-diff.py"
        # Try default script for usual clang-tidy default
        if not tidy_diff_script or not os.path.isfile(tidy_diff_script):
            if tidy_binary == "/usr/bin/clang-tidy":
               tidy_diff_script  = "/usr/share/clang/clang-tidy-diff.py"
        if not tidy_diff_script or not os.path.isfile(tidy_diff_script):
            tidy_diff_script = None

        verbose_log(
            f"Detected clang-tidy-diff.py script: \'{tidy_diff_script}\'")
    elif not os.path.isfile(tidy_diff_script):
        # Path given, but does not exist
        tidy_diff_script = None

    return tidy_binary, tidy_diff_script


def detect_source_root_dir():
    # Get git top dir assuming we are within git repository
    root_dir = git_repository_root_dir()
    if not root_dir:
        root_dir = get_current_dir()
        verbose_log(
            f"Assume current work dir is the source tree root: \'{root_dir}\'")
    return root_dir


def get_tidy_major_version(tidy_binary):
    # Returns tidy major version number or 0 on failure
    # Parses clang-tidy version output like (with result 15):
    # Ubuntu LLVM version 15.0.7
    #   Optimized build.
    #   Default target: x86_64-pc-linux-gnu
    #   Host CPU: tigerlake
    version, _, _ = shell_execute(f"{tidy_binary} --version")
    try:
        pos = version.index('version ')
    except ValueError:
        return 0
    version = version[pos + len('version '):]
    try:
        pos = version.index('.')
    except ValueError:
        return 0
    version = version[:pos]
    return int(version)


def git_repository_root_dir():
    """Return root directory, assumes this script executed somewhere
    within the git repository."""
    repo_root, _, _ = shell_execute("git rev-parse --show-toplevel")
    return repo_root.strip()


def git_branch_name():
    git_branch, _, _ = shell_execute("git branch --show-current")
    return git_branch.strip()


def get_current_dir():
    return os.getcwd()


def find_build_path(repo_root):
    # test if current folder is build path
    build_path = get_current_dir()
    if os.path.isfile(os.path.join(build_path, 'compile_commands.json')):
        verbose_log("Detected current working directory as build path")
        return '.'

    # fallback to "<root_dir>/bld"
    if not repo_root:
        repo_root = git_repository_root_dir()
    build_path = os.path.join(repo_root, 'bld')
    if os.path.isfile(os.path.join(build_path, 'compile_commands.json')):
        verbose_log(f"Detected build path as: \'{build_path}\'")
        return build_path

    # last try: search within root for folder having 'compile_commands.json'
    working_dir = Path(repo_root)
    for path in working_dir.glob("**/compile_commands.json"):
        build_path = os.path.dirname(path)
        verbose_log(f"Found build path as: \'{build_path}\'")
        return build_path

    verbose_log("Failed to detect build path (compile_commands.json missing)", True)
    return None


def list_sources(build_path, scan_root):
    # get (filtered) list of files from compile commands
    files = []
    with open(os.path.join(build_path, 'compile_commands.json')) as f:
        compile_commands = json.load(f)
        for item in compile_commands:
            path = item["file"]
            # Filter by scan-root if defined
            if scan_root and not path.startswith(scan_root):
                continue
            # Filter out the 3rd party code
            if '/extra/' in item["file"]:
                continue
            # File must exist (compile DB can be old)
            if not os.path.isfile(path):
                continue
            files.append(path)

    # remove duplicates
    files = list(dict.fromkeys(files))

    verbose_log(f"Found {len(files)} source code files to process")
    return files


def batched(iterable, n):
    """Batch data into lists of length n. The last batch may be shorter."""
    # batched('ABCDEFG', 3) --> [ABC] [DEF] [G]
    it = iter(iterable)
    while True:
        batch = list(itertools.islice(it, n))
        if not batch:
            return
        yield batch


def shell_execute(cmd):
    """Execute given shell command, return stdout, stderr outputs"""
    try:
       process = subprocess.run(cmd,
                             timeout=JOB_TIMEOUT_SEC,
                             universal_newlines=True,
                             shell=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.TimeoutExpired:
        verbose_log(f"\nCommand timed out: {cmd}", True)
        return None, None, 1
    return process.stdout, process.stderr, process.returncode


def progress_start(total):
    global START_TIME, TOTAL_FILES
    START_TIME = time.time()
    TOTAL_FILES = total


def progress_update(increment):
    """Periodically updates progress report"""
    global PROCESSED_FILES
    PROCESSED_FILES += increment


def progress_report():
    """Write progress report to console"""
    global PROCESSED_FILES, START_TIME, TOTAL_FILES

    if TOTAL_FILES > 0 and PROCESSED_FILES > 0:
        time_elapsed = int(time.time() - START_TIME)
        time_per_file = time_elapsed / PROCESSED_FILES
        time_remain = int((TOTAL_FILES - PROCESSED_FILES) * time_per_file)
        percent_done = 100 * PROCESSED_FILES / TOTAL_FILES
        time_elapsed_msg = datetime.timedelta(seconds=time_elapsed)
        time_remain_msg = datetime.timedelta(seconds=time_remain)
        print(
            f"\r{percent_done:.1f}% ({PROCESSED_FILES}/{TOTAL_FILES} files - {time_elapsed_msg} elapsed, {time_remain_msg} remaining)",
            end='', file=sys.stderr, flush=True)
    elif TOTAL_FILES > 0:
        # no files processed yet
        print(f"\r0% (0/{TOTAL_FILES} files)", end='', file=sys.stderr,
              flush=True)


def progress_done():
    # extra spaces to overwrite old progress text in the same line
    end_time = time.time()
    elapsed_time_msg = datetime.timedelta(seconds=int(end_time - START_TIME))
    print(
        f"\rDone in {elapsed_time_msg}                                              ",
        file=sys.stderr, flush=True)
