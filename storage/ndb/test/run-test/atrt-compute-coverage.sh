#!/bin/bash

# Copyright (c) 2020 Oracle and/or its affiliates.
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

# Compute baseline.info and combines it with test coverage file of
# each test case. Runs lcov tool on TEST_COVERAGE_DIR directory to then merge
# these test coverage files to generate coverage.info file.

set -e

if [ $# -lt 2 ]; then
  echo "Usage: ${0} result-base-dir build-dir" >&2
  exit 1
fi

RESULTS_BASE_DIR="${1}"
BUILD_DIR="${2}"

TEST_COVERAGE_DIR="${RESULTS_BASE_DIR}/test_coverage"
if [ ! -d "${TEST_COVERAGE_DIR}" ]; then
  echo "Test coverage files not present to compute coverage" >&2
  exit 1
fi

RESOURCES_TO_CLEANUP=''
trap "rm -rf ${RESOURCES_TO_CLEANUP}" EXIT

BASELINE_INFO="${RESULTS_BASE_DIR}/baseline.info"
lcov -c --no-external --initial -d "${BUILD_DIR}" -o "${BASELINE_INFO}"
RESOURCES_TO_CLEANUP+="${BASELINE_INFO}"


for coverage_file in "${TEST_COVERAGE_DIR}"/*; do
  lcov -a "${BASELINE_INFO}" -a "${coverage_file}" -o "${coverage_file}"
done

find "${TEST_COVERAGE_DIR}" -name "*.info" -exec echo "-a {}" \; | \
  xargs -r -x lcov -o "${RESULTS_BASE_DIR}/coverage.info"

lcov --remove "${RESULTS_BASE_DIR}/coverage.info" "${BUILD_DIR}*" \
  -o "${RESULTS_BASE_DIR}/coverage_reduced.info"
RESOURCES_TO_CLEANUP+="${RESULTS_BASE_DIR}/coverage_reduced.info"

lcov --extract "${RESULTS_BASE_DIR}/coverage_reduced.info" '*/storage/ndb/*' \
  -o "${RESULTS_BASE_DIR}/coverage.info"
RESULT="$?"

if [ "${RESULT}" -ne 0 ]; then
  echo "Coverage report could not be generated: ${RESULT}" >&2
  exit 1
fi
