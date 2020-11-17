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
#  * LCOV_FILES_DIR = Directory that stores lcov file of each test case.
#  * RESULTS_BASE_DIR = Directory holding "result/result.N" directories.

# Runs lcov tool on LCOV_FILES_DIR to merge info files of every test case
# to generate final_coverage.info file. This file will then be used by genhtml
# tool to generate html coverage report for the test run.

set -e

if [ $# -lt 2 ]; then
  echo "Usage: atrt-compute-coverage lcov-files-dir result-base-dir" >&2
  exit 1
fi

LCOV_FILES_DIR="$1"
RESULTS_BASE_DIR="$2"

find "$LCOV_FILES_DIR" -name "*.info" -exec echo "-a {}" \; | \
 xargs -r -x lcov --no-external -o "$RESULTS_BASE_DIR/final_coverage.info"

genhtml "$RESULTS_BASE_DIR/final_coverage.info" --show-details \
  --ignore-errors source -o "$RESULTS_BASE_DIR/coverage_report"
RESULT="$?"

if [ "$RESULT" -ne 0 ]; then
  echo "Html report could not be generated: $RESULT" >&2
  exit 1
fi
