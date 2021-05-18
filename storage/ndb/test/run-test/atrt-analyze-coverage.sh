#!/bin/bash

# Copyright (c) 2020 Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Input parameters:
#  * RESULTS_BASE_DIR = Directory holding "result/result.N" directories.
#  * BUILD_DIR = Directory where source code was built.
#  * LCOV_FILES_DIR = Directory that stores lcov file of each test case.
#  * TEST_CASE = Test case name and number are appended to form this
#     parameter. This parameter will be used as a parameter to --test-name(-t)
#     option in lcov tool.

# Copies ".gcno" files into result/coverage/hostDir that holds ".gcda" files
# after atrt-gather-result.sh is run with --coverage parameter. lcov is tool
# is run to obtain ".info" files for each host and are stored within
# result/coverage directory.

set -e

if [ $# -lt 4 ]; then
  echo "Usage: atrt-analyze-coverage results-base-dir build-dir" \
       "lcov-files-dir test-case-name-number" >&2
  exit 1
fi

RESULTS_BASE_DIR="${1}"
BUILD_DIR="${2}"
LCOV_FILES_DIR="${3}"
TEST_CASE="${4}"
shift 4

if [ ! -d "$RESULTS_BASE_DIR/result/coverage/" ]; then
  echo "Directory storing coverage files not found" >&2
  exit 1
fi

GCNO_FILES=$(find "$BUILD_DIR" -name "*.gcno" | wc -l)
if [ "$GCNO_FILES" -eq 0 ]; then
  echo "Gcno files are not present in build directory, coverage cannot" \
       "be computed." >&2
  exit 1
fi

mkdir -p "$LCOV_FILES_DIR"

cd "$RESULTS_BASE_DIR/result/coverage/"
RESOURCES_TO_CLEANUP=''
trap 'rm -rf $RESOURCES_TO_CLEANUP' EXIT
if [ ! -f "$LCOV_FILES_DIR/baseline.info" ]; then
  RESOURCES_TO_CLEANUP+=("$LCOV_FILES_DIR/baseline.info")
  lcov -c --initial -d "$BUILD_DIR" -o "$LCOV_FILES_DIR/baseline.info"
fi

for host_dir in */; do
  host_dir=${host_dir%%/}
  RESOURCES_TO_CLEANUP+="$RESULTS_BASE_DIR/result/coverage/$host_dir "
  find "$host_dir" -name '*.gcda' -printf '%P\n' | \
    sed -e 's/[.]gcda$/.gcno/' > "$host_dir/gcno-files"
  rsync -am --files-from="$host_dir/gcno-files" "$BUILD_DIR" "$host_dir"
  lcov -d "$host_dir" -c -t "$TEST_CASE" -o "$host_dir.info"
done

find . -name "*.info" -exec echo "-a {}" \; | \
  xargs -r -x lcov -o "$LCOV_FILES_DIR/$TEST_CASE.info"
RESULT="$?"

if [ "$RESULT" -ne 0 ]; then
  echo "Coverage Analysis Failed: $RESULT" >&2
  exit 1
fi
